add_definitions(-ObjC++)

if ("${CURRENT_OSX_VERSION}" MATCHES "10.9")
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceMavericks.a)
elif ("${CURRENT_OSX_VERSION}" MATCHES "10.10")
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceYosemite.a)
else ()
set(WEBKITSYSTEMINTERFACE_LIBRARY libWebKitSystemInterfaceElCapitan.a)
endif ()
link_directories(../../WebKitLibraries)

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

add_definitions(-iframework ${QUARTZ_LIBRARY}/Frameworks)

list(APPEND WebKit2_LIBRARIES
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
    ${QUARTZ_LIBRARY}
    ${QUARTZCORE_LIBRARY}
    ${SECURITY_LIBRARY}
    ${SQLITE3_LIBRARY}
    ${WEBKITSYSTEMINTERFACE_LIBRARY}
    ${XML2_LIBRARY}
    ${ZLIB_LIBRARIES}
)

list(APPEND WebKit2_SOURCES
    NetworkProcess/cocoa/NetworkProcessCocoa.mm

    NetworkProcess/mac/NetworkDiskCacheMonitor.mm
    NetworkProcess/mac/NetworkProcessMac.mm
    NetworkProcess/mac/NetworkResourceLoaderMac.mm
    NetworkProcess/mac/RemoteNetworkingContext.mm

    Shared/API/Cocoa/RemoteObjectRegistry.mm
    Shared/API/Cocoa/WKBrowsingContextHandle.mm
    Shared/API/Cocoa/WKRemoteObject.mm
    Shared/API/Cocoa/WKRemoteObjectCoder.mm
    Shared/API/Cocoa/WebKit.m
    Shared/API/Cocoa/_WKFrameHandle.mm
    Shared/API/Cocoa/_WKHitTestResult.mm
    Shared/API/Cocoa/_WKNSFileManagerExtras.mm
    Shared/API/Cocoa/_WKRemoteObjectInterface.mm
    Shared/API/Cocoa/_WKRemoteObjectRegistry.mm

    Shared/API/c/cf/WKErrorCF.cpp
    Shared/API/c/cf/WKStringCF.mm
    Shared/API/c/cf/WKURLCF.mm

    Shared/API/c/cg/WKImageCG.cpp

    Shared/API/c/mac/WKCertificateInfoMac.mm
    Shared/API/c/mac/WKObjCTypeWrapperRef.mm
    Shared/API/c/mac/WKURLRequestNS.mm
    Shared/API/c/mac/WKURLResponseNS.mm
    Shared/API/c/mac/WKWebArchive.cpp
    Shared/API/c/mac/WKWebArchiveResource.cpp

    UIProcess/API/Cocoa/APISerializedScriptValueCocoa.mm
    UIProcess/API/Cocoa/APIUserContentExtensionStoreCocoa.mm
    UIProcess/API/Cocoa/APIWebsiteDataStoreCocoa.mm
    UIProcess/API/Cocoa/LegacyBundleForClass.mm
    UIProcess/API/Cocoa/WKBackForwardList.mm
    UIProcess/API/Cocoa/WKBackForwardListItem.mm
    UIProcess/API/Cocoa/WKBrowsingContextController.mm
    UIProcess/API/Cocoa/WKError.mm
    UIProcess/API/Cocoa/WKFrameInfo.mm
    UIProcess/API/Cocoa/WKNavigation.mm
    UIProcess/API/Cocoa/WKNavigationAction.mm
    UIProcess/API/Cocoa/WKNavigationResponse.mm
    UIProcess/API/Cocoa/WKPreferences.mm
    UIProcess/API/Cocoa/WKProcessPool.mm
    UIProcess/API/Cocoa/WKScriptMessage.mm
    UIProcess/API/Cocoa/WKSecurityOrigin.mm
    UIProcess/API/Cocoa/WKUserContentController.mm
    UIProcess/API/Cocoa/WKUserScript.mm
    UIProcess/API/Cocoa/WKWebView.mm
    UIProcess/API/Cocoa/WKWebViewConfiguration.mm
    UIProcess/API/Cocoa/WKWebsiteDataRecord.mm
    UIProcess/API/Cocoa/WKWebsiteDataStore.mm
    UIProcess/API/Cocoa/WKWindowFeatures.mm
    UIProcess/API/Cocoa/_WKActivatedElementInfo.mm
    UIProcess/API/Cocoa/_WKDownload.mm
    UIProcess/API/Cocoa/_WKElementAction.mm
    UIProcess/API/Cocoa/_WKErrorRecoveryAttempting.mm
    UIProcess/API/Cocoa/_WKProcessPoolConfiguration.mm
    UIProcess/API/Cocoa/_WKSessionState.mm
    UIProcess/API/Cocoa/_WKThumbnailView.mm
    UIProcess/API/Cocoa/_WKUserContentExtensionStore.mm
    UIProcess/API/Cocoa/_WKUserContentFilter.mm
    UIProcess/API/Cocoa/_WKVisitedLinkStore.mm

    UIProcess/API/mac/WKView.mm

    UIProcess/Cocoa/DiagnosticLoggingClient.mm
    UIProcess/Cocoa/DownloadClient.mm
    UIProcess/Cocoa/FindClient.mm
    UIProcess/Cocoa/NavigationState.mm
    UIProcess/Cocoa/RemoteLayerTreeScrollingPerformanceData.mm
    UIProcess/Cocoa/SessionStateCoding.mm
    UIProcess/Cocoa/UIDelegate.mm
    UIProcess/Cocoa/VersionChecks.mm
    UIProcess/Cocoa/WKReloadFrameErrorRecoveryAttempter.mm
    UIProcess/Cocoa/WKWebViewContentProviderRegistry.mm
    UIProcess/Cocoa/WebPageProxyCocoa.mm
    UIProcess/Cocoa/WebPasteboardProxyCocoa.mm
    UIProcess/Cocoa/WebProcessPoolCocoa.mm
    UIProcess/Cocoa/WebProcessProxyCocoa.mm

    UIProcess/mac/CorrectionPanel.mm
    UIProcess/mac/LegacySessionStateCoding.cpp
    UIProcess/mac/PageClientImpl.mm
    UIProcess/mac/RemoteLayerTreeDrawingAreaProxy.mm
    UIProcess/mac/RemoteLayerTreeHost.mm
    UIProcess/mac/SecItemShimProxy.cpp
    UIProcess/mac/ServicesController.mm
    UIProcess/mac/TextCheckerMac.mm
    UIProcess/mac/TiledCoreAnimationDrawingAreaProxy.mm
    UIProcess/mac/ViewGestureControllerMac.mm
    UIProcess/mac/ViewSnapshotStore.mm
    UIProcess/mac/WKFullKeyboardAccessWatcher.mm
    UIProcess/mac/WKFullScreenWindowController.mm
    UIProcess/mac/WKImmediateActionController.mm
    UIProcess/mac/WKPrintingView.mm
    UIProcess/mac/WKSharingServicePickerDelegate.mm
    UIProcess/mac/WKTextInputWindowController.mm
    UIProcess/mac/WKViewLayoutStrategy.mm
    UIProcess/mac/WebColorPickerMac.mm
    UIProcess/mac/WebContextMenuProxyMac.mm
    UIProcess/mac/WebCookieManagerProxyMac.mm
    UIProcess/mac/WebInspectorProxyMac.mm
    UIProcess/mac/WebPageProxyMac.mm
    UIProcess/mac/WebPopupMenuProxyMac.mm
    UIProcess/mac/WebPreferencesMac.mm
    UIProcess/mac/WebProcessProxyMac.mm
    UIProcess/mac/WindowServerConnection.mm
)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKIT2_DIR})

list(APPEND WebKit2_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/cf"
    "${WEBCORE_DIR}/platform/mac"
    "${WEBCORE_DIR}/platform/network/cf"
    "${WEBCORE_DIR}/platform/network/cocoa"
    "${WEBCORE_DIR}/platform/spi/mac"
    "${WEBCORE_DIR}/platform/graphics/cg"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBKIT2_DIR}/NetworkProcess/cocoa"
    "${WEBKIT2_DIR}/NetworkProcess/mac"
    "${WEBKIT2_DIR}/UIProcess/mac"
    "${WEBKIT2_DIR}/UIProcess/API/C/mac"
    "${WEBKIT2_DIR}/UIProcess/API/Cocoa"
    "${WEBKIT2_DIR}/UIProcess/API/mac"
    "${WEBKIT2_DIR}/UIProcess/Cocoa"
    "${WEBKIT2_DIR}/UIProcess/Scrolling"
    "${WEBKIT2_DIR}/Platform/mac"
    "${WEBKIT2_DIR}/Platform/IPC/mac"
    "${WEBKIT2_DIR}/Platform/spi/Cocoa"
    "${WEBKIT2_DIR}/Shared/API/Cocoa"
    "${WEBKIT2_DIR}/Shared/API/c/cf"
    "${WEBKIT2_DIR}/Shared/API/c/cg"
    "${WEBKIT2_DIR}/Shared/API/c/mac"
    "${WEBKIT2_DIR}/Shared/cf"
    "${WEBKIT2_DIR}/Shared/Cocoa"
    "${WEBKIT2_DIR}/Shared/mac"
    "${WEBKIT2_DIR}/Shared/Plugins/mac"
    "${WEBKIT2_DIR}/Shared/Scrolling"
    "${WEBKIT2_DIR}/WebProcess/Plugins/PDF"
    "${WEBKIT2_DIR}/WebProcess/Scrolling"
    "${WEBKIT2_DIR}/WebProcess/WebPage/mac"
    "${WEBKIT2_DIR}/WebProcess/WebCoreSupport/mac"
    "${DERIVED_SOURCES_DIR}/ForwardingHeaders"
)

set(WEBKIT2_EXTRA_DEPENDENCIES
     WebKit2-forwarding-headers
)
set(WebProcess_SOURCES
     WebProcess/mac/SecItemShimLibrary.mm
)

list(APPEND NetworkProcess_SOURCES
     ${NetworkProcess_COMMON_SOURCES}
)

list(APPEND DatabaseProcess_SOURCES
    DatabaseProcess/EntryPoint/mac/XPCService/DatabaseServiceEntryPoint.mm
)

add_definitions("-include WebKit2Prefix.h")

set(WebKit2_FORWARDING_HEADERS_FILES
    Shared/API/c/WKDiagnosticLoggingResultType.h

    UIProcess/API/C/WKPageDiagnosticLoggingClient.h
    UIProcess/API/C/WKPageNavigationClient.h
    UIProcess/API/C/WKPageRenderingProgressEvents.h
)

list(APPEND WebKit2_MESSAGES_IN_FILES
    Shared/API/Cocoa/RemoteObjectRegistry.messages.in

    Shared/mac/SecItemShim.messages.in

    UIProcess/mac/RemoteLayerTreeDrawingAreaProxy.messages.in
    UIProcess/mac/SecItemShimProxy.messages.in
    UIProcess/mac/ViewGestureController.messages.in

    WebProcess/WebPage/ViewGestureGeometryCollector.messages.in
)

set(WebKit2_FORWARDING_HEADERS_DIRECTORIES
    Shared/API/Cocoa
    Shared/API/c

    Shared/API/c/mac

    UIProcess/Cocoa

    UIProcess/API/C
    UIProcess/API/Cocoa

    WebProcess/WebPage

    WebProcess/InjectedBundle/API/c
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebKit FILES ${WebKit2_FORWARDING_HEADERS_FILES} DIRECTORIES ${WebKit2_FORWARDING_HEADERS_DIRECTORIES})
