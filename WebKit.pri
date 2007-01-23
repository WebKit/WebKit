# Include file to make it easy to include WebKit into Qt projects

INCLUDEPATH += $$PWD/WebKitQt/Api 

# Remove this once we can actually use the API
INCLUDEPATH += $$PWD/JavaScriptCore \
               $$PWD/WebCore \
               $$PWD/WebCore/platform \
               $$PWD/WebCore/platform/qt \
               $$PWD/WebCore/platform/network \
               $$PWD/WebCore/platform/graphics \
               $$PWD/WebCore/editing \
               $$PWD/WebCore/page \
               $$PWD/WebCore/page/qt \
               $$PWD/WebCore/dom \
               $$PWD/WebCore/html \
               $$PWD/WebCore/history \
               $$PWD/WebCore/rendering \
               $$PWD/WebCore/loader \
               $$PWD/WebCore/loader/qt \
               $$PWD/WebCore/css \
               $$PWD/WebCore/bridge \
               $$PWD/WebKitQt/WebCoreSupport

DEFINES += BUILDING_QT__=1

isEmpty(OUTPUT_DIR):OUTPUT_DIR=$$PWD/WebKitBuild/Release

LIBS += -L$$OUTPUT_DIR/lib -lWebKitQt -lJavaScriptCore

macx {
	INCLUDEPATH += /opt/local/include /opt/local/include/libxml2
	INCLUDEPATH += /usr/include/libxml2
	LIBS += -L/opt/local/lib -lxml2 -lxslt
}
