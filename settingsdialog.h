#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

#include "settings.h"

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
	Q_OBJECT
	
public:
	explicit SettingsDialog(QWidget *parent = 0, Settings* settings = 0);
	~SettingsDialog();
	
private slots:
	void on_okCancel_accepted();
	void on_okCancel_rejected();
	void on_logComponents_stateChanged(int state);
	void on_indent_stateChanged(int state);
	void on_minimize_stateChanged(int state);
	void on_compactKernels_stateChanged(int state);
	void on_subdirs_stateChanged(int state);
	
private:
	Settings tempSet;
	Settings* settings;
	Ui::SettingsDialog *ui;
};

#endif // SETTINGSDIALOG_H
