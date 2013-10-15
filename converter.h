#ifndef CONVERTER_H
#define CONVERTER_H

#include <QString>
#include <QFileInfoList>
#include <QStringList>
#include <QJsonDocument>

#include "mainwindow.h"
#include "convertexception.h"

class Converter
{
private:
    QJsonDocument components;
    QJsonDocument tables;
    QString inPath;
    QString outPath;
    QByteArray json;
    QFileInfoList fileList;
	MainWindow* window;
	bool stop; // stops execution if set to true
	bool error; // some error occurred
    
    void loadConfig();
	void setFileList();
	
	QJsonValue decodePresetHeader(QByteArray blob) throw(ConvertException);
	QJsonObject decodeComponents(QByteArray blob, uint offset) throw(ConvertException);
	
public:
    Converter(QString inPath, QString outPath, MainWindow* window);
    
    QStringList getFileNameList();
	void logComponents();
    QString convertAll();
    QString convertAll(QString inPath);
	QString convertSingle(QFileInfo file);
	void setStop();
};

#endif // CONVERTER_H
