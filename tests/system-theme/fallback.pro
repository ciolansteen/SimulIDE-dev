QT += core
QT -= gui
CONFIG += console c++11
TEMPLATE = app
TARGET = system-theme-fallback
INCLUDEPATH += ../../src/gui
SOURCES += fallback.cpp ../../src/gui/systemthemeobserver.cpp
HEADERS += ../../src/gui/systemthemeobserver.h ../../src/gui/themepolicy.h
