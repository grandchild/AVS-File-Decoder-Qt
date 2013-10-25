#include "settings.h"

#include <QFile>
#include <QDir>
#include <QDebug>

Settings::Settings(QString settingsFile):
	file(settingsFile)
{
	readSettingsFile();
}

Settings::Settings(Settings* other) {
	file = other->getFile();
	copyFrom(other);
}

void
Settings::copyFrom(Settings* other) {
	this->logLevel = other->logLevel;
	this->indent = other->indent;
	this->minimize = other->minimize;
	this->compactKernels = other->compactKernels;
}

bool
Settings::readSettingsFile() {
	QFile settingsFile(QDir::currentPath()+"/"+file);
	if(settingsFile.exists()) {
		settingsFile.open(QFile::ReadOnly);
		QByteArray settingsLine;
		QRegExp varVal("([a-zA-Z]+)\\s*=\\s*(\\S+)");
		bool ok = true;
		do {
			settingsLine = settingsFile.readLine();
			varVal.indexIn(settingsLine);
			qDebug() << "decoding " << settingsLine << " - cap(1) " << varVal.cap(1);
			if(varVal.cap(1)=="logLevel") {
				logLevel = varVal.cap(2).toInt(&ok);
				qDebug() << "logLevel " << logLevel;
			} else if(varVal.cap(1)=="indent") {
				indent = varVal.cap(2).toInt(&ok)==1;
				qDebug() << "indent " << indent;
			} else if(varVal.cap(1)=="minimize") {
				minimize = varVal.cap(2).toInt(&ok)==1;
				qDebug() << "minimize " << minimize;
			} else if(varVal.cap(1)=="compactKernels") {
				compactKernels = varVal.cap(2).toInt(&ok)==1;
				qDebug() << "compactKernels " << compactKernels;
			}
			if(!ok) break;
		} while(!settingsLine.isEmpty());
		settingsFile.close();
		return ok;
	} else {
		return false;
	}
}


bool
Settings::writeSettingsFile() {
	QFile settingsFile(QDir::currentPath()+"/"+file);
	settingsFile.open(QFile::WriteOnly);
	QByteArray outBA(
			(
				QString("logLevel=%1\r\n").arg(logLevel)+
				QString("indent=%1\r\n").arg(indent==1?1:0)+
				QString("minimize=%1\r\n").arg(minimize==1?1:0)+
				QString("compactKernels=%1\r\n").arg(compactKernels==1?1:0)
			).toLatin1()
		);
	settingsFile.write(outBA.data());
	settingsFile.close();
	return true;
}
