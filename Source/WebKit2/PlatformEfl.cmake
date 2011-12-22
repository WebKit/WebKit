INCLUDE(WebKitEfl)

LIST(APPEND WebKit2_LINK_FLAGS
    ${ECORE_X_LDFLAGS}
    ${EDJE_LDFLAGS}
    ${EFLDEPS_LDFLAGS}
    ${EVAS_LDFLAGS}
)

LIST(APPEND WebKit2_SOURCES
    Platform/efl/ModuleEfl.cpp
    Platform/efl/RunLoopEfl.cpp
    Platform/efl/WorkQueueEfl.cpp
    Platform/unix/SharedMemoryUnix.cpp

    Platform/CoreIPC/unix/ConnectionUnix.cpp
    Platform/CoreIPC/unix/AttachmentUnix.cpp

    Shared/API/c/gtk/WKGraphicsContextGtk.cpp

    Shared/cairo/LayerTreeContextCairo.cpp
    Shared/cairo/ShareableBitmapCairo.cpp

    Shared/efl/NativeWebKeyboardEventEfl.cpp
    Shared/efl/NativeWebWheelEventEfl.cpp
    Shared/efl/NativeWebMouseEventEfl.cpp
    Shared/efl/WebEventFactory.cpp
    Shared/efl/WebCoreArgumentCodersEfl.cpp

    UIProcess/API/efl/PageClientImpl.cpp
    UIProcess/API/efl/ewk_view.cpp

    UIProcess/cairo/BackingStoreCairo.cpp

    UIProcess/efl/TextCheckerEfl.cpp
    UIProcess/efl/WebContextEfl.cpp
    UIProcess/efl/WebInspectorEfl.cpp
    UIProcess/efl/WebPageProxyEfl.cpp
    UIProcess/efl/WebPreferencesEfl.cpp

    UIProcess/Launcher/efl/ProcessLauncherEfl.cpp
    UIProcess/Launcher/efl/ThreadLauncherEfl.cpp

    UIProcess/Plugins/efl/PluginInfoStoreEfl.cpp
    UIProcess/Plugins/efl/PluginProcessProxyEfl.cpp

    WebProcess/efl/WebProcessEfl.cpp
    WebProcess/efl/WebProcessMainEfl.cpp

    WebProcess/InjectedBundle/efl/InjectedBundleEfl.cpp

    WebProcess/WebCoreSupport/efl/WebContextMenuClientEfl.cpp
    WebProcess/WebCoreSupport/efl/WebEditorClientEfl.cpp
    WebProcess/WebCoreSupport/efl/WebErrorsEfl.cpp
    WebProcess/WebCoreSupport/efl/WebPopupMenuEfl.cpp

    WebProcess/WebPage/efl/WebInspectorEfl.cpp
    WebProcess/WebPage/efl/WebPageEfl.cpp
)

LIST(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/efl"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBKIT2_DIR}/Shared/efl"
    "${WEBKIT2_DIR}/UIProcess/API/efl/"
    "${WEBKIT2_DIR}/WebProcess/efl"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/efl"
    ${Cairo_INCLUDE_DIRS}
    ${ECORE_X_INCLUDE_DIRS}
    ${EDJE_INCLUDE_DIRS}
    ${EFLDEPS_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIRS}
    ${SQLITE_INCLUDE_DIRS}
)

LIST(APPEND WebKit2_LIBRARIES
    ${Cairo_LIBRARIES}
    ${ECORE_X_LIBRARIES}
    ${EFLDEPS_LIBRARIES}
    ${Freetype_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${SQLITE_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${PNG_LIBRARY}
    ${JPEG_LIBRARY}
    ${CMAKE_DL_LIBS}
)

LIST (APPEND WebProcess_SOURCES
    efl/MainEfl.cpp
)

LIST (APPEND WebProcess_LIBRARIES
    ${Cairo_LIBRARIES}
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

IF (WTF_USE_SOUP)
    LIST(APPEND WebKit2_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/network/soup"
        ${LIBSOUP24_INCLUDE_DIRS}
    )
    LIST(APPEND WebKit2_LIBRARIES ${LIBSOUP24_LIBRARIES})
    LIST(APPEND WebKit2_SOURCES
        WebProcess/Cookies/soup/WebCookieManagerSoup.cpp
        WebProcess/Downloads/soup/DownloadSoup.cpp
    )

    ADD_CUSTOM_TARGET(forwarding-headerSoup
        COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl ${WEBKIT2_DIR} ${DERIVED_SOURCES_WEBKIT2_DIR}/include soup
    )
    SET(ForwardingNetworkHeaders_NAME forwarding-headerSoup)
ENDIF ()

IF (WTF_USE_CURL)
    LIST(APPEND WebKit2_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/network/curl"
        ${CURL_INCLUDE_DIRS}
    )
    LIST(APPEND WebKit2_LIBRARIES ${CURL_LIBRARIES})
    LIST(APPEND WebKit2_SOURCES
        WebProcess/Cookies/curl/WebCookieManagerCurl.cpp
        WebProcess/Downloads/curl/DownloadCurl.cpp
    )
ENDIF ()

IF (ENABLE_GLIB_SUPPORT)
    LIST(APPEND WebKit2_INCLUDE_DIRECTORIES
        ${Glib_INCLUDE_DIRS}
        ${JAVASCRIPTCORE_DIR}/wtf/gobject
    )
    LIST(APPEND WebKit2_LIBRARIES
        ${Glib_LIBRARIES}
    )
ENDIF ()
