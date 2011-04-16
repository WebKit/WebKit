
TEMPLATE = subdirs
SUBDIRS = qwebframe qwebpage qwebelement qgraphicswebview qwebhistoryinterface qwebview qwebhistory qwebinspector hybridPixmap MIMESniffing
contains(QT_CONFIG, declarative): SUBDIRS += qdeclarativewebview
SUBDIRS += benchmarks/painting benchmarks/loading
contains(DEFINES, ENABLE_WEBGL=1) {
    SUBDIRS += benchmarks/webgl
}
