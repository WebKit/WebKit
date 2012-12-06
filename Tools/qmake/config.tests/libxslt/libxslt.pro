CONFIG -= qt
SOURCES = libxslt.cpp
mac {
    INCLUDEPATH += /usr/include/libxslt /usr/include/libxml2
    LIBS += -lxslt
} else {
    PKGCONFIG += libxslt
    CONFIG += link_pkgconfig
}
