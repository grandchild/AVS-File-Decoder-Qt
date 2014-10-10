#include "createoutputdialog.h"
#include "ui_createoutputdialog.h"
#include <QDir>

CreateOutputDialog::CreateOutputDialog(QWidget *parent, QFileInfo path) :
	QDialog(parent),
	path(path),
	ui(new Ui::CreateOutputDialog)
{
	ui->setupUi(this);
	ui->label->setText(ui->label->text().replace("$PATH", path.canonicalFilePath()));
}

CreateOutputDialog::~CreateOutputDialog()
{
	delete ui;
}
