add_definitions("-ObjC++ -std=c++17")
find_library(APPLICATIONSERVICES_LIBRARY ApplicationServices)
find_library(CARBON_LIBRARY Carbon)
find_library(SECURITY_LIBRARY Security)
find_library(SECURITYINTERFACE_LIBRARY SecurityInterface)
find_library(QUARTZ_LIBRARY Quartz)
find_library(AVFOUNDATION_LIBRARY AVFoundation)
find_library(AVFAUDIO_LIBRARY AVFAudio HINTS ${AVFOUNDATION_LIBRARY}/Versions/*/Frameworks)
find_library(DEVICEIDENTITY_LIBRARY DeviceIdentity HINTS /System/Library/PrivateFrameworks)
add_definitions(-iframework ${QUARTZ_LIBRARY}/Frameworks)
add_definitions(-iframework ${CARBON_LIBRARY}/Frameworks)
add_definitions(-iframework ${APPLICATIONSERVICES_LIBRARY}/Versions/Current/Frameworks)
add_definitions(-DWK_XPC_SERVICE_SUFFIX=".Development")

set(MACOSX_FRAMEWORK_IDENTIFIER com.apple.WebKit)

list(APPEND WebKit_PRIVATE_LIBRARIES
    WebKitLegacy
    ${APPLICATIONSERVICES_LIBRARY}
    ${DEVICEIDENTITY_LIBRARY}
    ${SECURITYINTERFACE_LIBRARY}
)

if (NOT AVFAUDIO_LIBRARY-NOTFOUND)
    list(APPEND WebKit_LIBRARIES ${AVFAUDIO_LIBRARY})
endif ()

list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES
    "SourcesCocoa.txt"
)

list(APPEND WebKit_SOURCES
    NetworkProcess/cocoa/WebSocketTaskCocoa.mm

    NetworkProcess/Downloads/cocoa/WKDownloadProgress.mm

    Shared/API/Cocoa/WKMain.mm

    UIProcess/Cocoa/WKSafeBrowsingWarning.mm
    UIProcess/Cocoa/WKShareSheet.mm
    UIProcess/Cocoa/WKStorageAccessAlert.mm

    WebProcess/InjectedBundle/API/c/mac/WKBundlePageMac.mm
)

list(APPEND WebKit_SOURCES
    UIProcess/API/Cocoa/WKContentWorld.mm
    UIProcess/API/Cocoa/_WKResourceLoadStatisticsFirstParty.mm
    UIProcess/API/Cocoa/_WKResourceLoadStatisticsThirdParty.mm
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${ICU_INCLUDE_DIRS}"
    "${WEBKIT_DIR}/NetworkProcess/cocoa"
    "${WEBKIT_DIR}/NetworkProcess/mac"
    "${WEBKIT_DIR}/PluginProcess/mac"
    "${WEBKIT_DIR}/UIProcess/mac"
    "${WEBKIT_DIR}/UIProcess/API/C/mac"
    "${WEBKIT_DIR}/UIProcess/API/Cocoa"
    "${WEBKIT_DIR}/UIProcess/API/mac"
    "${WEBKIT_DIR}/UIProcess/API/ios"
    "${WEBKIT_DIR}/UIProcess/Authentication/cocoa"
    "${WEBKIT_DIR}/UIProcess/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Cocoa/SOAuthorization"
    "${WEBKIT_DIR}/UIProcess/Inspector/Cocoa"
    "${WEBKIT_DIR}/UIProcess/Inspector/mac"
    "${WEBKIT_DIR}/UIProcess/Launcher/mac"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/ios"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree/mac"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication/Cocoa"
    "${WEBKIT_DIR}/UIProcess/ios"
    "${WEBKIT_DIR}/Platform/cg"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Platform/classifier/cocoa"
    "${WEBKIT_DIR}/Platform/cocoa"
    "${WEBKIT_DIR}/Platform/mac"
    "${WEBKIT_DIR}/Platform/unix"
    "${WEBKIT_DIR}/Platform/spi/Cocoa"
    "${WEBKIT_DIR}/Platform/spi/mac"
    "${WEBKIT_DIR}/Platform/IPC/mac"
    "${WEBKIT_DIR}/Platform/IPC/cocoa"
    "${WEBKIT_DIR}/Platform/spi/Cocoa"
    "${WEBKIT_DIR}/Shared/API/Cocoa"
    "${WEBKIT_DIR}/Shared/API/c/cf"
    "${WEBKIT_DIR}/Shared/API/c/cg"
    "${WEBKIT_DIR}/Shared/API/c/mac"
    "${WEBKIT_DIR}/Shared/ApplePay/cocoa/"
    "${WEBKIT_DIR}/Shared/Authentication/cocoa"
    "${WEBKIT_DIR}/Shared/ios"
    "${WEBKIT_DIR}/Shared/cf"
    "${WEBKIT_DIR}/Shared/Cocoa"
    "${WEBKIT_DIR}/Shared/EntryPointUtilities/Cocoa/XPCService"
    "${WEBKIT_DIR}/Shared/mac"
    "${WEBKIT_DIR}/Shared/Plugins/mac"
    "${WEBKIT_DIR}/Shared/Scrolling"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication/fido"
    "${WEBKIT_DIR}/WebProcess/WebAuthentication"
    "${WEBKIT_DIR}/WebProcess/cocoa"
    "${WEBKIT_DIR}/WebProcess/mac"
    "${WEBKIT_DIR}/WebProcess/Inspector/mac"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/Cocoa"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/mac"
    "${WEBKIT_DIR}/WebProcess/Plugins/PDF"
    "${WEBKIT_DIR}/WebProcess/Plugins/Netscape/mac"
    "${WEBKIT_DIR}/WebProcess/WebPage/Cocoa"
    "${WEBKIT_DIR}/WebProcess/WebPage/RemoteLayerTree"
    "${WEBKIT_DIR}/WebProcess/WebPage/mac"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/mac"
    "${WEBKITLEGACY_DIR}"
    "${FORWARDING_HEADERS_DIR}/WebCore"
)

set(XPCService_SOURCES
    Shared/EntryPointUtilities/Cocoa/AuxiliaryProcessMain.cpp

    Shared/EntryPointUtilities/Cocoa/XPCService/XPCServiceEntryPoint.mm
    Shared/EntryPointUtilities/Cocoa/XPCService/XPCServiceMain.mm
)

set(WebProcess_SOURCES
    WebProcess/EntryPoint/Cocoa/XPCService/WebContentServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

set(PluginProcess_SOURCES
    PluginProcess/EntryPoint/Cocoa/XPCService/PluginServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

set(GPUProcess_SOURCES
    GPUProcess/EntryPoint/Cocoa/XPCService/GPUServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/Cocoa/XPCService/NetworkServiceEntryPoint.mm
    ${XPCService_SOURCES}
)

list(APPEND NetworkProcess_LIBRARIES
    SecItemShim
)

# FIXME: These should not have Development in production builds.
set(WebProcess_OUTPUT_NAME com.apple.WebKit.WebContent.Development)
set(NetworkProcess_OUTPUT_NAME com.apple.WebKit.Networking.Development)

set(WebProcess_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})
set(NetworkProcess_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})
set(PluginProcess_INCLUDE_DIRECTORIES ${CMAKE_BINARY_DIR})

add_definitions("-include WebKit2Prefix.h")

set(WebKit_FORWARDING_HEADERS_FILES
    Platform/cocoa/WKCrashReporter.h

    Shared/API/c/WKDiagnosticLoggingResultType.h

    UIProcess/API/C/WKPageDiagnosticLoggingClient.h
    UIProcess/API/C/WKPageNavigationClient.h
    UIProcess/API/C/WKPageRenderingProgressEvents.h
)

list(APPEND WebKit_MESSAGES_IN_FILES
    GPUProcess/media/ios/RemoteMediaSessionHelperProxy.messages.in

    NetworkProcess/CustomProtocols/LegacyCustomProtocolManager.messages.in

    Shared/ApplePay/WebPaymentCoordinatorProxy.messages.in

    Shared/API/Cocoa/RemoteObjectRegistry.messages.in

    UIProcess/ViewGestureController.messages.in

    UIProcess/Cocoa/PlaybackSessionManagerProxy.messages.in
    UIProcess/Cocoa/UserMediaCaptureManagerProxy.messages.in
    UIProcess/Cocoa/VideoFullscreenManagerProxy.messages.in

    UIProcess/Network/CustomProtocols/LegacyCustomProtocolManagerProxy.messages.in

    UIProcess/RemoteLayerTree/RemoteLayerTreeDrawingAreaProxy.messages.in

    UIProcess/WebAuthentication/WebAuthenticatorCoordinatorProxy.messages.in

    UIProcess/ios/EditableImageController.messages.in

    UIProcess/mac/SecItemShimProxy.messages.in

    WebProcess/ApplePay/WebPaymentCoordinator.messages.in

    WebProcess/cocoa/PlaybackSessionManager.messages.in
    WebProcess/cocoa/RemoteCaptureSampleManager.messages.in
    WebProcess/cocoa/UserMediaCaptureManager.messages.in
    WebProcess/cocoa/VideoFullscreenManager.messages.in

    WebProcess/GPU/media/ios/RemoteMediaSessionHelper.messages.in

    WebProcess/WebPage/ViewGestureGeometryCollector.messages.in
    WebProcess/WebPage/ViewUpdateDispatcher.messages.in

    WebProcess/WebPage/Cocoa/TextCheckingControllerProxy.messages.in

    WebProcess/WebPage/RemoteLayerTree/RemoteScrollingCoordinator.messages.in
)

set(WebKit_FORWARDING_HEADERS_DIRECTORIES
    Platform
    Shared

    NetworkProcess/Downloads

    Platform/IPC

    Shared/API
    Shared/Cocoa

    Shared/API/Cocoa
    Shared/API/c

    Shared/API/c/cf
    Shared/API/c/mac

    UIProcess/Cocoa

    UIProcess/API/C

    UIProcess/API/C/Cocoa
    UIProcess/API/C/mac
    UIProcess/API/cpp
    UIProcess/API/ios

    WebProcess/InjectedBundle/API/Cocoa
    WebProcess/InjectedBundle/API/c
    WebProcess/InjectedBundle/API/mac
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebKit FILES ${WebKit_FORWARDING_HEADERS_FILES} DIRECTORIES ${WebKit_FORWARDING_HEADERS_DIRECTORIES})

# This is needed right now to import ObjC headers instead of including them.
# FIXME: Forwarding headers should be copies of actual headers.
file(GLOB ObjCHeaders UIProcess/API/Cocoa/*.h)
foreach (_file ${ObjCHeaders})
    get_filename_component(_name ${_file} NAME)
    if (NOT EXISTS ${FORWARDING_HEADERS_DIR}/WebKit/${_name})
        file(WRITE ${FORWARDING_HEADERS_DIR}/WebKit/${_name} "#import <WebKit/UIProcess/API/Cocoa/${_name}>")
    endif ()
endforeach ()
if (NOT EXISTS ${FORWARDING_HEADERS_DIR}/WebKit/WKWebViewPrivateForTestingIOS.h)
    file(WRITE ${FORWARDING_HEADERS_DIR}/WebKit/WKWebViewPrivateForTestingIOS.h "#import <WebKit/UIProcess/API/ios/WKWebViewPrivateForTestingIOS.h>")
endif ()
if (NOT EXISTS ${FORWARDING_HEADERS_DIR}/WebKit/WKWebViewPrivateForTestingMac.h)
    file(WRITE ${FORWARDING_HEADERS_DIR}/WebKit/WKWebViewPrivateForTestingMac.h "#import <WebKit/UIProcess/API/mac/WKWebViewPrivateForTestingMac.h>")
endif ()

# FIXME: Forwarding headers should be complete copies of the header.
set(WebKitLegacyForwardingHeaders
    DOM.h
    DOMCore.h
    DOMElement.h
    DOMException.h
    DOMObject.h
    DOMPrivate.h
    WebApplicationCache.h
    WebCache.h
    WebCoreStatistics.h
    WebDOMOperations.h
    WebDOMOperationsPrivate.h
    WebDatabaseManagerPrivate.h
    WebDataSource.h
    WebDataSourcePrivate.h
    WebDefaultPolicyDelegate.h
    WebDeviceOrientation.h
    WebDeviceOrientationProviderMock.h
    WebDocument.h
    WebDocumentPrivate.h
    WebDynamicScrollBarsView.h
    WebEditingDelegate.h
    WebFrame.h
    WebFramePrivate.h
    WebFrameViewPrivate.h
    WebGeolocationPosition.h
    WebHTMLRepresentation.h
    WebHTMLView.h
    WebHTMLViewPrivate.h
    WebHistory.h
    WebHistoryItem.h
    WebHistoryItemPrivate.h
    WebHistoryPrivate.h
    WebIconDatabasePrivate.h
    WebInspector.h
    WebInspectorPrivate.h
    WebKitNSStringExtras.h
    WebNSURLExtras.h
    WebNavigationData.h
    WebNotification.h
    WebPluginDatabase.h
    WebPolicyDelegate.h
    WebPolicyDelegatePrivate.h
    WebPreferenceKeysPrivate.h
    WebPreferences.h
    WebPreferencesPrivate.h
    WebQuotaManager.h
    WebScriptWorld.h
    WebSecurityOriginPrivate.h
    WebStorageManagerPrivate.h
    WebTypesInternal.h
    WebUIDelegate.h
    WebUIDelegatePrivate.h
    WebView.h
    WebViewPrivate
    WebViewPrivate.h
)
foreach (_file ${WebKitLegacyForwardingHeaders})
    file(WRITE ${FORWARDING_HEADERS_DIR}/WebKit/${_file} "#import <WebKitLegacy/${_file}>")
endforeach ()

set(ObjCForwardingHeaders
    DOMAbstractView.h
    DOMAttr.h
    DOMBeforeLoadEvent.h
    DOMBlob.h
    DOMCDATASection.h
    DOMCSSCharsetRule.h
    DOMCSSFontFaceRule.h
    DOMCSSImportRule.h
    DOMCSSKeyframeRule.h
    DOMCSSKeyframesRule.h
    DOMCSSMediaRule.h
    DOMCSSPageRule.h
    DOMCSSPrimitiveValue.h
    DOMCSSRule.h
    DOMCSSRuleList.h
    DOMCSSStyleDeclaration.h
    DOMCSSStyleRule.h
    DOMCSSStyleSheet.h
    DOMCSSSupportsRule.h
    DOMCSSUnknownRule.h
    DOMCSSValue.h
    DOMCSSValueList.h
    DOMCharacterData.h
    DOMComment.h
    DOMCounter.h
    DOMDOMImplementation.h
    DOMDOMNamedFlowCollection.h
    DOMDOMTokenList.h
    DOMDocument.h
    DOMDocumentFragment.h
    DOMDocumentType.h
    DOMElement.h
    DOMEntity.h
    DOMEntityReference.h
    DOMEvent.h
    DOMEventException.h
    DOMEventListener.h
    DOMEventTarget.h
    DOMFile.h
    DOMFileList.h
    DOMHTMLAnchorElement.h
    DOMHTMLAppletElement.h
    DOMHTMLAreaElement.h
    DOMHTMLBRElement.h
    DOMHTMLBaseElement.h
    DOMHTMLBaseFontElement.h
    DOMHTMLBodyElement.h
    DOMHTMLButtonElement.h
    DOMHTMLCanvasElement.h
    DOMHTMLCollection.h
    DOMHTMLDListElement.h
    DOMHTMLDirectoryElement.h
    DOMHTMLDivElement.h
    DOMHTMLDocument.h
    DOMHTMLElement.h
    DOMHTMLEmbedElement.h
    DOMHTMLFieldSetElement.h
    DOMHTMLFontElement.h
    DOMHTMLFormElement.h
    DOMHTMLFrameElement.h
    DOMHTMLFrameSetElement.h
    DOMHTMLHRElement.h
    DOMHTMLHeadElement.h
    DOMHTMLHeadingElement.h
    DOMHTMLHtmlElement.h
    DOMHTMLIFrameElement.h
    DOMHTMLImageElement.h
    DOMHTMLInputElement.h
    DOMHTMLInputElementPrivate.h
    DOMHTMLLIElement.h
    DOMHTMLLabelElement.h
    DOMHTMLLegendElement.h
    DOMHTMLLinkElement.h
    DOMHTMLMapElement.h
    DOMHTMLMarqueeElement.h
    DOMHTMLMediaElement.h
    DOMHTMLMenuElement.h
    DOMHTMLMetaElement.h
    DOMHTMLModElement.h
    DOMHTMLOListElement.h
    DOMHTMLObjectElement.h
    DOMHTMLOptGroupElement.h
    DOMHTMLOptionElement.h
    DOMHTMLOptionsCollection.h
    DOMHTMLParagraphElement.h
    DOMHTMLParamElement.h
    DOMHTMLPreElement.h
    DOMHTMLQuoteElement.h
    DOMHTMLScriptElement.h
    DOMHTMLSelectElement.h
    DOMHTMLStyleElement.h
    DOMHTMLTableCaptionElement.h
    DOMHTMLTableCellElement.h
    DOMHTMLTableColElement.h
    DOMHTMLTableElement.h
    DOMHTMLTableRowElement.h
    DOMHTMLTableSectionElement.h
    DOMHTMLTextAreaElement.h
    DOMHTMLTitleElement.h
    DOMHTMLUListElement.h
    DOMHTMLVideoElement.h
    DOMImplementation.h
    DOMKeyboardEvent.h
    DOMMediaError.h
    DOMMediaList.h
    DOMMessageEvent.h
    DOMMessagePort.h
    DOMMouseEvent.h
    DOMMutationEvent.h
    DOMNamedNodeMap.h
    DOMNode.h
    DOMNodeFilter.h
    DOMNodeIterator.h
    DOMNodeList.h
    DOMOverflowEvent.h
    DOMProcessingInstruction.h
    DOMProgressEvent.h
    DOMRGBColor.h
    DOMRange.h
    DOMRangeException.h
    DOMRect.h
    DOMStyleSheet.h
    DOMStyleSheetList.h
    DOMText.h
    DOMTextEvent.h
    DOMTimeRanges.h
    DOMTreeWalker.h
    DOMUIEvent.h
    DOMValidityState.h
    DOMWebKitCSSFilterValue.h
    DOMWebKitCSSRegionRule.h
    DOMWebKitCSSTransformValue.h
    DOMWebKitNamedFlow.h
    DOMWheelEvent.h
    DOMXPathException.h
    DOMXPathExpression.h
    DOMXPathNSResolver.h
    DOMXPathResult.h
)
foreach (_file ${ObjCForwardingHeaders})
    file(WRITE ${FORWARDING_HEADERS_DIR}/WebKit/${_file} "#import <WebKitLegacy/${_file}>")
endforeach ()

set(SecItemShimDirectory ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/Frameworks)
add_library(SecItemShim SHARED WebProcess/mac/SecItemShimLibrary.mm)
WEBKIT_CREATE_SYMLINK(SecItemShim ${SecItemShimDirectory} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Frameworks)
set_target_properties(SecItemShim PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${SecItemShimDirectory})
set_target_properties(SecItemShim PROPERTIES PREFIX "")
target_link_libraries(SecItemShim ${SECURITY_LIBRARY})
target_include_directories(SecItemShim PRIVATE
    ${CMAKE_BINARY_DIR}
    ${FORWARDING_HEADERS_DIR}
    ${WEBKIT_DIR}
)
add_dependencies(SecItemShim WebCore)

# FIXME: These should not be necessary.
file(WRITE ${FORWARDING_HEADERS_DIR}/WebKit/WKImageCG.h "#import <WebKit/Shared/API/c/cg/WKImageCG.h>")

set(CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS} "-compatibility_version 1 -current_version ${WEBKIT_MAC_VERSION}")
target_link_options(WebKit PRIVATE -weak_framework SafariSafeBrowsing -lsandbox -framework AuthKit)

set(WebKit_OUTPUT_NAME WebKit)

# XPC Services

function(WEBKIT_DEFINE_XPC_SERVICES)
    set(RUNLOOP_TYPE _WebKit)
    set(WebKit_XPC_SERVICE_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/XPCServices)
    WEBKIT_CREATE_SYMLINK(WebProcess ${WebKit_XPC_SERVICE_DIR} ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/XPCServices)

    function(WEBKIT_XPC_SERVICE _target _bundle_identifier _info_plist _executable_name)
        set(_service_dir ${WebKit_XPC_SERVICE_DIR}/${_bundle_identifier}.xpc/Contents)
        make_directory(${_service_dir}/MacOS)
        make_directory(${_service_dir}/_CodeSignature)
        make_directory(${_service_dir}/Resources)

        # FIXME: These version strings don't match Xcode's.
        set(BUNDLE_VERSION ${WEBKIT_VERSION})
        set(SHORT_VERSION_STRING ${WEBKIT_VERSION_MAJOR})
        set(BUNDLE_VERSION ${WEBKIT_VERSION})
        set(EXECUTABLE_NAME ${_executable_name})
        set(PRODUCT_BUNDLE_IDENTIFIER ${_bundle_identifier})
        set(PRODUCT_NAME ${_bundle_identifier})
        configure_file(${_info_plist} ${_service_dir}/Info.plist)

        set_target_properties(${_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_service_dir}/MacOS")
    endfunction()

    WEBKIT_XPC_SERVICE(WebProcess
        "com.apple.WebKit.WebContent"
        ${WEBKIT_DIR}/WebProcess/EntryPoint/Cocoa/XPCService/WebContentService/Info-OSX.plist
        ${WebProcess_OUTPUT_NAME})

    WEBKIT_XPC_SERVICE(NetworkProcess
        "com.apple.WebKit.Networking"
        ${WEBKIT_DIR}/NetworkProcess/EntryPoint/Cocoa/XPCService/NetworkService/Info-OSX.plist
        ${NetworkProcess_OUTPUT_NAME})

    set(WebKit_RESOURCES_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/WebKit.framework/Versions/A/Resources)
    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb COMMAND
        grep -o "^[^;]*" ${WEBKIT_DIR}/WebProcess/com.apple.WebProcess.sb.in | clang -E -P -w -include wtf/Platform.h -I ${FORWARDING_HEADERS_DIR} - > ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb
        VERBATIM)
    add_custom_command(OUTPUT ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb COMMAND
        grep -o "^[^;]*" ${WEBKIT_DIR}/NetworkProcess/mac/com.apple.WebKit.NetworkProcess.sb.in | clang -E -P -w -include wtf/Platform.h -I ${FORWARDING_HEADERS_DIR} - > ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb
        VERBATIM)
    add_custom_target(WebKitSandboxProfiles ALL DEPENDS ${WebKit_RESOURCES_DIR}/com.apple.WebProcess.sb ${WebKit_RESOURCES_DIR}/com.apple.WebKit.NetworkProcess.sb)
    add_dependencies(WebKit WebKitSandboxProfiles)

    add_custom_command(OUTPUT ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib COMMAND
        ibtool --compile ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib ${WEBKIT_DIR}/Resources/WebContentProcess.xib
        VERBATIM)
    add_custom_target(WebContentProcessNib ALL DEPENDS ${WebKit_XPC_SERVICE_DIR}/com.apple.WebKit.WebContent.xpc/Contents/Resources/WebContentProcess.nib)
    add_dependencies(WebKit WebContentProcessNib)
endfunction()
