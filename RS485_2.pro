QT = core
QT += serialport
QT += network

CONFIG += c++17 cmdline

DEFINES += PROJECT_DIR=\\\"$$PWD\\\"

SOURCES += \
        main.cpp \
        portlistener.cpp \
        snmpconverter.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    portlistener.h \
    snmpconverter.h \

DISTFILES += \
    .gitignore
