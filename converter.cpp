#include "converter.h"

#include <QDebug>
#include <QDir>
#include <QFileInfoList>
#include <QDirIterator>
#include <QByteArray>
#include <QJsonObject>
#include <QDateTime>

#include "defines.h"

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
		
		this->components = QJsonDocument::fromJson(componentsJSON, &parseError);
		if(parseError.error != QJsonParseError::NoError) {
			window->log(QString(parseError.errorString()+" at offset %1 in '"+componentsFile.fileName()+"'").arg(parseError.offset), true);
		}
	} else {
		window->log("'"+componentsFile.fileName()+"' does not exist.");
    }
	
    QFile tablesFile("tables.json");
	if(tablesFile.exists()) {
		tablesFile.open(QFile::ReadOnly);
		QByteArray tablesJSON = tablesFile.readAll();
		tablesFile.close();
		
		this->tables = QJsonDocument::fromJson(tablesJSON, &parseError);
		if(parseError.error != QJsonParseError::NoError) {
			window->log(QString(parseError.errorString()+" at offset %1 in '"+tablesFile.fileName()+"'").arg(parseError.offset), true);
		}
	} else {
		window->log("'"+tablesFile.fileName()+"' does not exist.");
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
	window->log("Comps: "+QString(components.toJson(QJsonDocument::Compact)));
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

QJsonObject
Converter::decodeComponents(QByteArray blob, uint offset) throw(ConvertException) {
	return QJsonObject();
}

void
Converter::setStop() {
	stop = true;
}
