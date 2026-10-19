BUILD_DIR = $$OUT_PWD
include(../../SimulIDE.pri)
QT += testlib dbus
TARGET = theme-system-ui
DESTDIR = $$OUT_PWD
SOURCES -= $$clean_path($$PWD/../../src/main.cpp)
SOURCES += $$PWD/theme-system-ui.cpp
