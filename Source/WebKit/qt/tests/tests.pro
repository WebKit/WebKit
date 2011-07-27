
TEMPLATE = subdirs
SUBDIRS = qwebframe qwebpage qwebelement qgraphicswebview qwebhistoryinterface qwebview qwebhistory qwebinspector hybridPixmap

linux-* {
    # This test bypasses the library and links the tested code's object itself.
    # This stresses the build system in some corners so we only run it on linux.
    SUBDIRS += MIMESniffing
}

lessThan(QT_MAJOR_VERSION, 5) {
    contains(QT_CONFIG, declarative): SUBDIRS += qdeclarativewebview
} else {
    contains(QT_CONFIG, qtquick1): SUBDIRS += qdeclarativewebview
}


SUBDIRS += benchmarks/painting benchmarks/loading
contains(DEFINES, ENABLE_WEBGL=1) {
    SUBDIRS += benchmarks/webgl
}
