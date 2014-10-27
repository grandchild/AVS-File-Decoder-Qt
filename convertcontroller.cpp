#include "convertcontroller.h"

#include <QDebug>
#include <QDir>
#include <QFileInfoList>
#include <QDirIterator>
#include <QByteArray>
#include <QJsonDocument>
#include <QFile>
//#include <QMutex>

#include "defines.h"
#include "converter.h"

ConvertController::ConvertController(QString inPath, QString outPath, MainWindow* window, Settings* settings):
	inPath(inPath),
	outPath(outPath),
	started(false),
	window(window),
	settings(settings),
	stop(false),
	error(false)
{
	loadConfig();
	// setFileList(); // is now done exernally, after the thread for the controller is started, because otherwise the signals don't go through
	
	//// about multithreading this task
	// there is some disk I/O, so maybe we can have 2 threads on each core? -> not even close, see below.
	// testing shows the bottleneck isn't just cpu. a single thread is noticably slower, but between 2 and 8 threads there's little difference, if any. hmpf :/
	// disk I/O is also not everything. not writing to disk takes only ~25% less time.
	// postprocessing is a major factor. with postprocessing disabled it takes only ~1/3 of the normal time. TODO: optimize the regex stuff, esp. minimizing.
	threadCount = QThread::idealThreadCount();
	if(VERBOSE>=1) log("<b>"+QString("%1").arg(threadCount)+" threads</b>");
	threadPool = QVector<QThread*>();
	for(uint i=0; i<threadCount; ++i) {
		threadPool.append(new QThread());
	}
	jobQueue = QQueue<Job>();
}

ConvertController::~ConvertController() {
	for(uint i=0; i<threadCount; ++i) {
		threadPool[i]->exit();
		bool finished = threadPool[i]->wait(5000);
		if(finished) {
			delete threadPool[i];
		} else {
			log("Thread no "+QString("%1").arg(i)+"'s wait() timed out.", true);
		}
	}
}

void
ConvertController::loadConfig() {
	QJsonParseError parseError;
	
	// components.json
	QFile componentsFile(QDir::currentPath()+"/components.json");
	if(componentsFile.exists()) {
		componentsFile.open(QFile::ReadOnly);
		QByteArray componentsJSON = componentsFile.readAll();
		componentsFile.close();
		
		componentsJSON = clearComments(componentsJSON);
		components = QJsonDocument::fromJson(componentsJSON, &parseError).array();
		if(parseError.error != QJsonParseError::NoError) {
			log(QString(parseError.errorString()+" at offset %1 in '"+componentsFile.fileName()+"'").arg(parseError.offset), /*error*/true);
		}
		
		for(int i=0; i<components.size(); i++) {
			QJsonValue code = components[i].toObject()["code"];
			if(code.isArray()) {
				QByteArray apeCode;
				foreach(QJsonValue byte, code.toArray()) {
					apeCode.append((char)byte.toString().toUInt(0, 0));
				}
				componentDllCodes[i] = apeCode;
			}
		}
	} else {
		log("'"+componentsFile.fileName()+"' does not exist.", /*error*/true);
	}
	
	// tables.json
	QFile tablesFile("tables.json");
	if(tablesFile.exists()) {
		tablesFile.open(QFile::ReadOnly);
		QByteArray tablesJSON = tablesFile.readAll();
		tablesFile.close();
		
		tablesJSON = clearComments(tablesJSON);
		tables = QJsonDocument::fromJson(tablesJSON, &parseError).object();
		if(parseError.error != QJsonParseError::NoError) {
			log(QString(parseError.errorString()+" at offset %1 in '"+tablesFile.fileName()+"'").arg(parseError.offset), /*error*/true);
		}
	} else {
		log("'"+tablesFile.fileName()+"' does not exist.", /*error*/true);
	}
}

QByteArray
ConvertController::clearComments(QByteArray json) {
	char* jsonOut = new char[json.size()];
	bool skip = false;
	bool string = false;
	char* c = json.data();
	uint i=0;
	do {
		if(*c=='\"') {
			string = !string;
		}
		if(!string && *c=='/' && *(c+1)=='/') {
			skip = true;
		}
		if(*c=='\x0A' || *c=='\x0D') {
			skip = false;
		}
		jsonOut[i] = *(c++);
		if(!skip) {
			++i;
		}
	} while(*c!='\0');
	QByteArray jsonOutBA(jsonOut, i);
	delete[] jsonOut;
	return jsonOutBA;
}

void
ConvertController::setFileList() {
	if(QFileInfo(inPath).exists()) {
		fileList.clear();
		//uint inPathLength = inPath.length();
		QDirIterator directory_walker(inPath, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
		while(directory_walker.hasNext()) {
			directory_walker.next();
			QFileInfo fileInfo = directory_walker.fileInfo();
			if(fileInfo.completeSuffix() == "avs") {
				//if (fileInfo.canonicalPath().right(fileInfo.canonicalPath().length()-inPathLength)) {
				//	
				//}
				fileList.append(fileInfo);
				fileInfo.fileName().chop(4); // remove ".avs"
				emit(controller_addSelectPreset(fileInfo.fileName()));
			}
		}
	} else {
		error = true;
		log("Input path does not exist.", /*error*/true);
	}
}

void
ConvertController::logFileNameList() {
	if(error) return;
	foreach (QFileInfo file, fileList) {
		QString relPath = file.filePath();
		relPath.remove(inPath);
		log(relPath);
	}
	log(QString("%1 files").arg(fileList.length()));
}

void
ConvertController::logComponents() {
	log("Comps: "+QString(QJsonDocument(components).toJson(QJsonDocument::Compact)));
}

void
ConvertController::convert() {
	if(error) return;
	emit controller_setProgressMax(fileList.length()-1); // why it's length()-1 i have no idea...
	emit controller_resetProgress();
	foreach(QFileInfo file, fileList) {
		jobQueue.enqueue(Job(file, Job::FileOut));
		if(stop or error) break;
	}
	start();
}

void
ConvertController::convert(uint index) {
	if(error) return;
	if(fileList.empty()) {
		setFileList();
	}
	jobQueue.enqueue(Job(fileList[index],Job::Single|Job::Preview));
	start();
}

void
ConvertController::start() {
	if(!started) {
		started = !jobQueue.isEmpty();
		QThread* thread = getFreeThread();
		while(thread!=NULL and !jobQueue.isEmpty()) {
			convertSingle(jobQueue.dequeue(), thread);
			thread = getFreeThread();
		}
	} // else { start on already started controller does nothing }
}

void
ConvertController::convertSingle(Job job, QThread* thread) {
	if(thread->isRunning()) {
		log("Thread no "+QString("%1").arg(threadPool.indexOf(thread))+" is not ready.", true);
		return;
	}
	Converter* converter = new Converter(job, &components, &tables, &componentDllCodes, window, settings->getCompactKernels());
	converter->moveToThread(thread);
	connect(thread, &QThread::finished, converter, &QObject::deleteLater);
	connect(this, &ConvertController::go, converter, &Converter::worker_toJson);
	connect(converter, &Converter::worker_log, window, &MainWindow::on_log);
	connect(converter, &Converter::worker_done, this, &ConvertController::finishJob);
	thread->setObjectName(job.file().fileName());
	//emit controller_updateThreadInfo(((uint)thread), job.file().filePath());
	thread->start();
	emit go();
	disconnect(this, &ConvertController::go, converter, &Converter::worker_toJson);
}

void
ConvertController::finishJob(Converter* converter, QThread* thread, Job* job, QString json) {
	//log("signal received.");
	bool ok = !json.isEmpty();
	if(ok) {
		// post-process here...
		postProcess(&json);
		
		if(job->preview()) {
			output(json);
		}
		if(job->fileOut()) {
			// write to file here...
			write(job->file(), json);
		}
	}
	if(!job->single()) {
		emit controller_incrProgress();
	}
	//log("Finished conversion "+converter->fileName());
	thread->exit(converter->error()?1:0);
	bool finished = thread->wait(5000);
	if(finished and !(stop or error)) {
		nextJob(thread);
	} else {
		log("Thread no "+QString("%1").arg(threadPool.indexOf(thread))+"'s wait() timed out.", true);
		setStop();
	}
}

void
ConvertController::nextJob(QThread* thread) {
	if(!jobQueue.isEmpty()) {
		convertSingle(jobQueue.dequeue(), thread);
	} else {
		started = false;
	}
}

QThread*
ConvertController::getFreeThread() {
	for(uint i=0; i<threadCount; ++i) {
		if(!threadPool[i]->isRunning()) {
			return threadPool[i];
		}
	}
	return NULL;
}

void
ConvertController::postProcess(QString* preset) {
	
	// minimize empty fields
	if(settings->getMinimize()) {
		QString rx = "\"\\(\\w|\\d|_)+\" ?: ?\"\",?";
		if(settings->getIndent()) {
			rx = "(\\n|\\r|\\n\\r) *"+rx;
		}
		preset->replace(QRegExp(rx), "");
	}
	
	// remove ordering indices (TODO: how much deep copying is done here? more than once? optimization possible? measure!)
	preset->replace(QRegExp("\"__\\d{5}__([^\"]+\":)"), "\"\\1");
	
	// format convolution kernels
	if(settings->getCompactKernels()) {
		preset->replace(QRegExp("\"__kernelLine__([^\"]+)\""), "\\1");
	}
	/*
	QRegExp kernelHeaderRx("\\s+\"kernel\": \\{\\n"
						"\\s+\"width\": ([0-9]+),\\n"
						"\\s+\"height\": [0-9]+,\\n"
						"\\s+\"data\": \\[\\n");
	int pos = 0;
	uint length = 0;
	while((pos = kernelHeaderRx.indexIn(*preset, pos+length)) >= 0) {
		length = kernelHeaderRx.matchedLength();
		bool ok = true;
		uint width = kernelHeaderRx.cap(1).toUInt(&ok);
		QString substr = preset->mid(pos, length);
		for(uint i=0; i<width; i++) {
			preset->replace(QRegExp("\\s*"), "");
		}
	}
	//*/
}

bool
ConvertController::write(QFileInfo file, QString preset) {
	QString subdir("");
	QDir outDir(outPath.replace("\\", "/"));
	if(settings->getSubdirs()) {
		subdir = file.path();
		subdir.remove(QFileInfo(inPath).absolutePath()); // has leading '/'
		outDir = QDir(outDir.path()+subdir);
		if(!outDir.mkpath(outDir.absolutePath())) {
			log("Could not create output directory.", /*error*/true);
			return false;
		}
	} else {
		
	}
	QFile outFile(outDir.path()+"/"+file.completeBaseName()+".webvs");
	outFile.open(QIODevice::WriteOnly);
	bool success = outFile.write(preset.toUtf8()) > 0;
	outFile.close();
	return success;
}

void
ConvertController::setStop() {
	stop = true;
}

void
ConvertController::finish(uint threadTimeout) {
	setStop();
	for(uint i=0; i<threadCount; ++i) {
		bool finished = threadPool[i]->wait(threadTimeout>0?threadTimeout:ULONG_MAX);
		if(!finished) {
			log("Thread wait() expired while finishing Converter");
		}
	}
}

uint
ConvertController::threadNum() {
	return threadCount;
}

void
ConvertController::log(QString message, bool error) {
	emit controller_log(message, error);
}

void
ConvertController::output(QString json) {
	emit controller_output(json);
}
