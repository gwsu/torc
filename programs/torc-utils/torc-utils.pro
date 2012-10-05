include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
TARGET = torc-utils
target.path = $${PREFIX}/bin
INSTALLS = target

QT -= gui

INCLUDEPATH += ../../libs/libtorc-core
INCLUDEPATH += ../../libs/libtorc-audio
LIBS += -L../../libs/libtorc-core -ltorc-core-$$LIBVERSION
LIBS += -L../../libs/libtorc-audio -ltorc-audio-$$LIBVERSION

setting.path = $${PREFIX}/share/$${PROJECTNAME}/
setting.extra = -ldconfig

INSTALLS += setting

QMAKE_CLEAN += $(TARGET)

# Input
HEADERS += utilscommandlineparser.h
SOURCES += main.cpp
SOURCES += utilscommandlineparser.cpp

macx {
    # OS X has no ldconfig
    setting.extra -= -ldconfig
}

# OpenBSD ldconfig expects different arguments than the Linux one
openbsd {
    setting.extra -= -ldconfig
    setting.extra += -ldconfig -R
}
