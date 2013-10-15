#include "converter.h"

#include <QDebug>
#include <QDir>
#include <QFileInfoList>
#include <QDirIterator>
#include <QByteArray>
#include <QJsonDocument>
#include <QDateTime>

#include "defines.h"
#include "component.h"

Converter::Converter(QString inPath, QString outPath, MainWindow* window):
    inPath(inPath),
    outPath(outPath),
	window(window),
	stop(false),
	error(false)
{
    loadConfig();
	setFileList();
}

void
Converter::loadConfig() {
    QJsonParseError parseError;
    
    QFile componentsFile(QDir::currentPath()+"/components.json");
	if(componentsFile.exists()) {
		componentsFile.open(QFile::ReadOnly);
		QByteArray componentsJSON = componentsFile.readAll();
		componentsFile.close();
		
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
				componentApeCodes[i] = apeCode;
			}
		}
	} else {
		window->log("'"+componentsFile.fileName()+"' does not exist.", /*error*/true);
    }
	
    QFile tablesFile("tables.json");
	if(tablesFile.exists()) {
		tablesFile.open(QFile::ReadOnly);
		QByteArray tablesJSON = tablesFile.readAll();
		tablesFile.close();
		
		tables = QJsonDocument::fromJson(tablesJSON, &parseError).object();
		if(parseError.error != QJsonParseError::NoError) {
			window->log(QString(parseError.errorString()+" at offset %1 in '"+tablesFile.fileName()+"'").arg(parseError.offset), /*error*/true);
		}
	} else {
		window->log("'"+tablesFile.fileName()+"' does not exist.", /*error*/true);
	}
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

QString
Converter::convertAll(QString inPath) {
	if(error) return QString();
    this->inPath = inPath;
	setFileList();
    return convertAll();
}

void
Converter::logComponents() {
	window->log("Comps: "+QString(QJsonDocument(components).toJson(QJsonDocument::Compact)));
}

QString
Converter::convertAll() {
	if(error) return QString();
	window->setProgressMax(fileList.length());
	foreach(QFileInfo file, fileList) {
		if(stop || error) break;
		QString json = convertSingle(file);
		bool ok = !json.isEmpty();
		if(ok) {
			window->log("<b>"+file.fileName()+"</b> converted.");
			window->setSampleName(file.fileName());
			// write to file here...
			window->output(json);
		} else {
		}
		window->incrProgress();
	}
	return QString();
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
	rootObj["name"] = file.fileName();
	rootObj["date"] = file.lastModified().toString(Qt::ISODate);
	//rootObj["author"] = guessAuthor(file.fileName()); // TODO: extract likely author name from filename
	try {
		rootObj["clearFrame"] = decodePresetHeader(blob);
		rootObj["components"] = decodeComponents(blob, AVS_HEADER_LENGTH);
	} catch(ConvertException e) {
		window->log("Failed to convert: <b>"+e.qwhat()+"</b>", /*error*/true);
	}
	
	preset.setObject(rootObj);
	
	QString json = preset.toJson(QJsonDocument::Indented);
	ok = !json.isEmpty();
	if(ok) {
		return json;
	} else {
		window->log("Output for '"+file.fileName()+"' is empty.", /*error*/true);
		return "";
	}
}

QJsonValue
Converter::decodePresetHeader(QByteArray blob) throw(ConvertException) {
	QByteArray presetHeader0_1("Nullsoft AVS Preset 0.1");
	QByteArray presetHeader0_2("Nullsoft AVS Preset 0.2");
	presetHeader0_1[AVS_HEADER_LENGTH-2] = 0x1A; // there is one final '26' byte after the readable header...
	presetHeader0_2[AVS_HEADER_LENGTH-2] = 0x1A; // ...and repurpose the \0 byte position from the str ctor above.
	if(!blob.startsWith(presetHeader0_2) &&
		!blob.startsWith(presetHeader0_1)) { // 0.1 only if 0.2 failed because it's far rarer.
		throw new ConvertException("Invalid preset header.");
	}
	return QJsonValue(blob[AVS_HEADER_LENGTH-1]==(char)1); // "Clear Every Frame"
}

QJsonArray
Converter::decodeComponents(QByteArray blob, uint offset) throw(ConvertException) {
	QJsonArray componentArray;
	while(offset < (uint)blob.length()) {
		uint code = Component::getUInt32(blob, offset);
		int i = getComponentIndex(code, blob, offset);
		bool isDll = code!=0xfffffffe && code>BUILTIN_MAX;
		uint size = Component::getUInt32(blob, offset+SIZE_INT+isDll*32);
		QJsonObject res;
		if(i<0) {
			res["type"] = QJsonValue("Unknown: ("+QString("%1").arg(-i)+")");
		} else {
			offset = offset+SIZE_INT*2+isDll*32;
			QJsonObject compConf = components[i].toObject();
			Component comp(
						blob,
						offset,
						offset+size,
						compConf["name"].toString(),
						compConf["group"].toString(),
						compConf["func"].toString(),
						compConf["fields"].toObject(),
						window);
			//res = comp.decode();
		}
		componentArray.append(res);
		offset += size + SIZE_INT*2 + isDll*32;
	}
	return components;
}

int
Converter::getComponentIndex(uint code, QByteArray blob, uint offset) {
	int limit = components.size();
	for (int i = 0; i < components.size(); i++) {
		QJsonValue currentCode = components[i].toObject()["code"];
		if(currentCode.isString()) {
			if(code == (uint)(currentCode.toString().toUInt(0, 0))) {
				window->log("Found component: "+components[i].toObject()["name"].toString()+" ("+QString("%1").arg(code)+")");
				return i;
			}
		} else if(currentCode.isArray()) {
			if(blob.mid(offset+SIZE_INT, 32).startsWith(componentApeCodes[i])) {
				window->log("Found component: "+components[i].toObject()["name"].toString());
				return i;
			}
		}
	}
	window->log("Found unknown component (code: "+QString("%1").arg(code)+")", /*error*/true);
	return -code;
}

void
Converter::setStop() {
	stop = true;
}
