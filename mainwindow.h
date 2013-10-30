#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QDebug>
#include <QLineEdit>

#include "settings.h"

namespace Ui {
class MainWindow;
}

class Converter;

class MainWindow : public QWidget
{
	Q_OBJECT
	
public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();
	
	void log(QString message, bool error = false);
	void output(QString json);
	void setProgressMax(int size);
	void incrProgress();
	void resetProgress();
	void setSampleName(QString name);
	bool indent();
	
private slots:
	void on_convertButton_clicked();
	void on_inputPathButton_clicked();
	void on_cancelButton_clicked();
	void on_outputPathButton_clicked();
	void on_listFilesButton_clicked();
	void on_inputPathEdit_textChanged(const QString &inPath);
	void on_outputPathEdit_textChanged(const QString &outPath);
	void on_samplePresetSelectBox_currentIndexChanged(int index);
	
	void on_pushButton_clicked();
	
private:
	Ui::MainWindow* ui;
	Converter* converter;
	QString noneText;
	Settings settings;
	
	void selectDirPath(QLineEdit* pathEdit);
	void updatePath(QString path, QLineEdit* lineEdit);
	void newConverter();
};

#endif // MAINWINDOW_H
