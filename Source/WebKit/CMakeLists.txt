include(WebKitCommon)

set_property(DIRECTORY . PROPERTY FOLDER "WebKit")

if (PAL_LIBRARY_TYPE MATCHES STATIC)
    add_definitions(-DSTATICALLY_LINKED_WITH_PAL=1)
endif ()

set(WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
)

set(WebKit_INCLUDE_DIRECTORIES
    "${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}"
    "${WebKit_FRAMEWORK_HEADERS_DIR}"
    "${WebKit_DERIVED_SOURCES_DIR}"
    "${WebKit_DERIVED_SOURCES_DIR}/include"
    "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/NetworkProcess"
    "${WEBKIT_DIR}/NetworkProcess/Classifier"
    "${WEBKIT_DIR}/NetworkProcess/Cookies"
    "${WEBKIT_DIR}/NetworkProcess/CustomProtocols"
    "${WEBKIT_DIR}/NetworkProcess/Downloads"
    "${WEBKIT_DIR}/NetworkProcess/FileAPI"
    "${WEBKIT_DIR}/NetworkProcess/IndexedDB"
    "${WEBKIT_DIR}/NetworkProcess/ServiceWorker"
    "${WEBKIT_DIR}/NetworkProcess/WebStorage"
    "${WEBKIT_DIR}/NetworkProcess/cache"
    "${WEBKIT_DIR}/NetworkProcess/watchos"
    "${WEBKIT_DIR}/NetworkProcess/webrtc"
    "${WEBKIT_DIR}/Platform"
    "${WEBKIT_DIR}/Platform/IPC"
    "${WEBKIT_DIR}/PluginProcess"
    "${WEBKIT_DIR}/Shared"
    "${WEBKIT_DIR}/Shared/API"
    "${WEBKIT_DIR}/Shared/API/c"
    "${WEBKIT_DIR}/Shared/ApplePay"
    "${WEBKIT_DIR}/Shared/Authentication"
    "${WEBKIT_DIR}/Shared/CoreIPCSupport"
    "${WEBKIT_DIR}/Shared/Databases"
    "${WEBKIT_DIR}/Shared/Databases/IndexedDB"
    "${WEBKIT_DIR}/Shared/FileAPI"
    "${WEBKIT_DIR}/Shared/Gamepad"
    "${WEBKIT_DIR}/Shared/Plugins"
    "${WEBKIT_DIR}/Shared/Plugins/Netscape"
    "${WEBKIT_DIR}/Shared/Plugins/Netscape/x11"
    "${WEBKIT_DIR}/Shared/RemoteLayerTree"
    "${WEBKIT_DIR}/Shared/WebsiteData"
    "${WEBKIT_DIR}/UIProcess"
    "${WEBKIT_DIR}/UIProcess/API"
    "${WEBKIT_DIR}/UIProcess/API/C"
    "${WEBKIT_DIR}/UIProcess/API/cpp"
    "${WEBKIT_DIR}/UIProcess/Authentication"
    "${WEBKIT_DIR}/UIProcess/Automation"
    "${WEBKIT_DIR}/UIProcess/Downloads"
    "${WEBKIT_DIR}/UIProcess/Gamepad"
    "${WEBKIT_DIR}/UIProcess/Launcher"
    "${WEBKIT_DIR}/UIProcess/Network"
    "${WEBKIT_DIR}/UIProcess/Network/CustomProtocols"
    "${WEBKIT_DIR}/UIProcess/Notifications"
    "${WEBKIT_DIR}/UIProcess/Plugins"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree"
    "${WEBKIT_DIR}/UIProcess/UserContent"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication/Mock"
    "${WEBKIT_DIR}/UIProcess/WebsiteData"
    "${WEBKIT_DIR}/WebProcess"
    "${WEBKIT_DIR}/WebProcess/ApplePay"
    "${WEBKIT_DIR}/WebProcess/ApplicationCache"
    "${WEBKIT_DIR}/WebProcess/Automation"
    "${WEBKIT_DIR}/WebProcess/Cache"
    "${WEBKIT_DIR}/WebProcess/Databases"
    "${WEBKIT_DIR}/WebProcess/Databases/IndexedDB"
    "${WEBKIT_DIR}/WebProcess/FileAPI"
    "${WEBKIT_DIR}/WebProcess/FullScreen"
    "${WEBKIT_DIR}/WebProcess/Gamepad"
    "${WEBKIT_DIR}/WebProcess/Geolocation"
    "${WEBKIT_DIR}/WebProcess/IconDatabase"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/c"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/DOM"
    "${WEBKIT_DIR}/WebProcess/Launching"
    "${WEBKIT_DIR}/WebProcess/MediaCache"
    "${WEBKIT_DIR}/WebProcess/MediaStream"
    "${WEBKIT_DIR}/WebProcess/Network"
    "${WEBKIT_DIR}/WebProcess/Network/webrtc"
    "${WEBKIT_DIR}/WebProcess/Notifications"
    "${WEBKIT_DIR}/WebProcess/OriginData"
    "${WEBKIT_DIR}/WebProcess/Plugins"
    "${WEBKIT_DIR}/WebProcess/Plugins/Netscape"
    "${WEBKIT_DIR}/WebProcess/ResourceCache"
    "${WEBKIT_DIR}/WebProcess/Storage"
    "${WEBKIT_DIR}/WebProcess/UserContent"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport"
    "${WEBKIT_DIR}/WebProcess/WebPage"
    "${WEBKIT_DIR}/WebProcess/WebPage/RemoteLayerTree"
    "${WEBKIT_DIR}/WebProcess/WebStorage"
    "${CMAKE_SOURCE_DIR}/Source"
    "${CMAKE_SOURCE_DIR}/WebKitLibraries"
)

set(PROTOCOL_GENERATOR_SCRIPTS_DIR "${JAVASCRIPTCORE_DIR}/inspector/scripts")

list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES
    "Sources.txt"
)

set(WebKit_MESSAGES_IN_FILES
    NetworkProcess/Cookies/WebCookieManager.messages.in

    NetworkProcess/IndexedDB/WebIDBConnectionToClient.messages.in

    NetworkProcess/NetworkConnectionToWebProcess.messages.in
    NetworkProcess/NetworkContentRuleListManager.messages.in
    NetworkProcess/NetworkProcess.messages.in
    NetworkProcess/NetworkResourceLoader.messages.in
    NetworkProcess/NetworkSocketChannel.messages.in
    NetworkProcess/NetworkSocketStream.messages.in

    NetworkProcess/ServiceWorker/ServiceWorkerFetchTask.messages.in
    NetworkProcess/ServiceWorker/WebSWServerConnection.messages.in
    NetworkProcess/ServiceWorker/WebSWServerToContextConnection.messages.in

    NetworkProcess/WebStorage/StorageManagerSet.messages.in

    NetworkProcess/cache/CacheStorageEngineConnection.messages.in

    NetworkProcess/webrtc/NetworkMDNSRegister.messages.in
    NetworkProcess/webrtc/NetworkRTCMonitor.messages.in
    NetworkProcess/webrtc/NetworkRTCProvider.messages.in
    NetworkProcess/webrtc/NetworkRTCSocket.messages.in

    PluginProcess/PluginControllerProxy.messages.in
    PluginProcess/PluginProcess.messages.in
    PluginProcess/WebProcessConnection.messages.in

    Shared/AuxiliaryProcess.messages.in
    Shared/WebConnection.messages.in

    Shared/Authentication/AuthenticationManager.messages.in

    Shared/Plugins/NPObjectMessageReceiver.messages.in

    UIProcess/DrawingAreaProxy.messages.in
    UIProcess/RemoteWebInspectorProxy.messages.in
    UIProcess/VisitedLinkStore.messages.in
    UIProcess/WebCookieManagerProxy.messages.in
    UIProcess/WebFullScreenManagerProxy.messages.in
    UIProcess/WebGeolocationManagerProxy.messages.in
    UIProcess/WebInspectorProxy.messages.in
    UIProcess/WebPageProxy.messages.in
    UIProcess/WebPasteboardProxy.messages.in
    UIProcess/WebProcessPool.messages.in
    UIProcess/WebProcessProxy.messages.in

    UIProcess/Automation/WebAutomationSession.messages.in

    UIProcess/Downloads/DownloadProxy.messages.in

    UIProcess/Network/NetworkProcessProxy.messages.in

    UIProcess/Plugins/PluginProcessProxy.messages.in

    UIProcess/UserContent/WebUserContentControllerProxy.messages.in

    WebProcess/WebProcess.messages.in

    WebProcess/Automation/WebAutomationSessionProxy.messages.in

    WebProcess/Databases/IndexedDB/WebIDBConnectionToServer.messages.in

    WebProcess/FullScreen/WebFullScreenManager.messages.in

    WebProcess/Geolocation/WebGeolocationManager.messages.in

    WebProcess/Network/webrtc/WebMDNSRegister.messages.in
    WebProcess/Network/webrtc/WebRTCMonitor.messages.in
    WebProcess/Network/webrtc/WebRTCResolver.messages.in
    WebProcess/Network/webrtc/WebRTCSocket.messages.in

    WebProcess/Network/NetworkProcessConnection.messages.in
    WebProcess/Network/WebResourceLoader.messages.in
    WebProcess/Network/WebSocketChannel.messages.in
    WebProcess/Network/WebSocketStream.messages.in

    WebProcess/Notifications/WebNotificationManager.messages.in

    WebProcess/Plugins/PluginProcessConnection.messages.in
    WebProcess/Plugins/PluginProcessConnectionManager.messages.in
    WebProcess/Plugins/PluginProxy.messages.in

    WebProcess/Storage/ServiceWorkerClientFetch.messages.in
    WebProcess/Storage/WebSWClientConnection.messages.in
    WebProcess/Storage/WebSWContextManagerConnection.messages.in

    WebProcess/UserContent/WebUserContentController.messages.in

    WebProcess/WebPage/DrawingArea.messages.in
    WebProcess/WebPage/EventDispatcher.messages.in
    WebProcess/WebPage/RemoteWebInspectorUI.messages.in
    WebProcess/WebPage/VisitedLinkTableController.messages.in
    WebProcess/WebPage/WebInspector.messages.in
    WebProcess/WebPage/WebInspectorInterruptDispatcher.messages.in
    WebProcess/WebPage/WebInspectorUI.messages.in
    WebProcess/WebPage/WebPage.messages.in

    WebProcess/WebStorage/StorageAreaMap.messages.in
)

if (${WebCore_LIBRARY_TYPE} STREQUAL STATIC)
    # Link with WebCore as PRIVATE not to propagate WebCore to all users of WebKit.
    set(WebKit_LIBRARIES
        PRIVATE WebCore
        PUBLIC WebCoreHeaderInterface
    )
else ()
    # All users of WebKit need to link WebCore if WebCore is a shared library.
    set(WebKit_LIBRARIES PUBLIC WebCore)
endif ()

set(WebProcess_LIBRARIES
    WebKit
)

set(NetworkProcess_LIBRARIES
    WebKit
)

set(PluginProcess_LIBRARIES
    WebKit
)

if (ENABLE_GRAPHICS_CONTEXT_3D)
    # For platforms that want to use system-provided OpenGL (ES) / EGL headers,
    # these include directories, libraries or definitions need to be
    # added before the ANGLE directories.
    if (USE_LIBEPOXY)
        list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
            ${LIBEPOXY_INCLUDE_DIRS}
        )
        list(APPEND WebKit_LIBRARIES
            PRIVATE ${LIBEPOXY_LIBRARIES}
        )
        add_definitions(${LIBEPOXY_DEFINITIONS})
    else ()
        if (USE_OPENGL)
            list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
                ${OPENGL_INCLUDE_DIRS}
            )
            list(APPEND WebKit_LIBRARIES
                PRIVATE ${OPENGL_LIBRARIES}
            )
            add_definitions(${OPENGL_DEFINITIONS})
        elseif (USE_OPENGL_ES)
            list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
                ${OPENGLES2_INCLUDE_DIRS}
            )
            list(APPEND WebKit_LIBRARIES
                PRIVATE ${OPENGLES2_LIBRARIES}
            )
            add_definitions(${OPENGLES2_DEFINITIONS})
        endif ()

        if (USE_EGL)
            list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
                ${EGL_INCLUDE_DIRS}
            )
            list(APPEND WebKit_LIBRARIES
                PRIVATE ${EGL_LIBRARIES}
            )
            add_definitions(${EGL_DEFINITIONS})
        endif ()
    endif ()
endif ()

if (ATOMIC_INT64_REQUIRES_LIBATOMIC)
    list(APPEND WebKit_LIBRARIES PRIVATE atomic)
endif ()

if (UNIX)
    check_function_exists(shm_open SHM_OPEN_EXISTS)
    if (NOT SHM_OPEN_EXISTS)
        set(CMAKE_REQUIRED_LIBRARIES rt)
        check_function_exists(shm_open SHM_OPEN_REQUIRES_LIBRT)
        if (SHM_OPEN_REQUIRES_LIBRT)
            list(APPEND WebKit_LIBRARIES PRIVATE rt)
        endif ()
        unset(CMAKE_REQUIRED_LIBRARIES)
    endif ()
endif ()

macro(ADD_WEBKIT_PREFIX_HEADER _target)
    if (WebKit_USE_PREFIX_HEADER)
        get_target_property(OLD_COMPILE_FLAGS ${_target} COMPILE_FLAGS)
        if (${OLD_COMPILE_FLAGS} STREQUAL "OLD_COMPILE_FLAGS-NOTFOUND")
            set(OLD_COMPILE_FLAGS "")
        endif ()
        set_target_properties(${_target} PROPERTIES COMPILE_FLAGS "${OLD_COMPILE_FLAGS} -include ${WEBKIT_DIR}/WebKit2Prefix.h")
    endif ()
endmacro()

set(WebKit_AUTOMATION_PROTOCOL_GENERATOR_EXTRA_FLAGS
)

WEBKIT_FRAMEWORK_DECLARE(WebKit)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

# Helper macro which wraps generate-message-receiver.py and generate-message-header.py scripts
#   _output_source is a list name which will contain generated sources.(eg. WebKit_SOURCES)
#   _input_files are messages.in files to generate.
macro(GENERATE_MESSAGE_SOURCES _output_source _input_files)
    foreach (_file ${_input_files})
        get_filename_component(_name ${_file} NAME_WE)
        add_custom_command(
            OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/${_name}MessageReceiver.cpp ${WebKit_DERIVED_SOURCES_DIR}/${_name}Messages.h
            MAIN_DEPENDENCY ${_file}
            DEPENDS ${WEBKIT_DIR}/Scripts/webkit/__init__.py
                    ${WEBKIT_DIR}/Scripts/webkit/messages.py
                    ${WEBKIT_DIR}/Scripts/webkit/model.py
                    ${WEBKIT_DIR}/Scripts/webkit/parser.py
            COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-message-receiver.py ${_file} > ${WebKit_DERIVED_SOURCES_DIR}/${_name}MessageReceiver.cpp
            COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-messages-header.py ${_file} > ${WebKit_DERIVED_SOURCES_DIR}/${_name}Messages.h
            WORKING_DIRECTORY ${WEBKIT_DIR}
            VERBATIM)

        list(APPEND ${_output_source} ${WebKit_DERIVED_SOURCES_DIR}/${_name}MessageReceiver.cpp)
    endforeach ()
endmacro()
GENERATE_MESSAGE_SOURCES(WebKit_DERIVED_SOURCES "${WebKit_MESSAGES_IN_FILES}")

set(WebKit_AUTOMATION_PROTOCOL_GENERATOR_SCRIPTS
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/generate-inspector-protocol-bindings.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/cpp_generator.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/cpp_generator_templates.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_backend_dispatcher_header.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_backend_dispatcher_implementation.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_frontend_dispatcher_header.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_frontend_dispatcher_implementation.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_protocol_types_header.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generate_cpp_protocol_types_implementation.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generator.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/generator_templates.py
    ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/codegen/models.py
)

set(WebKit_AUTOMATION_PROTOCOL_GENERATOR_INPUTS
    ${WEBKIT_DIR}/UIProcess/Automation/Automation.json
)

add_custom_command(
    OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/AutomationBackendDispatchers.h ${WebKit_DERIVED_SOURCES_DIR}/AutomationBackendDispatchers.cpp ${WebKit_DERIVED_SOURCES_DIR}/AutomationProtocolObjects.h ${WebKit_DERIVED_SOURCES_DIR}/AutomationProtocolObjects.cpp ${WebKit_DERIVED_SOURCES_DIR}/AutomationFrontendDispatchers.h ${WebKit_DERIVED_SOURCES_DIR}/AutomationFrontendDispatchers.cpp
    MAIN_DEPENDENCY ${WebKit_AUTOMATION_PROTOCOL_GENERATOR_INPUTS}
    DEPENDS ${WebKit_AUTOMATION_PROTOCOL_GENERATOR_SCRIPTS}
    COMMAND ${PYTHON_EXECUTABLE} ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/generate-inspector-protocol-bindings.py --outputDir "${WebKit_DERIVED_SOURCES_DIR}" --framework WebKit ${WebKit_AUTOMATION_PROTOCOL_GENERATOR_EXTRA_FLAGS} --backend ${WebKit_AUTOMATION_PROTOCOL_GENERATOR_INPUTS}
    VERBATIM)

list(APPEND WebKit_HEADERS
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationBackendDispatchers.h
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationFrontendDispatchers.h
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationProtocolObjects.h
)

list(APPEND WebKit_SOURCES
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationBackendDispatchers.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationFrontendDispatchers.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationProtocolObjects.cpp
)

add_custom_command(
    OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/WebAutomationSessionProxyScriptSource.h ${WebKit_DERIVED_SOURCES_DIR}/WebAutomationSessionProxy.min.js
    MAIN_DEPENDENCY ${WEBKIT_DIR}/WebProcess/Automation/WebAutomationSessionProxy.js
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
    COMMAND ${CMAKE_COMMAND} -E echo "//# sourceURL=__InjectedScript_WebAutomationSessionProxy.js" > ${WebKit_DERIVED_SOURCES_DIR}/WebAutomationSessionProxy.min.js
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py < ${WEBKIT_DIR}/WebProcess/Automation/WebAutomationSessionProxy.js >> ${WebKit_DERIVED_SOURCES_DIR}/WebAutomationSessionProxy.min.js
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl WebAutomationSessionProxyScriptSource ${WebKit_DERIVED_SOURCES_DIR}/WebAutomationSessionProxy.min.js ${WebKit_DERIVED_SOURCES_DIR}/WebAutomationSessionProxyScriptSource.h
    VERBATIM)

list(APPEND WebKit_HEADERS
    ${WebKit_DERIVED_SOURCES_DIR}/WebAutomationSessionProxyScriptSource.h
)

set(WebKit_WEB_PREFERENCES_TEMPLATES
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPageUpdatePreferences.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesDefinitions.h.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesExperimentalFeatures.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesInternalDebugFeatures.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesKeys.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesKeys.h.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesStoreDefaultsMap.cpp.erb
)

add_custom_command(
    OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/WebPageUpdatePreferences.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesDefinitions.h ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesExperimentalFeatures.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesInternalDebugFeatures.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.h ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesStoreDefaultsMap.cpp
    MAIN_DEPENDENCY ${WEBKIT_DIR}/Shared/WebPreferences.yaml
    DEPENDS ${WebKit_WEB_PREFERENCES_TEMPLATES}
    COMMAND ${RUBY_EXECUTABLE} ${WEBKIT_DIR}/Scripts/GeneratePreferences.rb --input ${WEBKIT_DIR}/Shared/WebPreferences.yaml --outputDir "${WebKit_DERIVED_SOURCES_DIR}"
    VERBATIM)

list(APPEND WebKit_HEADERS
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesDefinitions.h
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.h
)

list(APPEND WebKit_SOURCES
    ${WebKit_DERIVED_SOURCES_DIR}/WebPageUpdatePreferences.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesExperimentalFeatures.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesInternalDebugFeatures.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesStoreDefaultsMap.cpp
)


list(APPEND WebKit_SOURCES ${WebKit_DERIVED_SOURCES})

WEBKIT_COMPUTE_SOURCES(WebKit)

WEBKIT_FRAMEWORK(WebKit)
add_dependencies(WebKit WebCore ${WEBKIT_EXTRA_DEPENDENCIES})
ADD_WEBKIT_PREFIX_HEADER(WebKit)

add_executable(WebProcess ${WebProcess_SOURCES})
ADD_WEBKIT_PREFIX_HEADER(WebProcess)
target_link_libraries(WebProcess ${WebProcess_LIBRARIES})

if (WebKit_WebProcess_OUTPUT_NAME)
    set_target_properties(WebProcess PROPERTIES OUTPUT_NAME ${WebKit_WebProcess_OUTPUT_NAME})
endif ()

add_executable(NetworkProcess ${NetworkProcess_SOURCES})
ADD_WEBKIT_PREFIX_HEADER(NetworkProcess)
target_link_libraries(NetworkProcess ${NetworkProcess_LIBRARIES})

if (WebKit_NetworkProcess_OUTPUT_NAME)
    set_target_properties(NetworkProcess PROPERTIES OUTPUT_NAME ${WebKit_NetworkProcess_OUTPUT_NAME})
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKit -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebProcess -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(NetworkProcess -Wno-unused-parameter)
endif ()

# FIXME: Mac should build a plugin process.
if (ENABLE_PLUGIN_PROCESS AND NOT "${PORT}" STREQUAL "Mac")
    add_definitions(-DENABLE_PLUGIN_PROCESS=1)
    add_executable(PluginProcess ${PluginProcess_SOURCES})
    ADD_WEBKIT_PREFIX_HEADER(PluginProcess)
    target_link_libraries(PluginProcess ${PluginProcess_LIBRARIES})
    install(TARGETS PluginProcess DESTINATION "${LIBEXEC_INSTALL_DIR}")

    if (COMPILER_IS_GCC_OR_CLANG)
        WEBKIT_ADD_TARGET_CXX_FLAGS(PluginProcess -Wno-unused-parameter)
    endif ()

    if (WebKit_PluginProcess_OUTPUT_NAME)
      set_target_properties(PluginProcess PROPERTIES OUTPUT_NAME ${WebKit_PluginProcess_OUTPUT_NAME})
    endif ()
endif ()

if ("${PORT}" STREQUAL "Mac")
    WEBKIT_DEFINE_XPC_SERVICES()
else ()
    WEBKIT_POPULATE_LIBRARY_VERSION(WEBKIT)

    set_target_properties(WebKit PROPERTIES VERSION ${WEBKIT_VERSION} SOVERSION ${WEBKIT_VERSION_MAJOR})

    install(TARGETS WebKit WebProcess NetworkProcess
        LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
        RUNTIME DESTINATION "${LIBEXEC_INSTALL_DIR}"
    )
endif ()
