# Include file to make it easy to include WebKit into Qt projects

INCLUDEPATH += $$PWD/WebKitQt/Api 

DEFINES += BUILDING_QT__=1

isEmpty(OUTPUT_DIR):OUTPUT_DIR=$$PWD/WebKitBuild/Release

LIBS += -L$$OUTPUT_DIR/lib -lWebKitQt -lJavaScriptCore

macx {
	INCLUDEPATH += /opt/local/include /opt/local/include/libxml2
	INCLUDEPATH += /usr/include/libxml2
	LIBS += -L/opt/local/lib -lxml2 -lxslt
}
