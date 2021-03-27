include(InspectorGResources.cmake)
include(GNUInstallDirs)

set(WebKit_OUTPUT_NAME WPEWebKit-${WPE_API_VERSION})
set(WebProcess_OUTPUT_NAME WPEWebProcess)
set(NetworkProcess_OUTPUT_NAME WPENetworkProcess)
set(GPUProcess_OUTPUT_NAME WPEGPUProcess)

file(MAKE_DIRECTORY ${DERIVED_SOURCES_WPE_API_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_EXTENSION_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_DOM_DIR})
file(MAKE_DIRECTORY ${FORWARDING_HEADERS_WPE_JSC_DIR})

configure_file(UIProcess/API/wpe/WebKitVersion.h.in ${DERIVED_SOURCES_WPE_API_DIR}/WebKitVersion.h)
configure_file(wpe/wpe-webkit.pc.in ${WPE_PKGCONFIG_FILE} @ONLY)
configure_file(wpe/wpe-web-extension.pc.in ${WPEWebExtension_PKGCONFIG_FILE} @ONLY)
configure_file(wpe/wpe-webkit-uninstalled.pc.in ${CMAKE_BINARY_DIR}/wpe-webkit-${WPE_API_VERSION}-uninstalled.pc @ONLY)
configure_file(wpe/wpe-web-extension-uninstalled.pc.in ${CMAKE_BINARY_DIR}/wpe-web-extension-${WPE_API_VERSION}-uninstalled.pc @ONLY)

add_definitions(-DWEBKIT2_COMPILATION)

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
    OUTPUT ${FORWARDING_HEADERS_WPE_DOM_DIR}/wpe
    DEPENDS ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM
    COMMAND ln -n -s -f ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM ${FORWARDING_HEADERS_WPE_DOM_DIR}/wpe
    VERBATIM
)

add_custom_command(
    OUTPUT ${FORWARDING_HEADERS_WPE_JSC_DIR}/jsc
    DEPENDS ${JAVASCRIPTCORE_DIR}/API/glib/
    COMMAND ln -n -s -f ${JAVASCRIPTCORE_DIR}/API/glib ${FORWARDING_HEADERS_WPE_JSC_DIR}/jsc
    VERBATIM
)

add_custom_target(webkitwpe-fake-api-headers
    DEPENDS ${FORWARDING_HEADERS_WPE_DIR}/wpe
            ${FORWARDING_HEADERS_WPE_EXTENSION_DIR}/wpe
            ${FORWARDING_HEADERS_WPE_DOM_DIR}/wpe
            ${FORWARDING_HEADERS_WPE_JSC_DIR}/jsc
)

list(APPEND WebKit_DEPENDENCIES
    webkitwpe-fake-api-headers
    webkitwpe-forwarding-headers
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

set(WPE_API_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitVersion.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitApplicationInfo.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitAuthenticationRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitAutocleanups.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitAutomationSession.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitBackForwardList.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitBackForwardListItem.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitCredential.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitColor.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitContextMenu.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitContextMenuActions.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitContextMenuItem.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitCookieManager.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitDefines.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitDeviceInfoPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitDownload.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitEditingCommands.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitEditorState.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitError.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitFaviconDatabase.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitFileChooserRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitFindController.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitFormSubmissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitGeolocationManager.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitGeolocationPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitHitTestResult.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitInputMethodContext.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitInstallMissingMediaPluginsPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitJavascriptResult.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitMediaKeySystemPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitMimeInfo.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitNavigationAction.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitNavigationPolicyDecision.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitNetworkProxySettings.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitNotificationPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitNotification.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitOptionMenu.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitOptionMenuItem.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitPlugin.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitPolicyDecision.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitRectangle.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitResponsePolicyDecision.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitScriptDialog.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitSecurityManager.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitSecurityOrigin.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitSettings.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitURIRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitURIResponse.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitURISchemeRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitURIUtilities.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitUserContent.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitUserContentFilterStore.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitUserContentManager.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitUserMediaPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitUserMessage.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebContext.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebResource.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebView.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebViewBackend.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebViewSessionState.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebsiteData.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebsiteDataAccessPermissionRequest.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebsiteDataManager.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWindowProperties.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitWebsitePolicies.h
    ${WEBKIT_DIR}/UIProcess/API/wpe/webkit.h
)

set(WPE_WEB_EXTENSION_API_INSTALLED_HEADERS
    ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitConsoleMessage.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitFrame.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitScriptWorld.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitWebEditor.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitWebExtension.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitWebExtensionAutocleanups.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitWebHitTestResult.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitWebPage.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/webkit-web-extension.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/webkitdom.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/WebKitDOMDefines.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/WebKitDOMDocument.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/WebKitDOMElement.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/WebKitDOMNode.h
    ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM/WebKitDOMObject.h
)

# To generate WebKitEnumTypes.h we want to use all installed headers, except WebKitEnumTypes.h itself.
set(WPE_ENUM_GENERATION_HEADERS ${WPE_API_INSTALLED_HEADERS})
list(REMOVE_ITEM WPE_ENUM_GENERATION_HEADERS ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h
           ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.cpp
    DEPENDS ${WPE_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitEnumTypes.h.template ${WPE_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.h

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/UIProcess/API/wpe/WebKitEnumTypes.cpp.template ${WPE_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitEnumTypes.cpp
    VERBATIM
)

set(WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS ${WPE_WEB_EXTENSION_API_INSTALLED_HEADERS})
list(REMOVE_ITEM WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h
           ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.cpp
    DEPENDS ${WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS}

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitWebProcessEnumTypes.h.template ${WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ | sed s/WEBKIT_TYPE_KIT/WEBKIT_TYPE/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.h

    COMMAND glib-mkenums --template ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/WebKitWebProcessEnumTypes.cpp.template ${WPE_WEB_PROCESS_ENUM_GENERATION_HEADERS} | sed s/web_kit/webkit/ > ${DERIVED_SOURCES_WPE_API_DIR}/WebKitWebProcessEnumTypes.cpp
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
    OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.c
    DEPENDS ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.xml
    COMMAND glib-compile-resources --generate --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/Resources --sourcedir=${CMAKE_SOURCE_DIR}/Source/WebCore/platform/audio/resources --target=${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.c ${WebKit_DERIVED_SOURCES_DIR}/WebKitResourcesGResourceBundle.xml
    VERBATIM
)

list(APPEND WebKit_INCLUDE_DIRECTORIES
    "${DERIVED_SOURCES_WPE_API_DIR}"
    "${FORWARDING_HEADERS_WPE_DIR}"
    "${FORWARDING_HEADERS_WPE_DOM_DIR}"
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
    "${WEBKIT_DIR}/UIProcess/API/C/wpe"
    "${WEBKIT_DIR}/UIProcess/API/glib"
    "${WEBKIT_DIR}/UIProcess/API/wpe"
    "${WEBKIT_DIR}/UIProcess/CoordinatedGraphics"
    "${WEBKIT_DIR}/UIProcess/geoclue"
    "${WEBKIT_DIR}/UIProcess/gstreamer"
    "${WEBKIT_DIR}/UIProcess/linux"
    "${WEBKIT_DIR}/UIProcess/soup"
    "${WEBKIT_DIR}/UIProcess/wpe"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/DOM"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport/soup"
    "${WEBKIT_DIR}/WebProcess/WebPage/CoordinatedGraphics"
    "${WEBKIT_DIR}/WebProcess/WebPage/atk"
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

file(WRITE ${CMAKE_BINARY_DIR}/gtkdoc-wpe.cfg
    "[wpe-${WPE_API_DOC_VERSION}]\n"
    "pkgconfig_file=${WPE_PKGCONFIG_FILE}\n"
    "decorator=WEBKIT_API|WEBKIT_DEPRECATED|WEBKIT_DEPRECATED_FOR\\(.+\\)\n"
    "deprecation_guard=WEBKIT_DISABLE_DEPRECATED\n"
    "namespace=webkit\n"
    "cflags=-I${CMAKE_SOURCE_DIR}/Source\n"
    "       -I${WEBKIT_DIR}/Shared/API/glib\n"
    "       -I${WEBKIT_DIR}/UIProcess/API/glib\n"
    "       -I${WEBKIT_DIR}/UIProcess/API/wpe\n"
    "       -I${FORWARDING_HEADERS_WPE_DIR}\n"
    "doc_dir=${WEBKIT_DIR}/UIProcess/API/wpe/docs\n"
    "source_dirs=${WEBKIT_DIR}/Shared/API/glib\n"
    "            ${WEBKIT_DIR}/UIProcess/API/glib\n"
    "            ${WEBKIT_DIR}/UIProcess/API/wpe\n"
    "            ${DERIVED_SOURCES_WPE_API_DIR}\n"
    "headers=${WPE_ENUM_GENERATION_HEADERS}\n"
    "main_sgml_file=wpe-docs.sgml\n"
)

file(WRITE ${CMAKE_BINARY_DIR}/gtkdoc-webextensions.cfg
    "[wpe-webextensions-${WPE_API_DOC_VERSION}]\n"
    "pkgconfig_file=${WPEWebExtension_PKGCONFIG_FILE}\n"
    "decorator=WEBKIT_API|WEBKIT_DEPRECATED|WEBKIT_DEPRECATED_FOR\\(.+\\)\n"
    "deprecation_guard=WEBKIT_DISABLE_DEPRECATED\n"
    "namespace=webkit_webextensions\n"
    "cflags=-I${CMAKE_SOURCE_DIR}/Source\n"
    "       -I${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe\n"
    "       -I${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM\n"
    "       -I${FORWARDING_HEADERS_WPE_DIR}\n"
    "doc_dir=${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/docs\n"
    "source_dirs=${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib\n"
    "            ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/glib/DOM\n"
    "            ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe\n"
    "            ${WEBKIT_DIR}/WebProcess/InjectedBundle/API/wpe/DOM\n"
    "headers=${WPE_WEB_EXTENSION_API_INSTALLED_HEADERS}\n"
    "main_sgml_file=wpe-webextensions-docs.sgml\n"
)

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
        ${CMAKE_BINARY_DIR}
        ${GLIB_INCLUDE_DIRS}
        ${Qt5_INCLUDE_DIRS}
        ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
        ${LIBEPOXY_INCLUDE_DIRS}
        ${LIBSOUP_INCLUDE_DIRS}
        ${WPE_INCLUDE_DIRS}
        ${WPEBACKEND_FDO_INCLUDE_DIRS}
    )

    list(APPEND WPE_API_INSTALLED_HEADERS
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
              ${WPE_WEB_EXTENSION_API_INSTALLED_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wpe-webkit-${WPE_API_VERSION}/wpe"
        COMPONENT "Development"
)
