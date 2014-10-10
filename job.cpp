#include "job.h"

Job::Job(QFileInfo file, uint properties):
	file_(file),
	properties_(properties)
{}

Job::Job(Job *other) {
	file_ = other->file();
	properties_ = other->properties();
}

bool
Job::setProperty(uint property, bool set) {
	if(set) {
		properties_ |= property;
	} else {
		properties_ &= ~property;
	}
}
