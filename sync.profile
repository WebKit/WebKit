%modules = ( # path to module name map
    # Has to be the same directory as the pro file that generates install rules,
    # otherwise the relative paths in headers.pri will not be correct.
    "QtWebKit" => "$basedir/Source"
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
    "QtWebKit" => "$basedir/Tools/qmake/mkspecs/modules/qt_webkit.pri",
);
@ignore_for_master_contents = ( "qwebscriptworld.h" );
