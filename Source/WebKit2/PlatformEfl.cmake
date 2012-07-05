LIST(APPEND WebKit2_LINK_FLAGS
    ${ECORE_X_LDFLAGS}
    ${EDJE_LDFLAGS}
    ${EFLDEPS_LDFLAGS}
    ${EVAS_LDFLAGS}
    ${LIBSOUP24_LDFLAGS}
)

LIST(APPEND WebKit2_SOURCES
    Platform/efl/ModuleEfl.cpp
    Platform/efl/WorkQueueEfl.cpp
    Platform/unix/SharedMemoryUnix.cpp

    Platform/CoreIPC/unix/ConnectionUnix.cpp
    Platform/CoreIPC/unix/AttachmentUnix.cpp

    Shared/API/c/cairo/WKImageCairo.cpp

    Shared/API/c/gtk/WKGraphicsContextGtk.cpp

    Shared/cairo/LayerTreeContextCairo.cpp
    Shared/cairo/ShareableBitmapCairo.cpp

    Shared/efl/NativeWebKeyboardEventEfl.cpp
    Shared/efl/NativeWebWheelEventEfl.cpp
    Shared/efl/NativeWebMouseEventEfl.cpp
    Shared/efl/WebEventFactory.cpp
    Shared/efl/WebCoreArgumentCodersEfl.cpp

    UIProcess/API/C/efl/WKView.cpp

    UIProcess/API/C/soup/WKContextSoup.cpp
    UIProcess/API/C/soup/WKSoupRequestManager.cpp

    UIProcess/API/efl/PageClientImpl.cpp
    UIProcess/API/efl/ewk_context.cpp
    UIProcess/API/efl/ewk_intent.cpp
    UIProcess/API/efl/ewk_intent_service.cpp
    UIProcess/API/efl/ewk_url_request.cpp
    UIProcess/API/efl/ewk_view.cpp
    UIProcess/API/efl/ewk_view_loader_client.cpp
    UIProcess/API/efl/ewk_view_resource_load_client.cpp
    UIProcess/API/efl/ewk_web_error.cpp
    UIProcess/API/efl/ewk_web_resource.cpp

    UIProcess/cairo/BackingStoreCairo.cpp

    UIProcess/efl/TextCheckerEfl.cpp
    UIProcess/efl/WebContextEfl.cpp
    UIProcess/efl/WebFullScreenManagerProxyEfl.cpp
    UIProcess/efl/WebInspectorEfl.cpp
    UIProcess/efl/WebPageProxyEfl.cpp
    UIProcess/efl/WebPreferencesEfl.cpp

    UIProcess/soup/WebSoupRequestManagerClient.cpp
    UIProcess/soup/WebSoupRequestManagerProxy.cpp

    UIProcess/Launcher/efl/ProcessLauncherEfl.cpp
    UIProcess/Launcher/efl/ThreadLauncherEfl.cpp

    UIProcess/Plugins/unix/PluginInfoStoreUnix.cpp

    WebProcess/Cookies/soup/WebCookieManagerSoup.cpp

    WebProcess/Downloads/efl/DownloadEfl.cpp
    WebProcess/Downloads/efl/FileDownloaderEfl.cpp

    WebProcess/efl/WebProcessEfl.cpp
    WebProcess/efl/WebProcessMainEfl.cpp

    WebProcess/InjectedBundle/efl/InjectedBundleEfl.cpp

    WebProcess/WebCoreSupport/efl/WebContextMenuClientEfl.cpp
    WebProcess/WebCoreSupport/efl/WebEditorClientEfl.cpp
    WebProcess/WebCoreSupport/efl/WebErrorsEfl.cpp
    WebProcess/WebCoreSupport/efl/WebPopupMenuEfl.cpp

    WebProcess/WebPage/efl/WebInspectorEfl.cpp
    WebProcess/WebPage/efl/WebPageEfl.cpp

    WebProcess/soup/WebSoupRequestManager.cpp
    WebProcess/soup/WebKitSoupRequestGeneric.cpp
    WebProcess/soup/WebKitSoupRequestInputStream.cpp
)

LIST(APPEND WebKit2_MESSAGES_IN_FILES
    UIProcess/soup/WebSoupRequestManagerProxy.messages.in
    WebProcess/soup/WebSoupRequestManager.messages.in
)

LIST(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${JAVASCRIPTCORE_DIR}/llint"
    "${WEBCORE_DIR}/platform/efl"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBCORE_DIR}/svg/graphics"
    "${WEBKIT2_DIR}/Shared/efl"
    "${WEBKIT2_DIR}/UIProcess/API/C/efl"
    "${WEBKIT2_DIR}/UIProcess/API/C/soup"
    "${WEBKIT2_DIR}/UIProcess/API/efl"
    "${WEBKIT2_DIR}/UIProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/Downloads/efl"
    "${WEBKIT2_DIR}/WebProcess/efl"
    "${WEBKIT2_DIR}/WebProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/efl"
    "${WTF_DIR}/wtf/gobject"
    ${CAIRO_INCLUDE_DIRS}
    ${ECORE_X_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EFLDEPS_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIRS}
    ${SQLITE_INCLUDE_DIRS}
    ${Glib_INCLUDE_DIRS}
    ${LIBSOUP24_INCLUDE_DIRS}
    ${WTF_DIR}
)

LIST(APPEND WebKit2_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${ECORE_X_LIBRARIES}
    ${EFLDEPS_LIBRARIES}
    ${Freetype_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${PNG_LIBRARY}
    ${JPEG_LIBRARY}
    ${CMAKE_DL_LIBS}
    ${Glib_LIBRARIES}
    ${LIBSOUP24_LIBRARIES}
)

LIST (APPEND WebProcess_SOURCES
    efl/MainEfl.cpp
)

LIST (APPEND WebProcess_LIBRARIES
    ${CAIRO_LIBRARIES}
    ${ECORE_X_LIBRARIES}
    ${EDJE_LIBRARIES}
    ${EFLDEPS_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${SQLITE_LIBRARIES}
)

ADD_CUSTOM_TARGET(forwarding-headerEfl
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT2_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include efl
)
SET(ForwardingHeaders_NAME forwarding-headerEfl)

ADD_CUSTOM_TARGET(forwarding-headerSoup
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT2_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include soup
)
SET(ForwardingNetworkHeaders_NAME forwarding-headerSoup)

CONFIGURE_FILE(efl/ewebkit2.pc.in ${CMAKE_BINARY_DIR}/WebKit2/efl/ewebkit2.pc @ONLY)
SET (EWebKit2_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/EWebKit2.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_context.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/UIProcess/API/efl/ewk_view.h"
)

INSTALL(FILES ${CMAKE_BINARY_DIR}/WebKit2/efl/ewebkit2.pc DESTINATION lib/pkgconfig)
INSTALL(FILES ${EWebKit2_HEADERS} DESTINATION include/${WebKit2_LIBRARY_NAME}-${PROJECT_VERSION_MAJOR})
