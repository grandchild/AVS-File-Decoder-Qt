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
    converter.cpp \
    component.cpp

HEADERS  += mainwindow.h \
    converter.h \
    component.h \
    convertexception.h \
    defines.h

FORMS    += mainwindow.ui

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
