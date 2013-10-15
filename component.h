#ifndef COMPONENT_H
#define COMPONENT_H

#include <QString>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "mainwindow.h"
#include "converter.h"
#include "convertexception.h"

class Component
{
public:
    Component(QByteArray blob,
			  uint offset,
			  uint end,
			  QString name,
			  QString group,
			  QString function,
			  QJsonObject fields = QJsonObject(),
			  MainWindow* window = NULL);
    QJsonObject decode() throw(ConvertException);
	static uint getUInt32(QByteArray blob, uint offset);
	
private:
	QByteArray blob;
	uint offset;
	uint end;
    QString name;
    QString group;
    QString function;
    QJsonObject fields;
    MainWindow* window;
    
    QJsonObject decodeGeneric() throw(ConvertException);
    QJsonObject callSizeFunction(QString function, uint offset, QJsonValue other = QJsonValue()) throw(ConvertException);
    void log(QString message);
};

#endif // COMPONENT_H
