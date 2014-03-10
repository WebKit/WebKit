file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKITGTK_DIR})
file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKITGTK_API_DIR})
configure_file(gtk/webkit/webkitversion.h.in ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitversion.h)
configure_file(gtk/webkit.pc.in ${WebKit_PKGCONFIG_FILE} @ONLY)

add_definitions(-DPACKAGE_LOCALE_DIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

list(APPEND WebKit_INCLUDE_DIRECTORIES
    ${DERIVED_SOURCES_DIR}
    ${DERIVED_SOURCES_GOBJECT_DOM_BINDINGS_DIR}
    ${DERIVED_SOURCES_WEBKITGTK_DIR}
    ${DERIVED_SOURCES_WEBKITGTK_API_DIR}
    ${THIRDPARTY_DIR}/ANGLE/include/GLSLANG
    ${THIRDPARTY_DIR}/ANGLE/src
    ${THIRDPARTY_DIR}/ANGLE/include
    ${THIRDPARTY_DIR}/ANGLE/include/KHR
    ${THIRDPARTY_DIR}/ANGLE/include/GLSLANG
    ${WEBCORE_DIR}/ForwardingHeaders
    ${WEBCORE_DIR}/accessibility/atk
    ${WEBCORE_DIR}/platform/cairo
    ${WEBCORE_DIR}/platform/geoclue
    ${WEBCORE_DIR}/platform/graphics/cairo
    ${WEBCORE_DIR}/platform/graphics/gtk
    ${WEBCORE_DIR}/platform/graphics/opentype
    ${WEBCORE_DIR}/platform/graphics/texmap
    ${WEBCORE_DIR}/platform/gtk
    ${WEBCORE_DIR}/platform/network/soup
    ${WEBCORE_DIR}/platform/text/enchant
    ${WEBKIT_DIR}/gtk
    ${WEBKIT_DIR}/gtk/webkit
    ${WEBKIT_DIR}/gtk/WebCoreSupport
    ${ENCHANT_INCLUDE_DIRS}
    ${GEOCLUE_INCLUDE_DIRS}
    ${GTK_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

list(APPEND WebKit_SOURCES
    ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitenumtypes.cpp
    ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitmarshal.cpp

    gtk/WebCoreSupport/AcceleratedCompositingContextGL.cpp
    gtk/WebCoreSupport/ChromeClientGtk.cpp
    gtk/WebCoreSupport/ContextMenuClientGtk.cpp
    gtk/WebCoreSupport/DeviceMotionClientGtk.cpp
    gtk/WebCoreSupport/DeviceOrientationClientGtk.cpp
    gtk/WebCoreSupport/DocumentLoaderGtk.cpp
    gtk/WebCoreSupport/DragClientGtk.cpp
    gtk/WebCoreSupport/DumpRenderTreeSupportGtk.cpp
    gtk/WebCoreSupport/EditorClientGtk.cpp
    gtk/WebCoreSupport/FrameLoaderClientGtk.cpp
    gtk/WebCoreSupport/FrameNetworkingContextGtk.cpp
    gtk/WebCoreSupport/GeolocationClientGtk.cpp
    gtk/WebCoreSupport/GtkAdjustmentWatcher.cpp
    gtk/WebCoreSupport/InspectorClientGtk.cpp
    gtk/WebCoreSupport/NavigatorContentUtilsClientGtk.cpp
    gtk/WebCoreSupport/PlatformStrategiesGtk.cpp
    gtk/WebCoreSupport/ProgressTrackerClientGtk.cpp
    gtk/WebCoreSupport/TextCheckerClientGtk.cpp
    gtk/WebCoreSupport/UserMediaClientGtk.cpp
    gtk/WebCoreSupport/WebViewInputMethodFilter.cpp

    gtk/webkit/webkitapplicationcache.cpp
    gtk/webkit/webkitauthenticationdialog.cpp
    gtk/webkit/webkitdownload.cpp
    gtk/webkit/webkiterror.cpp
    gtk/webkit/webkitfavicondatabase.cpp
    gtk/webkit/webkitfilechooserrequest.cpp
    gtk/webkit/webkitgeolocationpolicydecision.cpp
    gtk/webkit/webkitglobals.cpp
    gtk/webkit/webkithittestresult.cpp
    gtk/webkit/webkiticondatabase.cpp
    gtk/webkit/webkitnetworkrequest.cpp
    gtk/webkit/webkitnetworkresponse.cpp
    gtk/webkit/webkitsecurityorigin.cpp
    gtk/webkit/webkitsoupauthdialog.cpp
    gtk/webkit/webkitspellchecker.cpp
    gtk/webkit/webkitspellcheckerenchant.cpp
    gtk/webkit/webkitversion.cpp
    gtk/webkit/webkitviewportattributes.cpp
    gtk/webkit/webkitwebbackforwardlist.cpp
    gtk/webkit/webkitwebdatabase.cpp
    gtk/webkit/webkitwebdatasource.cpp
    gtk/webkit/webkitwebframe.cpp
    gtk/webkit/webkitwebhistoryitem.cpp
    gtk/webkit/webkitwebinspector.cpp
    gtk/webkit/webkitwebnavigationaction.cpp
    gtk/webkit/webkitwebplugin.cpp
    gtk/webkit/webkitwebplugindatabase.cpp
    gtk/webkit/webkitwebpolicydecision.cpp
    gtk/webkit/webkitwebresource.cpp
    gtk/webkit/webkitwebsettings.cpp
    gtk/webkit/webkitwebview.cpp
    gtk/webkit/webkitwebwindowfeatures.cpp
)

list(APPEND WebKitGTK_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitenumtypes.h
    ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitversion.h
    ${WEBKIT_DIR}/gtk/webkit/webkit.h
    ${WEBKIT_DIR}/gtk/webkit/webkitapplicationcache.h
    ${WEBKIT_DIR}/gtk/webkit/webkitdefines.h
    ${WEBKIT_DIR}/gtk/webkit/webkitdom.h
    ${WEBKIT_DIR}/gtk/webkit/webkitdownload.h
    ${WEBKIT_DIR}/gtk/webkit/webkiterror.h
    ${WEBKIT_DIR}/gtk/webkit/webkitfavicondatabase.h
    ${WEBKIT_DIR}/gtk/webkit/webkitfilechooserrequest.h
    ${WEBKIT_DIR}/gtk/webkit/webkitgeolocationpolicydecision.h
    ${WEBKIT_DIR}/gtk/webkit/webkitglobals.h
    ${WEBKIT_DIR}/gtk/webkit/webkithittestresult.h
    ${WEBKIT_DIR}/gtk/webkit/webkiticondatabase.h
    ${WEBKIT_DIR}/gtk/webkit/webkitnetworkrequest.h
    ${WEBKIT_DIR}/gtk/webkit/webkitnetworkresponse.h
    ${WEBKIT_DIR}/gtk/webkit/webkitsecurityorigin.h
    ${WEBKIT_DIR}/gtk/webkit/webkitsoupauthdialog.h
    ${WEBKIT_DIR}/gtk/webkit/webkitspellchecker.h
    ${WEBKIT_DIR}/gtk/webkit/webkitviewportattributes.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebbackforwardlist.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebdatabase.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebdatasource.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebframe.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebhistoryitem.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebinspector.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebnavigationaction.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebplugin.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebplugindatabase.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebpolicydecision.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebresource.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebsettings.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebview.h
    ${WEBKIT_DIR}/gtk/webkit/webkitwebwindowfeatures.h
)

# Since the GObjectDOMBindings convenience library exports API that is unused except
# in embedding applications we need to instruct the linker to link all symbols explicitly.
list(APPEND WebKit_LIBRARIES
    GObjectDOMBindings
    WebCorePlatformGTK
)
ADD_WHOLE_ARCHIVE_TO_LIBRARIES(WebKit_LIBRARIES)

set(WebKit_MARSHAL_LIST ${WEBKIT_DIR}/gtk/webkitmarshal.list)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitmarshal.cpp
           ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitmarshal.h
    MAIN_DEPENDENCY ${WebKit_MARSHAL_LIST}

    COMMAND echo extern \"C\" { > ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitmarshal.cpp
    COMMAND glib-genmarshal --prefix=webkit_marshal ${WebKit_MARSHAL_LIST} --body >> ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitmarshal.cpp
    COMMAND echo } >> ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitmarshal.cpp

    COMMAND glib-genmarshal --prefix=webkit_marshal ${WebKit_MARSHAL_LIST} --header > ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitmarshal.h
    VERBATIM
)

# To generate webkitenumtypes.h we want to use all installed headers, except webkitenumtypes.h itself.
set(WebKitGTK_ENUM_GENERATION_HEADERS ${WebKitGTK_INSTALLED_HEADERS})
list(REMOVE_ITEM WebKitGTK_ENUM_GENERATION_HEADERS ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitenumtypes.h)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitenumtypes.h
           ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitenumtypes.cpp
    DEPENDS ${WebKitGTK_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/gtk/webkit/webkitenumtypes.h.template ${WebKitGTK_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitenumtypes.h

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/gtk/webkit/webkitenumtypes.cpp.template ${WebKitGTK_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${DERIVED_SOURCES_WEBKITGTK_API_DIR}/webkitenumtypes.cpp
    VERBATIM
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/WebKit-${WEBKITGTK_API_VERSION}.gir
    DEPENDS WebKit
    DEPENDS ${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
    COMMAND CC=${CMAKE_C_COMPILER} CFLAGS=-Wno-deprecated-declarations
        ${INTROSPECTION_SCANNER}
        --quiet
        --warn-all
        --symbol-prefix=webkit
        --identifier-prefix=WebKit
        --namespace=WebKit
        --nsversion=${WEBKITGTK_API_VERSION}
        --include=GObject-2.0
        --include=Gtk-${WEBKITGTK_API_VERSION}
        --include=Soup-2.4
        --include-uninstalled=${CMAKE_BINARY_DIR}/JavaScriptCore-${WEBKITGTK_API_VERSION}.gir
        --library=webkitgtk-${WEBKITGTK_API_VERSION}
        --library=javascriptcoregtk-${WEBKITGTK_API_VERSION}
        -L${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
        --no-libtool
        --pkg=gobject-2.0
        --pkg=gtk+-${WEBKITGTK_API_VERSION}
        --pkg=libsoup-2.4
        --pkg-export=webkitgtk-${WEBKITGTK_API_VERSION}
        --output=${CMAKE_BINARY_DIR}/WebKit-${WEBKITGTK_API_VERSION}.gir
        --c-include="webkit/webkit.h"
        -DBUILDING_WEBKIT
        -I${CMAKE_SOURCE_DIR}/Source
        -I${WEBKIT_DIR}/gtk
        -I${JAVASCRIPTCORE_DIR}/ForwardingHeaders
        -I${DERIVED_SOURCES_DIR}
        -I${DERIVED_SOURCES_WEBKITGTK_DIR}
        -I${WEBCORE_DIR}/platform/gtk
        ${GObjectDOMBindings_GIR_HEADERS}
        ${WebKitGTK_INSTALLED_HEADERS}
        ${WEBKIT_DIR}/gtk/webkit/*.cpp
)

add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/WebKit-${WEBKITGTK_API_VERSION}.typelib
    DEPENDS ${CMAKE_BINARY_DIR}/WebKit-${WEBKITGTK_API_VERSION}.gir
    COMMAND ${INTROSPECTION_COMPILER} --includedir=${CMAKE_BINARY_DIR} ${CMAKE_BINARY_DIR}/WebKit-${WEBKITGTK_API_VERSION}.gir -o ${CMAKE_BINARY_DIR}/WebKit-${WEBKITGTK_API_VERSION}.typelib
)

ADD_TYPELIB(${CMAKE_BINARY_DIR}/WebKit-${WEBKITGTK_API_VERSION}.typelib)

install(FILES "${CMAKE_BINARY_DIR}/Source/WebKit/gtk/webkitgtk-${WEBKITGTK_API_VERSION}.pc"
        DESTINATION "${LIB_INSTALL_DIR}/pkgconfig"
)
install(FILES "${WEBKIT_DIR}/gtk/resources/error.html"
        DESTINATION "${DATA_INSTALL_DIR}/resources"
)
install(FILES ${WebKitGTK_INSTALLED_HEADERS}
        DESTINATION "${WEBKITGTK_HEADER_INSTALL_DIR}/webkit"
)
install(FILES ${CMAKE_BINARY_DIR}/WebKit-${WEBKITGTK_API_VERSION}.gir
        DESTINATION ${INTROSPECTION_INSTALL_GIRDIR}
)
install(FILES ${CMAKE_BINARY_DIR}/WebKit-${WEBKITGTK_API_VERSION}.typelib
        DESTINATION ${INTROSPECTION_INSTALL_TYPELIBDIR}
)

file(WRITE ${CMAKE_BINARY_DIR}/gtkdoc-webkitgtk.cfg
    "[webkitgtk]\n"
    "pkgconfig_file=${WebKit_PKGCONFIG_FILE}\n"
    "namespace=webkit\n"
    "cflags=-I${DERIVED_SOURCES_DIR}\n"
    "       -I${CMAKE_SOURCE_DIR}\n"
    "       -I${CMAKE_SOURCE_DIR}/Source\n"
    "       -I${CMAKE_SOURCE_DIR}/JavaScriptCore/ForwardingHeaders\n"
    "doc_dir=${WEBKIT_DIR}/gtk/docs\n"
    "source_dirs=${WEBKIT_DIR}/gtk/webkit\n"
    "            ${DERIVED_SOURCES_WEBKITGTK_API_DIR}\n"
    "headers=${WebKitGTK_ENUM_GENERATION_HEADERS}\n"
)
