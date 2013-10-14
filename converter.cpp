#include "converter.h"

#include <QDebug>
#include <QDir>
#include <QFileInfoList>
#include <QDirIterator>
#include <QByteArray>
#include <QJsonObject>
#include <QDateTime>

#include "convertexception.h"

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
Converter::convert(QString inPath) {
	if(error) return QString();
    this->inPath = inPath;
	setFileList();
    return convert();
}

void
Converter::logComponents() {
	window->log("Comps: "+QString(components.toJson(QJsonDocument::Compact)));
}

QString
Converter::convert() {
	if(error) return QString();
	foreach(QFileInfo file, fileList) {
		if(stop || error) break;
		QFile presetFile(file.fileName());
		presetFile.open(QFile::ReadOnly);
		QByteArray presetData = presetFile.readAll();
		presetFile.close();
		
		// now build the preset JSON document
		QJsonDocument preset;
		preset.setObject(QJsonObject());
		preset.object()["name"] = file.fileName();
		preset.object()["date"] = file.lastModified().toString(Qt::ISODate);
		//preset.object()["author"] = guessAuthor(file.fileName()); // TODO: extract likely author name from filename
		
		
		bool ok = true;
		json = preset.toJson(QJsonDocument::Indented);
		
		if(!ok) {
			window->log("Failed to convert to JSON string.", /*error*/true);
		}
	}
	return QString();
}

