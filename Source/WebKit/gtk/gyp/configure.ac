AC_PREREQ(2.60)

m4_include([WebKitMacros/Versions.m4])

AC_INIT([WebKitGTK],[webkit_major_version.webkit_minor_version.webkit_micro_version],[http://bugs.webkit.org/])
AC_CONFIG_MACRO_DIR([Macros])
AC_CONFIG_AUX_DIR([Macros])
AC_CONFIG_HEADERS([autotoolsconfig.h])

WEBKIT_MAJOR_VERSION=webkit_major_version
WEBKIT_MINOR_VERSION=webkit_minor_version
WEBKIT_MICRO_VERSION=webkit_micro_version
WEBKIT_USER_AGENT_MAJOR_VERSION=webkit_user_agent_major_version
WEBKIT_USER_AGENT_MINOR_VERSION=webkit_user_agent_minor_version
AC_SUBST(WEBKIT_MAJOR_VERSION)
AC_SUBST(WEBKIT_MINOR_VERSION)
AC_SUBST(WEBKIT_MICRO_VERSION)
AC_SUBST(WEBKIT_USER_AGENT_MAJOR_VERSION)
AC_SUBST(WEBKIT_USER_AGENT_MINOR_VERSION)

m4_include([WebKitMacros/CheckSystemAndBasicDependencies.m4])
m4_include([WebKitMacros/ReadCommandLineArguments.m4])
m4_include([WebKitMacros/FindDependencies.m4])
m4_include([WebKitMacros/SetupCompilerFlags.m4])
m4_include([WebKitMacros/SetupAutoconfHeader.m4])
m4_include([WebKitMacros/SetupWebKitFeatures.m4])

# Expose the WebKit features as one long string to the configure template processor.
WEBKIT_FEATURES=`cat WebKitFeatures.txt | tr '\n' ' '`
AC_SUBST(WEBKIT_FEATURES)

AC_SUBST(TOPLEVEL_DIRECTORY)
AC_CONFIG_FILES([Configuration.gypi:Configuration.gypi.in])

AC_CONFIG_COMMANDS([gyp], [./run-gyp])

m4_include([WebKitMacros/PrintBuildConfiguration.m4])

