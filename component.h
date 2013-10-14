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
private:
    QString name;
    unsigned int code;
    QString group;
    QString function;
    QJsonObject fields;
	MainWindow* window;
	
    QJsonObject decodeGeneric(QByteArray blob, uint offset, uint end) throw(ConvertException);
	QJsonObject callSizeFunction(QString function, QByteArray blob, uint offset, QJsonValue other = QJsonValue()) throw(ConvertException);
	uint getUInt32(QByteArray blob, uint offset);
	void log(QString message);
public:
    Component(QString name, unsigned int code, QString group, QString function, QJsonObject fields = QJsonObject(), MainWindow* window = NULL);
    QJsonObject decode(QByteArray blob, uint offset, uint end) throw(ConvertException);
};

#endif // COMPONENT_H
