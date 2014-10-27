#include <QFileDialog>
#include <QString>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "convertcontroller.h"
#include "settingsdialog.h"
#include "defines.h"
#include "createoutputdialog.h"

MainWindow::MainWindow(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::MainWindow),
	converter(NULL),
	noneText("[none]"),
	settings("avsdecoder.ini"),
	inputChanged(false),
	threadInfo(QMap<uint, QString>())
{
	converterThread = new QThread();
	ui->setupUi(this);
	ui->cancelButton->setDisabled(true);
	ui->samplePresetSelectBox->addItem(noneText);
	ui->inputPathEdit->setText("D:\\Daten\\Dropbox");
	ui->outputPathEdit->setText("D:\\Daten\\Desktop");
}

MainWindow::~MainWindow() {
	delete ui;
}

void
MainWindow::log(QString message, bool error) {
	QString logString;
	if(error) {
		logString = "<div style='color:#ff0000'> <b>Error:</b> "+message+"</div>";
	} else {
		logString = "<div>"+message+"</div>";
	}
	ui->logField->append(logString);
}

void
MainWindow::output(QString json) {
	ui->outputSample->setText(json);
}

void
MainWindow::on_setProgressMax(int size) {
	ui->progressBar->setMaximum(size);
}

void
MainWindow::on_incrProgress() {
	int curValue = ui->progressBar->value();
	ui->progressBar->setValue(++curValue);
}

void
MainWindow::on_resetProgress() {
	ui->progressBar->reset();
}

void
MainWindow::setSampleName(QString name) {
	int curIndex = ui->samplePresetSelectBox->findData(name);
	ui->samplePresetSelectBox->setCurrentIndex(curIndex);
}

bool
MainWindow::indent() {
	return settings.getIndent();
}

bool
MainWindow::newConverter() {
	if(!QFile(ui->inputPathEdit->text()).exists()){
		log("Input path does not exist.", /*error*/true);
		return false;
	}
	if(converter != NULL) {
		converter->finish();
		delete converter;
	}
	inputChanged = false;
	on_resetProgress();
	converter = new ConvertController(
				ui->inputPathEdit->text(),
				ui->outputPathEdit->text(),
				this,
				&settings);
	converter->moveToThread(converterThread);
	connect(converterThread, &QThread::finished, converter, &QObject::deleteLater);
	connect(converter, &ConvertController::controller_log, this, &MainWindow::on_log);
	connect(converter, &ConvertController::controller_output, this, &MainWindow::on_output);
	connect(converter, &ConvertController::controller_setProgressMax, this, &MainWindow::on_setProgressMax);
	connect(converter, &ConvertController::controller_resetProgress, this, &MainWindow::on_resetProgress);
	connect(converter, &ConvertController::controller_incrProgress, this, &MainWindow::on_incrProgress);
	connect(converter, &ConvertController::controller_addSelectPreset, this, &MainWindow::on_addSelectPreset);
	connect(converter, &ConvertController::controller_updateThreadInfo, this, &MainWindow::on_updateThreadInfo);
	converterThread->start();
	converter->setFileList(); // there are slots to be signalled, so we start this here and not in the constructor.
	return true;
}

// Slots

void
MainWindow::on_convertButton_clicked()
{
	ui->cancelButton->setDisabled(false);
	ui->convertButton->setDisabled(true);
	
	if(converter==NULL or inputChanged) {
		if(!newConverter()) {
			return;
		}
	}
	QFileInfo outPath(ui->outputPathEdit->text());
	if(!outPath.isDir()) {
		CreateOutputDialog createOutputDialog(this, outPath);
		if(createOutputDialog.exec()==QDialog::Accepted) {
			QDir outDir;
			outDir.mkpath(outPath.absoluteFilePath());
			// update the display coloring of the output dir, now that we've created it.
			on_outputPathEdit_textChanged(ui->outputPathEdit->text());
		} else {
			return;
		}
	}
	converter->convert();
	ui->cancelButton->setDisabled(true);
	ui->convertButton->setDisabled(false);
}

void
MainWindow::on_cancelButton_clicked()
{
	converter->finish();
	ui->cancelButton->setDisabled(true);
	ui->convertButton->setDisabled(false);
}

void
MainWindow::on_inputPathButton_clicked()
{
	selectDirPath(ui->inputPathEdit);
}

void
MainWindow::on_outputPathButton_clicked()
{
	selectDirPath(ui->outputPathEdit);
}

void
MainWindow::selectDirPath(QLineEdit* pathEdit) {
	QString selectedDir = QFileDialog::getExistingDirectory(this, "", pathEdit->text());
	if(!selectedDir.isEmpty()) {
		pathEdit->setText(selectedDir);
	}
}

void
MainWindow::on_listFilesButton_clicked()
{
	if(converter==NULL or inputChanged) {
		if(!newConverter()) {
			return;
		}
	}
	converter->logFileNameList();
}

void
MainWindow::on_inputPathEdit_textChanged(const QString &inPath)
{
	ui->samplePresetSelectBox->clear();
	ui->samplePresetSelectBox->addItem(noneText);
	updatePath(inPath, ui->inputPathEdit);
}

void
MainWindow::on_outputPathEdit_textChanged(const QString &outPath)
{
	updatePath(outPath, ui->outputPathEdit);
}

void
MainWindow::updatePath(QString path, QLineEdit* lineEdit) {
	QFileInfo dirInfo(path);
	if(!path.isEmpty() && (!dirInfo.exists() || !dirInfo.isDir())) {
		lineEdit->setStyleSheet("color: red;");
	} else {
		lineEdit->setStyleSheet("");
	}
	inputChanged = true;
}

void
MainWindow::on_samplePresetSelectBox_currentIndexChanged(int index)
{
	if(ui->convertButton->isEnabled() and index>0) {
		if(converter==NULL) {
			if(!newConverter()) {
				return;
			}
		}
		converter->convert(index-1);
	}
}

void MainWindow::on_updateThreadInfo(uint threadId, QString info)
{
	if(!threadInfo.contains(threadId) && ((uint)threadInfo.size()) >= converter->threadNum()) {
		qDebug() << "A wild thread appeared, it's not very effective.";
		threadInfo.remove(threadInfo.keys()[0]);
	}
	threadInfo[threadId] = info;
	QString threadList("");
	QMapIterator<uint, QString> it(threadInfo);
	while(it.hasNext()) {
		it.next();
		threadList = threadList+QString::number(it.key(), 16)+ ": <b>"+it.value()+"</b><br />";
	}
	qDebug() << threadList;
	ui->threadLog->setHtml("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\
				<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\
				p, li { white-space: pre-wrap; }\
				</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:8.25pt; font-weight:400; font-style:normal;\">\
				<p style=\"-qt-paragraph-type:empty; margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\">"+
				threadList+
				"</p></body></html>");
}

void
MainWindow::on_pushButton_clicked()
{
	SettingsDialog settingsDialog(this, &settings);
	settingsDialog.exec();
}

void
MainWindow::on_log(QString message, bool error) {
	log(message, error);
}

void
MainWindow::on_output(QString json) {
	output(json);
}

void
MainWindow::on_addSelectPreset(QString filename) {
	ui->samplePresetSelectBox->addItem(filename);
}
