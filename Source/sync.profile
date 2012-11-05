%modules = ( # path to module name map
    "QtWebKitWidgets" => "$basedir"
);
%moduleheaders = ( # restrict the module headers to those found in relative path
    "QtWebKitWidgets" => "WebKit/qt/Api;WebKit2/UIProcess/API/qt",
);
%classnames = (
);
@ignore_for_master_contents = ( "qwebscriptworld.h", "testwindow.h", "util.h", "bytearraytestdata.h" );

# Module dependencies.
# Every module that is required to build this module should have one entry.
# Each of the module version specifiers can take one of the following values:
#   - A specific Git revision.
#   - any git symbolic ref resolvable from the module's repository (e.g. "refs/heads/master" to track master branch)
#
%dependencies = (
    "qtbase" => "refs/heads/master",
    "qtdeclarative" => "refs/heads/master",
    "qtjsbackend" => "refs/heads/master",
);
