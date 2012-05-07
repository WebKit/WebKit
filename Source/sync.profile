%modules = ( # path to module name map
    "QtWebKit" => "$basedir"
);
%moduleheaders = ( # restrict the module headers to those found in relative path
    "QtWebKit" => "WebKit/qt/Api;WebKit2/UIProcess/API/qt",
);
%classnames = (
);
%mastercontent = (
    "core" => "#include <QtCore/QtCore>\n",
    "gui" => "#include <QtGui/QtGui>\n",
    "network" => "#include <QtNetwork/QtNetwork>\n",
    "script" => "#include <QtScript/QtScript>\n",
);
%modulepris = (
    "QtWebKit" => "$basedir/../Tools/qmake/qt_webkit.pri",
);
@ignore_for_master_contents = ( "qwebscriptworld.h", "testwindow.h", "util.h", "bytearraytestdata.h" );
