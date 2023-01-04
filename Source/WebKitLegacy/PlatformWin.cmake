if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DUSE_CAIRO=1 -DUSE_CURL=1 -DWEBKIT_EXPORTS=1)
    list(APPEND WebKitLegacy_SOURCES_Classes
        win/WebDownloadCURL.cpp
        win/WebURLAuthenticationChallengeSenderCURL.cpp
    )
else ()
    list(APPEND WebKitLegacy_SOURCES_Classes
        win/WebDownloadCFNet.cpp
        win/WebURLAuthenticationChallengeSenderCFNet.cpp
    )
    list(APPEND WebKitLegacy_PRIVATE_LIBRARIES
        Apple::CFNetwork
        Apple::CoreGraphics
        Apple::CoreText
        Apple::QuartzCore
        Apple::libdispatch
        LibXml2::LibXml2
        LibXslt::LibXslt
        SQLite::SQLite3
        ZLIB::ZLIB
    )
endif ()

add_custom_command(
    OUTPUT ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebKitVersion.h
    MAIN_DEPENDENCY ${WEBKITLEGACY_DIR}/scripts/generate-webkitversion.pl
    DEPENDS ${WEBKITLEGACY_DIR}/../../Configurations/Version.xcconfig
    COMMAND ${PERL_EXECUTABLE} ${WEBKITLEGACY_DIR}/scripts/generate-webkitversion.pl --config ${WEBKITLEGACY_DIR}/../../Configurations/Version.xcconfig --outputDir ${WebKitLegacy_DERIVED_SOURCES_DIR}
    VERBATIM)
list(APPEND WebKitLegacy_SOURCES ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebKitVersion.h)

list(APPEND WebKitLegacy_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}/../include/private"
    "${CMAKE_BINARY_DIR}/../include/private/JavaScriptCore"
    "${CMAKE_BINARY_DIR}/../include/private/WebCore"
    "${WEBKITLEGACY_DIR}/win"
    "${WEBKITLEGACY_DIR}/win/WebCoreSupport"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/include"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces"
)

list(APPEND WebKitLegacy_INCLUDES
    win/CFDictionaryPropertyBag.h
    win/COMEnumVariant.h
    win/COMPropertyBag.h
    win/COMVariantSetter.h
    win/CodeAnalysisConfig.h
    win/DOMCSSClasses.h
    win/DOMCoreClasses.h
    win/DOMEventsClasses.h
    win/DOMHTMLClasses.h
    win/DefaultDownloadDelegate.h
    win/DefaultPolicyDelegate.h
    win/ForEachCoClass.h
    win/FullscreenVideoController.h
    win/MarshallingHelpers.h
    win/MemoryStream.h
    win/ProgIDMacros.h
    win/WebActionPropertyBag.h
    win/WebApplicationCache.h
    win/WebArchive.h
    win/WebBackForwardList.h
    win/WebCache.h
    win/WebCachedFramePlatformData.h
    win/WebCoreStatistics.h
    win/WebDataSource.h
    win/WebDatabaseManager.h
    win/WebDocumentLoader.h
    win/WebDownload.h
    win/WebDropSource.h
    win/WebElementPropertyBag.h
    win/WebError.h
    win/WebFrame.h
    win/WebFramePolicyListener.h
    win/WebGeolocationPolicyListener.h
    win/WebGeolocationPosition.h
    win/WebHTMLRepresentation.h
    win/WebHistory.h
    win/WebHistoryItem.h
    win/WebJavaScriptCollector.h
    win/WebKitCOMAPI.h
    win/WebKitClassFactory.h
    win/WebKitDLL.h
    win/WebKitGraphics.h
    win/WebKitLogging.h
    win/WebKitStatistics.h
    win/WebKitStatisticsPrivate.h
    win/WebKitSystemBits.h
    win/WebLocalizableStrings.h
    win/WebMutableURLRequest.h
    win/WebNavigationData.h
    win/WebNotification.h
    win/WebNotificationCenter.h
    win/WebPreferenceKeysPrivate.h
    win/WebPreferences.h
    win/WebResource.h
    win/WebScriptObject.h
    win/WebScriptWorld.h
    win/WebSecurityOrigin.h
    win/WebSerializedJSValue.h
    win/WebTextRenderer.h
    win/WebURLAuthenticationChallenge.h
    win/WebURLAuthenticationChallengeSender.h
    win/WebURLCredential.h
    win/WebURLProtectionSpace.h
    win/WebURLResponse.h
    win/WebUserContentURLPattern.h
    win/WebView.h
    win/WebWorkersPrivate.h
)

list(APPEND WebKitLegacy_SOURCES_Classes
    win/AccessibleBase.cpp
    win/AccessibleDocument.cpp
    win/AccessibleImage.cpp
    win/AccessibleTextImpl.cpp
    win/BackForwardList.cpp
    win/CFDictionaryPropertyBag.cpp
    win/DOMCSSClasses.cpp
    win/DOMCoreClasses.cpp
    win/DOMEventsClasses.cpp
    win/DOMHTMLClasses.cpp
    win/DefaultDownloadDelegate.cpp
    win/DefaultPolicyDelegate.cpp
    win/ForEachCoClass.cpp
    win/FullscreenVideoController.cpp
    win/MarshallingHelpers.cpp
    win/MemoryStream.cpp
    win/WebActionPropertyBag.cpp
    win/WebApplicationCache.cpp
    win/WebArchive.cpp
    win/WebBackForwardList.cpp
    win/WebCache.cpp
    win/WebCoreStatistics.cpp
    win/WebDataSource.cpp
    win/WebDatabaseManager.cpp
    win/WebDocumentLoader.cpp
    win/WebDownload.cpp
    win/WebDropSource.cpp
    win/WebElementPropertyBag.cpp
    win/WebError.cpp
    win/WebFrame.cpp
    win/WebFramePolicyListener.cpp
    win/WebGeolocationPolicyListener.cpp
    win/WebGeolocationPosition.cpp
    win/WebHTMLRepresentation.cpp
    win/WebHistory.cpp
    win/WebHistoryItem.cpp
    win/WebInspector.cpp
    win/WebJavaScriptCollector.cpp
    win/WebKitCOMAPI.cpp
    win/WebKitClassFactory.cpp
    win/WebKitDLL.cpp
    win/WebKitLogging.cpp
    win/WebKitMessageLoop.cpp
    win/WebKitStatistics.cpp
    win/WebKitSystemBits.cpp
    win/WebLocalizableStrings.cpp
    win/WebMutableURLRequest.cpp
    win/WebNavigationData.cpp
    win/WebNodeHighlight.cpp
    win/WebNotification.cpp
    win/WebNotificationCenter.cpp
    win/WebPreferences.cpp
    win/WebResource.cpp
    win/WebScriptObject.cpp
    win/WebScriptWorld.cpp
    win/WebSecurityOrigin.cpp
    win/WebSerializedJSValue.cpp
    win/WebTextRenderer.cpp
    win/WebURLAuthenticationChallenge.cpp
    win/WebURLAuthenticationChallengeSender.cpp
    win/WebURLCredential.cpp
    win/WebURLProtectionSpace.cpp
    win/WebURLResponse.cpp
    win/WebUserContentURLPattern.cpp
    win/WebView.cpp
    win/WebWorkersPrivate.cpp

    win/storage/WebDatabaseProvider.cpp
)

list(APPEND WebKitLegacy_SOURCES_WebCoreSupport
    win/WebCoreSupport/AcceleratedCompositingContext.cpp
    win/WebCoreSupport/WebChromeClient.cpp
    win/WebCoreSupport/WebChromeClient.h
    win/WebCoreSupport/WebContextMenuClient.cpp
    win/WebCoreSupport/WebContextMenuClient.h
    win/WebCoreSupport/WebDesktopNotificationsDelegate.cpp
    win/WebCoreSupport/WebDesktopNotificationsDelegate.h
    win/WebCoreSupport/WebDragClient.cpp
    win/WebCoreSupport/WebDragClient.h
    win/WebCoreSupport/WebEditorClient.cpp
    win/WebCoreSupport/WebEditorClient.h
    win/WebCoreSupport/WebFrameLoaderClient.cpp
    win/WebCoreSupport/WebFrameLoaderClient.h
    win/WebCoreSupport/WebFrameNetworkingContext.cpp
    win/WebCoreSupport/WebFrameNetworkingContext.h
    win/WebCoreSupport/WebGeolocationClient.cpp
    win/WebCoreSupport/WebGeolocationClient.h
    win/WebCoreSupport/WebInspectorClient.cpp
    win/WebCoreSupport/WebInspectorClient.h
    win/WebCoreSupport/WebInspectorDelegate.cpp
    win/WebCoreSupport/WebInspectorDelegate.h
    win/WebCoreSupport/WebPlatformStrategies.cpp
    win/WebCoreSupport/WebPlatformStrategies.h
    win/WebCoreSupport/WebPluginInfoProvider.cpp
    win/WebCoreSupport/WebPluginInfoProvider.h
    win/WebCoreSupport/WebProgressTrackerClient.cpp
    win/WebCoreSupport/WebProgressTrackerClient.h
    win/WebCoreSupport/WebVisitedLinkStore.cpp
    win/WebCoreSupport/WebVisitedLinkStore.h
)

if (USE_CF)
    list(APPEND WebKitLegacy_SOURCES_Classes
        cf/WebCoreSupport/WebInspectorClientCF.cpp
    )

    list(APPEND WebKitLegacy_LIBRARIES
        Apple::CoreFoundation
    )
endif ()


set(WebKitLegacy_WEB_PREFERENCES_TEMPLATES
    ${WEBKITLEGACY_DIR}/win/Scripts/PreferencesTemplates/WebPreferencesDefinitions.h.erb
    ${WEBKITLEGACY_DIR}/win/Scripts/PreferencesTemplates/WebViewPreferencesChangedGenerated.cpp.erb
)

set(WebKitLegacy_WEB_PREFERENCES
    ${WTF_SCRIPTS_DIR}/Preferences/UnifiedWebPreferences.yaml
)

set_source_files_properties(${WebKitLegacy_WEB_PREFERENCES} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesDefinitions.h ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebViewPreferencesChangedGenerated.cpp
    DEPENDS ${WebKitLegacy_WEB_PREFERENCES_TEMPLATES} ${WebKitLegacy_WEB_PREFERENCES} WTF_CopyPreferences
    COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/GeneratePreferences.rb --frontend WebKitLegacy --outputDir "${WebKitLegacy_DERIVED_SOURCES_DIR}" --template "$<JOIN:${WebKitLegacy_WEB_PREFERENCES_TEMPLATES},;--template;>" ${WebKitLegacy_WEB_PREFERENCES}
    COMMAND_EXPAND_LISTS
    VERBATIM
)

list(APPEND WebKitLegacy_SOURCES
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebPreferencesDefinitions.h
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/WebViewPreferencesChangedGenerated.cpp
)

list(APPEND WebKitLegacy_SOURCES ${WebKitLegacy_INCLUDES} ${WebKitLegacy_SOURCES_Classes} ${WebKitLegacy_SOURCES_WebCoreSupport})

source_group(Includes FILES ${WebKitLegacy_INCLUDES})
source_group(Classes FILES ${WebKitLegacy_SOURCES_Classes})
source_group(WebCoreSupport FILES ${WebKitLegacy_SOURCES_WebCoreSupport})

set(MIDL_DEFINES /D\ \"__PRODUCTION__=01\")

set(WEBKITLEGACY_IDL_DEPENDENCIES
    win/Interfaces/AccessibleComparable.idl
    win/Interfaces/DOMCSS.idl
    win/Interfaces/DOMCore.idl
    win/Interfaces/DOMEvents.idl
    win/Interfaces/DOMExtensions.idl
    win/Interfaces/DOMHTML.idl
    win/Interfaces/DOMPrivate.idl
    win/Interfaces/DOMRange.idl
    win/Interfaces/DOMWindow.idl
    win/Interfaces/IGEN_DOMObject.idl
    win/Interfaces/IWebArchive.idl
    win/Interfaces/IWebBackForwardList.idl
    win/Interfaces/IWebBackForwardListPrivate.idl
    win/Interfaces/IWebCache.idl
    win/Interfaces/IWebCoreStatistics.idl
    win/Interfaces/IWebDataSource.idl
    win/Interfaces/IWebDatabaseManager.idl
    win/Interfaces/IWebDesktopNotificationsDelegate.idl
    win/Interfaces/IWebDocument.idl
    win/Interfaces/IWebDownload.idl
    win/Interfaces/IWebEditingDelegate.idl
    win/Interfaces/IWebEmbeddedView.idl
    win/Interfaces/IWebError.idl
    win/Interfaces/IWebErrorPrivate.idl
    win/Interfaces/IWebFormDelegate.idl
    win/Interfaces/IWebFrame.idl
    win/Interfaces/IWebFrameLoadDelegate.idl
    win/Interfaces/IWebFrameLoadDelegatePrivate.idl
    win/Interfaces/IWebFrameLoadDelegatePrivate2.idl
    win/Interfaces/IWebFramePrivate.idl
    win/Interfaces/IWebFrameView.idl
    win/Interfaces/IWebGeolocationPolicyListener.idl
    win/Interfaces/IWebGeolocationPosition.idl
    win/Interfaces/IWebGeolocationProvider.idl
    win/Interfaces/IWebHTMLRepresentation.idl
    win/Interfaces/IWebHTTPURLResponse.idl
    win/Interfaces/IWebHistory.idl
    win/Interfaces/IWebHistoryDelegate.idl
    win/Interfaces/IWebHistoryItem.idl
    win/Interfaces/IWebHistoryItemPrivate.idl
    win/Interfaces/IWebHistoryPrivate.idl
    win/Interfaces/IWebInspector.idl
    win/Interfaces/IWebInspectorPrivate.idl
    win/Interfaces/IWebJavaScriptCollector.idl
    win/Interfaces/IWebKitStatistics.idl
    win/Interfaces/IWebMutableURLRequest.idl
    win/Interfaces/IWebMutableURLRequestPrivate.idl
    win/Interfaces/IWebNavigationData.idl
    win/Interfaces/IWebNotification.idl
    win/Interfaces/IWebNotificationCenter.idl
    win/Interfaces/IWebNotificationObserver.idl
    win/Interfaces/IWebPolicyDelegate.idl
    win/Interfaces/IWebPolicyDelegatePrivate.idl
    win/Interfaces/IWebPreferences.idl
    win/Interfaces/IWebPreferencesPrivate.idl
    win/Interfaces/IWebResource.idl
    win/Interfaces/IWebResourceLoadDelegate.idl
    win/Interfaces/IWebResourceLoadDelegatePrivate.idl
    win/Interfaces/IWebResourceLoadDelegatePrivate2.idl
    win/Interfaces/IWebScriptObject.idl
    win/Interfaces/IWebScriptWorld.idl
    win/Interfaces/IWebSecurityOrigin.idl
    win/Interfaces/IWebSerializedJSValue.idl
    win/Interfaces/IWebSerializedJSValuePrivate.idl
    win/Interfaces/IWebTextRenderer.idl
    win/Interfaces/IWebUIDelegate.idl
    win/Interfaces/IWebUIDelegate2.idl
    win/Interfaces/IWebUIDelegatePrivate.idl
    win/Interfaces/IWebURLAuthenticationChallenge.idl
    win/Interfaces/IWebURLRequest.idl
    win/Interfaces/IWebURLResponse.idl
    win/Interfaces/IWebURLResponsePrivate.idl
    win/Interfaces/IWebUndoManager.idl
    win/Interfaces/IWebUndoTarget.idl
    win/Interfaces/IWebUserContentURLPattern.idl
    win/Interfaces/IWebView.idl
    win/Interfaces/IWebViewPrivate.idl
    win/Interfaces/IWebWorkersPrivate.idl
    win/Interfaces/JavaScriptCoreAPITypes.idl
    win/Interfaces/WebKit.idl
    win/Interfaces/WebScrollbarTypes.idl

    win/Interfaces/Accessible2/Accessible2.idl
    win/Interfaces/Accessible2/Accessible2_2.idl
    win/Interfaces/Accessible2/AccessibleApplication.idl
    win/Interfaces/Accessible2/AccessibleEditableText.idl
    win/Interfaces/Accessible2/AccessibleRelation.idl
    win/Interfaces/Accessible2/AccessibleStates.idl
    win/Interfaces/Accessible2/AccessibleText.idl
    win/Interfaces/Accessible2/AccessibleText2.idl
    win/Interfaces/Accessible2/IA2CommonTypes.idl
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/include/autoversion.h"
)

# Build the COM interface:
function(GENERATE_INTERFACE _infile)
    cmake_parse_arguments(opt "HEADER_ONLY" "" "" ${ARGN})
    get_filename_component(_filewe ${_infile} NAME_WE)
    set(output ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/${_filewe}.h)
    if (NOT ${opt_HEADER_ONLY})
        list(APPEND output ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/${_filewe}_i.c)
    endif ()
    add_custom_command(
        OUTPUT ${output}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${WEBKITLEGACY_IDL_DEPENDENCIES}
        COMMAND midl.exe /I "${CMAKE_CURRENT_SOURCE_DIR}/win/Interfaces" /I "${CMAKE_CURRENT_SOURCE_DIR}/win/Interfaces/Accessible2" /I "${WebKitLegacy_DERIVED_SOURCES_DIR}/include" /I "${CMAKE_CURRENT_SOURCE_DIR}/win" /WX /char signed /env win32 /tlb "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_filewe}.tlb" /out "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces" /h "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/${_filewe}.h" /iid "${_filewe}_i.c" ${MIDL_DEFINES} "${CMAKE_CURRENT_SOURCE_DIR}/${_infile}"
        USES_TERMINAL VERBATIM)
endfunction()

if (WTF_PLATFORM_WIN_CAIRO)
    file(WRITE ${WebKitLegacy_DERIVED_SOURCES_DIR}/include/autoversion.h
        "#define __BUILD_NUMBER_MAJOR__ 0\n"
        "#define __BUILD_NUMBER_MINOR__ 0\n"
        "#define __BUILD_NUMBER_SHORT__ \"\"\n")
else ()
    add_custom_command(
        OUTPUT ${WebKitLegacy_DERIVED_SOURCES_DIR}/include/autoversion.h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMAND ${PERL_EXECUTABLE} ${WEBKIT_LIBRARIES_DIR}/tools/scripts/auto-version.pl ${WebKitLegacy_DERIVED_SOURCES_DIR}
        VERBATIM)
endif ()

GENERATE_INTERFACE(win/Interfaces/WebKit.idl)
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleApplication.idl)
GENERATE_INTERFACE(win/Interfaces/Accessible2/Accessible2.idl)
GENERATE_INTERFACE(win/Interfaces/Accessible2/Accessible2_2.idl)
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleRelation.idl)
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleStates.idl HEADER_ONLY)
GENERATE_INTERFACE(win/Interfaces/Accessible2/IA2CommonTypes.idl HEADER_ONLY)
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleEditableText.idl)
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleText.idl)
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleText2.idl)

add_library(WebKitLegacyGUID STATIC
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/WebKit.h"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleApplication.h"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/Accessible2.h"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/Accessible2_2.h"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleRelation.h"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleStates.h"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/IA2CommonTypes.h"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleEditableText.h"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleText.h"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleText2.h"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/WebKit_i.c"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleApplication_i.c"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/Accessible2_i.c"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/Accessible2_2_i.c"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleRelation_i.c"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleEditableText_i.c"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleText_i.c"
    "${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleText2_i.c"
)
set_target_properties(WebKitLegacyGUID PROPERTIES OUTPUT_NAME WebKitGUID${DEBUG_SUFFIX})

list(APPEND WebKitLegacy_LIBRARIES WebKitLegacyGUID)
list(APPEND WebKitLegacy_PRIVATE_LIBRARIES
    Comctl32
    Comsupp
    Crypt32
    D2d1
    Dwrite
    Iphlpapi
    Psapi
    Rpcrt4
    Shlwapi
    Usp10
    Version
    WebKitLegacyGUID
    WindowsCodecs
    Winmm
    dxguid
)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SUBSYSTEM:WINDOWS /FORCE:MULTIPLE")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

# We need the webkit libraries to come before the system default libraries to prevent symbol conflicts with uuid.lib.
# To do this we add system default libs as webkit libs and zero out system default libs.
string(REPLACE " " "\;" CXX_LIBS ${CMAKE_CXX_STANDARD_LIBRARIES})
list(APPEND WebKitLegacy_PRIVATE_LIBRARIES ${CXX_LIBS})
set(CMAKE_CXX_STANDARD_LIBRARIES "")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${MSVC_RUNTIME_LINKER_FLAGS}")

# If this directory isn't created before midl runs and attempts to output WebKit.tlb,
# It fails with an unusual error - midl failed - failed to save all changes
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
file(MAKE_DIRECTORY ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces)

set(WebKitLegacy_PUBLIC_FRAMEWORK_HEADERS
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/Accessible2.h
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/Accessible2_2.h
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleApplication.h
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleEditableText.h
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleRelation.h
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleStates.h
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleText.h
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/AccessibleText2.h
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/IA2CommonTypes.h
    ${WebKitLegacy_DERIVED_SOURCES_DIR}/Interfaces/WebKit.h

    win/AccessibleBase.h
    win/AccessibleDocument.h
    win/CFDictionaryPropertyBag.h
    win/WebDataSource.h
    win/WebFrame.h
    win/WebKitCOMAPI.h
)

set(WebKitLegacy_OUTPUT_NAME
    WebKit${DEBUG_SUFFIX}
)
