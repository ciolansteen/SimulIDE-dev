BUILD_DIR = $$OUT_PWD
include(../../SimulIDE.pri)
TARGET = theme-startup
DESTDIR = $$OUT_PWD
SOURCES -= $$clean_path($$PWD/../../src/main.cpp)
SOURCES += $$PWD/theme-startup.cpp
