
TEMPLATE = subdirs
SUBDIRS = qwebframe qwebpage qwebelement qgraphicswebview qwebhistoryinterface qwebview qwebhistory qwebinspector hybridPixmap

linux-* {
    # This test bypasses the library and links the tested code's object itself.
    # This stresses the build system in some corners so we only run it on linux.
    SUBDIRS += MIMESniffing
}

contains(QT_CONFIG, declarative): SUBDIRS += qdeclarativewebview
SUBDIRS += benchmarks/painting benchmarks/loading
contains(DEFINES, ENABLE_WEBGL=1) {
    SUBDIRS += benchmarks/webgl
}
