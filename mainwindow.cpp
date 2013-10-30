#include <QFileDialog>
#include <QString>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "converter.h"
#include "settingsdialog.h"

MainWindow::MainWindow(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::MainWindow),
	converter(NULL),
	noneText("[none]"),
	settings("avsdecoder.ini")
{
	ui->setupUi(this);
	ui->cancelButton->setDisabled(true);
	ui->samplePresetSelectBox->addItem(noneText);
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
MainWindow::setProgressMax(int size) {
	ui->progressBar->setMaximum(size);
}

void
MainWindow::incrProgress() {
	int curValue = ui->progressBar->value();
	ui->progressBar->setValue(++curValue);
}

void
MainWindow::resetProgress() {
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

void
MainWindow::newConverter() {
	converter = new Converter(
				ui->inputPathEdit->text(),
				ui->outputPathEdit->text(),
				this,
				&settings);
}

// Slots

void
MainWindow::on_convertButton_clicked()
{
	ui->cancelButton->setDisabled(false);
	ui->convertButton->setDisabled(true);	
	resetProgress();
	
	if(converter==NULL) {
		newConverter();
	}
	converter->convertAll();
	delete converter;
	converter = NULL;
	ui->cancelButton->setDisabled(true);
	ui->convertButton->setDisabled(false);
}

void
MainWindow::on_cancelButton_clicked()
{
	converter->setStop();
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
	if(converter==NULL) {
		newConverter();
	}
	QStringList fileList = converter->getFileNameList();
	ui->samplePresetSelectBox->addItems(fileList);
	foreach (QString fileName, fileList) {
		log(fileName);
	}
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
	if(converter!=NULL) {
		delete converter;
		converter = NULL;
		log("Converter reset.");
	}
	resetProgress();
}

void
MainWindow::on_samplePresetSelectBox_currentIndexChanged(int index)
{
	if(ui->convertButton->isEnabled() and index>0) {
		if(converter==NULL) {
			newConverter();
		}
		converter->convert(index-1);
	}
}

void
MainWindow::on_pushButton_clicked()
{
	SettingsDialog settingsDialog(this, &settings);
	settingsDialog.exec();
}
