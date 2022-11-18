include(WebKitCommon)

set_property(DIRECTORY . PROPERTY FOLDER "WebKit")

set(WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
)

set(WebKit_INCLUDE_DIRECTORIES
    "${PAL_FRAMEWORK_HEADERS_DIR}"
    "${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}"
    "${WebKit_FRAMEWORK_HEADERS_DIR}"
    "${WebKit_DERIVED_SOURCES_DIR}"
    "${WebKit_DERIVED_SOURCES_DIR}/include"
    "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/GPUProcess"
    "${WEBKIT_DIR}/GPUProcess/graphics"
    "${WEBKIT_DIR}/GPUProcess/graphics/WebGPU"
    "${WEBKIT_DIR}/GPUProcess/media"
    "${WEBKIT_DIR}/GPUProcess/webrtc"
    "${WEBKIT_DIR}/NetworkProcess"
    "${WEBKIT_DIR}/NetworkProcess/Classifier"
    "${WEBKIT_DIR}/NetworkProcess/Cookies"
    "${WEBKIT_DIR}/NetworkProcess/CustomProtocols"
    "${WEBKIT_DIR}/NetworkProcess/Downloads"
    "${WEBKIT_DIR}/NetworkProcess/FileAPI"
    "${WEBKIT_DIR}/NetworkProcess/IndexedDB"
    "${WEBKIT_DIR}/NetworkProcess/Notifications"
    "${WEBKIT_DIR}/NetworkProcess/PrivateClickMeasurement"
    "${WEBKIT_DIR}/NetworkProcess/ServiceWorker"
    "${WEBKIT_DIR}/NetworkProcess/SharedWorker"
    "${WEBKIT_DIR}/NetworkProcess/cache"
    "${WEBKIT_DIR}/NetworkProcess/storage"
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
    "${WEBKIT_DIR}/Shared/Notifications"
    "${WEBKIT_DIR}/Shared/Plugins"
    "${WEBKIT_DIR}/Shared/Plugins/Netscape"
    "${WEBKIT_DIR}/Shared/Plugins/Netscape/x11"
    "${WEBKIT_DIR}/Shared/RemoteLayerTree"
    "${WEBKIT_DIR}/Shared/WebGPU"
    "${WEBKIT_DIR}/Shared/WebsiteData"
    "${WEBKIT_DIR}/Shared/XR"
    "${WEBKIT_DIR}/UIProcess"
    "${WEBKIT_DIR}/UIProcess/API"
    "${WEBKIT_DIR}/UIProcess/API/C"
    "${WEBKIT_DIR}/UIProcess/API/cpp"
    "${WEBKIT_DIR}/UIProcess/Authentication"
    "${WEBKIT_DIR}/UIProcess/Automation"
    "${WEBKIT_DIR}/UIProcess/Downloads"
    "${WEBKIT_DIR}/UIProcess/Gamepad"
    "${WEBKIT_DIR}/UIProcess/GPU"
    "${WEBKIT_DIR}/UIProcess/Inspector"
    "${WEBKIT_DIR}/UIProcess/Inspector/Agents"
    "${WEBKIT_DIR}/UIProcess/Launcher"
    "${WEBKIT_DIR}/UIProcess/Media"
    "${WEBKIT_DIR}/UIProcess/Network"
    "${WEBKIT_DIR}/UIProcess/Network/CustomProtocols"
    "${WEBKIT_DIR}/UIProcess/Notifications"
    "${WEBKIT_DIR}/UIProcess/Plugins"
    "${WEBKIT_DIR}/UIProcess/RemoteLayerTree"
    "${WEBKIT_DIR}/UIProcess/UserContent"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication"
    "${WEBKIT_DIR}/UIProcess/WebAuthentication/Mock"
    "${WEBKIT_DIR}/UIProcess/WebsiteData"
    "${WEBKIT_DIR}/UIProcess/XR"
    "${WEBKIT_DIR}/WebAuthnProcess"
    "${WEBKIT_DIR}/WebProcess"
    "${WEBKIT_DIR}/WebProcess/ApplePay"
    "${WEBKIT_DIR}/WebProcess/ApplicationCache"
    "${WEBKIT_DIR}/WebProcess/Automation"
    "${WEBKIT_DIR}/WebProcess/Cache"
    "${WEBKIT_DIR}/WebProcess/Databases"
    "${WEBKIT_DIR}/WebProcess/Databases/IndexedDB"
    "${WEBKIT_DIR}/WebProcess/EncryptedMedia"
    "${WEBKIT_DIR}/WebProcess/FileAPI"
    "${WEBKIT_DIR}/WebProcess/FullScreen"
    "${WEBKIT_DIR}/WebProcess/Gamepad"
    "${WEBKIT_DIR}/WebProcess/Geolocation"
    "${WEBKIT_DIR}/WebProcess/GPU"
    "${WEBKIT_DIR}/WebProcess/GPU/graphics"
    "${WEBKIT_DIR}/WebProcess/GPU/graphics/WebGPU"
    "${WEBKIT_DIR}/WebProcess/GPU/media"
    "${WEBKIT_DIR}/WebProcess/GPU/webrtc"
    "${WEBKIT_DIR}/WebProcess/IconDatabase"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/API/c"
    "${WEBKIT_DIR}/WebProcess/InjectedBundle/DOM"
    "${WEBKIT_DIR}/WebProcess/Inspector"
    "${WEBKIT_DIR}/WebProcess/Launching"
    "${WEBKIT_DIR}/WebProcess/MediaCache"
    "${WEBKIT_DIR}/WebProcess/MediaStream"
    "${WEBKIT_DIR}/WebProcess/Model"
    "${WEBKIT_DIR}/WebProcess/Network"
    "${WEBKIT_DIR}/WebProcess/Network/webrtc"
    "${WEBKIT_DIR}/WebProcess/Notifications"
    "${WEBKIT_DIR}/WebProcess/OriginData"
    "${WEBKIT_DIR}/WebProcess/Plugins"
    "${WEBKIT_DIR}/WebProcess/Plugins/Netscape"
    "${WEBKIT_DIR}/WebProcess/ResourceCache"
    "${WEBKIT_DIR}/WebProcess/Speech"
    "${WEBKIT_DIR}/WebProcess/Storage"
    "${WEBKIT_DIR}/WebProcess/UserContent"
    "${WEBKIT_DIR}/WebProcess/WebCoreSupport"
    "${WEBKIT_DIR}/WebProcess/WebPage"
    "${WEBKIT_DIR}/WebProcess/WebPage/RemoteLayerTree"
    "${WEBKIT_DIR}/WebProcess/WebStorage"
    "${WEBKIT_DIR}/WebProcess/XR"
    "${CMAKE_SOURCE_DIR}/Source"
    "${CMAKE_SOURCE_DIR}/WebKitLibraries"
)

set(PROTOCOL_GENERATOR_SCRIPTS_DIR "${JAVASCRIPTCORE_DIR}/inspector/scripts")

list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES
    "Sources.txt"

    "Platform/Sources.txt"
)

set(WebKit_MESSAGES_IN_FILES
    GPUProcess/GPUConnectionToWebProcess
    GPUProcess/GPUProcess

    GPUProcess/graphics/RemoteDisplayListRecorder
    GPUProcess/graphics/RemoteGraphicsContextGL
    GPUProcess/graphics/RemoteRenderingBackend

    GPUProcess/graphics/WebGPU/RemoteAdapter
    GPUProcess/graphics/WebGPU/RemoteBindGroup
    GPUProcess/graphics/WebGPU/RemoteBindGroupLayout
    GPUProcess/graphics/WebGPU/RemoteBuffer
    GPUProcess/graphics/WebGPU/RemoteCommandBuffer
    GPUProcess/graphics/WebGPU/RemoteCommandEncoder
    GPUProcess/graphics/WebGPU/RemoteComputePassEncoder
    GPUProcess/graphics/WebGPU/RemoteComputePipeline
    GPUProcess/graphics/WebGPU/RemoteDevice
    GPUProcess/graphics/WebGPU/RemoteExternalTexture
    GPUProcess/graphics/WebGPU/RemoteGPU
    GPUProcess/graphics/WebGPU/RemotePipelineLayout
    GPUProcess/graphics/WebGPU/RemoteQuerySet
    GPUProcess/graphics/WebGPU/RemoteQueue
    GPUProcess/graphics/WebGPU/RemoteRenderBundle
    GPUProcess/graphics/WebGPU/RemoteRenderBundleEncoder
    GPUProcess/graphics/WebGPU/RemoteRenderPassEncoder
    GPUProcess/graphics/WebGPU/RemoteRenderPipeline
    GPUProcess/graphics/WebGPU/RemoteSampler
    GPUProcess/graphics/WebGPU/RemoteShaderModule
    GPUProcess/graphics/WebGPU/RemoteSurface
    GPUProcess/graphics/WebGPU/RemoteSwapChain
    GPUProcess/graphics/WebGPU/RemoteTexture
    GPUProcess/graphics/WebGPU/RemoteTextureView

    GPUProcess/media/RemoteAudioDestinationManager
    GPUProcess/media/RemoteAudioSessionProxy
    GPUProcess/media/RemoteCDMFactoryProxy
    GPUProcess/media/RemoteCDMInstanceProxy
    GPUProcess/media/RemoteCDMInstanceSessionProxy
    GPUProcess/media/RemoteCDMProxy
    GPUProcess/media/RemoteLegacyCDMFactoryProxy
    GPUProcess/media/RemoteLegacyCDMProxy
    GPUProcess/media/RemoteLegacyCDMSessionProxy
    GPUProcess/media/RemoteMediaEngineConfigurationFactoryProxy
    GPUProcess/media/RemoteMediaPlayerManagerProxy
    GPUProcess/media/RemoteMediaPlayerProxy
    GPUProcess/media/RemoteMediaResourceManager
    GPUProcess/media/RemoteMediaSourceProxy
    GPUProcess/media/RemoteRemoteCommandListenerProxy
    GPUProcess/media/RemoteSourceBufferProxy
    GPUProcess/media/RemoteVideoFrameObjectHeap

    GPUProcess/webrtc/LibWebRTCCodecsProxy
    GPUProcess/webrtc/RemoteAudioMediaStreamTrackRendererInternalUnitManager
    GPUProcess/webrtc/RemoteMediaRecorder
    GPUProcess/webrtc/RemoteMediaRecorderManager
    GPUProcess/webrtc/RemoteSampleBufferDisplayLayer
    GPUProcess/webrtc/RemoteSampleBufferDisplayLayerManager

    NetworkProcess/Cookies/WebCookieManager

    NetworkProcess/NetworkBroadcastChannelRegistry
    NetworkProcess/NetworkConnectionToWebProcess
    NetworkProcess/NetworkContentRuleListManager
    NetworkProcess/NetworkProcess
    NetworkProcess/NetworkResourceLoader
    NetworkProcess/NetworkSocketChannel
    NetworkProcess/NetworkSocketStream

    NetworkProcess/ServiceWorker/ServiceWorkerDownloadTask
    NetworkProcess/ServiceWorker/ServiceWorkerFetchTask
    NetworkProcess/ServiceWorker/WebSWServerConnection
    NetworkProcess/ServiceWorker/WebSWServerToContextConnection

    NetworkProcess/SharedWorker/WebSharedWorkerServerConnection
    NetworkProcess/SharedWorker/WebSharedWorkerServerToContextConnection

    NetworkProcess/cache/CacheStorageEngineConnection

    NetworkProcess/storage/NetworkStorageManager

    NetworkProcess/webrtc/NetworkMDNSRegister
    NetworkProcess/webrtc/NetworkRTCMonitor
    NetworkProcess/webrtc/NetworkRTCProvider
    NetworkProcess/webrtc/RTCDataChannelRemoteManagerProxy

    Shared/AuxiliaryProcess
    Shared/IPCConnectionTester
    Shared/IPCStreamTester
    Shared/IPCStreamTesterProxy
    Shared/IPCTester
    Shared/IPCTesterReceiver
    Shared/WebConnection

    Shared/Authentication/AuthenticationManager

    Shared/Notifications/NotificationManagerMessageHandler

    UIProcess/DrawingAreaProxy

    UIProcess/Media/RemoteMediaSessionCoordinatorProxy

    UIProcess/SpeechRecognitionRemoteRealtimeMediaSourceManager
    UIProcess/SpeechRecognitionServer
    UIProcess/VisitedLinkStore
    UIProcess/WebFrameProxy
    UIProcess/WebFullScreenManagerProxy
    UIProcess/WebGeolocationManagerProxy
    UIProcess/WebLockRegistryProxy
    UIProcess/WebPageProxy
    UIProcess/WebPasteboardProxy
    UIProcess/WebPermissionControllerProxy
    UIProcess/WebProcessPool
    UIProcess/WebProcessProxy
    UIProcess/WebScreenOrientationManagerProxy

    UIProcess/Automation/WebAutomationSession

    UIProcess/Downloads/DownloadProxy

    UIProcess/GPU/GPUProcessProxy

    UIProcess/Inspector/RemoteWebInspectorUIProxy
    UIProcess/Inspector/WebInspectorUIProxy

    UIProcess/Network/NetworkProcessProxy

    UIProcess/UserContent/WebUserContentControllerProxy

    UIProcess/XR/PlatformXRSystem

    WebProcess/WebProcess

    WebProcess/Automation/WebAutomationSessionProxy

    WebProcess/Databases/IndexedDB/WebIDBConnectionToServer

    WebProcess/FullScreen/WebFullScreenManager

    WebProcess/GPU/GPUProcessConnection

    WebProcess/GPU/graphics/RemoteGraphicsContextGLProxy
    WebProcess/GPU/graphics/RemoteRenderingBackendProxy

    WebProcess/GPU/graphics/WebGPU/RemoteGPUProxy

    WebProcess/GPU/media/RemoteAudioHardwareListener
    WebProcess/GPU/media/MediaPlayerPrivateRemote
    WebProcess/GPU/media/MediaSourcePrivateRemote
    WebProcess/GPU/media/RemoteAudioSession
    WebProcess/GPU/media/RemoteAudioSourceProviderManager
    WebProcess/GPU/media/RemoteCDMInstance
    WebProcess/GPU/media/RemoteCDMInstanceSession
    WebProcess/GPU/media/RemoteLegacyCDMSession
    WebProcess/GPU/media/RemoteRemoteCommandListener
    WebProcess/GPU/media/SourceBufferPrivateRemote

    WebProcess/GPU/webrtc/LibWebRTCCodecs
    WebProcess/GPU/webrtc/RemoteVideoFrameObjectHeapProxyProcessor
    WebProcess/GPU/webrtc/SampleBufferDisplayLayer

    WebProcess/Geolocation/WebGeolocationManager

    WebProcess/Inspector/RemoteWebInspectorUI
    WebProcess/Inspector/WebInspector
    WebProcess/Inspector/WebInspectorInterruptDispatcher
    WebProcess/Inspector/WebInspectorUI

    WebProcess/MediaSession/RemoteMediaSessionCoordinator

    WebProcess/Network/webrtc/LibWebRTCNetwork
    WebProcess/Network/webrtc/RTCDataChannelRemoteManager
    WebProcess/Network/webrtc/WebMDNSRegister
    WebProcess/Network/webrtc/WebRTCMonitor
    WebProcess/Network/webrtc/WebRTCResolver

    WebProcess/Network/NetworkProcessConnection
    WebProcess/Network/WebResourceLoader
    WebProcess/Network/WebSocketChannel
    WebProcess/Network/WebSocketStream

    WebProcess/Notifications/WebNotificationManager

    WebProcess/Speech/SpeechRecognitionRealtimeMediaSourceManager

    WebProcess/Storage/WebSharedWorkerContextManagerConnection
    WebProcess/Storage/WebSharedWorkerObjectConnection
    WebProcess/Storage/WebSWClientConnection
    WebProcess/Storage/WebSWContextManagerConnection

    WebProcess/UserContent/WebUserContentController

    WebProcess/WebCoreSupport/RemoteWebLockRegistry
    WebProcess/WebCoreSupport/WebBroadcastChannelRegistry
    WebProcess/WebCoreSupport/WebFileSystemStorageConnection
    WebProcess/WebCoreSupport/WebPermissionController
    WebProcess/WebCoreSupport/WebScreenOrientationManager
    WebProcess/WebCoreSupport/WebSpeechRecognitionConnection

    WebProcess/WebPage/DrawingArea
    WebProcess/WebPage/EventDispatcher
    WebProcess/WebPage/VisitedLinkTableController
    WebProcess/WebPage/WebFrame
    WebProcess/WebPage/WebPage

    WebProcess/WebStorage/StorageAreaMap

    WebProcess/XR/PlatformXRSystemProxy
)

set(WebKit_FRAMEWORKS
    JavaScriptCore
    PAL
    WTF
    WebCore
)
if (NOT USE_SYSTEM_MALLOC)
    list(APPEND WebKit_FRAMEWORKS bmalloc)
endif ()

set(WebProcess_LIBRARIES WebKit)
set(NetworkProcess_LIBRARIES WebKit)
set(GPUProcess_LIBRARIES WebKit)
set(PluginProcess_LIBRARIES WebKit)
set(WebAuthnProcess_LIBRARIES WebKit)

if (ENABLE_ENCRYPTED_MEDIA AND ENABLE_OPENCDM)
  list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
      ${OPENCDM_INCLUDE_DIRS}
  )
endif ()

# For platforms that want to use system-provided OpenGL (ES) / EGL headers,
# these include directories, libraries or definitions need to be
# added before the ANGLE directories.
if (USE_LIBEPOXY)
    list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
        ${LIBEPOXY_INCLUDE_DIRS}
    )
    list(APPEND WebKit_PRIVATE_LIBRARIES
        ${LIBEPOXY_LIBRARIES}
    )
    add_definitions(${LIBEPOXY_DEFINITIONS})
elseif (USE_ANGLE_EGL)
    # Prepend to make sure the ANGLE headers are found before system headers
    list(PREPEND WebKit_PRIVATE_LIBRARIES
        ANGLE::GLES
        ANGLE::EGL
    )
else ()
    if (USE_EGL)
        list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
            ${EGL_INCLUDE_DIRS}
        )
        list(APPEND WebKit_PRIVATE_LIBRARIES
            ${EGL_LIBRARIES}
        )
        add_definitions(${EGL_DEFINITIONS})
    endif ()
endif ()

if (USE_LIBWEBRTC)
    list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES "${THIRDPARTY_DIR}/libwebrtc/Source/"
        "${THIRDPARTY_DIR}/libwebrtc/Source/webrtc"
        "${THIRDPARTY_DIR}/libwebrtc/Source/third_party/abseil-cpp")
    list(APPEND WebKit_LIBRARIES webrtc)
endif ()

if (ATOMICS_REQUIRE_LIBATOMIC)
    list(APPEND WebKit_PRIVATE_LIBRARIES atomic)
endif ()

if (UNIX)
    check_function_exists(shm_open SHM_OPEN_EXISTS)
    if (NOT SHM_OPEN_EXISTS)
        set(CMAKE_REQUIRED_LIBRARIES rt)
        check_function_exists(shm_open SHM_OPEN_REQUIRES_LIBRT)
        if (SHM_OPEN_REQUIRES_LIBRT)
            list(APPEND WebKit_PRIVATE_LIBRARIES rt)
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

set(WebKit_AUTOMATION_PROTOCOL_GENERATOR_EXTRA_FLAGS)

set(WebKit_INTERFACE_LIBRARIES WebKit)
set(WebKit_INTERFACE_INCLUDE_DIRECTORIES
    ${WebKit_FRAMEWORK_HEADERS_DIR}
    ${WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR}
)
set(WebKit_INTERFACE_DEPENDENCIES
    NetworkProcess
    WebProcess
)

WEBKIT_EXECUTABLE_DECLARE(WebProcess)
WEBKIT_EXECUTABLE_DECLARE(NetworkProcess)

if (ENABLE_GPU_PROCESS)
    WEBKIT_EXECUTABLE_DECLARE(GPUProcess)
    list(APPEND WebKit_INTERFACE_DEPENDENCIES GPUProcess)
endif ()

if (ENABLE_PLUGIN_PROCESS)
    WEBKIT_EXECUTABLE_DECLARE(PluginProcess)
    list(APPEND WebKit_INTERFACE_DEPENDENCIES PluginProcess)
endif ()

if (ENABLE_WEB_AUTHN)
    WEBKIT_EXECUTABLE_DECLARE(WebAuthnProcess)
    list(APPEND WebKit_INTERFACE_DEPENDENCIES WebAuthnProcess)
endif ()

WEBKIT_FRAMEWORK_DECLARE(WebKit)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

if (WebKit_PUBLIC_FRAMEWORK_HEADERS)
    WEBKIT_COPY_FILES(WebKit_CopyHeaders
        DESTINATION ${WebKit_FRAMEWORK_HEADERS_DIR}/WebKit
        FILES ${WebKit_PUBLIC_FRAMEWORK_HEADERS}
        FLATTENED
    )
    list(APPEND WebKit_DEPENDENCIES WebKit_CopyHeaders)
endif ()

if (WebKit_PRIVATE_FRAMEWORK_HEADERS)
    WEBKIT_COPY_FILES(WebKit_CopyPrivateHeaders
        DESTINATION ${WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebKit
        FILES ${WebKit_PRIVATE_FRAMEWORK_HEADERS}
        FLATTENED
    )
    list(APPEND WebKit_INTERFACE_DEPENDENCIES WebKit_CopyPrivateHeaders)
endif ()

# Helper macro which wraps the generate-message-receiver.py script
#   _output_source is a list name which will contain generated sources.(eg. WebKit_SOURCES)
#   _inputs are messages.in files to generate.
macro(GENERATE_MESSAGE_SOURCES _output_source _inputs)
    unset(_input_files)
    unset(_outputs)
    foreach (_file IN ITEMS ${_inputs})
        get_filename_component(_name ${_file} NAME_WE)
        list(APPEND _input_files ${WEBKIT_DIR}/${_file}.messages.in)
        list(APPEND _outputs
            ${WebKit_DERIVED_SOURCES_DIR}/${_name}MessageReceiver.cpp
            ${WebKit_DERIVED_SOURCES_DIR}/${_name}Messages.h
            ${WebKit_DERIVED_SOURCES_DIR}/${_name}MessagesReplies.h
        )
        list(APPEND ${_output_source} ${WebKit_DERIVED_SOURCES_DIR}/${_name}MessageReceiver.cpp)
    endforeach ()

    list(APPEND ${_output_source}
        ${WebKit_DERIVED_SOURCES_DIR}/MessageArgumentDescriptions.cpp
        ${WebKit_DERIVED_SOURCES_DIR}/MessageNames.cpp
    )

    add_custom_command(
        OUTPUT
            ${WebKit_DERIVED_SOURCES_DIR}/MessageArgumentDescriptions.cpp
            ${WebKit_DERIVED_SOURCES_DIR}/MessageNames.cpp
            ${WebKit_DERIVED_SOURCES_DIR}/MessageNames.h
            ${_outputs}
        MAIN_DEPENDENCY ${WEBKIT_DIR}/Scripts/generate-message-receiver.py
        DEPENDS
            ${WEBKIT_DIR}/Scripts/webkit/__init__.py
            ${WEBKIT_DIR}/Scripts/webkit/messages.py
            ${WEBKIT_DIR}/Scripts/webkit/model.py
            ${WEBKIT_DIR}/Scripts/webkit/parser.py
            ${_input_files}
        COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-message-receiver.py ${WEBKIT_DIR} ${_inputs}
        WORKING_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR}
        VERBATIM
    )
endmacro()
GENERATE_MESSAGE_SOURCES(WebKit_DERIVED_SOURCES "${WebKit_MESSAGES_IN_FILES}")

set(WebKit_SERIALIZATION_IN_FILES
    GPUProcess/GPUProcessSessionParameters.serialization.in

    GPUProcess/graphics/RemoteRenderingBackendCreationParameters.serialization.in

    GPUProcess/media/InitializationSegmentInfo.serialization.in
    GPUProcess/media/MediaDescriptionInfo.serialization.in
    GPUProcess/media/TextTrackPrivateRemoteConfiguration.serialization.in

    NetworkProcess/NetworkProcessCreationParameters.serialization.in

    Shared/EditorState.serialization.in
    Shared/FocusedElementInformation.serialization.in
    Shared/FrameInfoData.serialization.in
    Shared/FrameTreeNodeData.serialization.in
    Shared/LayerTreeContext.serialization.in
    Shared/Model.serialization.in
    Shared/Pasteboard.serialization.in
    Shared/SessionState.serialization.in
    Shared/ShareableBitmap.serialization.in
    Shared/TextFlags.serialization.in
    Shared/WTFArgumentCoders.serialization.in
    Shared/WebCoreArgumentCoders.serialization.in
    Shared/WebEvent.serialization.in
    Shared/WebPushDaemonConnectionConfiguration.serialization.in
    Shared/WebPushMessage.serialization.in
    Shared/WebsiteDataStoreParameters.serialization.in

    Shared/API/APIGeometry.serialization.in

    Shared/WebGPU/WebGPUBindGroupDescriptor.serialization.in
    Shared/WebGPU/WebGPUBindGroupEntry.serialization.in
    Shared/WebGPU/WebGPUBindGroupLayoutDescriptor.serialization.in
    Shared/WebGPU/WebGPUBindGroupLayoutEntry.serialization.in
    Shared/WebGPU/WebGPUBlendComponent.serialization.in
    Shared/WebGPU/WebGPUBlendState.serialization.in
    Shared/WebGPU/WebGPUBufferBinding.serialization.in
    Shared/WebGPU/WebGPUBufferBindingLayout.serialization.in
    Shared/WebGPU/WebGPUBufferDescriptor.serialization.in
    Shared/WebGPU/WebGPUCanvasConfiguration.serialization.in
    Shared/WebGPU/WebGPUColor.serialization.in
    Shared/WebGPU/WebGPUColorTargetState.serialization.in
    Shared/WebGPU/WebGPUCommandBufferDescriptor.serialization.in
    Shared/WebGPU/WebGPUCommandEncoderDescriptor.serialization.in
    Shared/WebGPU/WebGPUCompilationMessage.serialization.in
    Shared/WebGPU/WebGPUComputePassDescriptor.serialization.in
    Shared/WebGPU/WebGPUComputePassTimestampWrites.serialization.in
    Shared/WebGPU/WebGPUComputePipelineDescriptor.serialization.in
    Shared/WebGPU/WebGPUDepthStencilState.serialization.in
    Shared/WebGPU/WebGPUDeviceDescriptor.serialization.in
    Shared/WebGPU/WebGPUExtent3D.serialization.in
    Shared/WebGPU/WebGPUExternalTextureBindingLayout.serialization.in
    Shared/WebGPU/WebGPUExternalTextureDescriptor.serialization.in
    Shared/WebGPU/WebGPUFeatureName.serialization.in
    Shared/WebGPU/WebGPUFragmentState.serialization.in
    Shared/WebGPU/WebGPUImageCopyBuffer.serialization.in
    Shared/WebGPU/WebGPUImageCopyExternalImage.serialization.in
    Shared/WebGPU/WebGPUImageCopyTexture.serialization.in
    Shared/WebGPU/WebGPUImageCopyTextureTagged.serialization.in
    Shared/WebGPU/WebGPUImageDataLayout.serialization.in
    Shared/WebGPU/WebGPUMultisampleState.serialization.in
    Shared/WebGPU/WebGPUObjectDescriptorBase.serialization.in
    Shared/WebGPU/WebGPUOrigin2D.serialization.in
    Shared/WebGPU/WebGPUOrigin3D.serialization.in
    Shared/WebGPU/WebGPUOutOfMemoryError.serialization.in
    Shared/WebGPU/WebGPUPipelineDescriptorBase.serialization.in
    Shared/WebGPU/WebGPUPipelineLayoutDescriptor.serialization.in
    Shared/WebGPU/WebGPUPrimitiveState.serialization.in
    Shared/WebGPU/WebGPUProgrammableStage.serialization.in
    Shared/WebGPU/WebGPUQuerySetDescriptor.serialization.in
    Shared/WebGPU/WebGPURenderBundleDescriptor.serialization.in
    Shared/WebGPU/WebGPURenderBundleEncoderDescriptor.serialization.in
    Shared/WebGPU/WebGPURenderPassColorAttachment.serialization.in
    Shared/WebGPU/WebGPURenderPassDepthStencilAttachment.serialization.in
    Shared/WebGPU/WebGPURenderPassDescriptor.serialization.in
    Shared/WebGPU/WebGPURenderPassLayout.serialization.in
    Shared/WebGPU/WebGPURenderPassTimestampWrites.serialization.in
    Shared/WebGPU/WebGPURenderPipelineDescriptor.serialization.in
    Shared/WebGPU/WebGPURequestAdapterOptions.serialization.in
    Shared/WebGPU/WebGPUSamplerBindingLayout.serialization.in
    Shared/WebGPU/WebGPUSamplerDescriptor.serialization.in
    Shared/WebGPU/WebGPUShaderModuleCompilationHint.serialization.in
    Shared/WebGPU/WebGPUShaderModuleDescriptor.serialization.in
    Shared/WebGPU/WebGPUStencilFaceState.serialization.in
    Shared/WebGPU/WebGPUStorageTextureBindingLayout.serialization.in
    Shared/WebGPU/WebGPUSupportedFeatures.serialization.in
    Shared/WebGPU/WebGPUSupportedLimits.serialization.in
    Shared/WebGPU/WebGPUSurfaceDescriptor.serialization.in
    Shared/WebGPU/WebGPUSwapChainDescriptor.serialization.in
    Shared/WebGPU/WebGPUTextureBindingLayout.serialization.in
    Shared/WebGPU/WebGPUTextureDescriptor.serialization.in
    Shared/WebGPU/WebGPUTextureViewDescriptor.serialization.in
    Shared/WebGPU/WebGPUValidationError.serialization.in
    Shared/WebGPU/WebGPUVertexAttribute.serialization.in
    Shared/WebGPU/WebGPUVertexBufferLayout.serialization.in
    Shared/WebGPU/WebGPUVertexState.serialization.in

    Shared/WebsiteData/WebsiteDataFetchOption.serialization.in

    Shared/XR/XRSystem.serialization.in

    WebProcess/GPU/media/RemoteMediaPlayerState.serialization.in
)

list(APPEND WebKit_DERIVED_SOURCES
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializers.h
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializers.cpp
)

add_custom_command(
    OUTPUT
        ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializers.h
        ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializers.cpp
        ${WebKit_DERIVED_SOURCES_DIR}/SerializedTypeInfo.cpp
    MAIN_DEPENDENCY ${WEBKIT_DIR}/Scripts/generate-serializers.py
    DEPENDS
        ${WebKit_SERIALIZATION_IN_FILES}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-serializers.py ${WEBKIT_DIR}/ ${WebKit_SERIALIZATION_IN_FILES}
    WORKING_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR}
    VERBATIM
)

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
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesGetterSetters.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesInternalDebugFeatures.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesKeys.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesKeys.h.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesStoreDefaultsMap.cpp.erb
)

set(WebKit_WEB_PREFERENCES
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferences.yaml
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml
    ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml
)

set_source_files_properties(${WebKit_WEB_PREFERENCES} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/WebPageUpdatePreferences.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesDefinitions.h ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesExperimentalFeatures.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesGetterSetters.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesInternalDebugFeatures.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.h ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesStoreDefaultsMap.cpp
    DEPENDS ${WebKit_WEB_PREFERENCES_TEMPLATES} ${WebKit_WEB_PREFERENCES} WTF_CopyPreferences
    COMMAND ${RUBY_EXECUTABLE} ${WTF_SCRIPTS_DIR}/GeneratePreferences.rb --frontend WebKit --base ${WTF_SCRIPTS_DIR}/Preferences/WebPreferences.yaml --debug ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesDebug.yaml --experimental ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesExperimental.yaml --internal ${WTF_SCRIPTS_DIR}/Preferences/WebPreferencesInternal.yaml --outputDir "${WebKit_DERIVED_SOURCES_DIR}" --template ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPageUpdatePreferences.cpp.erb --template ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesDefinitions.h.erb --template ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesExperimentalFeatures.cpp.erb --template ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesGetterSetters.cpp.erb --template ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesInternalDebugFeatures.cpp.erb --template ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesKeys.cpp.erb --template ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesKeys.h.erb --template ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesStoreDefaultsMap.cpp.erb
    VERBATIM)

list(APPEND WebKit_HEADERS
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesDefinitions.h
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.h
)

list(APPEND WebKit_SOURCES
    ${WebKit_DERIVED_SOURCES_DIR}/WebPageUpdatePreferences.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesExperimentalFeatures.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesGetterSetters.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesInternalDebugFeatures.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesStoreDefaultsMap.cpp
)

list(APPEND WebKit_SOURCES ${WebKit_DERIVED_SOURCES})

WEBKIT_COMPUTE_SOURCES(WebKit)
WEBKIT_FRAMEWORK(WebKit)
WEBKIT_FRAMEWORK_TARGET(WebKit)
ADD_WEBKIT_PREFIX_HEADER(WebKit)

list(APPEND WebProcess_PRIVATE_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
WEBKIT_EXECUTABLE(WebProcess)
ADD_WEBKIT_PREFIX_HEADER(WebProcess)

list(APPEND NetworkProcess_PRIVATE_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
WEBKIT_EXECUTABLE(NetworkProcess)
ADD_WEBKIT_PREFIX_HEADER(NetworkProcess)

if (ENABLE_GPU_PROCESS)
    list(APPEND GPUProcess_PRIVATE_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
    WEBKIT_EXECUTABLE(GPUProcess)
    ADD_WEBKIT_PREFIX_HEADER(GPUProcess)
endif ()

if (ENABLE_PLUGIN_PROCESS)
    list(APPEND PluginProcess_PRIVATE_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
    WEBKIT_EXECUTABLE(PluginProcess)
    ADD_WEBKIT_PREFIX_HEADER(PluginProcess)
endif ()

if (ENABLE_WEB_AUTHN)
    list(APPEND WebAuthnProcess_PRIVATE_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
    WEBKIT_EXECUTABLE(WebAuthnProcess)
    ADD_WEBKIT_PREFIX_HEADER(WebAuthnProcess)
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKit -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebProcess -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(NetworkProcess -Wno-unused-parameter)
    if (ENABLE_GPU_PROCESS)
        WEBKIT_ADD_TARGET_CXX_FLAGS(GPUProcess -Wno-unused-parameter)
    endif ()
    if (ENABLE_PLUGIN_PROCESS)
        WEBKIT_ADD_TARGET_CXX_FLAGS(PluginProcess -Wno-unused-parameter)
    endif ()
    if (ENABLE_WEB_AUTHN)
        WEBKIT_ADD_TARGET_CXX_FLAGS(WebAuthnProcess -Wno-unused-parameter)
    endif ()
endif ()

if (APPLE)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKit -fobjc-weak)
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
