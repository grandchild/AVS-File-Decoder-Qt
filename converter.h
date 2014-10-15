#ifndef CONVERTER_H
#define CONVERTER_H

#include <QString>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "mainwindow.h"
#include "convertcontroller.h"\
#include "job.h"
//#include "convertexception.h"

class Converter : public QObject
{
	Q_OBJECT
	
public:
	Converter(Job job,
				QJsonArray* components,
				QJsonObject* tables,
				QHash<int, QByteArray>* componentDllCodes,
				MainWindow* window,
				QByteArray blob = QByteArray(),
				uint offset = 0);
	QString toJson();
	bool error();
	int errorCount();
	QString fileName();
	
public slots:
	void worker_toJson();
	
signals:
	void worker_done(Converter* converter, QThread* thread, Job* job, QString json);
	void worker_log(QString message, bool error);
	void worker_output(QString json);
	
private:
	Job job;
	QFileInfo file;
	QFile preset;
	QJsonArray* components;
	QJsonObject* tables;
	QHash<int, QByteArray>* componentDllCodes;
	QByteArray blob;
	uint offset;
	bool oldVersion;
	MainWindow* window;
	uint indexCounter;
	bool error_;
	int errorCount_;
	
	QJsonArray toJsonArray();
	QJsonValue decodePresetHeader();
	QJsonArray decode();
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
	char byte(uint offset, uint *sizeOut = NULL);
	qint32 int32(uint offset, uint* sizeOut = NULL);
	uint uInt32(QByteArray blob, uint offset);
	quint64 uInt64(QByteArray blob, uint offset);
	QJsonValue float32 (uint offset, uint* sizeOut = NULL);
	QJsonValue bit (uint offset, QJsonValue pos, uint* sizeOut = NULL);
	
	QString ordKey(QString key);
	void log(QString message, bool error = false);
};

#endif // CONVERTER_H
