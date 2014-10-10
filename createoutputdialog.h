#ifndef CREATEOUTPUTDIALOG_H
#define CREATEOUTPUTDIALOG_H

#include <QDialog>
#include <QFileInfo>

namespace Ui {
class CreateOutputDialog;
}

class CreateOutputDialog : public QDialog
{
	Q_OBJECT
	
public:
	explicit CreateOutputDialog(QWidget *parent = 0, QFileInfo path = QFileInfo());
	~CreateOutputDialog();
	
private:
	QFileInfo path;
	Ui::CreateOutputDialog *ui;
};

#endif // CREATEOUTPUTDIALOG_H
