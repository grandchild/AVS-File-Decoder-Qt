#ifndef CONVERTER_H
#define CONVERTER_H

#include <QString>
#include <QFileInfoList>
#include <QStringList>
#include <QJsonDocument>

#include "mainwindow.h"

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
public:
    Converter(QString inPath, QString outPath, MainWindow* window);
    
    QStringList getFileNameList();
	void logComponents();
    QString convert();
    QString convert(QString inPath);
};

#endif // CONVERTER_H
