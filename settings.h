#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>

class Settings
{
public:
	Settings(QString settingsFile);
	Settings(Settings* other);
	
	QString getFile() { return file; }
	int getLogLevel() { return logLevel; }
	bool getIndent() { return indent; }
	bool getMinimize() { return minimize; }
	bool getCompactKernels() { return compactKernels; }
	
	void setLogLevel(int logLevel) { this->logLevel = logLevel; }
	void setIndent(bool indent) { this->indent = indent; }
	void setMinimize(bool minimize) { this->minimize = minimize; }
	void setCompactKernels(bool compactKernels) { this->compactKernels = compactKernels; }
	
	void copyFrom(Settings* other);
	bool writeSettingsFile();
	
private:
	QString file;
	
	// program settings
	int logLevel;
	
	// output settings
	bool indent;
	bool minimize;
	bool compactKernels;
	
	bool readSettingsFile();
};

#endif // SETTINGS_H
