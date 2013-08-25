include ( ../../settings.pro )
include ( ../../version.pro )
include ( ../programs-libs.pro )

TEMPLATE = app
CONFIG += thread
TARGET = torc-tv
target.path = $${PREFIX}/bin
INSTALLS = target

QT += qml quick

INCLUDEPATH += ../../libs/libtorc-core ../../libs/libtorc-media ../../libs/libtorc-qml
LIBS += -L../../libs/libtorc-core -ltorc-core-$$LIBVERSION
LIBS += -L../../libs/libtorc-media -ltorc-media-$$LIBVERSION
LIBS += -L../../libs/libtorc-qml -ltorc-qml-$$LIBVERSION

SOURCES += main.cpp

qmlfiles.path  = $${PREFIX}/share/$${PROJECTNAME}/torc-tv/
qmlfiles.files = qml
INSTALLS += qmlfiles

macx {
    # OS X has no ldconfig
    setting.extra -= -ldconfig
}

# OpenBSD ldconfig expects different arguments than the Linux one
openbsd {
    setting.extra -= -ldconfig
    setting.extra += -ldconfig -R
}

win32 : !debug {
    # To hide the window that contains logging output:
    CONFIG -= console
    DEFINES += WINDOWS_CLOSE_CONSOLE
}