#include "settingsdialog.h"
#include "ui_settingsdialog.h"

SettingsDialog::SettingsDialog(QWidget *parent, Settings* settings) :
	QDialog(parent),
	tempSet(settings),
	settings(settings),
	ui(new Ui::SettingsDialog)
{
	ui->setupUi(this);
	ui->logComponents->setChecked(tempSet.getLogLevel()>=1);
	ui->indent->setChecked(tempSet.getIndent());
	ui->minimize->setChecked(tempSet.getMinimize());
	ui->compactKernels->setChecked(tempSet.getCompactKernels());
}

SettingsDialog::~SettingsDialog() {
	delete ui;
}

void
SettingsDialog::on_okCancel_accepted() {
	settings->copyFrom(&tempSet);
	settings->writeSettingsFile();
}

void
SettingsDialog::on_okCancel_rejected() {
	tempSet.copyFrom(settings);
}

void
SettingsDialog::on_logComponents_stateChanged(int state) {
	tempSet.setLogLevel(state==Qt::Checked?1:0);
}

void
SettingsDialog::on_indent_stateChanged(int state) {
	tempSet.setIndent(state==Qt::Checked);
}

void
SettingsDialog::on_minimize_stateChanged(int state) {
	bool minimize = state==Qt::Checked;
	tempSet.setMinimize(minimize);
	ui->compactKernels->setDisabled(minimize);
}

void
SettingsDialog::on_compactKernels_stateChanged(int state) {
	tempSet.setCompactKernels(state==Qt::Checked);
}
