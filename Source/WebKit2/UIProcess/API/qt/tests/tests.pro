TEMPLATE = subdirs
SUBDIRS = qtouchwebview qdesktopwebview commonviewtests
lessThan(QT_MAJOR_VERSION, 5): SUBDIRS += qmltests
