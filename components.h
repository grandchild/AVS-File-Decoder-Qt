#ifndef COMPONENT_H
#define COMPONENT_H

#include <QString>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "mainwindow.h"
#include "converter.h"
//#include "convertexception.h"

class Components
{
public:
    Components(QJsonArray* components,
			   QJsonObject* tables,
			   QHash<int, QByteArray>* componentDllCodes,
			   QByteArray blob, uint offset,
			   MainWindow* window = NULL);
    QJsonArray decode();
	static uint uInt32(QByteArray blob, uint offset);
	static quint64 uInt64(QByteArray blob, uint offset);
	
private:
	QJsonArray* components;
	QJsonObject* tables;
	QHash<int, QByteArray>* componentDllCodes;
	QByteArray blob;
	uint offset;
    MainWindow* window;
    
	QJsonObject decodeComponent(uint end, QString name, QString group, QString function, QJsonArray fields);
	
	int getComponentIndex(uint code, QByteArray blob, uint offset);
	
    QJsonObject decodeGeneric(uint end, QString name, QString group, QJsonArray fields);
	QJsonObject decodeMovement();
	QJsonObject decodeEffectList();
	QJsonObject decodeSimple();
	QJsonObject decodeAvi();
    QJsonObject callSizeFunction(QString function, uint offset, QJsonValue other = QJsonValue());
	QJsonValue call2ndFunction(QString function, uint offset, QJsonValue value);
	
	QJsonObject map1 (uint offset, QJsonObject map);
	QJsonObject map4 (uint offset, QJsonObject map);
	QJsonObject map8 (uint offset, QJsonObject map);
	QJsonObject radioButton (uint offset, QJsonObject map);
	QJsonValue mapping (QJsonObject map, int key);
	QJsonObject codeSection (uint offset, QJsonArray map, bool nt);
	QJsonObject colorList (uint offset);
	QJsonObject colorMaps (uint offset);
	QJsonArray colorMap (uint offset, uint num);
	QJsonObject color (uint offset);
	QJsonObject convoFilter (uint offset, QJsonValue dimensions);
	QJsonValue semiColSplit(QString str);
	QJsonObject bufferNum (uint offset, uint size);
	
	QJsonObject sizeString (uint offset, uint size = 0);
	QJsonObject ntString (uint offset);
	QJsonObject boolean (uint offset, uint size);
	QJsonObject int32(uint offset);
	QJsonObject float32 (uint offset);
	QJsonObject bit (uint offset, QJsonValue pos);
	
    void log(QString message, bool error = false);
};

#endif // COMPONENT_H
