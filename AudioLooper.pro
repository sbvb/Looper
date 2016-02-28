#-------------------------------------------------
#
# Project created by QtCreator 2016-01-25T14:51:21
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = AudioLooper
TEMPLATE = app


SOURCES += \
    looper.cpp \
    main.cpp

HEADERS  += \
    portaudio.h \
    looper.h

FORMS    += \
    looper.ui

LIBS += -L$$PWD/libportaudio/ -lportaudio

RESOURCES += \
    buttons.qrc



