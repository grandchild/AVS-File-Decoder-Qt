#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <QDebug>
#include <QLineEdit>

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
	
private slots:
	void on_convertButton_clicked();
	void on_inputPathButton_clicked();
	void on_cancelButton_clicked();
	void on_outputPathButton_clicked();
	void on_listFilesButton_clicked();
	void on_inputPathEdit_textChanged(const QString &inPath);
	void on_outputPathEdit_textChanged(const QString &outPath);
	
private:
	Ui::MainWindow *ui;
	
	Converter* converter;
	void selectDirPath(QLineEdit* pathEdit);
	void updatePath(QString path, QLineEdit* lineEdit);
};

#endif // MAINWINDOW_H
