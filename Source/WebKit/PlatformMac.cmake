find_library(ACCELERATE_LIBRARY accelerate)
find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
find_library(AUDIOUNIT_LIBRARY AudioUnit)
find_library(CARBON_LIBRARY Carbon)
find_library(COCOA_LIBRARY Cocoa)
find_library(COREAUDIO_LIBRARY CoreAudio)
find_library(DISKARBITRATION_LIBRARY DiskArbitration)
find_library(IOKIT_LIBRARY IOKit)
find_library(IOSURFACE_LIBRARY IOSurface)
find_library(OPENGL_LIBRARY OpenGL)
find_library(QUARTZ_LIBRARY Quartz)
find_library(QUARTZCORE_LIBRARY QuartzCore)
find_library(SECURITY_LIBRARY Security)
find_library(SQLITE3_LIBRARY sqlite3)
find_library(XML2_LIBRARY XML2)
find_package(ZLIB REQUIRED)

find_library(PDFKIT_FRAMEWORK PDFKit HINTS ${QUARTZ_LIBRARY}/Versions/*/Frameworks)
find_path(PDFKIT_INCLUDE_DIRECTORY PDFKit.h HINTS ${PDFKIT_FRAMEWORK}/Versions/*/Headers)

if ("${CURRENT_OSX_VERSION}" MATCHES "10.9")
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceMavericks.a)
else ()
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceYosemite.a)
endif ()
link_directories(../../WebKitLibraries)

list(APPEND WebKit_LIBRARIES
    ${ACCELERATE_LIBRARY}
    ${AUDIOTOOLBOX_LIBRARY}
    ${AUDIOUNIT_LIBRARY}
    ${CARBON_LIBRARY}
    ${COCOA_LIBRARY}
    ${COREAUDIO_LIBRARY}
    ${DISKARBITRATION_LIBRARY}
    ${IOKIT_LIBRARY}
    ${IOSURFACE_LIBRARY}
    ${OPENGL_LIBRARY}
    ${PDFKIT_FRAMEWORK}/PDFKit
    ${QUARTZ_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${SECURITY_LIBRARY}
    ${SQLITE3_LIBRARY}
    ${WEBKITSYSTEMINTERFACE_LIBRARY}
    ${XML2_LIBRARY}
    ${ZLIB_LIBRARIES}
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_DIR}"
    "${DERIVED_SOURCES_WEBCORE_DIR}"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}"
    "${JAVASCRIPTCORE_DIR}/dfg"
    "${WEBCORE_DIR}/accessibility/mac"
    "${WEBCORE_DIR}/bindings/objc"
    "${WEBCORE_DIR}/bridge"
    "${WEBCORE_DIR}/bridge/jsc"
    "${WEBCORE_DIR}/bridge/objc"
    "${WEBCORE_DIR}/ForwardingHeaders/inspector"
    "${WEBCORE_DIR}/loader/archive/cf"
    "${WEBCORE_DIR}/loader/cf"
    "${WEBCORE_DIR}/loader/mac"
    "${WEBCORE_DIR}/page/cocoa"
    "${WEBCORE_DIR}/page/mac"
    "${WEBCORE_DIR}/platform/cf"
    "${WEBCORE_DIR}/platform/cocoa"
    "${WEBCORE_DIR}/platform/graphics/avfoundation"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/cf"
    "${WEBCORE_DIR}/platform/graphics/avfoundation/objc"
    "${WEBCORE_DIR}/platform/graphics/ca"
    "${WEBCORE_DIR}/platform/graphics/ca/mac"
    "${WEBCORE_DIR}/platform/graphics/cocoa"
    "${WEBCORE_DIR}/platform/graphics/cg"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/mac"
    "${WEBCORE_DIR}/platform/mac"
    "${WEBCORE_DIR}/platform/network/cocoa"
    "${WEBCORE_DIR}/platform/network/cf"
    "${WEBCORE_DIR}/platform/network/mac"
    "${WEBCORE_DIR}/platform/text/cf"
    "${WEBCORE_DIR}/platform/text/mac"
    "${WEBCORE_DIR}/plugins/mac"
    ../../WebKitLibraries
)

list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
    "${PDFKIT_INCLUDE_DIRECTORY}"
    mac
    mac/Carbon
    mac/DefaultDelegates
    mac/DOM
    mac/History
    mac/icu
    mac/Misc
    mac/Panels
    mac/Plugins
    mac/Plugins/Hosted
    mac/Storage
    mac/WebCoreSupport
    mac/WebInspector
    mac/WebView
    Storage
)

list(APPEND WebKit_SOURCES
    Storage/StorageAreaImpl.cpp
    Storage/StorageAreaSync.cpp
    Storage/StorageNamespaceImpl.cpp
    Storage/StorageSyncManager.cpp
    Storage/StorageThread.cpp
    Storage/StorageTracker.cpp
    Storage/WebDatabaseProvider.cpp
    Storage/WebStorageNamespaceProvider.cpp

    mac/Carbon/CarbonUtils.m
    mac/Carbon/CarbonWindowAdapter.mm
    mac/Carbon/CarbonWindowContentView.m
    mac/Carbon/CarbonWindowFrame.m
    mac/Carbon/HIViewAdapter.m
    mac/Carbon/HIWebView.mm

    mac/History/BinaryPropertyList.cpp
    mac/History/HistoryPropertyList.mm
    mac/History/WebBackForwardList.mm
    mac/History/WebHistory.mm
    mac/History/WebHistoryItem.mm
    mac/History/WebURLsWithTitles.m

    mac/Misc/OldWebAssertions.c
    mac/Misc/WebCache.mm
    mac/Misc/WebCoreStatistics.mm
    mac/Misc/WebDownload.mm
    mac/Misc/WebElementDictionary.mm
    mac/Misc/WebIconDatabase.mm
    mac/Misc/WebKitErrors.m
    mac/Misc/WebKitLogging.m
    mac/Misc/WebKitNSStringExtras.mm
    mac/Misc/WebKitStatistics.m
    mac/Misc/WebKitSystemBits.m
    mac/Misc/WebKitVersionChecks.m
    mac/Misc/WebLocalizableStrings.mm
    mac/Misc/WebLocalizableStringsInternal.mm
    mac/Misc/WebNSArrayExtras.m
    mac/Misc/WebNSControlExtras.m
    mac/Misc/WebNSDataExtras.m
    mac/Misc/WebNSDictionaryExtras.m
    mac/Misc/WebNSEventExtras.m
    mac/Misc/WebNSFileManagerExtras.mm
    mac/Misc/WebNSImageExtras.m
    mac/Misc/WebNSObjectExtras.mm
    mac/Misc/WebNSPasteboardExtras.mm
    mac/Misc/WebNSPrintOperationExtras.m
    mac/Misc/WebNSURLExtras.mm
    mac/Misc/WebNSURLRequestExtras.m
    mac/Misc/WebNSUserDefaultsExtras.mm
    mac/Misc/WebNSViewExtras.m
    mac/Misc/WebNSWindowExtras.m
    mac/Misc/WebSharingServicePickerController.mm
    mac/Misc/WebStringTruncator.mm
    mac/Misc/WebUserContentURLPattern.mm

    mac/Panels/WebAuthenticationPanel.m
    mac/Panels/WebPanelAuthenticationHandler.m

    mac/Plugins/WebBaseNetscapePluginView.mm
    mac/Plugins/WebBasePluginPackage.mm
    mac/Plugins/WebNetscapeContainerCheckContextInfo.mm
    mac/Plugins/WebNetscapeContainerCheckPrivate.mm
    mac/Plugins/WebNetscapePluginEventHandler.mm
    mac/Plugins/WebNetscapePluginEventHandlerCarbon.mm
    mac/Plugins/WebNetscapePluginEventHandlerCocoa.mm
    mac/Plugins/WebNetscapePluginPackage.mm
    mac/Plugins/WebNetscapePluginStream.mm
    mac/Plugins/WebNetscapePluginView.mm
    mac/Plugins/WebPluginContainerCheck.mm
    mac/Plugins/WebPluginController.mm
    mac/Plugins/WebPluginDatabase.mm
    mac/Plugins/WebPluginPackage.mm
    mac/Plugins/WebPluginRequest.m
    mac/Plugins/WebPluginsPrivate.m
    mac/Plugins/npapi.mm

    mac/Plugins/Hosted/HostedNetscapePluginStream.mm
    mac/Plugins/Hosted/NetscapePluginHostManager.mm
    mac/Plugins/Hosted/NetscapePluginHostProxy.mm
    mac/Plugins/Hosted/NetscapePluginInstanceProxy.mm
    mac/Plugins/Hosted/ProxyInstance.mm
    mac/Plugins/Hosted/ProxyRuntimeObject.mm
    mac/Plugins/Hosted/WebHostedNetscapePluginView.mm
    mac/Plugins/Hosted/WebKitPluginAgent.defs
    mac/Plugins/Hosted/WebKitPluginAgentReply.defs
    mac/Plugins/Hosted/WebKitPluginClient.defs
    mac/Plugins/Hosted/WebKitPluginHost.defs
    mac/Plugins/Hosted/WebKitPluginHostTypes.defs
    mac/Plugins/Hosted/WebTextInputWindowController.m

    mac/Storage/WebDatabaseManager.mm
    mac/Storage/WebDatabaseManagerClient.mm
    mac/Storage/WebDatabaseQuotaManager.mm
    mac/Storage/WebStorageManager.mm
    mac/Storage/WebStorageTrackerClient.mm

    mac/WebCoreSupport/CorrectionPanel.mm
    mac/WebCoreSupport/PopupMenuMac.mm
    mac/WebCoreSupport/SearchPopupMenuMac.mm
    mac/WebCoreSupport/WebAlternativeTextClient.mm
    mac/WebCoreSupport/WebApplicationCache.mm
    mac/WebCoreSupport/WebApplicationCacheQuotaManager.mm
    mac/WebCoreSupport/WebChromeClient.mm
    mac/WebCoreSupport/WebContextMenuClient.mm
    mac/WebCoreSupport/WebDeviceOrientationClient.mm
    mac/WebCoreSupport/WebDragClient.mm
    mac/WebCoreSupport/WebEditorClient.mm
    mac/WebCoreSupport/WebFrameLoaderClient.mm
    mac/WebCoreSupport/WebFrameNetworkingContext.mm
    mac/WebCoreSupport/WebGeolocationClient.mm
    mac/WebCoreSupport/WebIconDatabaseClient.mm
    mac/WebCoreSupport/WebInspectorClient.mm
    mac/WebCoreSupport/WebJavaScriptTextInputPanel.m
    mac/WebCoreSupport/WebKeyGenerator.mm
    mac/WebCoreSupport/WebKitFullScreenListener.mm
    mac/WebCoreSupport/WebNotificationClient.mm
    mac/WebCoreSupport/WebOpenPanelResultListener.mm
    mac/WebCoreSupport/WebPlatformStrategies.mm
    mac/WebCoreSupport/WebProgressTrackerClient.mm
    mac/WebCoreSupport/WebSecurityOrigin.mm
    mac/WebCoreSupport/WebSystemInterface.mm
    mac/WebCoreSupport/WebUserMediaClient.mm
    mac/WebCoreSupport/WebViewGroup.mm

    mac/WebInspector/WebInspector.mm
    mac/WebInspector/WebInspectorFrontend.mm
    mac/WebInspector/WebNodeHighlight.mm
    mac/WebInspector/WebNodeHighlightView.mm
    mac/WebInspector/WebNodeHighlighter.mm

    mac/WebView/WebArchive.mm
    mac/WebView/WebClipView.mm
    mac/WebView/WebDashboardRegion.mm
    mac/WebView/WebDataSource.mm
    mac/WebView/WebDelegateImplementationCaching.mm
    mac/WebView/WebDeviceOrientation.mm
    mac/WebView/WebDeviceOrientationProviderMock.mm
    mac/WebView/WebDocumentLoaderMac.mm
    mac/WebView/WebDynamicScrollBarsView.mm
    mac/WebView/WebFormDelegate.m
    mac/WebView/WebFrame.mm
    mac/WebView/WebFrameView.mm
    mac/WebView/WebFullScreenController.mm
    mac/WebView/WebGeolocationPosition.mm
    mac/WebView/WebHTMLRepresentation.mm
    mac/WebView/WebHTMLView.mm
    mac/WebView/WebIndicateLayer.mm
    mac/WebView/WebJSPDFDoc.mm
    mac/WebView/WebNavigationData.mm
    mac/WebView/WebNotification.mm
    mac/WebView/WebPDFDocumentExtras.mm
    mac/WebView/WebPDFRepresentation.mm
    mac/WebView/WebPDFView.mm
    mac/WebView/WebPolicyDelegate.mm
    mac/WebView/WebPreferences.mm
    mac/WebView/WebResource.mm
    mac/WebView/WebScriptDebugDelegate.mm
    mac/WebView/WebScriptDebugger.mm
    mac/WebView/WebScriptWorld.mm
    mac/WebView/WebTextCompletionController.mm
    mac/WebView/WebTextIterator.mm
    mac/WebView/WebView.mm
    mac/WebView/WebViewData.mm
)

set(WebKit_LIBRARY_TYPE SHARED)

set(WebKitLegacy_FORWARDING_HEADERS_DIRECTORIES
    mac/DOM
    mac/History
    mac/Misc
    mac/Panels
    mac/Plugins
    mac/WebCoreSupport
    mac/WebInspector
    mac/WebView
    ${DERIVED_SOURCES_WEBCORE_DIR}
    ${WEBCORE_DIR}/bindings/objc
    ${WEBCORE_DIR}/plugins
)

set(WebKitLegacy_FORWARDING_HEADERS_FILES
    mac/DOM/WebDOMOperations.h

    mac/History/WebHistory.h
    mac/History/WebHistoryItem.h

    mac/Misc/WebNSURLExtras.h

    mac/Panels/WebPanelAuthenticationHandler.h

    mac/Plugins/WebBasePluginPackage.h

    mac/WebCoreSupport/WebKeyGenerator.h

    mac/WebInspector/WebInspector.h

    mac/WebView/WebFrame.h
    mac/WebView/WebView.h

    ${DERIVED_SOURCES_WEBCORE_DIR}/DOMRange.h

    ${WEBCORE_DIR}/bindings/objc/DOMCore.h
    ${WEBCORE_DIR}/bindings/objc/DOMExtensions.h

    ${WEBCORE_DIR}/plugins/npfunctions.h
)

add_definitions("-include WebKitPrefix.h")

set_source_files_properties(
    ${WEBKIT_DIR}/mac/Carbon/CarbonUtils.m
    ${WEBKIT_DIR}/mac/Carbon/CarbonWindowContentView.m
    ${WEBKIT_DIR}/mac/Carbon/CarbonWindowFrame.m
    ${WEBKIT_DIR}/mac/Carbon/HIViewAdapter.m
    mac/History/WebURLsWithTitles.m

    mac/Misc/OldWebAssertions.c

    mac/Misc/WebKitErrors.m
    mac/Misc/WebKitLogging.m
    mac/Misc/WebKitStatistics.m
    mac/Misc/WebKitSystemBits.m
    mac/Misc/WebKitVersionChecks.m
    mac/Misc/WebNSArrayExtras.m
    mac/Misc/WebNSControlExtras.m
    mac/Misc/WebNSDataExtras.m
    mac/Misc/WebNSDictionaryExtras.m
    mac/Misc/WebNSEventExtras.m
    mac/Misc/WebNSImageExtras.m
    mac/Misc/WebNSPrintOperationExtras.m
    mac/Misc/WebNSURLRequestExtras.m
    mac/Misc/WebNSViewExtras.m
    mac/Misc/WebNSWindowExtras.m

    mac/Panels/WebAuthenticationPanel.m
    mac/Panels/WebPanelAuthenticationHandler.m

    mac/Plugins/WebPluginRequest.m
    mac/Plugins/WebPluginsPrivate.m

    mac/Plugins/Hosted/WebTextInputWindowController.m

    mac/WebCoreSupport/WebJavaScriptTextInputPanel.m

    mac/WebView/WebFormDelegate.m
PROPERTIES COMPILE_FLAGS -std=c99)

file(COPY
    mac/Plugins/Hosted/WebKitPluginAgent.defs
    mac/Plugins/Hosted/WebKitPluginAgentReply.defs
    mac/Plugins/Hosted/WebKitPluginClient.defs
    mac/Plugins/Hosted/WebKitPluginHost.defs
    mac/Plugins/Hosted/WebKitPluginHostTypes.defs
    mac/Plugins/Hosted/WebKitPluginHostTypes.h
DESTINATION ${DERIVED_SOURCES_WEBKITLEGACY_DIR})
add_custom_command(
    OUTPUT
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentReplyServer.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentReplyUser.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentServer.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentUser.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginHostServer.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginHostUser.c
    MAIN_DEPENDENCY mac/Plugins/Hosted/WebKitPluginAgent.defs
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBKITLEGACY_DIR}
    COMMAND mig -I.. WebKitPluginAgent.defs WebKitPluginAgentReply.defs WebKitPluginHost.defs
    VERBATIM)
add_custom_command(
    OUTPUT
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginClientServer.c
        ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginClientUser.c
    MAIN_DEPENDENCY mac/Plugins/Hosted/WebKitPluginAgent.defs
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBKITLEGACY_DIR}
    COMMAND mig -I.. -sheader WebKitPluginClientServer.h WebKitPluginClient.defs
    VERBATIM)
list(APPEND WebKit_SOURCES
    ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentReplyServer.c
    ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentReplyUser.c
    ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentServer.c
    ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginAgentUser.c
    ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginClientServer.c
    ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginClientUser.c
    ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginHostServer.c
    ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitPluginHostUser.c
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebKitLegacy DIRECTORIES ${WebKitLegacy_FORWARDING_HEADERS_DIRECTORIES} FILES ${WebKitLegacy_FORWARDING_HEADERS_FILES})
