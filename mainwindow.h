#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QDebug>
#include <QLineEdit>
#include <QThread>
#include <QMap>

#include "settings.h"

namespace Ui {
class MainWindow;
}

class ConvertController;

class MainWindow : public QWidget
{
	Q_OBJECT
	
public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();
	
	void log(QString message, bool error = false); // Thread-safe
	void output(QString json);
	void setSampleName(QString name);
	bool indent();
	
private slots:
	void on_setProgressMax(int size);
	void on_incrProgress(); // Thread-safe
	void on_resetProgress();
	void on_convertButton_clicked();
	void on_inputPathButton_clicked();
	void on_cancelButton_clicked();
	void on_outputPathButton_clicked();
	void on_listFilesButton_clicked();
	void on_addSelectPreset(QString filename);
	void on_inputPathEdit_textChanged(const QString &inPath);
	void on_outputPathEdit_textChanged(const QString &outPath);
	void on_samplePresetSelectBox_currentIndexChanged(int index);
	void on_updateThreadInfo(uint threadId, QString info);
	
	void on_pushButton_clicked();
	
public slots:
	void on_log(QString message, bool error);
	void on_output(QString json);
	
private:
	Ui::MainWindow* ui;
	ConvertController* converter;
	QThread* converterThread;
	QString noneText;
	Settings settings;
	bool inputChanged;
	QMap<uint, QString> threadInfo;
	
	void selectDirPath(QLineEdit* pathEdit);
	void updatePath(QString path, QLineEdit* lineEdit);
	bool newConverter();
};

#endif // MAINWINDOW_H
