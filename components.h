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
	
	int getComponentIndex(uint code);
	
    QJsonObject decodeGeneric(uint offset, uint end, QString name, QString group, QJsonArray fields);
	QJsonObject decodeMovement(uint offset);
	QJsonObject decodeEffectList(uint offset);
	QJsonObject decodeSimple(uint offset);
	QJsonObject decodeAvi(uint offset);
    QJsonValue call(QString function, uint offset, uint* sizeOut = NULL, QJsonValue other = QJsonValue());
	QJsonValue call2nd(QString function, QJsonValue value);
	
	QJsonValue map1 (uint offset, QJsonObject map);
	QJsonValue map4 (uint offset, QJsonObject map);
	QJsonValue map8 (uint offset, QJsonObject map);
	QJsonValue radioButton (uint offset, QJsonObject map, uint* sizeOut = NULL);
	QJsonValue mapping (QJsonObject map, int key);
	QJsonValue codeSection (uint offset, QJsonArray map, bool nt, uint* sizeOut = NULL);
	QJsonValue colorList (uint offset, uint* sizeOut = NULL);
	QJsonValue colorMaps (uint offset, uint* sizeOut = NULL);
	QJsonArray colorMap (uint offset, uint num);
	QString color(uint offset, uint* sizeOut = NULL);
	QJsonValue convoFilter (uint offset, QJsonValue dimensions, uint* sizeOut = NULL);
	QJsonValue semiColSplit(QString str);
	QJsonValue bufferNum (uint offset, uint size, uint* sizeOut = NULL);
	
	QString sizeString (uint offset, uint size = 0, uint* sizeOut = NULL);
	QString ntString (uint offset, uint* sizeOut = NULL);
	bool boolean(uint offset, uint size, uint* sizeOut = NULL);
	qint32 int32(uint offset, uint* sizeOut = NULL);
	QJsonValue float32 (uint offset, uint* sizeOut = NULL);
	QJsonValue bit (uint offset, QJsonValue pos, uint* sizeOut = NULL);
	
    void log(QString message, bool error = false);
};

#endif // COMPONENT_H
