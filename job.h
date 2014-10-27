#ifndef JOB_H
#define JOB_H

#include <QFileInfo>

class Job
{
private:
	QFileInfo file_;
	uint properties_;
	
public:
	static const uint Single = 1<<0;
	static const uint Preview = 1<<1;
	static const uint FileOut = 1<<2;
	
	Job(QFileInfo file, uint properties = 0);
	Job(Job* other);
	
	QFileInfo file() { return file_; }
	uint properties() { return properties_; }
	bool single() { return (this->properties_ & Single) > 0; }
	bool preview() { return (this->properties_ & Preview) > 0; }
	bool fileOut() { return (this->properties_ & FileOut) > 0; }
	
	void setProperty(uint property, bool set);
};

#endif // JOB_H
