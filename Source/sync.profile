%modules = ( # path to module name map
    "QtWebKitWidgets" => "$basedir"
);
%moduleheaders = ( # restrict the module headers to those found in relative path
    "QtWebKitWidgets" => "WebKit/qt/Api;WebKit2/UIProcess/API/qt",
);
%classnames = (
);
@ignore_for_master_contents = ( "qwebscriptworld.h", "testwindow.h", "util.h", "bytearraytestdata.h" );
