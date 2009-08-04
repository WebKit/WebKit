
TEMPLATE = subdirs
SUBDIRS = qwebframe qwebpage qwebelement qwebhistoryinterface qwebplugindatabase qwebview qwebhistory
greaterThan(QT_MINOR_VERSION, 4): SUBDIRS += benchmarks/painting/tst_painting.pro benchmarks/loading/tst_loading.pro
