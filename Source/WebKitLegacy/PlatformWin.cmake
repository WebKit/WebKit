if (${WTF_PLATFORM_WIN_CAIRO})
    add_definitions(-DUSE_CAIRO=1 -DUSE_CURL=1 -DWEBKIT_EXPORTS=1)
    list(APPEND WebKitLegacy_INCLUDE_DIRECTORIES
        ${CAIRO_INCLUDE_DIRS}
        "${WEBKIT_LIBRARIES_DIR}/include"
        "${WEBCORE_DIR}/platform/graphics/cairo"
    )
    list(APPEND WebKitLegacy_SOURCES_Classes
        win/WebDownloadCURL.cpp
        win/WebURLAuthenticationChallengeSenderCURL.cpp
    )
    list(APPEND WebKitLegacy_LIBRARIES
        ${OPENSSL_LIBRARIES}
        PRIVATE mfuuid.lib
        PRIVATE strmiids.lib
    )
else ()
    list(APPEND WebKitLegacy_SOURCES_Classes
        win/WebDownloadCFNet.cpp
        win/WebURLAuthenticationChallengeSenderCFNet.cpp
    )
    list(APPEND WebKitLegacy_LIBRARIES
        PRIVATE CFNetwork${DEBUG_SUFFIX}
        PRIVATE CoreGraphics${DEBUG_SUFFIX}
        PRIVATE CoreText${DEBUG_SUFFIX}
        PRIVATE QuartzCore${DEBUG_SUFFIX}
        PRIVATE libdispatch${DEBUG_SUFFIX}
        PRIVATE libicuin${DEBUG_SUFFIX}
        PRIVATE libicuuc${DEBUG_SUFFIX}
        PRIVATE ${LIBXML2_LIBRARIES}
        PRIVATE ${LIBXSLT_LIBRARIES}
        PRIVATE ${SQLITE_LIBRARIES}
        PRIVATE ${ZLIB_LIBRARIES}
    )
endif ()

list(APPEND WebKitLegacy_LIBRARIES PRIVATE WTF${DEBUG_SUFFIX})

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitVersion.h
    MAIN_DEPENDENCY ${WEBKITLEGACY_DIR}/scripts/generate-webkitversion.pl
    DEPENDS ${WEBKITLEGACY_DIR}/mac/Configurations/Version.xcconfig
    COMMAND ${PERL_EXECUTABLE} ${WEBKITLEGACY_DIR}/scripts/generate-webkitversion.pl --config ${WEBKITLEGACY_DIR}/mac/Configurations/Version.xcconfig --outputDir ${DERIVED_SOURCES_WEBKITLEGACY_DIR}
    VERBATIM)
list(APPEND WebKitLegacy_SOURCES ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/WebKitVersion.h)

list(APPEND WebKitLegacy_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}/../include/private"
    "${CMAKE_BINARY_DIR}/../include/private/JavaScriptCore"
    "${CMAKE_BINARY_DIR}/../include/private/WebCore"
    "${WEBKITLEGACY_DIR}/win"
    "${WEBKITLEGACY_DIR}/win/plugins"
    "${WEBKITLEGACY_DIR}/win/WebCoreSupport"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/include"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces"
    "${FORWARDING_HEADERS_DIR}/ANGLE"
    "${FORWARDING_HEADERS_DIR}/ANGLE/include"
    "${FORWARDING_HEADERS_DIR}/ANGLE/include/egl"
    "${FORWARDING_HEADERS_DIR}/ANGLE/include/khr"
    "${DERIVED_SOURCES_DIR}/WebKitLegacy"
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

    win/plugins/PluginDatabase.cpp
    win/plugins/PluginDatabaseWin.cpp
    win/plugins/PluginDebug.cpp
    win/plugins/PluginMainThreadScheduler.cpp
    win/plugins/PluginMessageThrottlerWin.cpp
    win/plugins/PluginPackage.cpp
    win/plugins/PluginPackageWin.cpp
    win/plugins/PluginStream.cpp
    win/plugins/PluginView.cpp
    win/plugins/PluginViewWin.cpp
    win/plugins/npapi.cpp

    win/storage/WebDatabaseProvider.cpp
)

list(APPEND WebKitLegacy_SOURCES_WebCoreSupport
    win/WebCoreSupport/AcceleratedCompositingContext.cpp
    win/WebCoreSupport/EmbeddedWidget.cpp
    win/WebCoreSupport/EmbeddedWidget.h
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
    win/WebCoreSupport/WebVisitedLinkStore.cpp
    win/WebCoreSupport/WebVisitedLinkStore.h
)

if (USE_CF)
    list(APPEND WebKitLegacy_SOURCES_Classes
        cf/WebCoreSupport/WebInspectorClientCF.cpp
    )

    list(APPEND WebKitLegacy_LIBRARIES
        ${COREFOUNDATION_LIBRARY}
    )
endif ()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    enable_language(ASM_MASM)
    if (MSVC)
        set(MASM_EXECUTABLE ml64)
        set(MASM_FLAGS /c /Fo)
        add_custom_command(
            OUTPUT ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/PaintHooks.obj
            MAIN_DEPENDENCY win/plugins/PaintHooks.asm
            COMMAND ${MASM_EXECUTABLE} ${MASM_FLAGS}
                ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/PaintHooks.obj
                ${CMAKE_CURRENT_SOURCE_DIR}/win/plugins/PaintHooks.asm
            VERBATIM)
        list(APPEND WebKitLegacy_SOURCES
            ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/PaintHooks.obj
        )
    else ()
        list(APPEND WebKitLegacy_SOURCES
            win/plugins/PaintHooks.asm
        )
    endif ()
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKitLegacy -Wno-overloaded-virtual)
endif ()

list(APPEND WebKitLegacy_SOURCES ${WebKitLegacy_INCLUDES} ${WebKitLegacy_SOURCES_Classes} ${WebKitLegacy_SOURCES_WebCoreSupport})

source_group(Includes FILES ${WebKitLegacy_INCLUDES})
source_group(Classes FILES ${WebKitLegacy_SOURCES_Classes})
source_group(WebCoreSupport FILES ${WebKitLegacy_SOURCES_WebCoreSupport})

# Build the COM interface:
macro(GENERATE_INTERFACE _infile _defines _depends)
    get_filename_component(_filewe ${_infile} NAME_WE)
    add_custom_command(
        OUTPUT  ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/${_filewe}.h
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${_depends}
        COMMAND midl.exe /I "${CMAKE_CURRENT_SOURCE_DIR}/win/Interfaces" /I "${CMAKE_CURRENT_SOURCE_DIR}/win/Interfaces/Accessible2" /I "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/include" /I "${CMAKE_CURRENT_SOURCE_DIR}/win" /WX /char signed /env win32 /tlb "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_filewe}.tlb" /out "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces" /h "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/${_filewe}.h" /iid "${_filewe}_i.c" ${_defines} "${CMAKE_CURRENT_SOURCE_DIR}/${_infile}"
        USES_TERMINAL VERBATIM)
    set_source_files_properties(${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/${_filewe}.h PROPERTIES GENERATED TRUE)
    set_source_files_properties(${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/${_filewe}_i.c PROPERTIES GENERATED TRUE)
endmacro()

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
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/include/autoversion.h"
)

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/include/autoversion.h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_LIBRARIES_DIR}/tools/scripts/auto-version.pl ${DERIVED_SOURCES_WEBKITLEGACY_DIR}
    VERBATIM)

GENERATE_INTERFACE(win/Interfaces/WebKit.idl ${MIDL_DEFINES} "${WEBKITLEGACY_IDL_DEPENDENCIES}")
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleApplication.idl ${MIDL_DEFINES} "${WEBKITLEGACY_IDL_DEPENDENCIES}")
GENERATE_INTERFACE(win/Interfaces/Accessible2/Accessible2.idl ${MIDL_DEFINES} "${WEBKITLEGACY_IDL_DEPENDENCIES}")
GENERATE_INTERFACE(win/Interfaces/Accessible2/Accessible2_2.idl ${MIDL_DEFINES} "${WEBKITLEGACY_IDL_DEPENDENCIES}")
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleRelation.idl ${MIDL_DEFINES} "${WEBKITLEGACY_IDL_DEPENDENCIES}")
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleStates.idl ${MIDL_DEFINES} "${WEBKITLEGACY_IDL_DEPENDENCIES}")
GENERATE_INTERFACE(win/Interfaces/Accessible2/IA2CommonTypes.idl ${MIDL_DEFINES} "${WEBKITLEGACY_IDL_DEPENDENCIES}")
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleEditableText.idl ${MIDL_DEFINES} "${WEBKITLEGACY_IDL_DEPENDENCIES}")
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleText.idl ${MIDL_DEFINES} "${WEBKITLEGACY_IDL_DEPENDENCIES}")
GENERATE_INTERFACE(win/Interfaces/Accessible2/AccessibleText2.idl ${MIDL_DEFINES} "${WEBKITLEGACY_IDL_DEPENDENCIES}")

add_library(WebKitLegacyGUID STATIC
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/WebKit.h"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/AccessibleApplication.h"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/Accessible2.h"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/Accessible2_2.h"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/AccessibleRelation.h"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/AccessibleStates.h"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/IA2CommonTypes.h"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/AccessibleEditableText.h"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/AccessibleText.h"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/AccessibleText2.h"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/WebKit_i.c"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/AccessibleApplication_i.c"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/Accessible2_i.c"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/Accessible2_2_i.c"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/AccessibleRelation_i.c"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/AccessibleEditableText_i.c"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/AccessibleText_i.c"
    "${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces/AccessibleText2_i.c"
)
set_target_properties(WebKitLegacyGUID PROPERTIES OUTPUT_NAME WebKitGUID${DEBUG_SUFFIX})

list(APPEND WebKitLegacy_LIBRARIES
    PRIVATE Comctl32
    PRIVATE Comsupp
    PRIVATE Crypt32
    PRIVATE D2d1
    PRIVATE Dwrite
    PRIVATE dxguid
    PRIVATE Iphlpapi
    PRIVATE Psapi
    PRIVATE Rpcrt4
    PRIVATE Shlwapi
    PRIVATE Usp10
    PRIVATE Version
    PRIVATE Winmm
    PRIVATE WebKitGUID${DEBUG_SUFFIX}
    PRIVATE WindowsCodecs
)

if (ENABLE_GRAPHICS_CONTEXT_3D)
    list(APPEND WebKitLegacy_LIBRARIES
        libANGLE${DEBUG_SUFFIX}
        libEGL${DEBUG_SUFFIX}
        libGLESv2${DEBUG_SUFFIX}
    )
endif ()

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")

# We need the webkit libraries to come before the system default libraries to prevent symbol conflicts with uuid.lib.
# To do this we add system default libs as webkit libs and zero out system default libs.
string(REPLACE " " "\;" CXX_LIBS ${CMAKE_CXX_STANDARD_LIBRARIES})
list(APPEND WebKitLegacy_LIBRARIES ${CXX_LIBS})
set(CMAKE_CXX_STANDARD_LIBRARIES "")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${MSVC_RUNTIME_LINKER_FLAGS}")

# If this directory isn't created before midl runs and attempts to output WebKit.tlb,
# It fails with an unusual error - midl failed - failed to save all changes
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
file(MAKE_DIRECTORY ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces)

WEBKIT_MAKE_FORWARDING_HEADERS(WebKitLegacyGUID
    DESTINATION ${FORWARDING_HEADERS_DIR}/WebKitLegacy
    FILES win/WebKitCOMAPI.h win/CFDictionaryPropertyBag.h
    DERIVED_SOURCE_DIRECTORIES ${DERIVED_SOURCES_WEBKITLEGACY_DIR}/Interfaces
    FLATTENED
)

set(WebKitLegacy_OUTPUT_NAME
    WebKit${DEBUG_SUFFIX}
)
