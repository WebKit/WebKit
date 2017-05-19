file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2_DIR})

configure_file(wpe/wpe-webkit.pc.in ${CMAKE_BINARY_DIR}/wpe-webkit.pc @ONLY)

add_definitions(-DWEBKIT2_COMPILATION)

add_definitions(-DLIBEXECDIR="${LIBEXEC_INSTALL_DIR}")

set(WebKit2_USE_PREFIX_HEADER ON)

add_custom_target(webkit2wpe-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT2_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WEBKIT2_DIR} --output ${FORWARDING_HEADERS_DIR} --platform wpe --platform soup
)

set(WEBKIT2_EXTRA_DEPENDENCIES
    webkit2wpe-forwarding-headers
)

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/unix/WebProcessMain.cpp
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/unix/NetworkProcessMain.cpp
)

list(APPEND DatabaseProcess_SOURCES
    DatabaseProcess/EntryPoint/unix/DatabaseProcessMain.cpp
)

list(APPEND WebKit2_SOURCES
    NetworkProcess/CustomProtocols/soup/LegacyCustomProtocolManagerSoup.cpp

    NetworkProcess/cache/NetworkCacheCodersSoup.cpp
    NetworkProcess/cache/NetworkCacheDataSoup.cpp
    NetworkProcess/cache/NetworkCacheIOChannelSoup.cpp

    NetworkProcess/soup/NetworkDataTaskSoup.cpp
    NetworkProcess/soup/NetworkProcessMainSoup.cpp
    NetworkProcess/soup/NetworkProcessSoup.cpp
    NetworkProcess/soup/NetworkSessionSoup.cpp
    NetworkProcess/soup/RemoteNetworkingContextSoup.cpp

    Platform/IPC/glib/GSocketMonitor.cpp

    Platform/IPC/unix/AttachmentUnix.cpp
    Platform/IPC/unix/ConnectionUnix.cpp

    Platform/classifier/ResourceLoadStatisticsClassifier.cpp

    Platform/glib/ModuleGlib.cpp

    Platform/unix/LoggingUnix.cpp
    Platform/unix/SharedMemoryUnix.cpp

    PluginProcess/unix/PluginControllerProxyUnix.cpp
    PluginProcess/unix/PluginProcessMainUnix.cpp
    PluginProcess/unix/PluginProcessUnix.cpp

    Shared/API/c/cairo/WKImageCairo.cpp

    Shared/Authentication/soup/AuthenticationManagerSoup.cpp

    Shared/CoordinatedGraphics/CoordinatedBackingStore.cpp
    Shared/CoordinatedGraphics/CoordinatedGraphicsScene.cpp
    Shared/CoordinatedGraphics/SimpleViewportController.cpp

    Shared/CoordinatedGraphics/threadedcompositor/CompositingRunLoop.cpp
    Shared/CoordinatedGraphics/threadedcompositor/ThreadSafeCoordinatedSurface.cpp
    Shared/CoordinatedGraphics/threadedcompositor/ThreadedCompositor.cpp
    Shared/CoordinatedGraphics/threadedcompositor/ThreadedDisplayRefreshMonitor.cpp

    Shared/Plugins/Netscape/x11/NetscapePluginModuleX11.cpp

    Shared/cairo/ShareableBitmapCairo.cpp

    Shared/glib/WebErrorsGlib.cpp

    Shared/linux/WebMemorySamplerLinux.cpp

    Shared/soup/WebCoreArgumentCodersSoup.cpp
    Shared/soup/WebErrorsSoup.cpp

    Shared/unix/ChildProcessMain.cpp

    Shared/wpe/NativeWebKeyboardEventWPE.cpp
    Shared/wpe/NativeWebMouseEventWPE.cpp
    Shared/wpe/NativeWebTouchEventWPE.cpp
    Shared/wpe/NativeWebWheelEventWPE.cpp
    Shared/wpe/ProcessExecutablePathWPE.cpp
    Shared/wpe/WebEventFactory.cpp

    UIProcess/BackingStore.cpp
    UIProcess/DefaultUndoController.cpp
    UIProcess/LegacySessionStateCodingNone.cpp
    UIProcess/WebResourceLoadStatisticsManager.cpp
    UIProcess/WebResourceLoadStatisticsStore.cpp

    UIProcess/API/C/WKResourceLoadStatisticsManager.cpp

    UIProcess/API/C/cairo/WKIconDatabaseCairo.cpp

    UIProcess/API/C/soup/WKCookieManagerSoup.cpp

    UIProcess/API/C/wpe/WKView.cpp

    UIProcess/API/wpe/CompositingManagerProxy.cpp
    UIProcess/API/wpe/DrawingAreaProxyWPE.cpp
    UIProcess/API/wpe/PageClientImpl.cpp
    UIProcess/API/wpe/ScrollGestureController.cpp
    UIProcess/API/wpe/WPEView.cpp
    UIProcess/API/wpe/WPEViewClient.cpp

    UIProcess/InspectorServer/soup/WebSocketServerSoup.cpp

    UIProcess/Launcher/wpe/ProcessLauncherWPE.cpp

    UIProcess/Plugins/unix/PluginInfoStoreUnix.cpp
    UIProcess/Plugins/unix/PluginProcessProxyUnix.cpp

    UIProcess/Storage/StorageManager.cpp

    UIProcess/WebsiteData/unix/WebsiteDataStoreUnix.cpp

    UIProcess/cairo/BackingStoreCairo.cpp

    UIProcess/gstreamer/InstallMissingMediaPluginsPermissionRequest.cpp
    UIProcess/gstreamer/WebPageProxyGStreamer.cpp

    UIProcess/linux/MemoryPressureMonitor.cpp

    UIProcess/soup/WebCookieManagerProxySoup.cpp
    UIProcess/soup/WebProcessPoolSoup.cpp

    UIProcess/wpe/TextCheckerWPE.cpp
    UIProcess/wpe/WebInspectorProxyWPE.cpp
    UIProcess/wpe/WebPageProxyWPE.cpp
    UIProcess/wpe/WebPasteboardProxyWPE.cpp
    UIProcess/wpe/WebPreferencesWPE.cpp
    UIProcess/wpe/WebProcessPoolWPE.cpp

    WebProcess/Cookies/soup/WebCookieManagerSoup.cpp
    WebProcess/Cookies/soup/WebKitSoupCookieJarSqlite.cpp

    WebProcess/InjectedBundle/glib/InjectedBundleGlib.cpp

    WebProcess/MediaCache/WebMediaKeyStorageManager.cpp

    WebProcess/WebCoreSupport/soup/WebFrameNetworkingContext.cpp

    WebProcess/WebCoreSupport/wpe/WebContextMenuClientWPE.cpp
    WebProcess/WebCoreSupport/wpe/WebEditorClientWPE.cpp
    WebProcess/WebCoreSupport/wpe/WebPopupMenuWPE.cpp

    WebProcess/WebPage/AcceleratedSurface.cpp

    WebProcess/WebPage/CoordinatedGraphics/AreaAllocator.cpp
    WebProcess/WebPage/CoordinatedGraphics/CompositingCoordinator.cpp
    WebProcess/WebPage/CoordinatedGraphics/CoordinatedLayerTreeHost.cpp
    WebProcess/WebPage/CoordinatedGraphics/ThreadedCoordinatedLayerTreeHost.cpp
    WebProcess/WebPage/CoordinatedGraphics/UpdateAtlas.cpp

    WebProcess/WebPage/gstreamer/WebPageGStreamer.cpp

    WebProcess/WebPage/wpe/AcceleratedSurfaceWPE.cpp
    WebProcess/WebPage/wpe/CompositingManager.cpp
    WebProcess/WebPage/wpe/DrawingAreaWPE.cpp
    WebProcess/WebPage/wpe/WebInspectorUIWPE.cpp
    WebProcess/WebPage/wpe/WebPageWPE.cpp

    WebProcess/soup/WebKitSoupRequestInputStream.cpp
    WebProcess/soup/WebProcessSoup.cpp

    WebProcess/wpe/WebProcessMainWPE.cpp

    # FIXME-GWSHARE:
    DatabaseProcess/gtk/DatabaseProcessMainGtk.cpp
)

list(APPEND WebKit2_MESSAGES_IN_FILES
    UIProcess/API/wpe/CompositingManagerProxy.messages.in
)

list(APPEND WebKit2_DERIVED_SOURCES
    ${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.c
)

set(WebKit2Resources
)

if (ENABLE_WEB_AUDIO)
    list(APPEND WebKit2Resources
        "        <file alias=\"audio/Composite\">Composite.wav</file>\n"
    )
endif ()

file(WRITE ${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.xml
    "<?xml version=1.0 encoding=UTF-8?>\n"
    "<gresources>\n"
    "    <gresource prefix=\"/org/webkitwpe/resources\">\n"
    ${WebKit2Resources}
    "    </gresource>\n"
    "</gresources>\n"
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.c
    DEPENDS ${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.xml
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/Resources --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/platform/audio/resources --target=${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.c ${DERIVED_SOURCES_WEBKIT2_DIR}/WebKit2ResourcesGResourceBundle.xml
    VERBATIM
)

list(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${FORWARDING_HEADERS_DIR}"
    "${DERIVED_SOURCES_DIR}"
    "${WEBCORE_DIR}/platform/graphics/cairo"
    "${WEBCORE_DIR}/platform/graphics/freetype"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/texmap/coordinated"
    "${WEBCORE_DIR}/platform/network/soup"
    "${WEBKIT2_DIR}/DatabaseProcess/unix"
    "${WEBKIT2_DIR}/NetworkProcess/CustomProtocols/soup"
    "${WEBKIT2_DIR}/NetworkProcess/Downloads/soup"
    "${WEBKIT2_DIR}/NetworkProcess/soup"
    "${WEBKIT2_DIR}/NetworkProcess/unix"
    "${WEBKIT2_DIR}/Platform/IPC/glib"
    "${WEBKIT2_DIR}/Platform/IPC/unix"
    "${WEBKIT2_DIR}/Platform/classifier"
    "${WEBKIT2_DIR}/Shared/API/c/wpe"
    "${WEBKIT2_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT2_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT2_DIR}/Shared/soup"
    "${WEBKIT2_DIR}/Shared/unix"
    "${WEBKIT2_DIR}/Shared/wpe"
    "${WEBKIT2_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT2_DIR}/UIProcess/API/C/soup"
    "${WEBKIT2_DIR}/UIProcess/API/C/wpe"
    "${WEBKIT2_DIR}/UIProcess/API/wpe"
    "${WEBKIT2_DIR}/UIProcess/Network/CustomProtocols/soup"
    "${WEBKIT2_DIR}/UIProcess/linux"
    "${WEBKIT2_DIR}/UIProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/soup"
    "${WEBKIT2_DIR}/WebProcess/unix"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/soup"
    "${WEBKIT2_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT2_DIR}/WebProcess/WebPage/wpe"
    "${WTF_DIR}/wtf/gtk/"
    "${WTF_DIR}/wtf/gobject"
    "${WTF_DIR}"
    ${CAIRO_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${FREETYPE2_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${HARFBUZZ_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
)

list(APPEND WebKit2_LIBRARIES
    WebCorePlatformWPE
    ${CAIRO_LIBRARIES}
    ${FREETYPE2_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${HARFBUZZ_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${WPE_LIBRARIES}
)

set(InspectorFiles
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/Localizations/en.lproj/localizedStrings.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/*.html
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Base/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Controllers/*.css
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Controllers/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Debug/*.css
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Debug/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/CodeMirror/*.css
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/CodeMirror/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/ESLint/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/External/Esprima/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Images/gtk/*.png
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Images/gtk/*.svg
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Models/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Protocol/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Proxies/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Test/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Views/*.css
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Views/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Workers/Formatter/*.js
    ${CMAKE_SOURCE_DIR}/Source/WebInspectorUI/UserInterface/Workers/HeapSnapshot/*.js
)

file(GLOB InspectorFilesDependencies
    ${InspectorFiles}
)

# DerivedSources/JavaScriptCore/inspector/InspectorBackendCommands.js is
# expected in DerivedSources/WebInspectorUI/UserInterface/Protocol/.
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    DEPENDS ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js
    COMMAND cp ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}/inspector/InspectorBackendCommands.js ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml
    DEPENDS ${InspectorFilesDependencies}
            ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
            ${TOOLS_DIR}/wpe/generate-inspector-gresource-manifest.py
    COMMAND ${TOOLS_DIR}/wpe/generate-inspector-gresource-manifest.py --output=${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml ${InspectorFiles} ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/UserInterface/Protocol/InspectorBackendCommands.js
    VERBATIM
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.c
    DEPENDS ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebInspectorUI --sourcedir=${DERIVED_SOURCES_WEBINSPECTORUI_DIR} --target=${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.c ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.xml
    VERBATIM
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/WebKit2InspectorGResourceBundle.c
    DEPENDS ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKit2InspectorGResourceBundle.xml
            ${WEBKIT2_DIR}/UIProcess/InspectorServer/front-end/inspectorPageIndex.html
    COMMAND glib-compile-resources --generate --sourcedir=${WEBKIT2_DIR}/UIProcess/InspectorServer/front-end --target=${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/WebKit2InspectorGResourceBundle.c ${WEBKIT2_DIR}/UIProcess/API/wpe/WebKit2InspectorGResourceBundle.xml
    VERBATIM
)

list(APPEND WPEWebInspectorResources_DERIVED_SOURCES
    ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/InspectorGResourceBundle.c
    ${DERIVED_SOURCES_WEBINSPECTORUI_DIR}/WebKit2InspectorGResourceBundle.c
)

list(APPEND WPEWebInspectorResources_LIBRARIES
    ${GLIB_GIO_LIBRARIES}
)

list(APPEND WPEWebInspectorResources_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

add_library(WPEWebInspectorResources SHARED ${WPEWebInspectorResources_DERIVED_SOURCES})
add_dependencies(WPEWebInspectorResources WebKit2)
target_link_libraries(WPEWebInspectorResources ${WPEWebInspectorResources_LIBRARIES})
target_include_directories(WPEWebInspectorResources PUBLIC ${WPEWebInspectorResources_INCLUDE_DIRECTORIES})
install(TARGETS WPEWebInspectorResources DESTINATION "${LIB_INSTALL_DIR}")

if (EXPORT_DEPRECATED_WEBKIT2_C_API)
    set(WPE_INSTALLED_WEBKIT_HEADERS
        ${WEBKIT2_DIR}/Shared/API/c/WKArray.h
        ${WEBKIT2_DIR}/Shared/API/c/WKBase.h
        ${WEBKIT2_DIR}/Shared/API/c/WKData.h
        ${WEBKIT2_DIR}/Shared/API/c/WKDeclarationSpecifiers.h
        ${WEBKIT2_DIR}/Shared/API/c/WKDiagnosticLoggingResultType.h
        ${WEBKIT2_DIR}/Shared/API/c/WKDictionary.h
        ${WEBKIT2_DIR}/Shared/API/c/WKErrorRef.h
        ${WEBKIT2_DIR}/Shared/API/c/WKEvent.h
        ${WEBKIT2_DIR}/Shared/API/c/WKFindOptions.h
        ${WEBKIT2_DIR}/Shared/API/c/WKGeometry.h
        ${WEBKIT2_DIR}/Shared/API/c/WKImage.h
        ${WEBKIT2_DIR}/Shared/API/c/WKMutableArray.h
        ${WEBKIT2_DIR}/Shared/API/c/WKMutableDictionary.h
        ${WEBKIT2_DIR}/Shared/API/c/WKNumber.h
        ${WEBKIT2_DIR}/Shared/API/c/WKPageLoadTypes.h
        ${WEBKIT2_DIR}/Shared/API/c/WKPageVisibilityTypes.h
        ${WEBKIT2_DIR}/Shared/API/c/WKSecurityOriginRef.h
        ${WEBKIT2_DIR}/Shared/API/c/WKSerializedScriptValue.h
        ${WEBKIT2_DIR}/Shared/API/c/WKString.h
        ${WEBKIT2_DIR}/Shared/API/c/WKType.h
        ${WEBKIT2_DIR}/Shared/API/c/WKURL.h
        ${WEBKIT2_DIR}/Shared/API/c/WKURLRequest.h
        ${WEBKIT2_DIR}/Shared/API/c/WKURLResponse.h
        ${WEBKIT2_DIR}/Shared/API/c/WKUserContentInjectedFrames.h
        ${WEBKIT2_DIR}/Shared/API/c/WKUserContentURLPattern.h
        ${WEBKIT2_DIR}/Shared/API/c/WKUserScriptInjectionTime.h

        ${WEBKIT2_DIR}/Shared/API/c/wpe/WKBaseWPE.h

        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundle.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleBackForwardList.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleBackForwardListItem.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleDOMWindowExtension.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleFileHandleRef.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleFrame.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleHitTestResult.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleInitialize.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleInspector.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleNavigationAction.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleNodeHandle.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePage.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageBanner.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageContextMenuClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageEditorClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageFormClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageFullScreenClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageGroup.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageLoaderClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageOverlay.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePagePolicyClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageResourceLoadClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundlePageUIClient.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleRangeHandle.h
        ${WEBKIT2_DIR}/WebProcess/InjectedBundle/API/c/WKBundleScriptWorld.h

        ${WEBKIT2_DIR}/UIProcess/API/C/WKBackForwardListItemRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKBackForwardListRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContextConfigurationRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContextConnectionClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContextDownloadClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContextHistoryClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContextInjectedBundleClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKContext.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKCookieManager.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKCredential.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKCredentialTypes.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKFrame.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKFrameInfoRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKFramePolicyListener.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKHitTestResult.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKNativeEvent.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKNavigationActionRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKNavigationDataRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKNavigationRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKNavigationResponseRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPage.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageConfigurationRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageContextMenuClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageDiagnosticLoggingClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageFindClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageFindMatchesClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageFormClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageGroup.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageInjectedBundleClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageLoaderClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageNavigationClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPagePolicyClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageRenderingProgressEvents.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPageUIClient.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPluginLoadPolicy.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKPreferencesRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKSessionStateRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKUserContentControllerRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKUserScriptRef.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKViewportAttributes.h
        ${WEBKIT2_DIR}/UIProcess/API/C/WKWindowFeaturesRef.h

        ${WEBKIT2_DIR}/UIProcess/API/C/wpe/WKView.h

        ${WEBKIT2_DIR}/UIProcess/API/C/soup/WKCookieManagerSoup.h
    )

    install(FILES ${WPE_INSTALLED_WEBKIT_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-${WPE_API_VERSION}/WPE/WebKit"
        COMPONENT "Development"
    )

    set(WPE_INSTALLED_HEADERS
        ${WEBKIT2_DIR}/Shared/API/c/wpe/WebKit.h
    )

    install(FILES ${WPE_INSTALLED_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-${WPE_API_VERSION}/WPE"
        COMPONENT "Development"
    )

    install(FILES ${CMAKE_BINARY_DIR}/wpe-webkit.pc
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
        COMPONENT "Development"
    )
endif ()
