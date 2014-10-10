#ifndef CONVERTCONTROLLER_H
#define CONVERTCONTROLLER_H

#include <QString>
#include <QFileInfoList>
#include <QStringList>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QThread>
#include <QQueue>

#include "mainwindow.h"
#include "job.h"
//#include "convertexception.h"

class Converter;

class ConvertController : public QObject
{
	Q_OBJECT
	
public:
	ConvertController(QString inPath, QString outPath, MainWindow* window, Settings* settings);
	~ConvertController();
	
	void logFileNameList();
	void logComponents();
	void convert();
	void convert(uint index);
	void setFileList();
	void setStop();
	void finish(uint threadTimeout=ULONG_MAX);
	uint threadNum();
	
private:
	QJsonArray components;
	QJsonObject tables;
	QString inPath;
	QString outPath;
	QByteArray json;
	QFileInfoList fileList;
	uint threadCount;
	QVector<QThread*> threadPool;
	QQueue<Job> jobQueue;
	bool started;
	MainWindow* window;
	Settings* settings;
	
	QHash<int, QByteArray> componentDllCodes;
	bool stop; // stops execution if set to true
	bool error; // some error occurred
	
	void loadConfig();
	
	void start();
	void convertSingle(Job job, QThread* thread);
	
	QByteArray clearComments(QByteArray json);
//	QJsonArray decodeComponents(QByteArray blob, uint offset);
	int getComponentIndex(uint code, QByteArray blob, uint offset);
	void postProcess(QString *preset);
	bool write(QFileInfo file, QString preset);
	void log(QString message, bool error=false);
	void output(QString json);
	
signals:
	void go();
	void controller_log(QString message, bool error=false);
	void controller_output(QString json);
	void controller_setProgressMax(uint max);
	void controller_resetProgress();
	void controller_incrProgress();
	void controller_addSelectPreset(QString filename);
	void controller_updateThreadInfo(uint threadId, QString info);
	
private slots:
	void finishJob(Converter* converter, QThread* thread, Job *job, QString json);
	void nextJob(QThread* thread);
	QThread* getFreeThread();
};

#endif // CONVERTCONTROLLER_H
