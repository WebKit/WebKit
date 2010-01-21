
TEMPLATE = subdirs
SUBDIRS = qwebframe qwebpage qwebelement qgraphicswebview qwebhistoryinterface qwebview qwebhistory qwebinspector hybridPixmap
greaterThan(QT_MINOR_VERSION, 4): SUBDIRS += benchmarks/painting benchmarks/loading
