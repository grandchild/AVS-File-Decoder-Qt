#include "converter.h"

#include <QDebug>
#include <QDir>
#include <QFileInfoList>
#include <QDirIterator>
#include <QByteArray>
#include <QJsonDocument>
#include <QDateTime>

#include "defines.h"
#include "components.h"

Converter::Converter(QString inPath, QString outPath, MainWindow* window, Settings* settings):
    inPath(inPath),
    outPath(outPath),
	window(window),
	settings(settings),
	stop(false),
	error(false)
{
    loadConfig();
	setFileList();
}

void
Converter::loadConfig() {
    QJsonParseError parseError;
    
	// components.json
    QFile componentsFile(QDir::currentPath()+"/components.json");
	if(componentsFile.exists()) {
		componentsFile.open(QFile::ReadOnly);
		QByteArray componentsJSON = componentsFile.readAll();
		componentsFile.close();
		
		componentsJSON = clearComments(componentsJSON);
		components = QJsonDocument::fromJson(componentsJSON, &parseError).array();
		if(parseError.error != QJsonParseError::NoError) {
			window->log(QString(parseError.errorString()+" at offset %1 in '"+componentsFile.fileName()+"'").arg(parseError.offset), /*error*/true);
		}
		
		for(int i=0; i<components.size(); i++) {
			QJsonValue code = components[i].toObject()["code"];
			if(code.isArray()) {
				QByteArray apeCode;
				foreach(QJsonValue byte, code.toArray()) {
					apeCode.append((char)byte.toString().toUInt(0, 0));
				}
				componentDllCodes[i] = apeCode;
			}
		}
	} else {
		window->log("'"+componentsFile.fileName()+"' does not exist.", /*error*/true);
    }
	
	// tables.json
	QFile tablesFile("tables.json");
	if(tablesFile.exists()) {
		tablesFile.open(QFile::ReadOnly);
		QByteArray tablesJSON = tablesFile.readAll();
		tablesFile.close();
		
		tablesJSON = clearComments(tablesJSON);
		tables = QJsonDocument::fromJson(tablesJSON, &parseError).object();
		if(parseError.error != QJsonParseError::NoError) {
			window->log(QString(parseError.errorString()+" at offset %1 in '"+tablesFile.fileName()+"'").arg(parseError.offset), /*error*/true);
		}
	} else {
		window->log("'"+tablesFile.fileName()+"' does not exist.", /*error*/true);
	}
}

QByteArray
Converter::clearComments(QByteArray json) {
	char* jsonOut = new char[json.size()];
	bool skip = false;
	bool string = false;
	char* c = json.data();
	uint i=0;
	do {
		if(*c=='\"') {
			string = !string;
		}
		if(!string && *c=='/' && *(c+1)=='/') {
			skip = true;
		}
		if(*c=='\x0A' || *c=='\x0D') {
			skip = false;
		}
		jsonOut[i] = *(c++);
		if(!skip) {
			++i;
		}
	} while(*c!='\0');
	QByteArray jsonOutBA(jsonOut, i);
	delete[] jsonOut;
	return jsonOutBA;
}

void
Converter::setFileList() {
	if(QFileInfo(inPath).exists()) {
		fileList.clear();
		QDirIterator directory_walker(inPath, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
		while(directory_walker.hasNext()) {
			directory_walker.next();
			QFileInfo fileInfo = directory_walker.fileInfo();
			if(fileInfo.completeSuffix() == "avs") {
				fileList.append(fileInfo);
			}
		}
	} else {
		error = true;
		window->log("Input path does not exist.", /*error*/true);
	}
}

QStringList
Converter::getFileNameList() {
	if(error) return QStringList();
	QStringList fileNameList;
	foreach (QFileInfo file, fileList) {
		QString relPath = file.filePath();
		relPath.remove(inPath);
		fileNameList.append(relPath);
	}
	return fileNameList;
}

bool
Converter::convert(uint index) {
	if(error) return false;
	if(fileList.empty()) {
		setFileList();
	}
	QString json = convertSingle(fileList[index]);
	window->output(json);
	return true;
}

void
Converter::logComponents() {
	window->log("Comps: "+QString(QJsonDocument(components).toJson(QJsonDocument::Compact)));
}

bool
Converter::convertAll() {
	if(error) return false;
	window->setProgressMax(fileList.length()-1); // why it's length()-1 i have no idea...
	foreach(QFileInfo file, fileList) {
		if(stop || error) break;
		QString json = convertSingle(file);
		bool ok = !json.isEmpty();
		if(ok) {
			// post-process here...
			// json = postProcess(json);
			
			// write to file here...
			// write(file, json);
			
			window->output(json);
		}
		window->incrProgress();
	}
	return true;
}

QString
Converter::convertSingle(QFileInfo file) {
	QByteArray blob;
	if(file.exists()) {
		QFile presetFile(file.filePath());
		presetFile.open(QFile::ReadOnly);
		blob = presetFile.readAll();
		presetFile.close();
	} else {
		window->log("File '"+file.fileName()+"' not found.", /*error*/true);
		return "";
	}
	
	// now build the preset JSON document
	bool ok = true;
	QJsonDocument preset;
	QJsonObject rootObj;
	Components comps(&components,
					 &tables,
					 &componentDllCodes,
					 blob,
					 /*offset*/AVS_HEADER_LENGTH,
					 window);
	rootObj[comps.ordKey("name")] = file.fileName();
	rootObj[comps.ordKey("date")] = file.lastModified().toString(Qt::ISODate);
	//rootObj["author"] = guessAuthor(file.fileName()); // TODO: extract likely author name from filename
	QJsonValue clearFrame = decodePresetHeader(blob);
	if(clearFrame.isBool()) {
		rootObj[comps.ordKey("clearFrame")] = clearFrame;
		rootObj[comps.ordKey("components")] = comps.decode();
		ok &= !comps.error();
	} else {
		window->log("Invalid header", /*error*/true);
		ok = false;
	}
	
	preset.setObject(rootObj);
	QString json = postProcess(preset);
	if(json.isEmpty()) {
		window->log("JSON output is empty.", /*error*/true);
		ok = false;
	}
	if(!ok) {
		window->log("Failed to convert <b>"+file.fileName()+"</b>.", /*error*/true);
		return "";
	} else {
		window->log("<b>"+file.fileName()+"</b> converted.");
		return json;
	}
}

QJsonValue
Converter::decodePresetHeader(QByteArray blob) {
	QByteArray presetHeader0_1("Nullsoft AVS Preset 0.1");
	QByteArray presetHeader0_2("Nullsoft AVS Preset 0.2");
	presetHeader0_1[AVS_HEADER_LENGTH-2] = 0x1A; // there is one final '26' byte after the readable header ...
	presetHeader0_2[AVS_HEADER_LENGTH-2] = 0x1A; // ... for that repurpose the \0 byte position from the str c'tor above.
	if(!blob.startsWith(presetHeader0_2) &&
		!blob.startsWith(presetHeader0_1)) { // 0.1 only if 0.2 failed because it's far rarer.
		window->log("Invalid preset header.", /*error*/true);
		return QJsonValue(0);
	}
	return QJsonValue(blob[AVS_HEADER_LENGTH-1]==(char)1); // "Clear Every Frame"
}

QByteArray
Converter::postProcess(QJsonDocument preset) {	
	QByteArray json = preset.toJson(window->indent() ? QJsonDocument::Indented : QJsonDocument::Compact);
	
	// remove ordering indices (TODO: how much deep copying is done here? more than once? optimization possible? measure!)	
	QString jsonStr(json);
	jsonStr.replace(QRegExp("([^\\\\])\"__\\d{5}__"), "\\1\"");
	
	// minimize empty fields
	if(settings->getMinimize()) {
		QString rx = "\"\\w+\" ?: ?\"\",?";
		if(settings->getIndent()) {
			rx = "(\\n|\\r|\\n\\r) *"+rx;
		}
		jsonStr.replace(QRegExp(rx), "");
	}
	json = jsonStr.toLatin1();
	
	// format convolution kernels
	/*
	QRegExp kernelRegEx("\"kernel\":");
	int pos = 0;
	uint length = 0;
	while((pos = kernelRegEx.indexIn(json, pos+length)) >= 0) {
		length = kernelRegEx.matchedLength();
		
	}
	//*/
	return json;
}

void
Converter::setStop() {
	stop = true;
}
