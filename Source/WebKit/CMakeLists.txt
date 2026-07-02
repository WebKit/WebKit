set_property(DIRECTORY . PROPERTY FOLDER "WebKit")

set(WebKit_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${PAL_FRAMEWORK_HEADERS_DIR}"
    "${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}"
    "${WebKit_FRAMEWORK_HEADERS_DIR}"
    "${WebKit_DERIVED_SOURCES_DIR}"
    "${WebKit_DERIVED_SOURCES_DIR}/include"
    "${WEBKIT_DIR}"
    "${WEBKIT_DIR}/GPUProcess"
    "${WEBKIT_DIR}/GPUProcess/ShapeDetection"
    "${WEBKIT_DIR}/GPUProcess/graphics"
    "${WEBKIT_DIR}/GPUProcess/graphics/Model"
    "${WEBKIT_DIR}/GPUProcess/graphics/WebGPU"
    "${WEBKIT_DIR}/GPUProcess/media"
    "${WEBKIT_DIR}/GPUProcess/webrtc"
    "${WEBKIT_DIR}/ModelProcess"
    "${WEBKIT_DIR}/NetworkProcess"
    "${WEBKIT_DIR}/NetworkProcess/Authentication"
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
    "${WEBKIT_DIR}/NetworkProcess/webtransport"
    "${WEBKIT_DIR}/Platform"
    "${WEBKIT_DIR}/Platform/IPC"
    "${WEBKIT_DIR}/Shared"
    "${WEBKIT_DIR}/Shared/API"
    "${WEBKIT_DIR}/Shared/API/c"
    "${WEBKIT_DIR}/Shared/ApplePay"
    "${WEBKIT_DIR}/Shared/Authentication"
    "${WEBKIT_DIR}/Shared/Automation"
    "${WEBKIT_DIR}/Shared/CoreIPCSupport"
    "${WEBKIT_DIR}/Shared/Databases"
    "${WEBKIT_DIR}/Shared/Databases/IndexedDB"
    "${WEBKIT_DIR}/Shared/Extensions"
    "${WEBKIT_DIR}/Shared/FileAPI"
    "${WEBKIT_DIR}/Shared/graphics"
    "${WEBKIT_DIR}/Shared/Gamepad"
    "${WEBKIT_DIR}/Shared/Model"
    "${WEBKIT_DIR}/Shared/Notifications"
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
    "${WEBKIT_DIR}/UIProcess/DigitalCredentials"
    "${WEBKIT_DIR}/UIProcess/Downloads"
    "${WEBKIT_DIR}/UIProcess/Extensions"
    "${WEBKIT_DIR}/UIProcess/Gamepad"
    "${WEBKIT_DIR}/UIProcess/GPU"
    "${WEBKIT_DIR}/UIProcess/Inspector"
    "${WEBKIT_DIR}/UIProcess/Inspector/Agents"
    "${WEBKIT_DIR}/UIProcess/Launcher"
    "${WEBKIT_DIR}/UIProcess/Media"
    "${WEBKIT_DIR}/UIProcess/Model"
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
    "${WEBKIT_DIR}/WebProcess"
    "${WEBKIT_DIR}/WebProcess/ApplePay"
    "${WEBKIT_DIR}/WebProcess/ApplicationCache"
    "${WEBKIT_DIR}/WebProcess/Automation"
    "${WEBKIT_DIR}/WebProcess/Cache"
    "${WEBKIT_DIR}/WebProcess/Databases"
    "${WEBKIT_DIR}/WebProcess/Databases/IndexedDB"
    "${WEBKIT_DIR}/WebProcess/EncryptedMedia"
    "${WEBKIT_DIR}/WebProcess/Extensions"
    "${WEBKIT_DIR}/WebProcess/Extensions/API"
    "${WEBKIT_DIR}/WebProcess/Extensions/Bindings"
    "${WEBKIT_DIR}/WebProcess/FileAPI"
    "${WEBKIT_DIR}/WebProcess/FullScreen"
    "${WEBKIT_DIR}/WebProcess/Gamepad"
    "${WEBKIT_DIR}/WebProcess/Geolocation"
    "${WEBKIT_DIR}/WebProcess/GPU"
    "${WEBKIT_DIR}/WebProcess/GPU/ShapeDetection"
    "${WEBKIT_DIR}/WebProcess/GPU/graphics"
    "${WEBKIT_DIR}/WebProcess/GPU/graphics/Model"
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
    "${WEBKIT_DIR}/WebProcess/Media"
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

set(WebKit_BINDINGS_IN_FILES
    WebProcess/Extensions/Interfaces/WebExtensionAPIAction
    WebProcess/Extensions/Interfaces/WebExtensionAPIAlarms
    WebProcess/Extensions/Interfaces/WebExtensionAPIBookmarks
    WebProcess/Extensions/Interfaces/WebExtensionAPICommands
    WebProcess/Extensions/Interfaces/WebExtensionAPICookies
    WebProcess/Extensions/Interfaces/WebExtensionAPIDOM
    WebProcess/Extensions/Interfaces/WebExtensionAPIDeclarativeNetRequest
    WebProcess/Extensions/Interfaces/WebExtensionAPIDevTools
    WebProcess/Extensions/Interfaces/WebExtensionAPIDevToolsExtensionPanel
    WebProcess/Extensions/Interfaces/WebExtensionAPIDevToolsInspectedWindow
    WebProcess/Extensions/Interfaces/WebExtensionAPIDevToolsNetwork
    WebProcess/Extensions/Interfaces/WebExtensionAPIDevToolsPanels
    WebProcess/Extensions/Interfaces/WebExtensionAPIEvent
    WebProcess/Extensions/Interfaces/WebExtensionAPIExtension
    WebProcess/Extensions/Interfaces/WebExtensionAPILocalization
    WebProcess/Extensions/Interfaces/WebExtensionAPIMenus
    WebProcess/Extensions/Interfaces/WebExtensionAPINamespace
    WebProcess/Extensions/Interfaces/WebExtensionAPINotifications
    WebProcess/Extensions/Interfaces/WebExtensionAPIPermissions
    WebProcess/Extensions/Interfaces/WebExtensionAPIPort
    WebProcess/Extensions/Interfaces/WebExtensionAPIRuntime
    WebProcess/Extensions/Interfaces/WebExtensionAPIScripting
    WebProcess/Extensions/Interfaces/WebExtensionAPISidePanel
    WebProcess/Extensions/Interfaces/WebExtensionAPISidebarAction
    WebProcess/Extensions/Interfaces/WebExtensionAPIStorage
    WebProcess/Extensions/Interfaces/WebExtensionAPIStorageArea
    WebProcess/Extensions/Interfaces/WebExtensionAPITabs
    WebProcess/Extensions/Interfaces/WebExtensionAPITest
    WebProcess/Extensions/Interfaces/WebExtensionAPIWebNavigation
    WebProcess/Extensions/Interfaces/WebExtensionAPIWebNavigationEvent
    WebProcess/Extensions/Interfaces/WebExtensionAPIWebPageNamespace
    WebProcess/Extensions/Interfaces/WebExtensionAPIWebPageRuntime
    WebProcess/Extensions/Interfaces/WebExtensionAPIWebRequest
    WebProcess/Extensions/Interfaces/WebExtensionAPIWebRequestEvent
    WebProcess/Extensions/Interfaces/WebExtensionAPIWindows
    WebProcess/Extensions/Interfaces/WebExtensionAPIWindowsEvent
)

set(WebKit_MESSAGES_IN_FILES
    GPUProcess/GPUConnectionToWebProcess
    GPUProcess/GPUProcess
    GPUProcess/RemoteSharedResourceCache

    GPUProcess/ShapeDetection/RemoteBarcodeDetector
    GPUProcess/ShapeDetection/RemoteFaceDetector
    GPUProcess/ShapeDetection/RemoteTextDetector

    GPUProcess/graphics/RemoteSnapshotRecorder
    GPUProcess/graphics/RemoteGraphicsContext
    GPUProcess/graphics/RemoteGraphicsContextGL
    GPUProcess/graphics/RemoteImageBuffer
    GPUProcess/graphics/RemoteImageBufferSet
    GPUProcess/graphics/RemoteRenderingBackend

    GPUProcess/graphics/Model/RemoteMesh

    GPUProcess/graphics/WebGPU/RemoteAdapter
    GPUProcess/graphics/WebGPU/RemoteBindGroup
    GPUProcess/graphics/WebGPU/RemoteBindGroupLayout
    GPUProcess/graphics/WebGPU/RemoteBuffer
    GPUProcess/graphics/WebGPU/RemoteCommandBuffer
    GPUProcess/graphics/WebGPU/RemoteCommandEncoder
    GPUProcess/graphics/WebGPU/RemoteCompositorIntegration
    GPUProcess/graphics/WebGPU/RemoteComputePassEncoder
    GPUProcess/graphics/WebGPU/RemoteComputePipeline
    GPUProcess/graphics/WebGPU/RemoteDevice
    GPUProcess/graphics/WebGPU/RemoteExternalTexture
    GPUProcess/graphics/WebGPU/RemoteGPU
    GPUProcess/graphics/WebGPU/RemotePipelineLayout
    GPUProcess/graphics/WebGPU/RemotePresentationContext
    GPUProcess/graphics/WebGPU/RemoteQuerySet
    GPUProcess/graphics/WebGPU/RemoteQueue
    GPUProcess/graphics/WebGPU/RemoteRenderBundle
    GPUProcess/graphics/WebGPU/RemoteRenderBundleEncoder
    GPUProcess/graphics/WebGPU/RemoteRenderPassEncoder
    GPUProcess/graphics/WebGPU/RemoteRenderPipeline
    GPUProcess/graphics/WebGPU/RemoteSampler
    GPUProcess/graphics/WebGPU/RemoteShaderModule
    GPUProcess/graphics/WebGPU/RemoteTexture
    GPUProcess/graphics/WebGPU/RemoteTextureView
    GPUProcess/graphics/WebGPU/RemoteXRBinding
    GPUProcess/graphics/WebGPU/RemoteXRProjectionLayer
    GPUProcess/graphics/WebGPU/RemoteXRSubImage
    GPUProcess/graphics/WebGPU/RemoteXRView

    GPUProcess/media/RemoteAudioDestinationManager
    GPUProcess/media/RemoteAudioVideoRendererProxyManager
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
    GPUProcess/media/RemoteMediaResourceLoader
    GPUProcess/media/RemoteRemoteCommandListenerProxy
    GPUProcess/media/RemoteVideoFrameObjectHeap

    GPUProcess/webrtc/LibWebRTCCodecsProxy
    GPUProcess/webrtc/RemoteAudioMediaStreamTrackRendererInternalUnitManager
    GPUProcess/webrtc/RemoteSampleBufferDisplayLayer
    GPUProcess/webrtc/RemoteSampleBufferDisplayLayerManager

    ModelProcess/ModelConnectionToWebProcess
    ModelProcess/ModelProcess
    ModelProcess/ModelProcessModelPlayerManagerProxy

    NetworkProcess/Authentication/AuthenticationManager

    NetworkProcess/Cookies/WebCookieManager

    NetworkProcess/NetworkBroadcastChannelRegistry
    NetworkProcess/NetworkConnectionToWebProcess
    NetworkProcess/NetworkContentRuleListManager
    NetworkProcess/NetworkProcess
    NetworkProcess/NetworkResourceLoader
    NetworkProcess/NetworkSocketChannel

    NetworkProcess/ServiceWorker/ServiceWorkerDownloadTask
    NetworkProcess/ServiceWorker/ServiceWorkerFetchTask
    NetworkProcess/ServiceWorker/WebSWServerConnection
    NetworkProcess/ServiceWorker/WebSWServerToContextConnection

    NetworkProcess/SharedWorker/WebSharedWorkerServerConnection
    NetworkProcess/SharedWorker/WebSharedWorkerServerToContextConnection

    NetworkProcess/storage/NetworkStorageManager

    NetworkProcess/webrtc/NetworkMDNSRegister
    NetworkProcess/webrtc/NetworkRTCMonitor
    NetworkProcess/webrtc/NetworkRTCProvider
    NetworkProcess/webrtc/RTCDataChannelRemoteManagerProxy

    NetworkProcess/webrtc/rice/RiceBackend

    NetworkProcess/webtransport/NetworkTransportSession

    Shared/AuxiliaryProcess
    Shared/IPCConnectionTester
    Shared/IPCStreamTester
    Shared/IPCStreamTesterProxy
    Shared/IPCTester
    Shared/IPCTesterReceiver

    Shared/Notifications/NotificationManagerMessageHandler

    UIProcess/DrawingAreaProxy

    UIProcess/Extensions/WebExtensionContext
    UIProcess/Extensions/WebExtensionController

    UIProcess/Media/RemoteMediaSessionCoordinatorProxy
    UIProcess/Media/RemoteMediaSessionManagerProxy

    UIProcess/SpeechRecognitionRemoteRealtimeMediaSourceManager
    UIProcess/SpeechRecognitionServer
    UIProcess/VisitedLinkStore
    UIProcess/WebBackForwardList
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

    UIProcess/Inspector/Agents/ProxyingNetworkAgent
    UIProcess/Inspector/Agents/ProxyingPageAgent

    UIProcess/Inspector/RemoteWebInspectorUIProxy
    UIProcess/Inspector/WebInspectorBackendProxy
    UIProcess/Inspector/WebInspectorUIProxy

    UIProcess/Model/ModelProcessProxy

    UIProcess/Network/NetworkProcessProxy

    UIProcess/XR/PlatformXRSystem

    WebProcess/WebProcess

    WebProcess/Automation/WebAutomationSessionProxy

    WebProcess/Databases/IndexedDB/WebIDBConnectionToServer

    WebProcess/DigitalCredentials/DigitalCredentialsCoordinator

    WebProcess/Extensions/WebExtensionContextProxy
    WebProcess/Extensions/WebExtensionControllerProxy

    WebProcess/FullScreen/WebFullScreenManager

    WebProcess/GPU/GPUProcessConnection

    WebProcess/GPU/graphics/RemoteGraphicsContextGLProxy
    WebProcess/GPU/graphics/RemoteImageBufferProxy
    WebProcess/GPU/graphics/RemoteRenderingBackendProxy

    WebProcess/GPU/graphics/WebGPU/RemoteGPUProxy

    WebProcess/GPU/media/AudioVideoRendererRemoteMessageReceiver
    WebProcess/GPU/media/RemoteAudioHardwareListener
    WebProcess/GPU/media/RemoteMediaResourceLoaderProxy
    WebProcess/GPU/media/MediaPlayerPrivateRemote
    WebProcess/GPU/media/RemoteAudioSession
    WebProcess/GPU/media/RemoteAudioSourceProviderManager
    WebProcess/GPU/media/RemoteCDMInstance
    WebProcess/GPU/media/RemoteCDMInstanceSession
    WebProcess/GPU/media/RemoteLegacyCDMSession
    WebProcess/GPU/media/RemoteRemoteCommandListener

    WebProcess/GPU/webrtc/LibWebRTCCodecs
    WebProcess/GPU/webrtc/RemoteVideoFrameObjectHeapProxyProcessor
    WebProcess/GPU/webrtc/SampleBufferDisplayLayer

    WebProcess/Geolocation/WebGeolocationManager

    WebProcess/Inspector/RemoteWebInspectorUI
    WebProcess/Inspector/WebInspectorBackend
    WebProcess/Inspector/WasmDebuggerDispatcher
    WebProcess/Inspector/WebInspectorInterruptDispatcher
    WebProcess/Inspector/WebInspectorUI

    WebProcess/Media/RemoteMediaSessionManager
    WebProcess/MediaSession/RemoteMediaSessionCoordinator

    WebProcess/Model/ModelProcessConnection
    WebProcess/Model/ModelProcessModelPlayer

    WebProcess/Network/webrtc/LibWebRTCNetwork
    WebProcess/Network/webrtc/RTCDataChannelRemoteManager
    WebProcess/Network/webrtc/WebRTCMonitor
    WebProcess/Network/webrtc/WebRTCResolver

    WebProcess/Network/webrtc/rice/RiceBackendProxy

    WebProcess/Network/NetworkProcessConnection
    WebProcess/Network/WebResourceLoader
    WebProcess/Network/WebSocketChannel
    WebProcess/Network/WebTransportSession

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
    WebProcess/WebPage/WebPageTesting

    WebProcess/WebStorage/StorageAreaMap

    WebProcess/XR/PlatformXRSystemProxy

    webpushd/PushClientConnection
)

set(WebKit_SERIALIZATION_IN_FILES
    GPUProcess/GPUProcessCreationParameters.serialization.in
    GPUProcess/GPUProcessPreferences.serialization.in
    GPUProcess/GPUProcessSessionParameters.serialization.in

    GPUProcess/graphics/PathSegment.serialization.in
    GPUProcess/graphics/RemoteGraphicsContextGLInitializationState.serialization.in

    GPUProcess/graphics/WebGPU/RemoteGPURequestAdapterResponse.serialization.in

    GPUProcess/media/AudioTrackPrivateRemoteConfiguration.serialization.in
    GPUProcess/media/InitializationSegmentInfo.serialization.in
    GPUProcess/media/MediaDescriptionInfo.serialization.in
    GPUProcess/media/RemoteMediaPlayerProxyConfiguration.serialization.in
    GPUProcess/media/TextTrackPrivateRemoteConfiguration.serialization.in
    GPUProcess/media/TrackPrivateRemoteConfiguration.serialization.in
    GPUProcess/media/VideoTrackPrivateRemoteConfiguration.serialization.in

    ModelProcess/ModelProcessCreationParameters.serialization.in

    NetworkProcess/NetworkActivityTracker.serialization.in
    NetworkProcess/NetworkProcessCreationParameters.serialization.in
    NetworkProcess/NetworkResourceLoadParameters.serialization.in
    NetworkProcess/NetworkSessionCreationParameters.serialization.in
    NetworkProcess/PreconnectRequest.serialization.in

    NetworkProcess/Classifier/ITPThirdPartyData.serialization.in
    NetworkProcess/Classifier/ITPThirdPartyDataForSpecificFirstParty.serialization.in

    NetworkProcess/PrivateClickMeasurement/PrivateClickMeasurementManagerInterface.serialization.in

    NetworkProcess/storage/FileSystemStorageError.serialization.in

    NetworkProcess/webtransport/WebTransport.serialization.in

    Platform/IPC/ConnectionHandle.serialization.in
    Platform/IPC/FormDataReference.serialization.in
    Platform/IPC/IPCEvent.serialization.in
    Platform/IPC/IPCSemaphore.serialization.in
    Platform/IPC/MessageFlags.serialization.in
    Platform/IPC/ObjectIdentifierReference.serialization.in
    Platform/IPC/SharedBufferReference.serialization.in
    Platform/IPC/SharedFileHandle.serialization.in
    Platform/IPC/StreamServerConnection.serialization.in
    Platform/IPC/TransferString.serialization.in

    Shared/AccessibilityPreferences.serialization.in
    Shared/AuxiliaryProcessCreationParameters.serialization.in
    Shared/BackgroundFetchChange.serialization.in
    Shared/BackgroundFetchState.serialization.in
    Shared/CacheModel.serialization.in
    Shared/CallbackID.serialization.in
    Shared/ContentWorldData.serialization.in
    Shared/ContextMenuContextData.serialization.in
    Shared/CoordinateSystem.serialization.in
    Shared/DebuggableInfoData.serialization.in
    Shared/DocumentEditingContext.serialization.in
    Shared/DragControllerAction.serialization.in
    Shared/DragEventForwardingData.serialization.in
    Shared/DrawingAreaInfo.serialization.in
    Shared/EditingRange.serialization.in
    Shared/EditorState.serialization.in
    Shared/FileSystemHandleInfo.serialization.in
    Shared/FileSystemSyncAccessHandleInfo.serialization.in
    Shared/FocusedElementInformation.serialization.in
    Shared/FrameInfoData.serialization.in
    Shared/FrameTreeCreationParameters.serialization.in
    Shared/FrameTreeNodeData.serialization.in
    Shared/FullScreenMediaDetails.serialization.in
    Shared/GPUProcessConnectionParameters.serialization.in
    Shared/GPUProcessMediaCodecCapabilities.serialization.in
    Shared/GoToBackForwardItemParameters.serialization.in
    Shared/IPCTester.serialization.in
    Shared/ImageOptions.serialization.in
    Shared/InspectorExtensionTypes.serialization.in
    Shared/InspectorNetworkTypes.serialization.in
    Shared/InspectorPageTypes.serialization.in
    Shared/JavaScriptCore.serialization.in
    Shared/JavaScriptEvaluationResult.serialization.in
    Shared/LayerTreeContext.serialization.in
    Shared/LoadParameters.serialization.in
    Shared/MediaPlaybackState.serialization.in
    Shared/Model.serialization.in
    Shared/ModelProcessConnectionParameters.serialization.in
    Shared/MonotonicObjectIdentifier.serialization.in
    Shared/NavigationActionData.serialization.in
    Shared/NetworkProcessConnectionParameters.serialization.in
    Shared/NodeHitTestResult.serialization.in
    Shared/PDFDisplayMode.serialization.in
    Shared/Pasteboard.serialization.in
    Shared/PlatformPopupMenuData.serialization.in
    Shared/PolicyDecision.serialization.in
    Shared/PrintInfo.serialization.in
    Shared/ProcessQualified.serialization.in
    Shared/ProvisionalFrameCreationParameters.serialization.in
    Shared/RTCNetwork.serialization.in
    Shared/RTCPacketOptions.serialization.in
    Shared/RemoteWebTouchEvent.serialization.in
    Shared/RemoteWorkerInitializationData.serialization.in
    Shared/RemoteWorkerType.serialization.in
    Shared/ResourceLoadInfo.serialization.in
    Shared/ResourceLoadStatisticsParameters.serialization.in
    Shared/RunJavaScriptParameters.serialization.in
    Shared/SameDocumentNavigationType.serialization.in
    Shared/SandboxExtension.serialization.in
    Shared/ScriptTrackingPrivacyFilter.serialization.in
    Shared/ScrollingAccelerationCurve.serialization.in
    Shared/SerializedNode.serialization.in
    Shared/ServiceWorkerTimingInfo.serialization.in
    Shared/SessionState.serialization.in
    Shared/ShareableSpatialImage.serialization.in
    Shared/SyntheticEditingCommandType.serialization.in
    Shared/TextFlags.serialization.in
    Shared/TextManipulationParameters.serialization.in
    Shared/TextRecognitionResult.serialization.in
    Shared/TextRecognitionUpdateResult.serialization.in
    Shared/URLSchemeTaskParameters.serialization.in
    Shared/UndoOrRedo.serialization.in
    Shared/UpdateInfo.serialization.in
    Shared/UserContentControllerParameters.serialization.in
    Shared/UserData.serialization.in
    Shared/VisibleContentRectUpdateInfo.serialization.in
    Shared/WTFArgumentCoders.serialization.in
    Shared/WebBackForwardListCounts.serialization.in
    Shared/WebContextMenuItemData.serialization.in
    Shared/WebCoreArgumentCoders.serialization.in
    Shared/WebCoreArgumentCodersAuth.serialization.in
    Shared/WebCoreArgumentCodersMedia.serialization.in
    Shared/WebCoreArgumentCodersNetwork.serialization.in
    Shared/WebCoreArgumentCodersPayment.serialization.in
    Shared/WebCoreArgumentCodersPlatform.serialization.in
    Shared/WebCoreArgumentCodersStorage.serialization.in
    Shared/WebCoreFont.serialization.in
    Shared/WebCompiledContentRuleListData.serialization.in
    Shared/WebEvent.serialization.in
    Shared/WebFindOptions.serialization.in
    Shared/WebFoundTextRange.serialization.in
    Shared/WebFrameMetrics.serialization.in
    Shared/WebGL.serialization.in
    Shared/WebHitTestResultData.serialization.in
    Shared/WebImage.serialization.in
    Shared/WebNavigationDataStore.serialization.in
    Shared/WebPageCreationParameters.serialization.in
    Shared/WebPageGroupData.serialization.in
    Shared/WebPageNetworkParameters.serialization.in
    Shared/WebPopupItem.serialization.in
    Shared/WebPreferencesStore.serialization.in
    Shared/WebProcessCreationParameters.serialization.in
    Shared/WebProcessDataStoreParameters.serialization.in
    Shared/WebPushDaemonConnectionConfiguration.serialization.in
    Shared/WebPushMessage.serialization.in
    Shared/WebUserContentControllerDataTypes.serialization.in
    Shared/WebsiteAutoplayPolicy.serialization.in
    Shared/WebsiteAutoplayQuirk.serialization.in
    Shared/WebsiteDataStoreParameters.serialization.in
    Shared/WebsitePoliciesData.serialization.in
    Shared/WebsitePopUpPolicy.serialization.in
    Shared/WebsitePushAndNotificationsEnabledPolicy.serialization.in
    Shared/WriteWebArchiveToPasteBoardResult.serialization.in

    Shared/API/APIArray.serialization.in
    Shared/API/APIData.serialization.in
    Shared/API/APIDictionary.serialization.in
    Shared/API/APIError.serialization.in
    Shared/API/APIFrameHandle.serialization.in
    Shared/API/APIGeometry.serialization.in
    Shared/API/APINumber.serialization.in
    Shared/API/APIObject.serialization.in
    Shared/API/APIPageHandle.serialization.in
    Shared/API/APIString.serialization.in
    Shared/API/APIURL.serialization.in
    Shared/API/APIURLRequest.serialization.in
    Shared/API/APIURLResponse.serialization.in
    Shared/API/APIUserContentURLPattern.serialization.in

    Shared/Authentication/AuthenticationChallengeDisposition.serialization.in

    Shared/Databases/IndexedDB/WebIDBResult.serialization.in

    Shared/Extensions/WebExtensionActionClickBehavior.serialization.in
    Shared/Extensions/WebExtensionAlarmParameters.serialization.in
    Shared/Extensions/WebExtensionBookmarksParameters.serialization.in
    Shared/Extensions/WebExtensionCommandParameters.serialization.in
    Shared/Extensions/WebExtensionContentWorldType.serialization.in
    Shared/Extensions/WebExtensionContext.serialization.in
    Shared/Extensions/WebExtensionContextParameters.serialization.in
    Shared/Extensions/WebExtensionControllerParameters.serialization.in
    Shared/Extensions/WebExtensionCookieParameters.serialization.in
    Shared/Extensions/WebExtensionDynamicScripts.serialization.in
    Shared/Extensions/WebExtensionEventListenerType.serialization.in
    Shared/Extensions/WebExtensionFrameParameters.serialization.in
    Shared/Extensions/WebExtensionMatchedRuleParameters.serialization.in
    Shared/Extensions/WebExtensionMenuItem.serialization.in
    Shared/Extensions/WebExtensionMessageSenderParameters.serialization.in
    Shared/Extensions/WebExtensionMessageTargetParameters.serialization.in
    Shared/Extensions/WebExtensionSidebarParameters.serialization.in
    Shared/Extensions/WebExtensionStorage.serialization.in
    Shared/Extensions/WebExtensionTab.serialization.in
    Shared/Extensions/WebExtensionWindow.serialization.in

    Shared/Gamepad/GamepadData.serialization.in

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
    Shared/WebGPU/WebGPUInternalError.serialization.in
    Shared/WebGPU/WebGPUMultisampleState.serialization.in
    Shared/WebGPU/WebGPUObjectDescriptorBase.serialization.in
    Shared/WebGPU/WebGPUOrigin2D.serialization.in
    Shared/WebGPU/WebGPUOrigin3D.serialization.in
    Shared/WebGPU/WebGPUOutOfMemoryError.serialization.in
    Shared/WebGPU/WebGPUPipelineDescriptorBase.serialization.in
    Shared/WebGPU/WebGPUPipelineLayoutDescriptor.serialization.in
    Shared/WebGPU/WebGPUPresentationContextDescriptor.serialization.in
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
    Shared/WebGPU/WebGPUTextureBindingLayout.serialization.in
    Shared/WebGPU/WebGPUTextureDescriptor.serialization.in
    Shared/WebGPU/WebGPUTextureViewDescriptor.serialization.in
    Shared/WebGPU/WebGPUValidationError.serialization.in
    Shared/WebGPU/WebGPUVertexAttribute.serialization.in
    Shared/WebGPU/WebGPUVertexBufferLayout.serialization.in
    Shared/WebGPU/WebGPUVertexState.serialization.in

    Shared/WebsiteData/TimeBasedEvictionMode.serialization.in
    Shared/WebsiteData/UnifiedOriginStorageLevel.serialization.in
    Shared/WebsiteData/WebsiteData.serialization.in
    Shared/WebsiteData/WebsiteDataFetchOption.serialization.in
    Shared/WebsiteData/WebsiteDataType.serialization.in

    Shared/XR/PlatformXR.serialization.in
    Shared/XR/XRSystem.serialization.in

    Shared/graphics/RemoteImageBufferSetConfiguration.serialization.in

    UIProcess/Extensions/WebExtension.serialization.in

    WebProcess/GPU/GPUProcessConnectionInfo.serialization.in

    WebProcess/GPU/graphics/BufferIdentifierSet.serialization.in
    WebProcess/GPU/graphics/PrepareBackingStoreBuffersData.serialization.in

    WebProcess/GPU/media/MediaOverridesForTesting.serialization.in
    WebProcess/GPU/media/MediaTimeUpdateData.serialization.in
    WebProcess/GPU/media/RemoteAudioSessionConfiguration.serialization.in
    WebProcess/GPU/media/RemoteAudioVideoRendererState.serialization.in
    WebProcess/GPU/media/RemoteCDMConfiguration.serialization.in
    WebProcess/GPU/media/RemoteCDMInstanceConfiguration.serialization.in
    WebProcess/GPU/media/RemoteMediaPlayerConfiguration.serialization.in
    WebProcess/GPU/media/RemoteMediaPlayerState.serialization.in
    WebProcess/GPU/media/RemoteVideoFrameProxyProperties.serialization.in

    WebProcess/GPU/webrtc/SharedVideoFrame.serialization.in

    WebProcess/Media/RemoteMediaSessionState.serialization.in

    WebProcess/MediaStream/MediaDeviceSandboxExtensions.serialization.in

    WebProcess/Model/ModelProcessConnectionInfo.serialization.in

    WebProcess/Network/NetworkProcessConnectionInfo.serialization.in

    WebProcess/UserContent/InjectUserScriptImmediately.serialization.in

    WebProcess/WebCoreSupport/WebSpeechSynthesisVoice.serialization.in
)

set(WebCore_GENERATED_SERIALIZATION_IN_FILES
    HTTPHeaderNames.serialization.in
)

set(WebCore_SERIALIZATION_IN_FILES
    ActivityState.serialization.in
    DocumentSyncData.serialization.in
    DragActions.serialization.in
    FrameTreeSyncData.serialization.in
    InbandTextTrackPrivate.serialization.in
    IndexedDB.serialization.in
    LayoutMilestones.serialization.in
    MDNSRegisterError.serialization.in
    MediaProducer.serialization.in
    PlatformEvent.serialization.in
    PlatformMediaSession.serialization.in
    PlatformScreen.serialization.in
    PlatformWheelEvent.serialization.in
    ProtectionSpaceBase.serialization.in
    ScrollTypes.serialization.in
    SharedTimebase.serialization.in
    WebGPU.serialization.in
)

set(WebKit_FRAMEWORKS
    JavaScriptCore
    PAL
    WTF
    WebCore
    bmalloc
)
if (APPLE)
    list(APPEND WebKit_FRAMEWORKS WebKitLegacy)
endif ()
if (USE_APPLE_INTERNAL_SDK)
    list(APPEND WebKit_FRAMEWORKS WebKitAdditions)
    list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES "${WebKitAdditions_FRAMEWORK_HEADERS_DIR}/WebKitAdditions")
endif ()

set(WebProcess_LIBRARIES WebKit)
set(NetworkProcess_LIBRARIES WebKit)
set(GPUProcess_LIBRARIES WebKit)

# For platforms that want to use system-provided OpenGL (ES) / EGL headers,
# these include directories, libraries or definitions need to be
# added before the ANGLE directories.
if (USE_LIBEPOXY)
    list(APPEND WebKit_PRIVATE_LIBRARIES Epoxy::Epoxy)
elseif (USE_ANGLE_EGL)
    # Prepend to make sure the ANGLE headers are found before system headers
    list(PREPEND WebKit_PRIVATE_LIBRARIES
        ANGLE::GLES
        ANGLE::EGL
    )
else ()
    list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES
        ${EGL_INCLUDE_DIRS}
    )
    list(APPEND WebKit_PRIVATE_LIBRARIES
        ${EGL_LIBRARIES}
    )
    add_definitions(${EGL_DEFINITIONS})
endif ()

if (USE_LIBWEBRTC)
    list(APPEND WebKit_SYSTEM_INCLUDE_DIRECTORIES "${THIRDPARTY_DIR}/libwebrtc/Source/"
        "${THIRDPARTY_DIR}/libwebrtc/Source/webrtc"
        "${THIRDPARTY_DIR}/libwebrtc/Source/third_party/abseil-cpp")
    if (NOT APPLE)
        list(APPEND WebKit_LIBRARIES webrtc)
    endif ()
endif ()

if (USE_GSTREAMER_WEBRTC AND USE_LIBRICE)
    list(APPEND WebKit_LIBRARIES Rice::Io)
    list(APPEND WebKit_PRIVATE_INCLUDE_DIRECTORIES "${WEBKIT_DIR}/NetworkProcess/webrtc/rice")
endif ()

if (ATOMICS_REQUIRE_LIBATOMIC)
    list(APPEND WebKit_PRIVATE_LIBRARIES atomic)
endif ()

set(WebKit_AUTOMATION_PROTOCOL_GENERATOR_EXTRA_FLAGS)

set(WebKit_INTERFACE_LIBRARIES WebKit)
if (NOT USE_HEADER_MAPS)
    set(WebKit_INTERFACE_INCLUDE_DIRECTORIES
        ${WebKit_FRAMEWORK_HEADERS_DIR}
        ${WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR}
    )
    set(WebKit_INTERFACE_DEPENDENCIES "")
endif ()
set(WebKit_INTERFACE_LINK_DEPENDS
    $<TARGET_FILE:NetworkProcess>
    $<TARGET_FILE:WebProcess>
)

WEBKIT_EXECUTABLE_DECLARE(WebProcess)
WEBKIT_EXECUTABLE_DECLARE(NetworkProcess)

if (ENABLE_GPU_PROCESS)
    WEBKIT_EXECUTABLE_DECLARE(GPUProcess)
    list(APPEND WebKit_INTERFACE_LINK_DEPENDS $<TARGET_FILE:GPUProcess>)
endif ()

set(WebKit_GENERATED_SERIALIZERS_SUFFIX cpp)

# Default Swift interop module path; matches Xcode's WebKit.xcconfig:
# SWIFT_INCLUDE_PATHS = $(SRCROOT)/Modules/Internal. Must remain Swift-only —
# loading WebKit_Internal in a Clang context creates a second parse of the
# WK*Private headers that conflicts with the WebKit_Private framework module.
set(WebKit_SWIFT_INTEROP_MODULE_PATH "${WEBKIT_DIR}/Modules/Internal")
set(WebKit_SWIFT_INTEROP_MODULE_PATH_SWIFT_ONLY TRUE)

# WebCore_Private.modulemap in-tree is a `framework module` that umbrellas the
# Xcode framework's PrivateHeaders/. CMake stages those headers as a flat
# directory instead, and umbrellaing it pulls in headers (ANGLEHeaders.h etc.)
# whose own dependencies aren't on the Swift Clang importer's search path.
# Expose only what WebBackForwardList.swift names directly; the rest of the
# WebCore:: types it uses are reachable transitively via WebKit_Internal headers.
if (ENABLE_BACK_FORWARD_LIST_SWIFT)
    set(WebCore_Private_SWIFT_MODULEMAP_DIR "${CMAKE_BINARY_DIR}/WebKit/SwiftModules/WebCorePrivate")
    file(MAKE_DIRECTORY "${WebCore_Private_SWIFT_MODULEMAP_DIR}")
    file(CONFIGURE OUTPUT "${WebCore_Private_SWIFT_MODULEMAP_DIR}/module.modulemap" CONTENT
"module WebCore_Private [system] {
    requires cplusplus
    header \"${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCore/DiagnosticLoggingKeys.h\"
    header \"${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCore/DiagnosticLoggingClient.h\"
    export *
}
")
endif ()

WEBKIT_FRAMEWORK_DECLARE(WebKit)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()
WEBKIT_ADD_TARGET_UNSAFE_BUFFER_WARNINGS(WebKit)

# The full WebKit_Internal C++ module pulls in WebPageProxy.h and friends, which
# quote-include across the entire WebKit/WebCore/JSC private header set. Mirror
# the C++ target's include directories to swiftc's Clang importer so those
# resolve. cmakeconfig.h is force-included because the headers assume the
# project's prefix header has already defined ENABLE()/HAVE() values.
set(WebKit_SWIFT_CLANG_INCLUDE_DIRS
    ${CMAKE_BINARY_DIR}
    ${WebKit_FRAMEWORK_HEADERS_DIR}
    ${WebKit_DERIVED_SOURCES_DIR}
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_FRAMEWORK_HEADERS_DIR}
    ${JavaScriptCore_PRIVATE_FRAMEWORK_HEADERS_DIR}
    ${WTF_FRAMEWORK_HEADERS_DIR}
    ${bmalloc_FRAMEWORK_HEADERS_DIR}
    ${PAL_FRAMEWORK_HEADERS_DIR}
    ${ICU_INCLUDE_DIRS}
    ${WebCore_Private_SWIFT_MODULEMAP_DIR}
    ${WebKit_PRIVATE_INCLUDE_DIRECTORIES}
)

if (NOT APPLE AND SWIFT_REQUIRED)
    # Export-macro stubs, NODELETE=, WK_SUPPORTS_SWIFT_OBJCXX_INTEROP and the
    # full ENABLE/HAVE/USE config-var set now come from
    # _WEBKIT_COMPUTE_SWIFT_SHARED_CLANG_FLAGS in WebKitMacros.cmake. Only
    # Linux-specific importer flags remain here.
    set(WebKit_SWIFT_CLANG_FLAG_PAIRS
        # libstdc++ gates coroutine support in <bits/version.h> behind
        # __glibcxx_want_coroutine.  Swift's Clang 21 compiles <bits/version.h>
        # as a PCM before that flag is set, so __cpp_lib_coroutine stays
        # undefined and std::coroutine_handle is not available.  Pre-defining
        # the flag ensures the PCM gets the right feature-test value.
        # rdar://175928621
        "-Xcc -D__glibcxx_want_coroutine=1"
        "-Xcc -fmodule-map-file=${WTF_FRAMEWORK_HEADERS_DIR}/wtf/module.modulemap"
    )
    foreach (_dir IN LISTS WebKit_SWIFT_CLANG_INCLUDE_DIRS)
        list(APPEND WebKit_SWIFT_CLANG_FLAG_PAIRS "-Xcc -I${_dir}")
    endforeach ()
    # VFS overlay: inject module.modulemap files for Linux system libraries
    # (glib, libsoup, etc.) that do not ship their own modulemaps.  The overlay
    # makes them appear inside the detected ${GLib_INCLUDE_DIR} /
    # ${Soup3_INCLUDE_DIR} (which may live under /usr or a jhbuild prefix)
    # without modifying the real filesystem.  Both the
    # -typecheck/-emit-clang-header custom command and the per-file Swift
    # compilation receive the flag via SWIFT_EXTRA_OPTIONS and
    # target_compile_options respectively.
    set(_linux_mm_src "${CMAKE_SOURCE_DIR}/Source/cmake/linux-modulemaps")
    set(_linux_mm_dir "${CMAKE_BINARY_DIR}/linux-modulemaps")
    file(MAKE_DIRECTORY "${_linux_mm_dir}")
    configure_file("${_linux_mm_src}/glib.modulemap"    "${_linux_mm_dir}/glib.modulemap"    COPYONLY)
    configure_file("${_linux_mm_src}/libsoup.modulemap" "${_linux_mm_dir}/libsoup.modulemap" COPYONLY)
    set(_vfs_glib_modulemap    "${_linux_mm_dir}/glib.modulemap")
    set(_vfs_libsoup_modulemap "${_linux_mm_dir}/libsoup.modulemap")
    configure_file("${_linux_mm_src}/vfs-overlay.yaml.in" "${_linux_mm_dir}/vfs-overlay.yaml" @ONLY)
    set(_vfs_overlay "${_linux_mm_dir}/vfs-overlay.yaml")
    list(APPEND WebKit_SWIFT_CLANG_FLAG_PAIRS "-Xcc -ivfsoverlay")
    list(APPEND WebKit_SWIFT_CLANG_FLAG_PAIRS "-Xcc ${_vfs_overlay}")

    foreach (_pair IN LISTS WebKit_SWIFT_CLANG_FLAG_PAIRS)
        target_compile_options(WebKit PRIVATE "$<$<COMPILE_LANGUAGE:Swift>:SHELL:${_pair}>")
    endforeach ()
endif ()

if (SWIFT_REQUIRED)
    set(WebKit_SWIFT_INTEROP_SOURCES
        UIProcess/ProvisionalPageProxy.cpp
        UIProcess/RemotePageProxy.cpp
        UIProcess/ViewGestureController.cpp
        UIProcess/WebBackForwardList.cpp
        UIProcess/WebPageProxy.cpp
        UIProcess/Automation/WebAutomationSession.cpp
        WebProcess/WebPage/IPCTestingAPI.cpp
        ${WebKit_DERIVED_SOURCES_DIR}/UIProcess/WebBackForwardListMessageReceiver.cpp
    )
    if (APPLE)
        list(APPEND WebKit_SWIFT_INTEROP_SOURCES
            UIProcess/API/ios/WKWebViewIOS.mm
            UIProcess/API/mac/WKWebViewMac.mm
            UIProcess/ios/ViewGestureControllerIOS.mm
            UIProcess/mac/ViewGestureControllerMac.mm
            UIProcess/mac/WebViewImpl.mm
        )
    endif ()
    foreach (_f IN LISTS WebKit_SWIFT_INTEROP_SOURCES)
        cmake_path(GET _f FILENAME _fn)
        list(APPEND WebKit_UNIFIED_SOURCE_EXCLUDES "(^|/)${_fn}( |$)")
    endforeach ()

    # FIXME(rdar://177840132): swiftc -emit-dependencies records imported
    # clang modulemaps, but sometimes skips individual C++ headers.
    file(STRINGS "${WebKit_SWIFT_INTEROP_MODULE_PATH}/module.modulemap" _mm_lines REGEX "header \"")
    foreach (_l IN LISTS _mm_lines)
        string(REGEX REPLACE ".*header \"(.*)\".*" "\\1" _h "${_l}")
        cmake_path(ABSOLUTE_PATH _h BASE_DIRECTORY "${WebKit_SWIFT_INTEROP_MODULE_PATH}" NORMALIZE)
        list(APPEND WebKit_SWIFT_INTEROP_HEADERS "${_h}")
    endforeach ()
endif ()

WEBKIT_SETUP_SWIFT_AND_GENERATE_SWIFT_CPP_INTEROP_HEADER(WebKit WebKit
    "${WebKit_SWIFT_INTEROP_MODULE_PATH}"
    "${WebKit_DERIVED_SOURCES_DIR}/WebKit-Swift-Generated.h")

if (WebKit_PUBLIC_FRAMEWORK_HEADERS)
    WEBKIT_COPY_FILES(WebKit_CopyHeaders
        PRUNE_STALE
        DESTINATION ${WebKit_FRAMEWORK_HEADERS_DIR}/WebKit
        FILES ${WebKit_PUBLIC_FRAMEWORK_HEADERS}
        FLATTENED
    )
    list(APPEND WebKit_DEPENDENCIES WebKit_CopyHeaders)
endif ()

if (WebKit_PRIVATE_FRAMEWORK_HEADERS)
    WEBKIT_COPY_FILES(WebKit_CopyPrivateHeaders
        PRUNE_STALE
        DESTINATION ${WebKit_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebKit
        FILES ${WebKit_PRIVATE_FRAMEWORK_HEADERS}
        FLATTENED
    )
    if (NOT USE_HEADER_MAPS)
        list(APPEND WebKit_INTERFACE_DEPENDENCIES WebKit_CopyPrivateHeaders)
    endif ()
endif ()

# Helper macro which wraps the generate-message-receiver.py script
#   _output_source is a list name which will contain generated sources.(eg. WebKit_SOURCES)
#   _inputs are messages.in files to generate.
macro(GENERATE_MESSAGE_SOURCES _output_source _inputs)
    unset(_input_files)
    unset(_outputs)
    foreach (_file IN ITEMS ${_inputs})
        get_filename_component(_name ${_file} NAME_WE)
        get_filename_component(_dir ${_file} DIRECTORY)
        # Derived .messages.in (LogStream: two-stage gen from LogMessages.in) lives in
        # DerivedSources, not ${WEBKIT_DIR}. The script searches cwd first so finds either;
        # DEPENDS needs the actual path. Check source tree first, then derived.
        if (EXISTS ${WEBKIT_DIR}/${_file}.messages.in)
            list(APPEND _input_files ${WEBKIT_DIR}/${_file}.messages.in)
            file(STRINGS ${WEBKIT_DIR}/${_file}.messages.in _swift_attr REGEX "SwiftReceiver")
            if (_swift_attr)
                list(APPEND _outputs ${_name}MessageReceiver.swift)
            endif ()
        else ()
            list(APPEND _input_files ${WebKit_DERIVED_SOURCES_DIR}/${_name}.messages.in)
        endif ()
        if (_dir)
            set(_receiver_cpp ${_dir}/${_name}MessageReceiver.cpp)
        else ()
            set(_receiver_cpp ${_name}MessageReceiver.cpp)
        endif ()
        list(APPEND _outputs
            ${_receiver_cpp}
            ${_name}Messages.h
        )
        list(APPEND ${_output_source} ${WebKit_DERIVED_SOURCES_DIR}/${_receiver_cpp})
    endforeach ()
    list(APPEND _outputs
        MessageArgumentDescriptions.cpp
        MessageNames.cpp
        MessageNames.h
    )

    list(APPEND ${_output_source}
        ${WebKit_DERIVED_SOURCES_DIR}/MessageArgumentDescriptions.cpp
        ${WebKit_DERIVED_SOURCES_DIR}/MessageNames.cpp
    )

    set(_messages_stage_dir ${WebKit_DERIVED_SOURCES_DIR}/.messages-stage)
    # copy_if_different (rather than copy_directory_if_different, which is unavailable on
    # older CMake versions) copies only changed files back, leaving the timestamps of
    # unchanged outputs untouched so downstream C++ is not needlessly rebuilt.
    unset(_absolute_outputs)
    unset(_messages_copy_commands)
    foreach (_rel IN LISTS _outputs)
        list(APPEND _absolute_outputs ${WebKit_DERIVED_SOURCES_DIR}/${_rel})
        list(APPEND _messages_copy_commands COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_messages_stage_dir}/${_rel} ${WebKit_DERIVED_SOURCES_DIR}/${_rel})
    endforeach ()
    add_custom_command(
        OUTPUT
            ${_absolute_outputs}
        MAIN_DEPENDENCY ${WEBKIT_DIR}/Scripts/generate-message-receiver.py
        DEPENDS
            ${WEBKIT_DIR}/Scripts/webkit/__init__.py
            ${WEBKIT_DIR}/Scripts/webkit/messages.py
            ${WEBKIT_DIR}/Scripts/webkit/model.py
            ${WEBKIT_DIR}/Scripts/webkit/parser.py
            ${_input_files}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_messages_stage_dir}
        COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-message-receiver.py --preserve-subdirs ${WEBKIT_DIR} ${_inputs} --output-dir ${_messages_stage_dir}
        ${_messages_copy_commands}
        WORKING_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR}
        VERBATIM
    )
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-message-receiver.py
            --preserve-subdirs
            --output-sources ${WebKit_DERIVED_SOURCES_DIR}/SourcesMessageReceivers.txt
            ${WEBKIT_DIR} ${_inputs}
        RESULT_VARIABLE _result
    )
    if (_result)
        message(FATAL_ERROR "generate-message-receiver.py --output-sources failed")
    endif ()
endmacro()
GENERATE_MESSAGE_SOURCES(WebKit_DERIVED_SOURCES "${WebKit_MESSAGES_IN_FILES}")
list(APPEND WebKit_UNIFIED_SOURCE_LIST_FILES SourcesMessageReceivers.txt)
list(APPEND WebKit_UNIFIED_SOURCE_EXCLUDES "^[A-Za-z0-9_]+MessageReceiver\\.cpp")

# Helper macro which wraps the bindings script
#   _output_source is a list name which will contain generated sources.(eg. WebKit_SOURCES)
#   _inputs are .idl files to generate.
macro(GENERATE_IDL_BINDINGS _output_source _inputs)
    unset(_input_files)
    unset(_outputs)
    unset(_import_names)
    foreach (_file IN ITEMS ${_inputs})
        get_filename_component(_name ${_file} NAME_WE)
        list(APPEND _input_files ${WEBKIT_DIR}/${_file}.idl)
        list(APPEND _outputs
            ${WebKit_DERIVED_SOURCES_DIR}/JS${_name}.mm

            ${WebKit_DERIVED_SOURCES_DIR}/JS${_name}.h
        )
        list(APPEND _import_names JS${_name}.mm)
        list(APPEND ${_output_source} ${WebKit_DERIVED_SOURCES_DIR}/JS${_name}.h)
    endforeach ()

    add_custom_command(
        OUTPUT
            ${_outputs}
            ${WebKit_DERIVED_SOURCES_DIR}/JSWebExtensionAPIUnified.mm
        MAIN_DEPENDENCY ${WEBCORE_DIR}/bindings/scripts/generate-bindings-all.pl
        DEPENDS
            ${_input_files}
           "${WEBCORE_DIR}/bindings/scripts/CodeGenerator.pm"
           "${WEBCORE_DIR}/bindings/scripts/generate-bindings.pl"
           "${WEBCORE_DIR}/bindings/scripts/IDLParser.pm"
           "${WEBKIT_DIR}/WebProcess/Extensions/Bindings/Scripts/CodeGeneratorExtensions.pm"
           "${WEBKIT_DIR}/WebProcess/Extensions/Bindings/Scripts/GenerateImports.pl"
           "${WEBKIT_DIR}/WebProcess/Extensions/Bindings/Scripts/IDLAttributes.json"
        COMMAND ${PERL_EXECUTABLE} -I "${WEBKIT_DIR}/WebProcess/Extensions/Bindings/Scripts" ${WEBCORE_DIR}/bindings/scripts/generate-bindings.pl --outputDir . --generator Extensions --idlAttributesFile "${WEBKIT_DIR}/WebProcess/Extensions/Bindings/Scripts/IDLAttributes.json" --idlFileNamesList WebExtensionIDLFileNamesList.txt ${_input_files}
        COMMAND ${PERL_EXECUTABLE} "${WEBKIT_DIR}/WebProcess/Extensions/Bindings/Scripts/GenerateImports.pl" JSWebExtensionAPIUnified.mm ${_import_names}
        WORKING_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR}
        VERBATIM
    )
endmacro()
GENERATE_IDL_BINDINGS(WebKit_DERIVED_SOURCES "${WebKit_BINDINGS_IN_FILES}")

foreach (in_file ${WebKit_SERIALIZATION_IN_FILES})
    list(APPEND WebKit_SERIALIZATION_DEPENDENCIES "${WEBKIT_DIR}/${in_file}")
endforeach ()
foreach (in_file ${WebCore_GENERATED_SERIALIZATION_IN_FILES})
    list(APPEND WebKit_SERIALIZATION_DEPENDENCIES "${WebCore_DERIVED_SOURCES_DIR}/${in_file}")
endforeach ()
foreach (in_file ${WebCore_SERIALIZATION_IN_FILES})
    list(APPEND WebKit_SERIALIZATION_DEPENDENCIES "${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCore/${in_file}")
endforeach ()
list(APPEND WebKit_SERIALIZATION_DEPENDENCIES "${WebKit_DERIVED_SOURCES_DIR}/SharedPreferencesForWebProcess.serialization.in")

list(APPEND WebKit_DERIVED_SOURCES
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializers.h
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersExtra.h
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersShared0.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersShared1.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedAPI.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedCocoa.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedEditorState.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedExtensions.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedModel.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedRemoteLayerTree.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebCoreArgumentCodersAuth.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebCoreArgumentCodersMedia.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebCoreArgumentCodersNetwork.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebCoreArgumentCodersPayment.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebCoreArgumentCodersPlatform.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebCoreArgumentCodersStorage.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebCoreFont.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebEvent.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebGL.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebGPU.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebPageCreationParameters.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedWebProcessCreationParameters.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersSharedXR.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersWebProcess.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersGPUProcess.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersNetworkProcess.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersPlatform.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersModelProcess.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersUIProcess.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/GeneratedSerializersCommon.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/SerializedTypeInfo.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    ${WebKit_DERIVED_SOURCES_DIR}/WebKitPlatformGeneratedSerializers.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
)

set(_serializers_stage_dir ${WebKit_DERIVED_SOURCES_DIR}/.serializers-stage)
set(_serializers_outputs
    GeneratedSerializers.h
    GeneratedSerializersExtra.h
    GeneratedSerializersShared0.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersShared1.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedAPI.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedCocoa.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedEditorState.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedExtensions.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedModel.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedRemoteLayerTree.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebCoreArgumentCodersAuth.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebCoreArgumentCodersMedia.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebCoreArgumentCodersNetwork.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebCoreArgumentCodersPayment.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebCoreArgumentCodersPlatform.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebCoreArgumentCodersStorage.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebCoreFont.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebEvent.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebGL.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebGPU.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebPageCreationParameters.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedWebProcessCreationParameters.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersSharedXR.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersWebProcess.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersGPUProcess.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersNetworkProcess.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersPlatform.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersModelProcess.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersUIProcess.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedSerializersCommon.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    GeneratedWebKitSecureCoding.h
    GeneratedWebKitSecureCoding.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    SerializedTypeInfo.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
    WebKitPlatformGeneratedSerializers.${WebKit_GENERATED_SERIALIZERS_SUFFIX}
)
# copy_if_different (rather than copy_directory_if_different, which is unavailable on
# older CMake versions) copies only changed files back, leaving the timestamps of
# unchanged outputs untouched so downstream C++ is not needlessly rebuilt.
unset(_serializers_absolute_outputs)
unset(_serializers_copy_commands)
foreach (_rel IN LISTS _serializers_outputs)
    list(APPEND _serializers_absolute_outputs ${WebKit_DERIVED_SOURCES_DIR}/${_rel})
    list(APPEND _serializers_copy_commands COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_serializers_stage_dir}/${_rel} ${WebKit_DERIVED_SOURCES_DIR}/${_rel})
endforeach ()
add_custom_command(
    OUTPUT
        ${_serializers_absolute_outputs}
    MAIN_DEPENDENCY ${WEBKIT_DIR}/Scripts/generate-serializers.py
    DEPENDS
        ${WEBKIT_DIR}/Scripts/webkit/opaque_ipc_types.py
        ${WebKit_SERIALIZATION_DEPENDENCIES}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_serializers_stage_dir}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBKIT_DIR}/Scripts/generate-serializers.py ${WebKit_GENERATED_SERIALIZERS_SUFFIX} --split-by-directory ${WebKit_SERIALIZATION_DEPENDENCIES} --output-dir ${_serializers_stage_dir}
    ${_serializers_copy_commands}
    WORKING_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR}
    VERBATIM
)

# These files are very large and exhaust virtual memory on 32-bit ARM.
if (WTF_CPU_ARM AND NOT CMAKE_CROSSCOMPILING)
    set(GeneratedSerializers_COMPILE_OPTIONS -O0 -DRELEASE_WITHOUT_OPTIMIZATIONS=ON)
    foreach (GeneratedSerializers_BASENAME
        GeneratedSerializersShared0
        GeneratedSerializersShared1
        GeneratedSerializersSharedAPI
        GeneratedSerializersSharedCocoa
        GeneratedSerializersSharedEditorState
        GeneratedSerializersSharedExtensions
        GeneratedSerializersSharedModel
        GeneratedSerializersSharedRemoteLayerTree
        GeneratedSerializersSharedWebCoreArgumentCodersAuth
        GeneratedSerializersSharedWebCoreArgumentCodersMedia
        GeneratedSerializersSharedWebCoreArgumentCodersNetwork
        GeneratedSerializersSharedWebCoreArgumentCodersPayment
        GeneratedSerializersSharedWebCoreArgumentCodersPlatform
        GeneratedSerializersSharedWebCoreArgumentCodersStorage
        GeneratedSerializersSharedWebCoreFont
        GeneratedSerializersSharedWebEvent
        GeneratedSerializersSharedWebGL
        GeneratedSerializersSharedWebGPU
        GeneratedSerializersSharedWebPageCreationParameters
        GeneratedSerializersSharedWebProcessCreationParameters
        GeneratedSerializersSharedXR
        GeneratedSerializersWebProcess
        GeneratedSerializersGPUProcess
        GeneratedSerializersNetworkProcess
        GeneratedSerializersPlatform
        GeneratedSerializersModelProcess
        GeneratedSerializersUIProcess
        GeneratedSerializersCommon
        WebKitPlatformGeneratedSerializers
    )
        set_source_files_properties(${WebKit_DERIVED_SOURCES_DIR}/${GeneratedSerializers_BASENAME}.cpp PROPERTIES
            COMPILE_OPTIONS "${GeneratedSerializers_COMPILE_OPTIONS}"
            SKIP_PRECOMPILE_HEADERS ON)
    endforeach ()
endif ()

set(WebKit_JSON_RPC_PROTOCOL_GENERATOR_SCRIPTS
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
    DEPENDS ${WebKit_JSON_RPC_PROTOCOL_GENERATOR_SCRIPTS}
    COMMAND ${PYTHON_EXECUTABLE} ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/generate-inspector-protocol-bindings.py --outputDir "${WebKit_DERIVED_SOURCES_DIR}" --framework WebKit ${WebKit_AUTOMATION_PROTOCOL_GENERATOR_EXTRA_FLAGS} --backend ${WebKit_AUTOMATION_PROTOCOL_GENERATOR_INPUTS}
    VERBATIM)

set(WebKit_WEBDRIVER_BIDI_PROTOCOL_GENERATOR_INPUTS
    ${WEBKIT_DIR}/UIProcess/Automation/protocol/BidiBrowser.json
    ${WEBKIT_DIR}/UIProcess/Automation/protocol/BidiBrowsingContext.json
    ${WEBKIT_DIR}/UIProcess/Automation/protocol/BidiLog.json
    ${WEBKIT_DIR}/UIProcess/Automation/protocol/BidiPermissions.json
    ${WEBKIT_DIR}/UIProcess/Automation/protocol/BidiScript.json
    ${WEBKIT_DIR}/UIProcess/Automation/protocol/BidiSession.json
    ${WEBKIT_DIR}/UIProcess/Automation/protocol/BidiStorage.json
)

add_custom_command(
    OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/CombinedWebDriverBidiDomains.json
    MAIN_DEPENDENCY ${JavaScriptCore_SCRIPTS_DIR}/generate-combined-inspector-json.py
    DEPENDS ${WebKit_WEBDRIVER_BIDI_PROTOCOL_GENERATOR_INPUTS}
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/generate-combined-inspector-json.py ${WebKit_WEBDRIVER_BIDI_PROTOCOL_GENERATOR_INPUTS} "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" > ${WebKit_DERIVED_SOURCES_DIR}/CombinedWebDriverBidiDomains.json
    VERBATIM)

add_custom_command(
    OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiBackendDispatchers.h ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiBackendDispatchers.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiProtocolObjects.h ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiProtocolObjects.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiFrontendDispatchers.h ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiFrontendDispatchers.cpp
    MAIN_DEPENDENCY ${WebKit_DERIVED_SOURCES_DIR}/CombinedWebDriverBidiDomains.json
    DEPENDS ${WebKit_JSON_RPC_PROTOCOL_GENERATOR_SCRIPTS}
    COMMAND ${PYTHON_EXECUTABLE} ${PROTOCOL_GENERATOR_SCRIPTS_DIR}/generate-inspector-protocol-bindings.py --outputDir "${WebKit_DERIVED_SOURCES_DIR}" --framework WebDriverBidi ${WebKit_AUTOMATION_PROTOCOL_GENERATOR_EXTRA_FLAGS} --backend ${WebKit_DERIVED_SOURCES_DIR}/CombinedWebDriverBidiDomains.json
    VERBATIM)

list(APPEND WebKit_HEADERS
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationBackendDispatchers.h
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationFrontendDispatchers.h
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationProtocolObjects.h
    ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiBackendDispatchers.h
    ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiFrontendDispatchers.h
    ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiProtocolObjects.h
)

list(APPEND WebKit_SOURCES
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationBackendDispatchers.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationFrontendDispatchers.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/AutomationProtocolObjects.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiBackendDispatchers.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiFrontendDispatchers.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebDriverBidiProtocolObjects.cpp
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

# Log messages
add_custom_command(
    OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/WebKitLogDefinitions.h
    MAIN_DEPENDENCY ${WEBKIT_DIR}/Platform/LogMessages.in
    DEPENDS ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCore/generate-log-declarations.py
    WORKING_DIRECTORY ${WebKit_DERIVED_SOURCES_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCore/generate-log-declarations.py ${WEBKIT_DIR}/Platform/LogMessages.in ${WebKit_DERIVED_SOURCES_DIR}/WebKitLogDefinitions.h
    VERBATIM)
list(APPEND WebKit_HEADERS ${WebKit_DERIVED_SOURCES_DIR}/WebKitLogDefinitions.h)

set(WebKit_WEB_PREFERENCES_TEMPLATES
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/SharedPreferencesForWebProcess.h.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/SharedPreferencesForWebProcess.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/SharedPreferencesForWebProcess.serialization.in.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPageUpdatePreferences.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesDefinitions.h.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesFeatures.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesGetterSetters.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesKeys.cpp.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesKeys.h.erb
    ${WEBKIT_DIR}/Scripts/PreferencesTemplates/WebPreferencesStoreDefaultsMap.cpp.erb
)

set(WebKit_WEB_PREFERENCES
    ${WTF_SCRIPTS_DIR}/Preferences/UnifiedWebPreferences.yaml
)

set_source_files_properties(${WebKit_WEB_PREFERENCES} PROPERTIES GENERATED TRUE)

add_custom_command(
    OUTPUT ${WebKit_DERIVED_SOURCES_DIR}/SharedPreferencesForWebProcess.h ${WebKit_DERIVED_SOURCES_DIR}/SharedPreferencesForWebProcess.cpp ${WebKit_DERIVED_SOURCES_DIR}/SharedPreferencesForWebProcess.serialization.in ${WebKit_DERIVED_SOURCES_DIR}/WebPageUpdatePreferences.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesDefinitions.h ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesFeatures.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesGetterSetters.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.cpp ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.h ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesStoreDefaultsMap.cpp
    DEPENDS ${WebKit_WEB_PREFERENCES_TEMPLATES} ${WebKit_WEB_PREFERENCES} WTF_CopyPreferences
    COMMAND ${Ruby_EXECUTABLE} ${WTF_SCRIPTS_DIR}/GeneratePreferences.rb --frontend WebKit --outputDir "${WebKit_DERIVED_SOURCES_DIR}" --template "$<JOIN:${WebKit_WEB_PREFERENCES_TEMPLATES},;--template;>" ${WebKit_WEB_PREFERENCES}
    COMMAND_EXPAND_LISTS
    VERBATIM)

list(APPEND WebKit_HEADERS
    ${WebKit_DERIVED_SOURCES_DIR}/SharedPreferencesForWebProcess.h
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesDefinitions.h
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.h
)

list(APPEND WebKit_SOURCES
    ${WebKit_DERIVED_SOURCES_DIR}/SharedPreferencesForWebProcess.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPageUpdatePreferences.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesFeatures.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesGetterSetters.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesKeys.cpp
    ${WebKit_DERIVED_SOURCES_DIR}/WebPreferencesStoreDefaultsMap.cpp
)

if (ENABLE_SWIFT_DEMO_URI_SCHEME)
    list(APPEND WebKit_SOURCES
        ${WEBKIT_DIR}/UIProcess/SwiftDemoLogo.swift
    )
endif ()

if (ENABLE_BACK_FORWARD_LIST_SWIFT)
    set_source_files_properties(
        ${WebKit_DERIVED_SOURCES_DIR}/WebBackForwardListMessageReceiver.swift
        PROPERTIES GENERATED TRUE)
    list(APPEND WebKit_SOURCES
        ${WebKit_DERIVED_SOURCES_DIR}/WebBackForwardListMessageReceiver.swift
        ${WEBKIT_DIR}/Shared/API/APIArray.swift
        ${WEBKIT_DIR}/UIProcess/StdlibExtras.swift
        ${WEBKIT_DIR}/UIProcess/WebBackForwardList.swift
    )
endif ()

if (SWIFT_REQUIRED)
    list(APPEND WebKit_SOURCES
        ${WEBKIT_DIR}/Shared/Foundation+Extras.swift
    )
endif ()

list(APPEND WebKit_SOURCES ${WebKit_DERIVED_SOURCES})

WEBKIT_COMPUTE_SOURCES(WebKit)
if (DEFINED WebKit_SWIFT_INTEROP_SOURCES)
    list(REMOVE_ITEM WebKit_SOURCES ${WebKit_SWIFT_INTEROP_SOURCES})
endif ()

WEBKIT_DEFINE_SUBTARGET_WITH_PREFIX(WebKit WebKitUIProcess
    PREFIX WebKitUIProcessPrefix.h
    PREFIX_LANGUAGES CXX OBJCXX
    DIRS UIProcess)
WEBKIT_DEFINE_SUBTARGET_WITH_PREFIX(WebKit WebKitWebProcess
    PREFIX WebKitWebProcessPrefix.h
    PREFIX_LANGUAGES CXX OBJCXX
    DIRS WebProcess)
WEBKIT_DEFINE_SUBTARGET_WITH_PREFIX(WebKit WebKitGPUProcess
    PREFIX WebKitGPUProcessPrefix.h
    PREFIX_LANGUAGES CXX OBJCXX
    DIRS GPUProcess)
WEBKIT_DEFINE_SUBTARGET_WITH_PREFIX(WebKit WebKitNetworkProcess
    PREFIX WebKitNetworkProcessPrefix.h
    PREFIX_LANGUAGES CXX OBJCXX
    DIRS NetworkProcess)
WEBKIT_DEFINE_SUBTARGET_WITH_PREFIX(WebKit WebKitShared
    PREFIX WebKitSharedPrefix.h
    PREFIX_LANGUAGES CXX OBJCXX
    DIRS DerivedSources)

WEBKIT_FRAMEWORK(WebKit)
WEBKIT_ADD_PREFIX_HEADER(WebKit WebKitPrefix.h PREFIX_LANGUAGES CXX OBJCXX)

if (SWIFT_REQUIRED AND "${PORT}" STREQUAL "Mac")
    WEBKIT_ADD_SWIFT_PREWARM(WebKit SwiftPrewarmMac.swift)
endif ()

if (DEFINED WebKit_SWIFT_INTEROP_SOURCES)
    WEBKIT_DEFINE_SUBTARGET(WebKit_SwiftInterop WebKit ${WebKit_SWIFT_INTEROP_SOURCES})
    WEBKIT_REUSE_PREFIX_HEADER(WebKit_SwiftInterop WebKit WebKitPrefix.h PREFIX_LANGUAGES CXX OBJCXX)
    target_compile_definitions(WebKit_SwiftInterop PRIVATE WK_COMPILING_SWIFT_INTEROP_SUBTARGET)
    # Feed objects into the WebKit link via a response file + LINK_DEPENDS so
    # cmake_object_order_depends_target_WebKit doesn't pick up this sub-target
    # (and through it, WebKit_SwiftCxxHeader).
    set(_interop_objs "$<FILTER:$<TARGET_OBJECTS:WebKit_SwiftInterop>,EXCLUDE,\\.(g|p)ch$>")
    set(_interop_rsp "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/WebKit_SwiftInterop.dir/$<CONFIG>/objects.rsp")
    file(GENERATE OUTPUT "${_interop_rsp}" CONTENT "$<JOIN:${_interop_objs},\n>")
    target_link_options(WebKit PRIVATE "@${_interop_rsp}")
    set_property(TARGET WebKit APPEND PROPERTY LINK_DEPENDS "${_interop_objs};${_interop_rsp}")
endif ()

if (WebKit_ARC_SOURCES)
    WEBKIT_DEFINE_SUBTARGET(WebKitARC WebKit ${WebKit_ARC_SOURCES})
    target_compile_options(WebKitARC PRIVATE -fobjc-arc)
    WEBKIT_ADD_PREFIX_HEADER(WebKitARC WebKitPrefix.h PREFIX_NO_CODEGEN PREFIX_LANGUAGES OBJCXX)
    target_sources(WebKit PRIVATE $<TARGET_OBJECTS:WebKitARC>)
endif ()

list(APPEND WebProcess_PRIVATE_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
WEBKIT_EXECUTABLE(WebProcess)
WEBKIT_REUSE_PREFIX_HEADER(WebProcess WebKit WebKitPrefix.h PREFIX_LANGUAGES CXX)

list(APPEND NetworkProcess_PRIVATE_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
WEBKIT_EXECUTABLE(NetworkProcess)
WEBKIT_REUSE_PREFIX_HEADER(NetworkProcess WebKit WebKitPrefix.h PREFIX_LANGUAGES CXX)

if (ENABLE_GPU_PROCESS)
    list(APPEND GPUProcess_PRIVATE_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:WebKit,INCLUDE_DIRECTORIES>)
    WEBKIT_EXECUTABLE(GPUProcess)
    WEBKIT_REUSE_PREFIX_HEADER(GPUProcess WebKit WebKitPrefix.h PREFIX_LANGUAGES CXX)
endif ()

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKit -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebProcess -Wno-unused-parameter)
    WEBKIT_ADD_TARGET_CXX_FLAGS(NetworkProcess -Wno-unused-parameter)
    if (ENABLE_GPU_PROCESS)
        WEBKIT_ADD_TARGET_CXX_FLAGS(GPUProcess -Wno-unused-parameter)
    endif ()
endif ()

if (APPLE)
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebKit -fobjc-weak)
    set_property(TARGET WebKit
        PROPERTY LINK_LIBRARY_OVERRIDE_WebKitLegacy REEXPORT_FRAMEWORK)
endif ()

if ("${PORT}" STREQUAL "Mac" OR "${PORT}" STREQUAL "IOS")
    WEBKIT_DEFINE_XPC_SERVICES()
else ()
    if (NOT ANDROID)
        WEBKIT_POPULATE_LIBRARY_VERSION(WEBKIT)
        set_target_properties(WebKit PROPERTIES VERSION ${WEBKIT_VERSION} SOVERSION ${WEBKIT_VERSION_MAJOR})
    endif ()

    install(TARGETS WebKit WebProcess NetworkProcess
        LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
        RUNTIME DESTINATION "${LIBEXEC_INSTALL_DIR}"
        FRAMEWORK DESTINATION "${LIB_INSTALL_DIR}"
        BUNDLE DESTINATION "${LIBEXEC_INSTALL_DIR}"
    )
    if (ENABLE_GPU_PROCESS)
        install(TARGETS GPUProcess
            LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
            RUNTIME DESTINATION "${LIBEXEC_INSTALL_DIR}"
            BUNDLE DESTINATION "${LIBEXEC_INSTALL_DIR}"
        )
    endif ()
endif ()
