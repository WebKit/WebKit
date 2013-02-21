AC_PREREQ(2.60)

m4_include([Source/autotools/Versions.m4])

AC_INIT([WebKitGTK],[webkit_major_version.webkit_minor_version.webkit_micro_version],[http://bugs.webkit.org/])
AC_CONFIG_MACRO_DIR([Source/autotools])
AC_CONFIG_AUX_DIR([Source/autotools])
AC_SUBST(ACLOCAL_AMFLAGS, "-I Source/autotools")
AC_CONFIG_HEADERS([autotoolsconfig.h])
AC_CONFIG_SRCDIR([Source/WebCore/config.h])

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

m4_include([Source/autotools/CheckSystemAndBasicDependencies.m4])
m4_include([Source/autotools/SetupLibtool.m4])
m4_include([Source/autotools/ReadCommandLineArguments.m4])
m4_include([Source/autotools/FindDependencies.m4])
m4_include([Source/autotools/SetupCompilerFlags.m4])
m4_include([Source/autotools/SetupAutoconfHeader.m4])
m4_include([Source/autotools/SetupWebKitFeatures.m4])

# We would put AM_INIT_AUTOMAKE into SetupAutomake.m4, but seems to cause autoconf errors.
AM_INIT_AUTOMAKE([foreign subdir-objects dist-xz no-dist-gzip tar-ustar])
m4_include([Source/autotools/SetupAutomake.m4])

######################################################################################
# Processing of configuration files
######################################################################################

AC_CONFIG_FILES([GNUmakefile])
AC_CONFIG_FILES([DerivedSources/WebCore/UserAgentGtk.h:Source/WebCore/platform/gtk/UserAgentGtk.h.in])
AC_CONFIG_FILES(
    [Source/JavaScriptCore/javascriptcoregtk-${WEBKITGTK_API_VERSION}.pc:Source/JavaScriptCore/javascriptcoregtk.pc.in
        Source/WebKit/gtk/JSCore-${WEBKITGTK_API_VERSION}.gir:Source/WebKit/gtk/JSCore.gir.in],
    [WEBKITGTK_API_VERSION=$WEBKITGTK_API_VERSION,WEBKITGTK_PC_NAME=$WEBKITGTK_PC_NAME])

if test "$enable_webkit1" = "yes"; then
    AC_CONFIG_FILES([Source/WebKit/gtk/webkit/webkitversion.h Source/WebKit/gtk/po/POTFILES])

    AC_CONFIG_FILES([Source/WebKit/gtk/${WEBKITGTK_PC_NAME}-${WEBKITGTK_API_VERSION}.pc:Source/WebKit/gtk/webkit.pc.in],
        [WEBKITGTK_API_VERSION=$WEBKITGTK_API_VERSION,WEBKITGTK_PC_NAME=$WEBKITGTK_PC_NAME])
fi

if test "$enable_webkit2" = "yes"; then
    AC_CONFIG_FILES(
        [DerivedSources/WebKit2/webkit2gtk/webkit2/WebKitVersion.h:Source/WebKit2/UIProcess/API/gtk/WebKitVersion.h.in])

    AC_CONFIG_FILES([Source/WebKit2/webkit2gtk-${WEBKITGTK_API_VERSION}.pc:Source/WebKit2/webkit2gtk.pc.in],
        [WEBKITGTK_API_VERSION=$WEBKITGTK_API_VERSION,WEBKITGTK_PC_NAME=$WEBKITGTK_PC_NAME])
fi

m4_include([Source/autotools/PrintBuildConfiguration.m4])
