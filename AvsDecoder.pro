#-------------------------------------------------
#
# Project created by QtCreator 2013-10-12T18:56:10
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = AvsDecoder
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
    settingsdialog.cpp \
    settings.cpp \
    createoutputdialog.cpp \
    convertcontroller.cpp \
    converter.cpp \
    job.cpp

HEADERS  += mainwindow.h \
    convertexception.h \
    defines.h \
    settingsdialog.h \
    settings.h \
    createoutputdialog.h \
    convertcontroller.h \
    converter.h \
    job.h

FORMS    += mainwindow.ui \
    settingsdialog.ui \
    createoutputdialog.ui

INCLUDEPATH += $$PWD/include

OTHER_FILES += \
    tables.json \
    components.json \
    .gitignore

### platform-specific compiler options
unix|macx  {
	
}

win32 {
}
