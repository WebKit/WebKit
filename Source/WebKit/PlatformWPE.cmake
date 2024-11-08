if (ENABLE_WPE_PLATFORM)
    add_subdirectory(${WEBKIT_DIR}/WPEPlatform)
endif ()

include(GNUInstallDirs)
include(GLibMacros)
include(InspectorGResources.cmake)

if (ENABLE_PDFJS)
    include(PdfJSGResources.cmake)
endif ()

if (ENABLE_MODERN_MEDIA_CONTROLS)
    include(ModernMediaControlsGResources.cmake)
endif ()

if (USE_SKIA)
    include(Platform/Skia.cmake)
endif ()

set(WebKit_OUTPUT_NAME WPEWebKit-${WPE_API_VERSION})
set(WebProcess_OUTPUT_NAME WPEWebProcess)
set(NetworkProcess_OUTPUT_NAME WPENetworkProcess)
set(GPUProcess_OUTPUT_NAME WPEGPUProcess)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WPE_API_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_EXTENSION_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_JSC_DIR})

if (ENABLE_WPE_PLATFORM)
    set(WPE_PLATFORM_PC_REQUIRES wpe-platform-${WPE_API_VERSION})
    set(WPE_PLATFORM_PC_UNINSTALLED_REQUIRES wpe-platform-${WPE_API_VERSION}-uninstalled)
endif ()

configure_file(Shared/glib/BuildRevision.h.in ${FORWARDING_HEADERS_WPE_DIR}/BuildRevision.h)
configure_file(UIProcess/API/wpe/WebKitVersion.h.in ${DERIVED_SOURCES_WPE_API_DIR}/WebKitVersion.h)
configure_file(wpe/wpe-webkit.pc.in ${WPE_PKGCONFIG_FILE} @ONLY)
configure_file(wpe/wpe-web-process-extension.pc.in ${WPEWebProcessExtension_PKGCONFIG_FILE} @ONLY)
configure_file(wpe/wpe-webkit-uninstalled.pc.in ${WPE_Uninstalled_PKGCONFIG_FILE} @ONLY)
configure_file(wpe/wpe-web-process-extension-uninstalled.pc.in ${WPEWebProcessExtension_Uninstalled_PKGCONFIG_FILE} @ONLY)

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
add_definitions(-DPKGDATADIR="${CMAKE_INSTALL_FULL_DATADIR}/wpe-webkit-${WPE_API_VERSION}")
add_definitions(-DLOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}")

if (NOT DEVELOPER_MODE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin")
    WEBKIT_ADD_TARGET_PROPERTIES(WebKit LINK_FLAGS "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/webkitglib-symbols.map")
    set_property(TARGET WebKit APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/webkitglib-symbols.map")
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

list(APPEND WebKit_SERIALIZATION_IN_FILES Shared/glib/DMABufRendererBufferFormat.serialization.in)

if (USE_GBM)
  list(APPEND WebKit_SERIALIZATION_IN_FILES Shared/gbm/DMABufBuffer.serialization.in)
endif ()

list(APPEND WebKit_SERIALIZATION_IN_FILES
    Shared/glib/DMABufRendererBufferMode.serialization.in
    Shared/glib/InputMethodState.serialization.in
    Shared/glib/SystemSettings.serialization.in
    Shared/glib/UserMessage.serialization.in

    Shared/soup/WebCoreArgumentCodersSoup.serialization.in
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
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFeature.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFileChooserRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFindController.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitFormSubmissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitGeolocationManager.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitGeolocationPermissionRequest.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitHitTestResult.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitInputMethodContext.h.in
    ${WEBKIT_DIR}/UIProcess/API/glib/WebKitInstallMissingMediaPluginsPermissionRequest.h.in
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

if (ENABLE_2022_GLIB_API)
    list(APPEND WPE_API_HEADER_TEMPLATES
        ${WEBKIT_DIR}/UIProcess/API/glib/WebKitNetworkSession.h.in
    )
endif ()

set(WPE_API_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitVersion.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitColor.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitRectangle.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebViewBackend.h
)

set(WPE_WEB_PROCESS_EXTENSION_API_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h
)

set(WPE_WEB_PROCESS_EXTENSION_API_HEADER_TEMPLATES
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitConsoleMessage.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitFrame.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitScriptWorld.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebEditor.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebFormManager.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebHitTestResult.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebPage.h.in
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/webkit-web-process-extension.h.in
)

if (ENABLE_2022_GLIB_API)
    list(APPEND WPE_WEB_PROCESS_EXTENSION_API_HEADER_TEMPLATES
        ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebProcessExtension.h.in
    )
    list(APPEND WebKit_SOURCES
        ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebProcessExtension.cpp
    )
else ()
    list(APPEND WPE_WEB_PROCESS_EXTENSION_API_HEADER_TEMPLATES
        ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebExtension.h.in
        ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebExtensionAutocleanups.h.in
    )
    list(APPEND WebKit_SOURCES
        ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitWebExtension.cpp
    )
endif ()

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

if (GI_VERSION VERSION_GREATER_EQUAL 1.79.2)
    set(USE_GI_FINISH_FUNC_ANNOTATION 1)
else ()
    set(USE_GI_FINISH_FUNC_ANNOTATION 0)
endif ()

GENERATE_GLIB_API_HEADERS(WebKit WPE_API_HEADER_TEMPLATES
    ${DERIVED_SOURCES_WPE_API_DIR}
    WPE_API_INSTALLED_HEADERS
    "-DWTF_PLATFORM_GTK=0"
    "-DWTF_PLATFORM_WPE=1"
    "-DUSE_GTK4=0"
    "-DENABLE_2022_GLIB_API=$<BOOL:${ENABLE_2022_GLIB_API}>"
    "-DENABLE_WPE_PLATFORM=$<BOOL:${ENABLE_WPE_PLATFORM}>"
    "-DUSE_GI_FINISH_FUNC_ANNOTATION=${USE_GI_FINISH_FUNC_ANNOTATION}"
)
unset(USE_GI_FINISH_FUNC_ANNOTATION)

GENERATE_GLIB_API_HEADERS(WebKit WPE_WEB_PROCESS_EXTENSION_API_HEADER_TEMPLATES
    ${DERIVED_SOURCES_WPE_API_DIR}
    WPE_WEB_PROCESS_EXTENSION_API_INSTALLED_HEADERS
    "-DWTF_PLATFORM_GTK=0"
    "-DWTF_PLATFORM_WPE=1"
    "-DUSE_GTK4=0"
    "-DENABLE_2022_GLIB_API=$<BOOL:${ENABLE_2022_GLIB_API}>"
)

if (NOT ENABLE_2022_GLIB_API)
    list(REMOVE_ITEM WPE_WEB_PROCESS_EXTENSION_API_INSTALLED_HEADERS ${DERIVED_SOURCES_WPE_API_DIR}/webkit-web-process-extension.h)
endif ()

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

if (ENABLE_2022_GLIB_API)
    set(WPE_WEB_PROCESS_ENUM_HEADER_TEMPLATE "WebKitWebProcessEnumTypesWPE2.h.in")
else ()
    set(WPE_WEB_PROCESS_ENUM_HEADER_TEMPLATE "WebKitWebProcessEnumTypesWPE1.h.in")
endif ()

set(WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS ${WPE_WEB_PROCESS_EXTENSION_API_INSTALLED_HEADERS})
list(REMOVE_ITEM WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h
           ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.cpp
    DEPENDS ${WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/${WPE_WEB_PROCESS_ENUM_HEADER_TEMPLATE} ${WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h

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

GLIB_COMPILE_RESOURCES(
    OUTPUT        ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.c
    SOURCE_XML    ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.xml
    RESOURCE_DIRS ${CMAKE_SOURCE_DIR}/Source/WebCore/Resources
                  ${CMAKE_SOURCE_DIR}/Source/WebCore/platform/audio/resources
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
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
    "${WEBKIT_DIR}/Shared/Extensions"
    "${WEBKIT_DIR}/Shared/glib"
    "${WEBKIT_DIR}/Shared/libwpe"
    "${WEBKIT_DIR}/Shared/soup"
    "${WEBKIT_DIR}/Shared/wpe"
    "${WEBKIT_DIR}/UIProcess/API/C/cairo"
    "${WEBKIT_DIR}/UIProcess/API/C/glib"
    "${WEBKIT_DIR}/UIProcess/API/C/wpe"
    "${WEBKIT_DIR}/UIProcess/API/glib"
    "${WEBKIT_DIR}/UIProcess/API/libwpe"
    "${WEBKIT_DIR}/UIProcess/API/wpe"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/UIProcess/Inspector/glib"
    "${WEBKIT_DIR}/UIProcess/Launcher/glib"
    "${WEBKIT_DIR}/UIProcess/Launcher/libwpe"
    "${WEBKIT_DIR}/UIProcess/Notifications/glib/"
    "${WEBKIT_DIR}/UIProcess/geoclue"
    "${WEBKIT_DIR}/UIProcess/glib"
    "${WEBKIT_DIR}/UIProcess/gstreamer"
    "${WEBKIT_DIR}/UIProcess/linux"
    "${WEBKIT_DIR}/UIProcess/soup"
    "${WEBKIT_DIR}/UIProcess/wpe"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/soup"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/dmabuf"
    "${WEBKIT_DIR}/WebProcess/WebPage/glib"
    "${WEBKIT_DIR}/WebProcess/WebPage/libwpe"
    "${WEBKIT_DIR}/WebProcess/WebPage/wpe"
    "${WEBKIT_DIR}/WebProcess/glib"
    "${WEBKIT_DIR}/WebProcess/soup"
)

list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc"
)

list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
)

list(APPEND WebKit_LIBRARIES
    WPE::libwpe
    ${GLIB_LIBRARIES}
    ${GLIB_GMODULE_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
)

if (USE_ATK)
    list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
        ${ATK_INCLUDE_DIRS}
    )

    list(APPEND WebKit_LIBRARIES
        ATK::Bridge
        ${ATK_LIBRARIES}
    )
endif ()

if (USE_CAIRO)
    list(APPEND WebKit_LIBRARIES
        Cairo::Cairo
        Freetype::Freetype
    )

    list(APPEND WebKit_SOURCES
        Shared/API/c/cairo/WKImageCairo.cpp

        UIProcess/Automation/cairo/WebAutomationSessionCairo.cpp
    )
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

list(APPEND WebKit_MESSAGES_IN_FILES
    WebProcess/glib/SystemSettingsManager
)

if (ENABLE_WPE_PLATFORM)
    list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES
        "${WPEPlatform_DERIVED_SOURCES_DIR}"
        "${WEBKIT_DIR}/WPEPlatform"
    )

    list(APPEND WebKit_PRIVATE_LIBRARIES
        WPEPlatform-${WPE_API_VERSION}
    )

    list(APPEND WebKit_MESSAGES_IN_FILES
        UIProcess/dmabuf/AcceleratedBackingStoreDMABuf

        WebProcess/WebPage/dmabuf/AcceleratedSurfaceDMABuf
    )
endif ()

list(APPEND WebKit_INTERFACE_INCLUDE_DIRECTORIES
    ${FORWARDING_HEADERS_WPE_DIR}
    ${WebKit_DERIVED_SOURCES_DIR}
)

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

WEBKIT_BUILD_INSPECTOR_GRESOURCES(${WebInspectorUI_DERIVED_SOURCES_DIR} "inspector.gresource")

install(FILES ${WebInspectorUI_DERIVED_SOURCES_DIR}/inspector.gresource DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/wpe-webkit-${WPE_API_VERSION}")

add_library(WPEInjectedBundle MODULE "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/WebKitInjectedBundleMain.cpp")
ADD_WEBKIT_PREFIX_HEADER(WPEInjectedBundle)
target_link_libraries(WPEInjectedBundle WebKit)

target_include_directories(WPEInjectedBundle PRIVATE $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)

target_include_directories(WPEInjectedBundle SYSTEM PRIVATE
    ${WebKit_SYSTEM_INCLUDE_DIRECTORIES}
)

if (ENABLE_WPE_QT_API)
    if (USE_QT6)
        list(APPEND WPE_QT_API_INSTALLED_HEADERS
            ${WEBKIT_DIR}/UIProcess/API/wpe/qt6/WPEQtView.h
            ${WEBKIT_DIR}/UIProcess/API/wpe/qt6/WPEQtViewLoadRequest.h
        )

        # FIXME: This should be MODULE, but tests link directly against it. Abusing
        #        SHARED here works on Linux and probably some other systems, but
        #        not on MacOS or Windows.
        add_library(qtwpe SHARED
            UIProcess/API/wpe/qt6/WPEDisplayQtQuick.cpp
            UIProcess/API/wpe/qt6/WPEToplevelQtQuick.cpp
            UIProcess/API/wpe/qt6/WPEViewQtQuick.cpp
            UIProcess/API/wpe/qt6/WPEQmlExtensionPlugin.cpp
            UIProcess/API/wpe/qt6/WPEQtView.cpp
            UIProcess/API/wpe/qt6/WPEQtViewLoadRequest.cpp
        )
        set_target_properties(qtwpe PROPERTIES
            OUTPUT_NAME qtwpe
            AUTOMOC ON
        )
        target_compile_definitions(qtwpe PUBLIC
            QT_NO_KEYWORDS=1
            QT_WPE_LIBRARY
        )
        target_link_libraries(qtwpe
            PUBLIC
                Qt::Quick
            PRIVATE
                Epoxy::Epoxy
                WebKit
                ${GLIB_GOBJECT_LIBRARIES}
                ${GLIB_LIBRARIES}
                WPEPlatform-${WPE_API_VERSION}
        )
        target_include_directories(qtwpe PRIVATE
            $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
            ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
            ${CMAKE_BINARY_DIR}
            ${GLIB_INCLUDE_DIRS}
            ${LIBSOUP_INCLUDE_DIRS}
            ${WPE_INCLUDE_DIRS}
            ${WEBKIT_DIR}/UIProcess/API/wpe/qt6
        )

        target_link_libraries(qtwpe PRIVATE Qt::QuickPrivate)

        install(TARGETS qtwpe
            DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/qt6/qml/org/wpewebkit/qtwpe/"
        )
        install(FILES ${WEBKIT_DIR}/UIProcess/API/wpe/qt6/qmldir
            DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/qt6/qml/org/wpewebkit/qtwpe/"
        )

        file(MAKE_DIRECTORY
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/qt6/qml/org/wpewebkit/qtwpe
        )
        add_custom_command(TARGET qtwpe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libqtwpe.so
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/qt6/qml/org/wpewebkit/qtwpe)
        add_custom_command(TARGET qtwpe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
            ${WEBKIT_DIR}/UIProcess/API/wpe/qt6/qmldir
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/qt6/qml/org/wpewebkit/qtwpe)
    else ()
        set(qtwpe_SOURCES
            ${WEBKIT_DIR}/UIProcess/API/wpe/qt5/WPEQtViewBackend.cpp
            ${WEBKIT_DIR}/UIProcess/API/wpe/qt5/WPEQmlExtensionPlugin.cpp
            ${WEBKIT_DIR}/UIProcess/API/wpe/qt5/WPEQtView.cpp
            ${WEBKIT_DIR}/UIProcess/API/wpe/qt5/WPEQtViewLoadRequest.cpp
        )

        set(qtwpe_LIBRARIES
            Epoxy::Epoxy
            Qt5::Core Qt5::Quick
            WPE::FDO
            WebKit
            ${GLIB_GOBJECT_LIBRARIES}
            ${GLIB_LIBRARIES}
        )

        set(qtwpe_INCLUDE_DIRECTORIES
            $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>
            ${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
            ${CMAKE_BINARY_DIR}
            ${GLIB_INCLUDE_DIRS}
            ${Qt5_INCLUDE_DIRS}
            ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
            ${LIBSOUP_INCLUDE_DIRS}
            ${WPE_INCLUDE_DIRS}
        )

        list(APPEND WPE_QT_API_INSTALLED_HEADERS
            ${WEBKIT_DIR}/UIProcess/API/wpe/qt5/WPEQtView.h
            ${WEBKIT_DIR}/UIProcess/API/wpe/qt5/WPEQtViewLoadRequest.h
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
        install(FILES ${WEBKIT_DIR}/UIProcess/API/wpe/qt5/qmldir DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/qt5/qml/org/wpewebkit/qtwpe/")

        file(MAKE_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/qt5/qml/org/wpewebkit/qtwpe)
        add_custom_command(TARGET qtwpe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libqtwpe.so
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/qt5/qml/org/wpewebkit/qtwpe)
        add_custom_command(TARGET qtwpe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy
            ${WEBKIT_DIR}/UIProcess/API/wpe/qt5/qmldir
            ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/qt5/qml/org/wpewebkit/qtwpe)
    endif ()
endif ()

install(TARGETS WPEInjectedBundle
        DESTINATION "${LIB_INSTALL_DIR}/wpe-webkit-${WPE_API_VERSION}/injected-bundle"
)

if (ENABLE_2022_GLIB_API)
    install(FILES "${CMAKE_BINARY_DIR}/wpe-webkit-${WPE_API_VERSION}.pc"
                  "${CMAKE_BINARY_DIR}/wpe-web-process-extension-${WPE_API_VERSION}.pc"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
            COMPONENT "Development"
    )
else ()
    install(FILES "${CMAKE_BINARY_DIR}/wpe-webkit-${WPE_API_VERSION}.pc"
                  "${CMAKE_BINARY_DIR}/wpe-web-extension-${WPE_API_VERSION}.pc"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
            COMPONENT "Development"
    )
endif ()

install(FILES ${WPE_API_INSTALLED_HEADERS}
              ${WPE_QT_API_INSTALLED_HEADERS}
              ${WPE_WEB_PROCESS_EXTENSION_API_INSTALLED_HEADERS}
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
        ${JAVASCRIPTCORE_DIR}/API/glib/JSCOptions.h
        ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/JSCClass.h
        ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/JSCContext.h
        ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/JSCDefines.h
        ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/JSCException.h
        ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/JSCValue.h
        ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/JSCVersion.h
        ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/JSCVirtualMachine.h
        ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/JSCWeakValue.h
        ${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}/jsc/jsc.h
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

set(WPE_LIBRARIES_FOR_INTROSPECTION
    WPEJavaScriptCore
    Soup-${SOUP_API_VERSION}:libsoup-${SOUP_API_VERSION}
)

set(WPE_INCLUDE_DIRS_FOR_INTROSPECTION
    -I${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
    -I${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}
)

if (ENABLE_WPE_PLATFORM)
    list(APPEND WPE_LIBRARIES_FOR_INTROSPECTION WPEPlatform)

    list(APPEND WPE_INCLUDE_DIRS_FOR_INTROSPECTION
        -I${WPEPlatform_DERIVED_SOURCES_DIR}
        -I${WEBKIT_DIR}/WPEPlatform
    )
endif ()

GI_INTROSPECT(WPEWebKit ${WPE_API_VERSION} wpe/webkit.h
    TARGET WebKit
    PACKAGE wpe-webkit
    IDENTIFIER_PREFIX WebKit
    SYMBOL_PREFIX webkit
    DEPENDENCIES
        ${WPE_LIBRARIES_FOR_INTROSPECTION}
    OPTIONS
        ${WPE_INCLUDE_DIRS_FOR_INTROSPECTION}
    SOURCES
        ${WPE_API_INSTALLED_HEADERS}
        Shared/API/glib
        UIProcess/API/glib
    NO_IMPLICIT_SOURCES
)
GI_DOCGEN(WPEWebKit wpe/wpewebkit.toml.in)

if (ENABLE_2022_GLIB_API)
    set(WPE_WEB_PROCESS_EXTENSION_API_NAME "WPEWebProcessExtension")
    set(WPE_WEB_PROCESS_EXTENSION_PACKAGE_NAME "wpe-web-process-extension")
else ()
    set(WPE_WEB_PROCESS_EXTENSION_API_NAME "WPEWebExtension")
    set(WPE_WEB_PROCESS_EXTENSION_PACKAGE_NAME "wpe-web-extension")
endif ()

GI_INTROSPECT(${WPE_WEB_PROCESS_EXTENSION_API_NAME} ${WPE_API_VERSION} wpe/${WPE_WEB_PROCESS_EXTENSION_PACKAGE_NAME}.h
    TARGET WebKit
    PACKAGE ${WPE_WEB_PROCESS_EXTENSION_PACKAGE_NAME}
    IDENTIFIER_PREFIX WebKit
    SYMBOL_PREFIX webkit
    DEPENDENCIES
        WPEJavaScriptCore
        Soup-${SOUP_API_VERSION}:libsoup-${SOUP_API_VERSION}
    OPTIONS
        -I${JavaScriptCoreGLib_FRAMEWORK_HEADERS_DIR}
        -I${JavaScriptCoreGLib_DERIVED_SOURCES_DIR}
    SOURCES
        ${WPE_WEB_PROCESS_EXTENSION_API_INSTALLED_HEADERS}
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
GI_DOCGEN(${WPE_WEB_PROCESS_EXTENSION_API_NAME} wpe/wpe-web-process-extension.toml.in)
