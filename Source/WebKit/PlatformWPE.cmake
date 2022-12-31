include(GNUInstallDirs)
include(GLib.cmake)
include(InspectorGResources.cmake)

if (ENABLE_PDFJS)
    include(PdfJSGResources.cmake)
endif ()

if (ENABLE_MODERN_MEDIA_CONTROLS)
    include(ModernMediaControlsGResources.cmake)
endif ()

set(WebKit_OUTPUT_NAME WPEWebKit-${WPE_API_VERSION})
set(WebProcess_OUTPUT_NAME WPEWebProcess)
set(NetworkProcess_OUTPUT_NAME WPENetworkProcess)
set(GPUProcess_OUTPUT_NAME WPEGPUProcess)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WPE_API_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_EXTENSION_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_JSC_DIR})

configure_file(Shared/glib/BuildRevision.h.in ${FORWARDING_HEADERS_WPE_DIR}/BuildRevision.h)
configure_file(UIProcess/API/wpe/WebKitVersion.h.in ${DERIVED_SOURCES_WPE_API_DIR}/WebKitVersion.h)
configure_file(wpe/wpe-webkit.pc.in ${WPE_PKGCONFIG_FILE} @ONLY)
configure_file(wpe/wpe-web-extension.pc.in ${WPEWebExtension_PKGCONFIG_FILE} @ONLY)
configure_file(wpe/wpe-webkit-uninstalled.pc.in ${CMAKE_BINARY_DIR}/wpe-webkit-${WPE_API_VERSION}-uninstalled.pc @ONLY)
configure_file(wpe/wpe-web-extension-uninstalled.pc.in ${CMAKE_BINARY_DIR}/wpe-web-extension-${WPE_API_VERSION}-uninstalled.pc @ONLY)

if (EXISTS "${TOOLS_DIR}/glib/apply-build-revision-to-files.py")
    add_custom_target(WebKit-build-revision
        ${PYTHON_EXECUTABLE} "${TOOLS_DIR}/glib/apply-build-revision-to-files.py" ${FORWARDING_HEADERS_WPE_DIR}/BuildRevision.h
        DEPENDS ${FORWARDING_HEADERS_WPE_DIR}/BuildRevision.h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} VERBATIM)
    list(APPEND WebKit_DEPENDENCIES
        WebKit-build-revision
    )
endif ()

add_definitions(-DLIBDIR="${LIB_INSTALL_DIR}")
add_definitions(-DPKGLIBDIR="${LIB_INSTALL_DIR}/wpe-webkit-${WPE_API_VERSION}")
add_definitions(-DPKGLIBEXECDIR="${LIBEXEC_INSTALL_DIR}")
add_definitions(-DDATADIR="${CMAKE_INSTALL_FULL_DATADIR}")
add_definitions(-DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

if (NOT DEVELOPER_MODE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
    WEBKIT_ADD_TARGET_PROPERTIES(WebKit LINK_FLAGS "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/webkitglib-symbols.map")
endif ()

set(WebKit_USE_PREFIX_HEADER ON)

add_custom_target(webkitwpe-forwarding-headers
    COMMAND ${PERL_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-forwarding-headers.pl --include-path ${WEBKIT_DIR} --output ${FORWARDING_HEADERS_DIR} --platform wpe --platform soup
)

 # These symbolic link allows includes like #include <wpe/WebkitWebView.h> which simulates installed headers.
add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WPE_DIR}/wpe
    DEPENDS ${WEBKIT_DIR}/UIProcess/API/wpe
    COMMAND ln -n -s -f ${WEBKIT_DIR}/UIProcess/API/wpe ${FORWARDING_HEADERS_WPE_DIR}/wpe
)

add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WPE_EXTENSION_DIR}/wpe
    DEPENDS ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe
    COMMAND ln -n -s -f ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe ${FORWARDING_HEADERS_WPE_EXTENSION_DIR}/wpe
)

add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WPE_JSC_DIR}/jsc
    DEPENDS ${JAVASCRIPTCORE_DIR}/API/glib/
    COMMAND ln -n -s -f ${JAVASCRIPTCORE_DIR}/API/glib ${FORWARDING_HEADERS_WPE_JSC_DIR}/jsc
    VERBATIM
)

list(APPEND WebProcess_SOURCES
    WebProcess/EntryPoint/unix/WebProcessMain.cpp
)

list(APPEND NetworkProcess_SOURCES
    NetworkProcess/EntryPoint/unix/NetworkProcessMain.cpp
)

list(APPEND GPUProcess_SOURCES
    GPUProcess/EntryPoint/unix/GPUProcessMain.cpp
)

list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES
    "SourcesWPE.txt"
)

list(APPEND WebKit_DERIVED_SOURCES
    ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.c
    ${WebKit_DERIVED_SOURCES_DIR}/WebKitDirectoryInputStreamData.cpp

    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.cpp
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.cpp
)

if (ENABLE_PDFJS)
    list(APPEND WebKit_DERIVED_SOURCES
        ${WebKit_DERIVED_SOURCES_DIR}/PdfJSGResourceBundle.c
        ${WebKit_DERIVED_SOURCES_DIR}/PdfJSGResourceBundleExtras.c
    )

    WEBKIT_BUILD_PDFJS_GRESOURCES(${WebKit_DERIVED_SOURCES_DIR})
endif ()

if (ENABLE_MODERN_MEDIA_CONTROLS)
  list(APPEND WebKit_DERIVED_SOURCES
      ${WebKit_DERIVED_SOURCES_DIR}/ModernMediaControlsGResourceBundle.c
  )

  WEBKIT_BUILD_MODERN_MEDIA_CONTROLS_GRESOURCES(${WebKit_DERIVED_SOURCES_DIR})
endif ()

set(WebKit_DirectoryInputStream_DATA
    ${WEBKIT_DIR}/NetworkProcess/soup/Resources/directory.css
    ${WEBKIT_DIR}/NetworkProcess/soup/Resources/directory.js
)

add_custom_command(
    OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/WebKitDirectoryInputStreamData.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebKitDirectoryInputStreamData.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/make-css-file-arrays.pl
    DEPENDS ${WebKit_DirectoryInputStream_DATA}
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/make-css-file-arrays.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" ${WebKit_DERIVED_SOURCES_DIR}/WebKitDirectoryInputStreamData.h ${WebKit_DERIVED_SOURCES_DIR}/WebKitDirectoryInputStreamData.cpp ${WebKit_DirectoryInputStream_DATA}
    VERBATIM
)

set(WPE_API_HEADER_TEMPLATES
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitApplicationInfo.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitAuthenticationRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitAutocleanups.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitAutomationSession.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitBackForwardList.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitBackForwardListItem.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitCredential.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitContextMenu.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitContextMenuActions.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitContextMenuItem.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitCookieManager.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitDefines.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitDeviceInfoPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitDownload.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitEditingCommands.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitEditorState.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitError.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFaviconDatabase.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFileChooserRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFindController.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFormSubmissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitGeolocationManager.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitGeolocationPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitHitTestResult.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitInputMethodContext.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitInstallMissingMediaPluginsPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitJavascriptResult.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitMediaKeySystemPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitMemoryPressureSettings.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitNavigationAction.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitNavigationPolicyDecision.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitNetworkProxySettings.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitNotificationPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitNotification.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitOptionMenu.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitOptionMenuItem.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitPermissionStateQuery.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitPolicyDecision.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitResponsePolicyDecision.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitScriptDialog.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitSecurityManager.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitSecurityOrigin.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitSettings.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitURIRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitURIResponse.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitURISchemeRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitURISchemeResponse.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitURIUtilities.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitUserContent.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitUserContentFilterStore.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitUserContentManager.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitUserMediaPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitUserMessage.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebContext.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebResource.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebView.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebViewSessionState.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebsiteData.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebsiteDataAccessPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebsiteDataManager.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWindowProperties.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitWebsitePolicies.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/webkit.h.in
)

set(WPE_API_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitVersion.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitColor.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitRectangle.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebViewBackend.h
)

set(WPE_WEB_EXTENSION_API_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h
)

set(WPE_WEB_EXTENSION_API_HEADER_TEMPLATES
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitFrame.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitScriptWorld.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebEditor.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebExtension.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebExtensionAutocleanups.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebFormManager.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebHitTestResult.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebPage.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/webkit-web-extension.h.in
)

set(WPE_FAKE_API_HEADERS
    ${FORWARDING_HEADERS_WPE_DIR}/wpe
    ${FORWARDING_HEADERS_WPE_EXTENSION_DIR}/wpe
    ${FORWARDING_HEADERS_WPE_JSC_DIR}/jsc
)

if (NOT ENABLE_2022_GLIB_API)
    include(PlatformWPEDeprecated.cmake)
endif ()

add_custom_target(webkitwpe-fake-api-headers
    DEPENDS ${WPE_FAKE_API_HEADERS}
)

list(APPEND WebKit_DEPENDENCIES
    webkitwpe-fake-api-headers
    webkitwpe-forwarding-headers
)

GENERATE_API_HEADERS(WPE_API_HEADER_TEMPLATES
    ${DERIVED_SOURCES_WPE_API_DIR}
    WPE_API_INSTALLED_HEADERS
    "-DWTF_PLATFORM_GTK=0"
    "-DWTF_PLATFORM_WPE=1"
    "-DUSE_GTK4=0"
    "-DENABLE_2022_GLIB_API=$<BOOL:${ENABLE_2022_GLIB_API}>"
)

GENERATE_API_HEADERS(WPE_WEB_EXTENSION_API_HEADER_TEMPLATES
    ${DERIVED_SOURCES_WPE_API_DIR}
    WPE_WEB_EXTENSION_API_INSTALLED_HEADERS
    "-DWTF_PLATFORM_GTK=0"
    "-DWTF_PLATFORM_WPE=1"
    "-DUSE_GTK4=0"
    "-DENABLE_2022_GLIB_API=$<BOOL:${ENABLE_2022_GLIB_API}>"
)

# To generate WebKitEnumTypes.h we want to use all installed headers, except WebKitEnumTypes.h itself.
set(WPE_ENUM_GENERATION_HEADERS ${WPE_API_INSTALLED_HEADERS})
list(REMOVE_ITEM WPE_ENUM_GENERATION_HEADERS ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h
           ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.cpp
    DEPENDS ${WPE_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitEnumTypes.h.in ${WPE_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitEnumTypes.cpp.in ${WPE_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.cpp
    VERBATIM
)

set(WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS ${WPE_WEB_EXTENSION_API_INSTALLED_HEADERS})
list(REMOVE_ITEM WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h
           ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.cpp
    DEPENDS ${WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitWebProcessEnumTypes.h.in ${WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitWebProcessEnumTypes.cpp.in ${WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.cpp
    VERBATIM
)

set(WebKitResources
)

if (ENABLE_WEB_AUDIO)
    list(APPEND WebKitResources
        "        <file alias=\"audio/Composite\">Composite.wav</file>\n"
    )
endif ()

file(WRITE ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.xml
    "<?xml version=1.0 encoding=UTF-8?>\n"
    "<gresources>\n"
    "    <gresource prefix=\"/org/webkitwpe/resources\">\n"
    ${WebKitResources}
    "    </gresource>\n"
    "</gresources>\n"
)

add_custom_command(
    OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.c ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.deps
    DEPENDS ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.xml
    DEPFILE ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.deps
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/Resources --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/platform/audio/resources --target=${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.c --dependency-file=${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.deps ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.xml
    VERBATIM
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_WPE_API_DIR}"
    "${FORWARDING_HEADERS_WPE_DIR}"
    "${FORWARDING_HEADERS_WPE_EXTENSION_DIR}"
    "${WEBKIT_DIR}/NetworkProcess/glib"
    "${WEBKIT_DIR}/NetworkProcess/soup"
    "${WEBKIT_DIR}/Platform/IPC/glib"
    "${WEBKIT_DIR}/Platform/IPC/unix"
    "${WEBKIT_DIR}/Platform/classifier"
    "${WEBKIT_DIR}/Platform/generic"
    "${WEBKIT_DIR}/Shared/API/c/wpe"
    "${WEBKIT_DIR}/Shared/API/glib"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics"
    "${WEBKIT_DIR}/Shared/CoordinatedGraphics/threadedcompositor"
    "${WEBKIT_DIR}/Shared/glib"
    "${WEBKIT_DIR}/Shared/libwpe"
    "${WEBKIT_DIR}/Shared/soup"
    "${WEBKIT_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT_DIR}/UIProcess/API/C/glib"
    "${WEBKIT_DIR}/UIProcess/API/C/wpe"
    "${WEBKIT_DIR}/UIProcess/API/glib"
    "${WEBKIT_DIR}/UIProcess/API/wpe"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/UIProcess/Inspector/glib"
    "${WEBKIT_DIR}/UIProcess/Launcher/glib"
    "${WEBKIT_DIR}/UIProcess/Launcher/libwpe"
    "${WEBKIT_DIR}/UIProcess/Notifications/glib/"
    "${WEBKIT_DIR}/UIProcess/geoclue"
    "${WEBKIT_DIR}/UIProcess/gstreamer"
    "${WEBKIT_DIR}/UIProcess/linux"
    "${WEBKIT_DIR}/UIProcess/soup"
    "${WEBKIT_DIR}/UIProcess/wpe"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/soup"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/libwpe"
    "${WEBKIT_DIR}/WebProcess/WebPage/wpe"
    "${WEBKIT_DIR}/WebProcess/glib"
    "${WEBKIT_DIR}/WebProcess/soup"
)

list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
    ${ATK_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

list(APPEND WebKit_LIBRARIES
    Cairo::Cairo
    Freetype::Freetype
    HarfBuzz::HarfBuzz
    HarfBuzz::ICU
    WPE::libwpe
    ${ATK_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
)

if (ENABLE_ACCESSIBILITY)
    list(APPEND WebKit_LIBRARIES ATK::Bridge)
endif ()

if (ENABLE_BUBBLEWRAP_SANDBOX)
    list(APPEND WebKit_LIBRARIES Libseccomp::Libseccomp)
endif ()

if (USE_GSTREAMER_FULL)
    list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
        ${GSTREAMER_FULL_INCLUDE_DIRS}
    )
    list(APPEND WebKit_LIBRARIES
        ${GSTREAMER_FULL_LIBRARIES}
    )
else ()
    list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTREAMER_AUDIO_INCLUDE_DIRS}
        ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
        ${GSTREAMER_VIDEO_INCLUDE_DIRS}
    )
    list(APPEND WebKit_LIBRARIES
        ${GSTREAMER_LIBRARIES}
    )
endif ()

if (ENABLE_MEDIA_STREAM)
    list(APPEND WebKit_SOURCES
        UIProcess/glib/UserMediaPermissionRequestManagerProxyGLib.cpp

        WebProcess/glib/UserMediaCaptureManager.cpp
    )
    list(APPEND WebKit_MESSAGES_IN_FILES
        WebProcess/glib/UserMediaCaptureManager
    )
endif ()

if (ENABLE_BREAKPAD)
    list(APPEND WebKit_SOURCES
        Shared/unix/BreakpadExceptionHandler.cpp
    )
    list(APPEND WebKit_LIBRARIES
        Breakpad::Breakpad
    )
endif ()

WEBKIT_BUILD_INSPECTOR_GRESOURCES(${WebInspectorUI_DERIVED_SOURCES_DIR})
list(APPEND WPEWebInspectorResources_DERIVED_SOURCES
    ${WebInspectorUI_DERIVED_SOURCES_DIR}/InspectorGResourceBundle.c
)

list(APPEND WPEWebInspectorResources_LIBRARIES
    ${GLIB_GIO_LIBRARIES}
)

list(APPEND WPEWebInspectorResources_SYSTEM_INCLUDE_DIRECTORIES
    ${GLIB_INCLUDE_DIRS}
)

add_library(WPEWebInspectorResources SHARED ${WPEWebInspectorResources_DERIVED_SOURCES})
add_dependencies(WPEWebInspectorResources WebKit)
target_link_libraries(WPEWebInspectorResources ${WPEWebInspectorResources_LIBRARIES})
target_include_directories(WPEWebInspectorResources SYSTEM PUBLIC ${WPEWebInspectorResources_SYSTEM_INCLUDE_DIRECTORIES})
install(TARGETS WPEWebInspectorResources DESTINATION "${LIB_INSTALL_DIR}/wpe-webkit-${WPE_API_VERSION}")

add_library(WPEInjectedBundle MODULE "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitInjectedBundleMain.cpp")
ADD_WEBKIT_PREFIX_HEADER(WPEInjectedBundle)
target_link_libraries(WPEInjectedBundle WebKit)

target_include_directories(WPEInjectedBundle PRIVATE $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)

if (ENABLE_WPE_QT_API)
    set(qtwpe_SOURCES
        ${WEBKIT_DIR}/UIProcess/API/wpe/qt/WPEQtViewBackend.cpp
        ${WEBKIT_DIR}/UIProcess/API/wpe/qt/WPEQmlExtensionPlugin.cpp
        ${WEBKIT_DIR}/UIProcess/API/wpe/qt/WPEQtView.cpp
        ${WEBKIT_DIR}/UIProcess/API/wpe/qt/WPEQtViewLoadRequest.cpp
    )

    set(qtwpe_LIBRARIES
        Qt5::Core Qt5::Quick
        WebKit
        ${GLIB_GOBJECT_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${LIBEPOXY_LIBRARIES}
        ${WPEBACKEND_FDO_LIBRARIES}
    )

    set(qtwpe_INCLUDE_DIRECTORIES
        $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
        ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
        ${CMAKE_BINARY_DIR}
        ${GLIB_INCLUDE_DIRS}
        ${Qt5_INCLUDE_DIRS}
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
        ${LIBEPOXY_INCLUDE_DIRS}
        ${LIBSOUP_INCLUDE_DIRS}
        ${WPE_INCLUDE_DIRS}
        ${WPEBACKEND_FDO_INCLUDE_DIRS}
    )

    list(APPEND WPE_QT_API_INSTALLED_HEADERS
        ${WEBKIT_DIR}/UIProcess/API/wpe/qt/WPEQtView.h
        ${WEBKIT_DIR}/UIProcess/API/wpe/qt/WPEQtViewLoadRequest.h
    )

    add_library(qtwpe SHARED ${qtwpe_SOURCES})
    set_target_properties(qtwpe PROPERTIES
        OUTPUT_NAME qtwpe
        AUTOMOC ON
    )
    target_compile_definitions(qtwpe PUBLIC QT_NO_KEYWORDS=1)
    target_link_libraries(qtwpe ${qtwpe_LIBRARIES})
    target_include_directories(qtwpe PRIVATE ${qtwpe_INCLUDE_DIRECTORIES})
    install(TARGETS qtwpe DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/qt5/qml/org/wpewebkit/qtwpe/")
    install(FILES ${WEBKIT_DIR}/UIProcess/API/wpe/qt/qmldir DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/qt5/qml/org/wpewebkit/qtwpe/")

    file(MAKE_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/qt5/qml/org/wpewebkit/qtwpe)
    add_custom_command(TARGET qtwpe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libqtwpe.so
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/qt5/qml/org/wpewebkit/qtwpe)
    add_custom_command(TARGET qtwpe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
        ${WEBKIT_DIR}/UIProcess/API/wpe/qt/qmldir
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/qt5/qml/org/wpewebkit/qtwpe)
endif ()

install(TARGETS WPEInjectedBundle
        DESTINATION "${LIB_INSTALL_DIR}/wpe-webkit-${WPE_API_VERSION}/injected-bundle"
)

install(FILES "${CMAKE_BINARY_DIR}/wpe-webkit-${WPE_API_VERSION}.pc"
              "${CMAKE_BINARY_DIR}/wpe-web-extension-${WPE_API_VERSION}.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
        COMPONENT "Development"
)

install(FILES ${WPE_API_INSTALLED_HEADERS}
              ${WPE_QT_API_INSTALLED_HEADERS}
              ${WPE_WEB_EXTENSION_API_INSTALLED_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-webkit-${WPE_API_VERSION}/wpe"
        COMPONENT "Development"
)

# XXX: Using ${JavaScriptCore_INSTALLED_HEADERS} here expands to nothing.
GI_INTROSPECT(WPEJavaScriptCore ${WPE_API_VERSION} jsc/jsc.h
    TARGET WebKit
    PACKAGE wpe-javascriptcore
    SYMBOL_PREFIX jsc
    DEPENDENCIES GObject-2.0
    OPTIONS
        -I${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
        -I${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}
    SOURCES
        ${JAVASCRIPTCORE_DIR}/API/glib/JSCAutocleanups.h
        ${JAVASCRIPTCORE_DIR}/API/glib/JSCClass.h
        ${JAVASCRIPTCORE_DIR}/API/glib/JSCContext.h
        ${JAVASCRIPTCORE_DIR}/API/glib/JSCDefines.h
        ${JAVASCRIPTCORE_DIR}/API/glib/JSCException.h
        ${JAVASCRIPTCORE_DIR}/API/glib/JSCOptions.h
        ${JAVASCRIPTCORE_DIR}/API/glib/JSCValue.h
        ${JAVASCRIPTCORE_DIR}/API/glib/JSCVirtualMachine.h
        ${JAVASCRIPTCORE_DIR}/API/glib/JSCWeakValue.h
        ${JAVASCRIPTCORE_DIR}/API/glib/jsc.h
        ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/JSCVersion.h
        ${JAVASCRIPTCORE_DIR}/API/glib
    NO_IMPLICIT_SOURCES
)
GI_DOCGEN(WPEJavaScriptCore "${JAVASCRIPTCORE_DIR}/API/glib/docs/jsc.toml.in")

set(WPE_SOURCES_FOR_INTROSPECTION
    UIProcess/API/wpe/WebKitColor.cpp
    UIProcess/API/wpe/WebKitInputMethodContextWPE.cpp
    UIProcess/API/wpe/WebKitRectangle.cpp
    UIProcess/API/wpe/WebKitWebViewBackend.cpp
    UIProcess/API/wpe/WebKitWebViewWPE.cpp
 )

 if (ENABLE_2022_GLIB_API)
     list(APPEND WPE_SOURCES_FOR_INTROSPECTION UIProcess/API/wpe/WebKitWebViewWPE2.cpp)
 else ()
     list(APPEND WPE_SOURCES_FOR_INTROSPECTION UIProcess/API/wpe/WebKitWebViewWPE1.cpp)
 endif ()

GI_INTROSPECT(WPEWebKit ${WPE_API_VERSION} wpe/webkit.h
    TARGET WebKit
    PACKAGE wpe-webkit
    IDENTIFIER_PREFIX WebKit
    SYMBOL_PREFIX webkit
    DEPENDENCIES
        WPEJavaScriptCore
        Soup-${SOUP_API_VERSION}:libsoup-${SOUP_API_VERSION}
    OPTIONS
        -I${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
        -I${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}
    SOURCES
        ${WPE_API_INSTALLED_HEADERS}
        Shared/API/glib
        UIProcess/API/glib
    NO_IMPLICIT_SOURCES
)
GI_DOCGEN(WPEWebKit wpe/wpewebkit.toml.in)

GI_INTROSPECT(WPEWebExtension ${WPE_API_VERSION} wpe/webkit-web-extension.h
    TARGET WebKit
    PACKAGE wpe-web-extension
    IDENTIFIER_PREFIX WebKit
    SYMBOL_PREFIX webkit
    DEPENDENCIES
        WPEJavaScriptCore
        Soup-${SOUP_API_VERSION}:libsoup-${SOUP_API_VERSION}
    OPTIONS
        -I${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
        -I${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}
    SOURCES
        ${WPE_WEB_EXTENSION_API_INSTALLED_HEADERS}
        ${WPE_DOM_SOURCES_FOR_INTROSPECTION}
        ${DERIVED_SOURCES_WPE_API_DIR}/WebKitContextMenu.h
        ${DERIVED_SOURCES_WPE_API_DIR}/WebKitContextMenuActions.h
        ${DERIVED_SOURCES_WPE_API_DIR}/WebKitContextMenuItem.h
        ${DERIVED_SOURCES_WPE_API_DIR}/WebKitHitTestResult.h
        ${DERIVED_SOURCES_WPE_API_DIR}/WebKitUserMessage.h
        ${DERIVED_SOURCES_WPE_API_DIR}/WebKitURIRequest.h
        ${DERIVED_SOURCES_WPE_API_DIR}/WebKitURIResponse.h
        Shared/API/glib/WebKitContextMenu.cpp
        Shared/API/glib/WebKitContextMenuItem.cpp
        Shared/API/glib/WebKitHitTestResult.cpp
        Shared/API/glib/WebKitUserMessage.cpp
        Shared/API/glib/WebKitURIRequest.cpp
        Shared/API/glib/WebKitURIResponse.cpp
        WebProcess/InjectedBundle/API/glib
    NO_IMPLICIT_SOURCES
)
GI_DOCGEN(WPEWebExtension wpe/wpewebextension.toml.in)
