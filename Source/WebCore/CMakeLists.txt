cmake_minimum_required(VERSION 3.10)

include(WebKitCommon)
include(target/TargetWTF)
include(target/TargetJavaScriptCore)
include(WebCoreMacros.cmake)
include(Headers.cmake)

set_property(DIRECTORY . PROPERTY FOLDER "WebCore")

add_subdirectory(PAL/pal)

if (PAL_LIBRARY_TYPE MATCHES STATIC)
    add_definitions(-DSTATICALLY_LINKED_WITH_PAL=1)
endif ()

list(APPEND WebCore_UNIFIED_SOURCE_LIST_FILES
    "Sources.txt"
)

set(WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${WebCore_DERIVED_SOURCES_DIR}"
    "${WEBCORE_DIR}"
    "${WEBCORE_DIR}/Modules/airplay"
    "${WEBCORE_DIR}/Modules/applepay"
    "${WEBCORE_DIR}/Modules/applepay/paymentrequest"
    "${WEBCORE_DIR}/Modules/applicationmanifest"
    "${WEBCORE_DIR}/Modules/async-clipboard"
    "${WEBCORE_DIR}/Modules/beacon"
    "${WEBCORE_DIR}/Modules/cache"
    "${WEBCORE_DIR}/Modules/credentialmanagement"
    "${WEBCORE_DIR}/Modules/encryptedmedia"
    "${WEBCORE_DIR}/Modules/encryptedmedia/legacy"
    "${WEBCORE_DIR}/Modules/entriesapi"
    "${WEBCORE_DIR}/Modules/fetch"
    "${WEBCORE_DIR}/Modules/geolocation"
    "${WEBCORE_DIR}/Modules/highlight"
    "${WEBCORE_DIR}/Modules/indexeddb"
    "${WEBCORE_DIR}/Modules/indexeddb/client"
    "${WEBCORE_DIR}/Modules/indexeddb/server"
    "${WEBCORE_DIR}/Modules/indexeddb/shared"
    "${WEBCORE_DIR}/Modules/mediacapabilities"
    "${WEBCORE_DIR}/Modules/mediacontrols"
    "${WEBCORE_DIR}/Modules/mediarecorder"
    "${WEBCORE_DIR}/Modules/mediasession"
    "${WEBCORE_DIR}/Modules/mediasource"
    "${WEBCORE_DIR}/Modules/mediastream"
    "${WEBCORE_DIR}/Modules/mediastream/libwebrtc"
    "${WEBCORE_DIR}/Modules/navigatorcontentutils"
    "${WEBCORE_DIR}/Modules/notifications"
    "${WEBCORE_DIR}/Modules/paymentrequest"
    "${WEBCORE_DIR}/Modules/pictureinpicture"
    "${WEBCORE_DIR}/Modules/plugins"
    "${WEBCORE_DIR}/Modules/quota"
    "${WEBCORE_DIR}/Modules/remoteplayback"
    "${WEBCORE_DIR}/Modules/speech"
    "${WEBCORE_DIR}/Modules/streams"
    "${WEBCORE_DIR}/Modules/webaudio"
    "${WEBCORE_DIR}/Modules/webauthn"
    "${WEBCORE_DIR}/Modules/webauthn/cbor"
    "${WEBCORE_DIR}/Modules/webauthn/fido"
    "${WEBCORE_DIR}/Modules/webdatabase"
    "${WEBCORE_DIR}/Modules/webdriver"
    "${WEBCORE_DIR}/Modules/webgpu"
    "${WEBCORE_DIR}/Modules/webgpu/WHLSL"
    "${WEBCORE_DIR}/Modules/webgpu/WHLSL/AST"
    "${WEBCORE_DIR}/Modules/websockets"
    "${WEBCORE_DIR}/Modules/webxr"
    "${WEBCORE_DIR}/accessibility"
    "${WEBCORE_DIR}/accessibility/isolatedtree"
    "${WEBCORE_DIR}/animation"
    "${WEBCORE_DIR}/bindings"
    "${WEBCORE_DIR}/bindings/js"
    "${WEBCORE_DIR}/bridge"
    "${WEBCORE_DIR}/bridge/c"
    "${WEBCORE_DIR}/bridge/jsc"
    "${WEBCORE_DIR}/contentextensions"
    "${WEBCORE_DIR}/crypto"
    "${WEBCORE_DIR}/crypto/algorithms"
    "${WEBCORE_DIR}/crypto/keys"
    "${WEBCORE_DIR}/crypto/parameters"
    "${WEBCORE_DIR}/css"
    "${WEBCORE_DIR}/css/parser"
    "${WEBCORE_DIR}/css/typedom"
    "${WEBCORE_DIR}/cssjit"
    "${WEBCORE_DIR}/display"
    "${WEBCORE_DIR}/display/css"
    "${WEBCORE_DIR}/dom"
    "${WEBCORE_DIR}/dom/messageports"
    "${WEBCORE_DIR}/domjit"
    "${WEBCORE_DIR}/editing"
    "${WEBCORE_DIR}/fileapi"
    "${WEBCORE_DIR}/history"
    "${WEBCORE_DIR}/html"
    "${WEBCORE_DIR}/html/canvas"
    "${WEBCORE_DIR}/html/forms"
    "${WEBCORE_DIR}/html/parser"
    "${WEBCORE_DIR}/html/shadow"
    "${WEBCORE_DIR}/html/track"
    "${WEBCORE_DIR}/inspector"
    "${WEBCORE_DIR}/inspector/agents"
    "${WEBCORE_DIR}/inspector/agents/page"
    "${WEBCORE_DIR}/inspector/agents/worker"
    "${WEBCORE_DIR}/layout"
    "${WEBCORE_DIR}/layout/blockformatting"
    "${WEBCORE_DIR}/layout/blockformatting/tablewrapper"
    "${WEBCORE_DIR}/layout/displaytree"
    "${WEBCORE_DIR}/layout/flexformatting"
    "${WEBCORE_DIR}/layout/floats"
    "${WEBCORE_DIR}/layout/inlineformatting"
    "${WEBCORE_DIR}/layout/inlineformatting/text"
    "${WEBCORE_DIR}/layout/integration"
    "${WEBCORE_DIR}/layout/invalidation"
    "${WEBCORE_DIR}/layout/layouttree"
    "${WEBCORE_DIR}/layout/tableformatting"
    "${WEBCORE_DIR}/loader"
    "${WEBCORE_DIR}/loader/appcache"
    "${WEBCORE_DIR}/loader/archive"
    "${WEBCORE_DIR}/loader/archive/mhtml"
    "${WEBCORE_DIR}/loader/cache"
    "${WEBCORE_DIR}/loader/icon"
    "${WEBCORE_DIR}/mathml"
    "${WEBCORE_DIR}/page"
    "${WEBCORE_DIR}/page/csp"
    "${WEBCORE_DIR}/page/scrolling"
    "${WEBCORE_DIR}/platform"
    "${WEBCORE_DIR}/platform/animation"
    "${WEBCORE_DIR}/platform/audio"
    "${WEBCORE_DIR}/platform/encryptedmedia"
    "${WEBCORE_DIR}/platform/gamepad"
    "${WEBCORE_DIR}/platform/graphics"
    "${WEBCORE_DIR}/platform/graphics/cpu/arm"
    "${WEBCORE_DIR}/platform/graphics/cpu/arm/filters"
    "${WEBCORE_DIR}/platform/graphics/displaylists"
    "${WEBCORE_DIR}/platform/graphics/filters"
    "${WEBCORE_DIR}/platform/graphics/iso"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/transforms"
    "${WEBCORE_DIR}/platform/mediacapabilities"
    "${WEBCORE_DIR}/platform/mediarecorder"
    "${WEBCORE_DIR}/platform/mediasession"
    "${WEBCORE_DIR}/platform/mediastream"
    "${WEBCORE_DIR}/platform/mediastream/libwebrtc"
    "${WEBCORE_DIR}/platform/mock"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/network"
    "${WEBCORE_DIR}/platform/sql"
    "${WEBCORE_DIR}/platform/text"
    "${WEBCORE_DIR}/platform/xr"
    "${WEBCORE_DIR}/plugins"
    "${WEBCORE_DIR}/rendering"
    "${WEBCORE_DIR}/rendering/line"
    "${WEBCORE_DIR}/rendering/mathml"
    "${WEBCORE_DIR}/rendering/shapes"
    "${WEBCORE_DIR}/rendering/style"
    "${WEBCORE_DIR}/rendering/svg"
    "${WEBCORE_DIR}/rendering/updating"
    "${WEBCORE_DIR}/replay"
    "${WEBCORE_DIR}/storage"
    "${WEBCORE_DIR}/style"
    "${WEBCORE_DIR}/svg"
    "${WEBCORE_DIR}/svg/animation"
    "${WEBCORE_DIR}/svg/graphics"
    "${WEBCORE_DIR}/svg/graphics/filters"
    "${WEBCORE_DIR}/svg/properties"
    "${WEBCORE_DIR}/websockets"
    "${WEBCORE_DIR}/workers"
    "${WEBCORE_DIR}/workers/service"
    "${WEBCORE_DIR}/workers/service/context"
    "${WEBCORE_DIR}/workers/service/server"
    "${WEBCORE_DIR}/worklets"
    "${WEBCORE_DIR}/xml"
    "${WEBCORE_DIR}/xml/parser"
)

set(WebCore_IDL_INCLUDES
    css
    dom
    fileapi
    html
    inspector
    page
    plugins
    storage
    svg
    workers
    xml

    Modules/airplay
    Modules/cache
    Modules/credentialmanagement
    Modules/encryptedmedia/legacy
    Modules/entriesapi
    Modules/fetch
    Modules/geolocation
    Modules/highlight
    Modules/indexeddb
    Modules/mediacapabilities
    Modules/mediarecorder
    Modules/mediasession
    Modules/mediastream
    Modules/notifications
    Modules/paymentrequest
    Modules/quota
    Modules/speech
    Modules/streams
    Modules/webaudio
    Modules/webdatabase
    Modules/webgpu
    Modules/websockets

    bindings/js

    css/typedom

    html/canvas
    html/shadow
    html/track

    loader/appcache
)

set(WebCore_NON_SVG_IDL_FILES
    Modules/airplay/WebKitPlaybackTargetAvailabilityEvent.idl

    Modules/async-clipboard/Clipboard.idl
    Modules/async-clipboard/ClipboardItem.idl
    Modules/async-clipboard/Navigator+Clipboard.idl

    Modules/beacon/Navigator+Beacon.idl

    Modules/cache/CacheQueryOptions.idl
    Modules/cache/DOMCache.idl
    Modules/cache/DOMCacheStorage.idl
    Modules/cache/DOMWindow+Caches.idl
    Modules/cache/WorkerGlobalScope+Caches.idl

    Modules/credentialmanagement/BasicCredential.idl
    Modules/credentialmanagement/CredentialCreationOptions.idl
    Modules/credentialmanagement/CredentialRequestOptions.idl
    Modules/credentialmanagement/CredentialsContainer.idl
    Modules/credentialmanagement/Navigator+Credentials.idl

    Modules/encryptedmedia/MediaKeyEncryptionScheme.idl
    Modules/encryptedmedia/MediaKeyMessageEvent.idl
    Modules/encryptedmedia/MediaKeySession.idl
    Modules/encryptedmedia/MediaKeySessionType.idl
    Modules/encryptedmedia/MediaKeyStatusMap.idl
    Modules/encryptedmedia/MediaKeySystemAccess.idl
    Modules/encryptedmedia/MediaKeySystemConfiguration.idl
    Modules/encryptedmedia/MediaKeySystemMediaCapability.idl
    Modules/encryptedmedia/MediaKeys.idl
    Modules/encryptedmedia/MediaKeysRequirement.idl
    Modules/encryptedmedia/Navigator+EME.idl

    Modules/entriesapi/DOMFileSystem.idl
    Modules/entriesapi/ErrorCallback.idl
    Modules/entriesapi/FileCallback.idl
    Modules/entriesapi/FileSystemDirectoryEntry.idl
    Modules/entriesapi/FileSystemDirectoryReader.idl
    Modules/entriesapi/FileSystemEntriesCallback.idl
    Modules/entriesapi/FileSystemEntry.idl
    Modules/entriesapi/FileSystemEntryCallback.idl
    Modules/entriesapi/FileSystemFileEntry.idl
    Modules/entriesapi/HTMLInputElement+EntriesAPI.idl

    Modules/fetch/FetchBody.idl
    Modules/fetch/FetchHeaders.idl
    Modules/fetch/FetchReferrerPolicy.idl
    Modules/fetch/FetchRequest.idl
    Modules/fetch/FetchRequestCache.idl
    Modules/fetch/FetchRequestCredentials.idl
    Modules/fetch/FetchRequestInit.idl
    Modules/fetch/FetchRequestMode.idl
    Modules/fetch/FetchRequestRedirect.idl
    Modules/fetch/FetchResponse.idl
    Modules/fetch/WindowOrWorkerGlobalScope+Fetch.idl

    Modules/geolocation/Geolocation.idl
    Modules/geolocation/GeolocationCoordinates.idl
    Modules/geolocation/GeolocationPosition.idl
    Modules/geolocation/GeolocationPositionError.idl
    Modules/geolocation/Navigator+Geolocation.idl
    Modules/geolocation/PositionCallback.idl
    Modules/geolocation/PositionErrorCallback.idl
    Modules/geolocation/PositionOptions.idl

    Modules/highlight/HighlightMap.idl
    Modules/highlight/HighlightRangeGroup.idl

    Modules/indexeddb/DOMWindow+IndexedDatabase.idl
    Modules/indexeddb/IDBCursor.idl
    Modules/indexeddb/IDBCursorDirection.idl
    Modules/indexeddb/IDBCursorWithValue.idl
    Modules/indexeddb/IDBDatabase.idl
    Modules/indexeddb/IDBFactory.idl
    Modules/indexeddb/IDBIndex.idl
    Modules/indexeddb/IDBKeyRange.idl
    Modules/indexeddb/IDBObjectStore.idl
    Modules/indexeddb/IDBOpenDBRequest.idl
    Modules/indexeddb/IDBRequest.idl
    Modules/indexeddb/IDBTransaction.idl
    Modules/indexeddb/IDBTransactionMode.idl
    Modules/indexeddb/IDBVersionChangeEvent.idl
    Modules/indexeddb/WorkerGlobalScope+IndexedDatabase.idl

    Modules/mediacapabilities/AudioConfiguration.idl
    Modules/mediacapabilities/ColorGamut.idl
    Modules/mediacapabilities/HdrMetadataType.idl
    Modules/mediacapabilities/MediaCapabilities.idl
    Modules/mediacapabilities/MediaCapabilitiesDecodingInfo.idl
    Modules/mediacapabilities/MediaCapabilitiesEncodingInfo.idl
    Modules/mediacapabilities/MediaCapabilitiesInfo.idl
    Modules/mediacapabilities/MediaConfiguration.idl
    Modules/mediacapabilities/MediaDecodingConfiguration.idl
    Modules/mediacapabilities/MediaDecodingType.idl
    Modules/mediacapabilities/MediaEncodingConfiguration.idl
    Modules/mediacapabilities/MediaEncodingType.idl
    Modules/mediacapabilities/Navigator+MediaCapabilities.idl
    Modules/mediacapabilities/TransferFunction.idl
    Modules/mediacapabilities/VideoConfiguration.idl

    Modules/mediacontrols/MediaControlsHost.idl

    Modules/mediarecorder/BlobEvent.idl
    Modules/mediarecorder/MediaRecorder.idl
    Modules/mediarecorder/MediaRecorderErrorEvent.idl

    Modules/mediasession/HTMLMediaElement+MediaSession.idl
    Modules/mediasession/MediaRemoteControls.idl
    Modules/mediasession/MediaSession.idl

    Modules/mediasource/AudioTrack+MediaSource.idl
    Modules/mediasource/DOMURL+MediaSource.idl
    Modules/mediasource/MediaSource.idl
    Modules/mediasource/SourceBuffer.idl
    Modules/mediasource/SourceBufferList.idl
    Modules/mediasource/TextTrack+MediaSource.idl
    Modules/mediasource/VideoPlaybackQuality.idl
    Modules/mediasource/VideoTrack+MediaSource.idl

    Modules/mediastream/CanvasCaptureMediaStreamTrack.idl
    Modules/mediastream/DoubleRange.idl
    Modules/mediastream/LongRange.idl
    Modules/mediastream/MediaDeviceInfo.idl
    Modules/mediastream/MediaDevices.idl
    Modules/mediastream/MediaStream.idl
    Modules/mediastream/MediaStreamTrack.idl
    Modules/mediastream/MediaStreamTrackEvent.idl
    Modules/mediastream/MediaTrackConstraints.idl
    Modules/mediastream/MediaTrackSupportedConstraints.idl
    Modules/mediastream/Navigator+MediaDevices.idl
    Modules/mediastream/OverconstrainedError.idl
    Modules/mediastream/OverconstrainedErrorEvent.idl
    Modules/mediastream/RTCAnswerOptions.idl
    Modules/mediastream/RTCCertificate.idl
    Modules/mediastream/RTCConfiguration.idl
    Modules/mediastream/RTCDTMFSender.idl
    Modules/mediastream/RTCDTMFToneChangeEvent.idl
    Modules/mediastream/RTCDataChannel.idl
    Modules/mediastream/RTCDataChannelEvent.idl
    Modules/mediastream/RTCDegradationPreference.idl
    Modules/mediastream/RTCDtxStatus.idl
    Modules/mediastream/RTCIceCandidate.idl
    Modules/mediastream/RTCIceCandidateInit.idl
    Modules/mediastream/RTCIceCandidateType.idl
    Modules/mediastream/RTCIceComponent.idl
    Modules/mediastream/RTCIceConnectionState.idl
    Modules/mediastream/RTCIceGatheringState.idl
    Modules/mediastream/RTCIceProtocol.idl
    Modules/mediastream/RTCIceServer.idl
    Modules/mediastream/RTCIceTcpCandidateType.idl
    Modules/mediastream/RTCIceTransport.idl
    Modules/mediastream/RTCIceTransportState.idl
    Modules/mediastream/RTCOfferAnswerOptions.idl
    Modules/mediastream/RTCOfferOptions.idl
    Modules/mediastream/RTCPeerConnection.idl
    Modules/mediastream/RTCPeerConnectionIceEvent.idl
    Modules/mediastream/RTCPeerConnectionState.idl
    Modules/mediastream/RTCPriorityType.idl
    Modules/mediastream/RTCRtpCapabilities.idl
    Modules/mediastream/RTCRtpCodecCapability.idl
    Modules/mediastream/RTCRtpCodecParameters.idl
    Modules/mediastream/RTCRtpCodingParameters.idl
    Modules/mediastream/RTCRtpContributingSource.idl
    Modules/mediastream/RTCRtpEncodingParameters.idl
    Modules/mediastream/RTCRtpFecParameters.idl
    Modules/mediastream/RTCRtpHeaderExtensionParameters.idl
    Modules/mediastream/RTCRtpParameters.idl
    Modules/mediastream/RTCRtpReceiver.idl
    Modules/mediastream/RTCRtpRtxParameters.idl
    Modules/mediastream/RTCRtpSendParameters.idl
    Modules/mediastream/RTCRtpSender.idl
    Modules/mediastream/RTCRtpSynchronizationSource.idl
    Modules/mediastream/RTCRtpTransceiver.idl
    Modules/mediastream/RTCRtpTransceiverDirection.idl
    Modules/mediastream/RTCSdpType.idl
    Modules/mediastream/RTCSessionDescription.idl
    Modules/mediastream/RTCSessionDescriptionInit.idl
    Modules/mediastream/RTCSignalingState.idl
    Modules/mediastream/RTCStatsReport.idl
    Modules/mediastream/RTCTrackEvent.idl

    Modules/notifications/Notification.idl
    Modules/notifications/NotificationPermission.idl
    Modules/notifications/NotificationPermissionCallback.idl

    Modules/paymentrequest/AddressErrors.idl
    Modules/paymentrequest/MerchantValidationEvent.idl
    Modules/paymentrequest/PayerErrorFields.idl
    Modules/paymentrequest/PaymentAddress.idl
    Modules/paymentrequest/PaymentComplete.idl
    Modules/paymentrequest/PaymentCurrencyAmount.idl
    Modules/paymentrequest/PaymentDetailsBase.idl
    Modules/paymentrequest/PaymentDetailsInit.idl
    Modules/paymentrequest/PaymentDetailsModifier.idl
    Modules/paymentrequest/PaymentDetailsUpdate.idl
    Modules/paymentrequest/PaymentItem.idl
    Modules/paymentrequest/PaymentMethodChangeEvent.idl
    Modules/paymentrequest/PaymentMethodData.idl
    Modules/paymentrequest/PaymentOptions.idl
    Modules/paymentrequest/PaymentRequest.idl
    Modules/paymentrequest/PaymentRequestUpdateEvent.idl
    Modules/paymentrequest/PaymentRequestUpdateEventInit.idl
    Modules/paymentrequest/PaymentResponse.idl
    Modules/paymentrequest/PaymentShippingOption.idl
    Modules/paymentrequest/PaymentShippingType.idl
    Modules/paymentrequest/PaymentValidationErrors.idl

    Modules/pictureinpicture/Document+PictureInPicture.idl
    Modules/pictureinpicture/DocumentOrShadowRoot+PictureInPicture.idl
    Modules/pictureinpicture/EnterPictureInPictureEvent.idl
    Modules/pictureinpicture/HTMLVideoElement+PictureInPicture.idl
    Modules/pictureinpicture/PictureInPictureWindow.idl

    Modules/remoteplayback/RemotePlayback.idl
    Modules/remoteplayback/RemotePlaybackAvailabilityCallback.idl

    Modules/speech/DOMWindow+SpeechSynthesis.idl
    Modules/speech/SpeechSynthesis.idl
    Modules/speech/SpeechSynthesisEvent.idl
    Modules/speech/SpeechSynthesisUtterance.idl
    Modules/speech/SpeechSynthesisVoice.idl

    Modules/streams/ByteLengthQueuingStrategy.idl
    Modules/streams/CountQueuingStrategy.idl
    Modules/streams/ReadableByteStreamController.idl
    Modules/streams/ReadableStream.idl
    Modules/streams/ReadableStreamBYOBReader.idl
    Modules/streams/ReadableStreamBYOBRequest.idl
    Modules/streams/ReadableStreamDefaultController.idl
    Modules/streams/ReadableStreamDefaultReader.idl
    Modules/streams/ReadableStreamSink.idl
    Modules/streams/ReadableStreamSource.idl
    Modules/streams/TransformStream.idl
    Modules/streams/TransformStreamDefaultController.idl
    Modules/streams/WritableStream.idl
    Modules/streams/WritableStreamDefaultController.idl
    Modules/streams/WritableStreamDefaultWriter.idl

    Modules/webaudio/AnalyserNode.idl
    Modules/webaudio/AnalyserOptions.idl
    Modules/webaudio/AudioBuffer.idl
    Modules/webaudio/AudioBufferCallback.idl
    Modules/webaudio/AudioBufferOptions.idl
    Modules/webaudio/AudioBufferSourceNode.idl
    Modules/webaudio/AudioBufferSourceOptions.idl
    Modules/webaudio/AudioContext.idl
    Modules/webaudio/AudioContextLatencyCategory.idl
    Modules/webaudio/AudioContextOptions.idl
    Modules/webaudio/AudioContextState.idl
    Modules/webaudio/AudioDestinationNode.idl
    Modules/webaudio/AudioListener.idl
    Modules/webaudio/AudioNode.idl
    Modules/webaudio/AudioNodeOptions.idl
    Modules/webaudio/AudioParam.idl
    Modules/webaudio/AudioProcessingEvent.idl
    Modules/webaudio/AudioProcessingEventInit.idl
    Modules/webaudio/AudioScheduledSourceNode.idl
    Modules/webaudio/AudioTimestamp.idl
    Modules/webaudio/BaseAudioContext.idl
    Modules/webaudio/BiquadFilterNode.idl
    Modules/webaudio/BiquadFilterOptions.idl
    Modules/webaudio/BiquadFilterType.idl
    Modules/webaudio/ChannelCountMode.idl
    Modules/webaudio/ChannelInterpretation.idl
    Modules/webaudio/ChannelMergerNode.idl
    Modules/webaudio/ChannelMergerOptions.idl
    Modules/webaudio/ChannelSplitterNode.idl
    Modules/webaudio/ChannelSplitterOptions.idl
    Modules/webaudio/ConstantSourceNode.idl
    Modules/webaudio/ConstantSourceOptions.idl
    Modules/webaudio/ConvolverNode.idl
    Modules/webaudio/ConvolverOptions.idl
    Modules/webaudio/DelayNode.idl
    Modules/webaudio/DelayOptions.idl
    Modules/webaudio/DistanceModelType.idl
    Modules/webaudio/DynamicsCompressorNode.idl
    Modules/webaudio/DynamicsCompressorOptions.idl
    Modules/webaudio/GainNode.idl
    Modules/webaudio/GainOptions.idl
    Modules/webaudio/IIRFilterNode.idl
    Modules/webaudio/IIRFilterOptions.idl
    Modules/webaudio/MediaElementAudioSourceNode.idl
    Modules/webaudio/MediaElementAudioSourceOptions.idl
    Modules/webaudio/MediaStreamAudioDestinationNode.idl
    Modules/webaudio/MediaStreamAudioSourceNode.idl
    Modules/webaudio/MediaStreamAudioSourceOptions.idl
    Modules/webaudio/OfflineAudioCompletionEvent.idl
    Modules/webaudio/OfflineAudioCompletionEventInit.idl
    Modules/webaudio/OfflineAudioContext.idl
    Modules/webaudio/OfflineAudioContextOptions.idl
    Modules/webaudio/OscillatorNode.idl
    Modules/webaudio/OscillatorOptions.idl
    Modules/webaudio/OscillatorType.idl
    Modules/webaudio/OverSampleType.idl
    Modules/webaudio/PannerNode.idl
    Modules/webaudio/PannerOptions.idl
    Modules/webaudio/PanningModelType.idl
    Modules/webaudio/PeriodicWave.idl
    Modules/webaudio/PeriodicWaveConstraints.idl
    Modules/webaudio/PeriodicWaveOptions.idl
    Modules/webaudio/StereoPannerNode.idl
    Modules/webaudio/StereoPannerOptions.idl
    Modules/webaudio/ScriptProcessorNode.idl
    Modules/webaudio/WaveShaperNode.idl
    Modules/webaudio/WaveShaperOptions.idl
    Modules/webaudio/WebKitAudioBufferSourceNode.idl
    Modules/webaudio/WebKitAudioContext.idl
    Modules/webaudio/WebKitAudioListener.idl
    Modules/webaudio/WebKitAudioPannerNode.idl
    Modules/webaudio/WebKitDynamicsCompressorNode.idl
    Modules/webaudio/WebKitOfflineAudioContext.idl
    Modules/webaudio/WebKitOscillatorNode.idl

    Modules/webauthn/AttestationConveyancePreference.idl
    Modules/webauthn/AuthenticationExtensionsClientInputs.idl
    Modules/webauthn/AuthenticationExtensionsClientOutputs.idl
    Modules/webauthn/AuthenticatorAssertionResponse.idl
    Modules/webauthn/AuthenticatorAttestationResponse.idl
    Modules/webauthn/AuthenticatorResponse.idl
    Modules/webauthn/AuthenticatorTransport.idl
    Modules/webauthn/PublicKeyCredential.idl
    Modules/webauthn/PublicKeyCredentialCreationOptions.idl
    Modules/webauthn/PublicKeyCredentialDescriptor.idl
    Modules/webauthn/PublicKeyCredentialRequestOptions.idl
    Modules/webauthn/PublicKeyCredentialType.idl
    Modules/webauthn/UserVerificationRequirement.idl

    Modules/webdatabase/DOMWindow+WebDatabase.idl
    Modules/webdatabase/Database.idl
    Modules/webdatabase/DatabaseCallback.idl
    Modules/webdatabase/SQLError.idl
    Modules/webdatabase/SQLResultSet.idl
    Modules/webdatabase/SQLResultSetRowList.idl
    Modules/webdatabase/SQLStatementCallback.idl
    Modules/webdatabase/SQLStatementErrorCallback.idl
    Modules/webdatabase/SQLTransaction.idl
    Modules/webdatabase/SQLTransactionCallback.idl
    Modules/webdatabase/SQLTransactionErrorCallback.idl

    Modules/webdriver/Navigator+WebDriver.idl

    Modules/webgpu/GPUBindGroupLayoutBinding.idl
    Modules/webgpu/GPUBindGroupLayoutDescriptor.idl
    Modules/webgpu/GPUBlendDescriptor.idl
    Modules/webgpu/GPUBufferDescriptor.idl
    Modules/webgpu/GPUBufferUsage.idl
    Modules/webgpu/GPUCanvasContext.idl
    Modules/webgpu/GPUColor.idl
    Modules/webgpu/GPUColorStateDescriptor.idl
    Modules/webgpu/GPUColorWrite.idl
    Modules/webgpu/GPUCompareFunction.idl
    Modules/webgpu/GPUDepthStencilStateDescriptor.idl
    Modules/webgpu/GPUErrorFilter.idl
    Modules/webgpu/GPUExtent3D.idl
    Modules/webgpu/GPULoadOp.idl
    Modules/webgpu/GPUOrigin3D.idl
    Modules/webgpu/GPUOutOfMemoryError.idl
    Modules/webgpu/GPURequestAdapterOptions.idl
    Modules/webgpu/GPUSamplerDescriptor.idl
    Modules/webgpu/GPUShaderStage.idl
    Modules/webgpu/GPUStoreOp.idl
    Modules/webgpu/GPUTextureDescriptor.idl
    Modules/webgpu/GPUTextureFormat.idl
    Modules/webgpu/GPUTextureUsage.idl
    Modules/webgpu/GPUUncapturedErrorEvent.idl
    Modules/webgpu/GPUValidationError.idl
    Modules/webgpu/GPUVertexAttributeDescriptor.idl
    Modules/webgpu/GPUVertexBufferDescriptor.idl
    Modules/webgpu/GPUVertexInputDescriptor.idl
    Modules/webgpu/Navigator+GPU.idl
    Modules/webgpu/WebGPU.idl
    Modules/webgpu/WebGPUAdapter.idl
    Modules/webgpu/WebGPUBindGroup.idl
    Modules/webgpu/WebGPUBindGroupBinding.idl
    Modules/webgpu/WebGPUBindGroupDescriptor.idl
    Modules/webgpu/WebGPUBindGroupLayout.idl
    Modules/webgpu/WebGPUBuffer.idl
    Modules/webgpu/WebGPUBufferBinding.idl
    Modules/webgpu/WebGPUCommandBuffer.idl
    Modules/webgpu/WebGPUCommandEncoder.idl
    Modules/webgpu/WebGPUComputePassEncoder.idl
    Modules/webgpu/WebGPUComputePipeline.idl
    Modules/webgpu/WebGPUComputePipelineDescriptor.idl
    Modules/webgpu/WebGPUDevice.idl
    Modules/webgpu/WebGPUDeviceErrorScopes.idl
    Modules/webgpu/WebGPUDeviceEventHandler.idl
    Modules/webgpu/WebGPUPipelineDescriptorBase.idl
    Modules/webgpu/WebGPUPipelineLayout.idl
    Modules/webgpu/WebGPUPipelineLayoutDescriptor.idl
    Modules/webgpu/WebGPUProgrammableStageDescriptor.idl
    Modules/webgpu/WebGPUProgrammablePassEncoder.idl
    Modules/webgpu/WebGPUQueue.idl
    Modules/webgpu/WebGPURenderPassDescriptor.idl
    Modules/webgpu/WebGPURenderPassEncoder.idl
    Modules/webgpu/WebGPURenderPipeline.idl
    Modules/webgpu/WebGPURenderPipelineDescriptor.idl
    Modules/webgpu/WebGPUSampler.idl
    Modules/webgpu/WebGPUShaderModule.idl
    Modules/webgpu/WebGPUShaderModuleDescriptor.idl
    Modules/webgpu/WebGPUSwapChain.idl
    Modules/webgpu/WebGPUTexture.idl
    Modules/webgpu/WebGPUTextureView.idl
    Modules/webgpu/WorkerNavigator+GPU.idl

    Modules/websockets/CloseEvent.idl
    Modules/websockets/WebSocket.idl

    accessibility/AccessibilityRole.idl
    accessibility/AriaAttributes.idl

    animation/Animatable.idl
    animation/AnimationEffect.idl
    animation/AnimationFrameProvider.idl
    animation/EffectTiming.idl
    animation/AnimationPlaybackEvent.idl
    animation/AnimationPlaybackEventInit.idl
    animation/AnimationTimeline.idl
    animation/CSSAnimation.idl
    animation/CSSTransition.idl
    animation/CompositeOperation.idl
    animation/CompositeOperationOrAuto.idl
    animation/ComputedEffectTiming.idl
    animation/Document+WebAnimations.idl
    animation/DocumentOrShadowRoot+WebAnimations.idl
    animation/DocumentTimeline.idl
    animation/DocumentTimelineOptions.idl
    animation/FillMode.idl
    animation/GetAnimationsOptions.idl
    animation/GlobalEventHandlers+CSSAnimations.idl
    animation/GlobalEventHandlers+CSSTransitions.idl
    animation/IterationCompositeOperation.idl
    animation/KeyframeAnimationOptions.idl
    animation/KeyframeEffect.idl
    animation/KeyframeEffectOptions.idl
    animation/OptionalEffectTiming.idl
    animation/PlaybackDirection.idl
    animation/WebAnimation.idl

    crypto/CryptoAlgorithmParameters.idl
    crypto/CryptoKey.idl
    crypto/CryptoKeyPair.idl
    crypto/CryptoKeyUsage.idl
    crypto/JsonWebKey.idl
    crypto/RsaOtherPrimesInfo.idl
    crypto/SubtleCrypto.idl

    crypto/keys/CryptoAesKeyAlgorithm.idl
    crypto/keys/CryptoEcKeyAlgorithm.idl
    crypto/keys/CryptoHmacKeyAlgorithm.idl
    crypto/keys/CryptoKeyAlgorithm.idl
    crypto/keys/CryptoRsaHashedKeyAlgorithm.idl
    crypto/keys/CryptoRsaKeyAlgorithm.idl

    crypto/parameters/AesCbcCfbParams.idl
    crypto/parameters/AesCtrParams.idl
    crypto/parameters/AesGcmParams.idl
    crypto/parameters/AesKeyParams.idl
    crypto/parameters/EcKeyParams.idl
    crypto/parameters/EcdhKeyDeriveParams.idl
    crypto/parameters/EcdsaParams.idl
    crypto/parameters/HkdfParams.idl
    crypto/parameters/HmacKeyParams.idl
    crypto/parameters/Pbkdf2Params.idl
    crypto/parameters/RsaHashedImportParams.idl
    crypto/parameters/RsaHashedKeyGenParams.idl
    crypto/parameters/RsaKeyGenParams.idl
    crypto/parameters/RsaOaepParams.idl
    crypto/parameters/RsaPssParams.idl

    css/CSSConditionRule.idl
    css/CSSFontFaceRule.idl
    css/CSSGroupingRule.idl
    css/CSSImportRule.idl
    css/CSSKeyframeRule.idl
    css/CSSKeyframesRule.idl
    css/CSSMediaRule.idl
    css/CSSNamespaceRule.idl
    css/CSSPaintCallback.idl
    css/CSSPaintSize.idl
    css/CSSPageRule.idl
    css/CSSRule.idl
    css/CSSRuleList.idl
    css/CSSStyleDeclaration.idl
    css/CSSStyleRule.idl
    css/CSSStyleSheet.idl
    css/CSSSupportsRule.idl
    css/CSSUnknownRule.idl
    css/DocumentOrShadowRoot+CSSOM.idl
    css/DOMCSSCustomPropertyDescriptor.idl
    css/DOMCSSNamespace+CSSPainting.idl
    css/DOMCSSNamespace+CSSPropertiesandValues.idl
    css/DOMCSSNamespace.idl
    css/DOMMatrix.idl
    css/DOMMatrix2DInit.idl
    css/DOMMatrixInit.idl
    css/DOMMatrixReadOnly.idl
    css/DeprecatedCSSOMCounter.idl
    css/DeprecatedCSSOMPrimitiveValue.idl
    css/DeprecatedCSSOMRGBColor.idl
    css/DeprecatedCSSOMRect.idl
    css/DeprecatedCSSOMValue.idl
    css/DeprecatedCSSOMValueList.idl
    css/ElementCSSInlineStyle.idl
    css/FontFace.idl
    css/FontFaceSet.idl
    css/FontFaceSource.idl
    css/MediaList.idl
    css/MediaQueryList.idl
    css/MediaQueryListEvent.idl
    css/StyleMedia.idl
    css/StyleSheet.idl
    css/StyleSheetList.idl

    css/typedom/StylePropertyMap.idl
    css/typedom/StylePropertyMapReadOnly.idl
    css/typedom/TypedOMCSSImageValue.idl
    css/typedom/TypedOMCSSNumericValue.idl
    css/typedom/TypedOMCSSStyleValue.idl
    css/typedom/TypedOMCSSUnitValue.idl
    css/typedom/TypedOMCSSUnparsedValue.idl

    css/WebKitCSSMatrix.idl

    dom/AbortAlgorithm.idl
    dom/AbortController.idl
    dom/AbortSignal.idl
    dom/AnimationEvent.idl
    dom/Attr.idl
    dom/BeforeLoadEvent.idl
    dom/BeforeUnloadEvent.idl
    dom/CDATASection.idl
    dom/CharacterData.idl
    dom/ChildNode.idl
    dom/ClipboardEvent.idl
    dom/Comment.idl
    dom/CompositionEvent.idl
    dom/CustomElementRegistry.idl
    dom/CustomEvent.idl
    dom/DOMException.idl
    dom/DOMImplementation.idl
    dom/DOMPoint.idl
    dom/DOMPointInit.idl
    dom/DOMPointReadOnly.idl
    dom/DOMQuad.idl
    dom/DOMQuadInit.idl
    dom/DOMRect.idl
    dom/DOMRectInit.idl
    dom/DOMRectList.idl
    dom/DOMRectReadOnly.idl
    dom/DOMStringList.idl
    dom/DOMStringMap.idl
    dom/DataTransfer.idl
    dom/DataTransferItem.idl
    dom/DataTransferItemList.idl
    dom/DeviceMotionEvent.idl
    dom/DeviceOrientationEvent.idl
    dom/DeviceOrientationOrMotionPermissionState.idl
    dom/Document+CSSOMView.idl
    dom/Document+Fullscreen.idl
    dom/Document+HTML.idl
    dom/Document+HTMLObsolete.idl
    dom/Document+PageVisibility.idl
    dom/Document+PointerLock.idl
    dom/Document+Selection.idl
    dom/Document+StorageAccess.idl
    dom/Document+Touch.idl
    dom/Document+UndoMananger.idl
    dom/Document.idl
    dom/DocumentAndElementEventHandlers.idl
    dom/DocumentFragment.idl
    dom/DocumentOrShadowRoot+PointerLock.idl
    dom/DocumentOrShadowRoot.idl
    dom/DocumentType.idl
    dom/DragEvent.idl
    dom/Element+DOMParsing.idl
    dom/Element.idl
    dom/ErrorEvent.idl
    dom/Event.idl
    dom/EventInit.idl
    dom/EventModifierInit.idl
    dom/EventTarget.idl
    dom/FocusEvent.idl
    dom/GlobalEventHandlers+PointerEvents.idl
    dom/GlobalEventHandlers.idl
    dom/HashChangeEvent.idl
    dom/IdleDeadline.idl
    dom/IdleRequestCallback.idl
    dom/IdleRequestOptions.idl
    dom/InnerHTML.idl
    dom/InputEvent.idl
    dom/KeyboardEvent.idl
    dom/MessageChannel.idl
    dom/MessageEvent.idl
    dom/MessagePort.idl
    dom/MouseEvent.idl
    dom/MouseEventInit.idl
    dom/MutationCallback.idl
    dom/MutationEvent.idl
    dom/MutationObserver.idl
    dom/MutationRecord.idl
    dom/NamedNodeMap.idl
    dom/NavigatorMaxTouchPoints.idl
    dom/Node.idl
    dom/NodeFilter.idl
    dom/NodeIterator.idl
    dom/NodeList.idl
    dom/NonDocumentTypeChildNode.idl
    dom/NonElementParentNode.idl
    dom/OverflowEvent.idl
    dom/PageTransitionEvent.idl
    dom/ParentNode.idl
    dom/PointerEvent.idl
    dom/PopStateEvent.idl
    dom/ProcessingInstruction.idl
    dom/ProgressEvent.idl
    dom/PromiseRejectionEvent.idl
    dom/Range.idl
    dom/RequestAnimationFrameCallback.idl
    dom/SecurityPolicyViolationEvent.idl
    dom/ShadowRoot.idl
    dom/ShadowRootMode.idl
    dom/Slotable.idl
    dom/StaticRange.idl
    dom/StringCallback.idl
    dom/Text.idl
    dom/TextDecoder.idl
    dom/TextDecoderStream.idl
    dom/TextDecoderStreamDecoder.idl
    dom/TextEncoder.idl
    dom/TextEncoderStream.idl
    dom/TextEncoderStreamEncoder.idl
    dom/TextEvent.idl
    dom/Touch.idl
    dom/TouchEvent.idl
    dom/TouchList.idl
    dom/TransitionEvent.idl
    dom/TreeWalker.idl
    dom/UIEvent.idl
    dom/UIEventInit.idl
    dom/VisibilityState.idl
    dom/WebKitAnimationEvent.idl
    dom/WebKitTransitionEvent.idl
    dom/WheelEvent.idl
    dom/XMLDocument.idl

    fileapi/Blob.idl
    fileapi/BlobCallback.idl
    fileapi/BlobPropertyBag.idl
    fileapi/EndingType.idl
    fileapi/File.idl
    fileapi/FileList.idl
    fileapi/FileReader.idl
    fileapi/FileReaderSync.idl

    html/DOMFormData.idl
    html/DOMTokenList.idl
    html/DOMURL.idl
    html/HTMLAllCollection.idl
    html/HTMLAnchorElement.idl
    html/HTMLAppletElement.idl
    html/HTMLAreaElement.idl
    html/HTMLAttachmentElement.idl
    html/HTMLAudioElement.idl
    html/HTMLBRElement.idl
    html/HTMLBaseElement.idl
    html/HTMLBodyElement.idl
    html/HTMLButtonElement.idl
    html/HTMLCanvasElement.idl
    html/HTMLCollection.idl
    html/HTMLDListElement.idl
    html/HTMLDataElement.idl
    html/HTMLDataListElement.idl
    html/HTMLDetailsElement.idl
    html/HTMLDialogElement.idl
    html/HTMLDirectoryElement.idl
    html/HTMLDivElement.idl
    html/HTMLDocument.idl
    html/HTMLElement.idl
    html/HTMLEmbedElement.idl
    html/HTMLFieldSetElement.idl
    html/HTMLFontElement.idl
    html/HTMLFormControlsCollection.idl
    html/HTMLFormElement.idl
    html/HTMLFrameElement.idl
    html/HTMLFrameSetElement.idl
    html/HTMLHRElement.idl
    html/HTMLHeadElement.idl
    html/HTMLHeadingElement.idl
    html/HTMLHtmlElement.idl
    html/HTMLHyperlinkElementUtils.idl
    html/HTMLIFrameElement.idl
    html/HTMLImageElement.idl
    html/HTMLInputElement.idl
    html/HTMLKeygenElement.idl
    html/HTMLLIElement.idl
    html/HTMLLabelElement.idl
    html/HTMLLegendElement.idl
    html/HTMLLinkElement.idl
    html/HTMLMapElement.idl
    html/HTMLMarqueeElement.idl
    html/HTMLMediaElement.idl
    html/HTMLMediaElementAudioOutput.idl
    html/HTMLMenuElement.idl
    html/HTMLMenuItemElement.idl
    html/HTMLMetaElement.idl
    html/HTMLMeterElement.idl
    html/HTMLModElement.idl
    html/HTMLOListElement.idl
    html/HTMLObjectElement.idl
    html/HTMLOptGroupElement.idl
    html/HTMLOptionElement.idl
    html/HTMLOptionsCollection.idl
    html/HTMLOrForeignElement.idl
    html/HTMLOutputElement.idl
    html/HTMLParagraphElement.idl
    html/HTMLParamElement.idl
    html/HTMLPictureElement.idl
    html/HTMLPreElement.idl
    html/HTMLProgressElement.idl
    html/HTMLQuoteElement.idl
    html/HTMLScriptElement.idl
    html/HTMLSelectElement.idl
    html/HTMLSlotElement.idl
    html/HTMLSourceElement.idl
    html/HTMLSpanElement.idl
    html/HTMLStyleElement.idl
    html/HTMLTableCaptionElement.idl
    html/HTMLTableCellElement.idl
    html/HTMLTableColElement.idl
    html/HTMLTableElement.idl
    html/HTMLTableRowElement.idl
    html/HTMLTableSectionElement.idl
    html/HTMLTemplateElement.idl
    html/HTMLTextAreaElement.idl
    html/HTMLTimeElement.idl
    html/HTMLTitleElement.idl
    html/HTMLTrackElement.idl
    html/HTMLUListElement.idl
    html/HTMLUnknownElement.idl
    html/HTMLVideoElement.idl
    html/ImageBitmap.idl
    html/ImageBitmapOptions.idl
    html/ImageData.idl
    html/MediaController.idl
    html/MediaEncryptedEvent.idl
    html/MediaError.idl
    html/OffscreenCanvas.idl
    html/RadioNodeList.idl
    html/TextMetrics.idl
    html/TimeRanges.idl
    html/URLSearchParams.idl
    html/ValidityState.idl
    html/VoidCallback.idl

    html/canvas/CanvasCompositing.idl
    html/canvas/CanvasDirection.idl
    html/canvas/CanvasDrawImage.idl
    html/canvas/CanvasDrawPath.idl
    html/canvas/CanvasFillRule.idl
    html/canvas/CanvasFillStrokeStyles.idl
    html/canvas/CanvasFilters.idl
    html/canvas/CanvasGradient.idl
    html/canvas/CanvasImageData.idl
    html/canvas/CanvasImageSmoothing.idl
    html/canvas/CanvasLineCap.idl
    html/canvas/CanvasLineJoin.idl
    html/canvas/CanvasPath.idl
    html/canvas/CanvasPathDrawingStyles.idl
    html/canvas/CanvasPattern.idl
    html/canvas/CanvasRect.idl
    html/canvas/CanvasRenderingContext2D.idl
    html/canvas/CanvasShadowStyles.idl
    html/canvas/CanvasState.idl
    html/canvas/CanvasText.idl
    html/canvas/CanvasTextAlign.idl
    html/canvas/CanvasTextBaseline.idl
    html/canvas/CanvasTextDrawingStyles.idl
    html/canvas/CanvasTransform.idl
    html/canvas/CanvasUserInterface.idl
    html/canvas/ImageBitmapRenderingContext.idl
    html/canvas/ImageBitmapRenderingContextSettings.idl
    html/canvas/ImageSmoothingQuality.idl
    html/canvas/OffscreenCanvasRenderingContext2D.idl
    html/canvas/PaintRenderingContext2D.idl
    html/canvas/Path2D.idl

    html/track/AudioTrack.idl
    html/track/AudioTrackList.idl
    html/track/DataCue.idl
    html/track/TextTrack.idl
    html/track/TextTrackCue.idl
    html/track/TextTrackCueGeneric.idl
    html/track/TextTrackCueList.idl
    html/track/TextTrackList.idl
    html/track/TrackEvent.idl
    html/track/VTTCue.idl
    html/track/VTTRegion.idl
    html/track/VTTRegionList.idl
    html/track/VideoTrack.idl
    html/track/VideoTrackList.idl

    inspector/CommandLineAPIHost.idl
    inspector/InspectorAuditAccessibilityObject.idl
    inspector/InspectorAuditDOMObject.idl
    inspector/InspectorAuditResourcesObject.idl
    inspector/InspectorFrontendHost.idl

    loader/appcache/DOMApplicationCache.idl

    mathml/MathMLElement.idl
    mathml/MathMLMathElement.idl

    page/BarProp.idl
    page/Crypto.idl
    page/DOMSelection.idl
    page/DOMWindow.idl
    page/EventSource.idl
    page/GlobalCrypto.idl
    page/GlobalPerformance.idl
    page/History.idl
    page/IntersectionObserver.idl
    page/IntersectionObserverCallback.idl
    page/IntersectionObserverEntry.idl
    page/Location.idl
    page/Navigator.idl
    page/NavigatorCookies.idl
    page/NavigatorID.idl
    page/NavigatorLanguage.idl
    page/NavigatorOnLine.idl
    page/NavigatorPlugins.idl
    page/NavigatorServiceWorker.idl
    page/NavigatorShare.idl
    page/Performance+NavigationTiming.idl
    page/Performance+PerformanceTimeline.idl
    page/Performance+ResourceTiming.idl
    page/Performance+UserTiming.idl
    page/Performance.idl
    page/PerformanceEntry.idl
    page/PerformanceMark.idl
    page/PerformanceMarkOptions.idl
    page/PerformanceMeasure.idl
    page/PerformanceMeasureOptions.idl
    page/PerformanceObserver.idl
    page/PerformanceObserverCallback.idl
    page/PerformanceObserverEntryList.idl
    page/PerformanceNavigation.idl
    page/PerformancePaintTiming.idl
    page/PerformanceResourceTiming.idl
    page/PerformanceServerTiming.idl
    page/PerformanceTiming.idl
    page/PostMessageOptions.idl
    page/RemoteDOMWindow.idl
    page/ResizeObserver.idl
    page/ResizeObserverCallback.idl
    page/ResizeObserverEntry.idl
    page/Screen.idl
    page/ScrollBehavior.idl
    page/ScrollIntoViewOptions.idl
    page/ScrollLogicalPosition.idl
    page/ScrollOptions.idl
    page/ScrollToOptions.idl
    page/ShareData.idl
    page/UndoItem.idl
    page/UndoManager.idl
    page/VisualViewport.idl
    page/WebKitPoint.idl
    page/WindowEventHandlers.idl
    page/WindowOrWorkerGlobalScope.idl
    page/WorkerNavigator.idl

    plugins/DOMMimeType.idl
    plugins/DOMMimeTypeArray.idl
    plugins/DOMPlugin.idl
    plugins/DOMPluginArray.idl

    storage/Storage.idl
    storage/StorageEvent.idl

    workers/AbstractWorker.idl
    workers/DedicatedWorkerGlobalScope.idl
    workers/Worker.idl
    workers/WorkerGlobalScope.idl
    workers/WorkerLocation.idl
    workers/WorkerType.idl

    workers/service/ExtendableEvent.idl
    workers/service/ExtendableEventInit.idl
    workers/service/ExtendableMessageEvent.idl
    workers/service/FetchEvent.idl
    workers/service/ServiceWorker.idl
    workers/service/ServiceWorkerClient.idl
    workers/service/ServiceWorkerClientType.idl
    workers/service/ServiceWorkerClients.idl
    workers/service/ServiceWorkerContainer.idl
    workers/service/ServiceWorkerGlobalScope.idl
    workers/service/ServiceWorkerRegistration.idl
    workers/service/ServiceWorkerUpdateViaCache.idl
    workers/service/ServiceWorkerWindowClient.idl

    worklets/PaintWorkletGlobalScope.idl
    worklets/Worklet.idl
    worklets/WorkletGlobalScope.idl

    xml/CustomXPathNSResolver.idl
    xml/DOMParser.idl
    xml/XMLHttpRequest.idl
    xml/XMLHttpRequestEventTarget.idl
    xml/XMLHttpRequestProgressEvent.idl
    xml/XMLHttpRequestUpload.idl
    xml/XMLSerializer.idl
    xml/XPathEvaluator.idl
    xml/XPathEvaluatorBase.idl
    xml/XPathExpression.idl
    xml/XPathNSResolver.idl
    xml/XPathResult.idl
    xml/XSLTProcessor.idl
)

set(WebCore_SVG_IDL_FILES
    svg/Document+SVG.idl
    svg/SVGAElement.idl
    svg/SVGAltGlyphDefElement.idl
    svg/SVGAltGlyphElement.idl
    svg/SVGAltGlyphItemElement.idl
    svg/SVGAngle.idl
    svg/SVGAnimateColorElement.idl
    svg/SVGAnimateElement.idl
    svg/SVGAnimateMotionElement.idl
    svg/SVGAnimateTransformElement.idl
    svg/SVGAnimatedAngle.idl
    svg/SVGAnimatedBoolean.idl
    svg/SVGAnimatedEnumeration.idl
    svg/SVGAnimatedInteger.idl
    svg/SVGAnimatedLength.idl
    svg/SVGAnimatedLengthList.idl
    svg/SVGAnimatedNumber.idl
    svg/SVGAnimatedNumberList.idl
    svg/SVGAnimatedPreserveAspectRatio.idl
    svg/SVGAnimatedRect.idl
    svg/SVGAnimatedString.idl
    svg/SVGAnimatedTransformList.idl
    svg/SVGAnimationElement.idl
    svg/SVGCircleElement.idl
    svg/SVGClipPathElement.idl
    svg/SVGComponentTransferFunctionElement.idl
    svg/SVGCursorElement.idl
    svg/SVGDefsElement.idl
    svg/SVGDescElement.idl
    svg/SVGElement.idl
    svg/SVGEllipseElement.idl
    svg/SVGFEBlendElement.idl
    svg/SVGFEColorMatrixElement.idl
    svg/SVGFEComponentTransferElement.idl
    svg/SVGFECompositeElement.idl
    svg/SVGFEConvolveMatrixElement.idl
    svg/SVGFEDiffuseLightingElement.idl
    svg/SVGFEDisplacementMapElement.idl
    svg/SVGFEDistantLightElement.idl
    svg/SVGFEDropShadowElement.idl
    svg/SVGFEFloodElement.idl
    svg/SVGFEFuncAElement.idl
    svg/SVGFEFuncBElement.idl
    svg/SVGFEFuncGElement.idl
    svg/SVGFEFuncRElement.idl
    svg/SVGFEGaussianBlurElement.idl
    svg/SVGFEImageElement.idl
    svg/SVGFEMergeElement.idl
    svg/SVGFEMergeNodeElement.idl
    svg/SVGFEMorphologyElement.idl
    svg/SVGFEOffsetElement.idl
    svg/SVGFEPointLightElement.idl
    svg/SVGFESpecularLightingElement.idl
    svg/SVGFESpotLightElement.idl
    svg/SVGFETileElement.idl
    svg/SVGFETurbulenceElement.idl
    svg/SVGFilterElement.idl
    svg/SVGFilterPrimitiveStandardAttributes.idl
    svg/SVGFitToViewBox.idl
    svg/SVGFontElement.idl
    svg/SVGFontFaceElement.idl
    svg/SVGFontFaceFormatElement.idl
    svg/SVGFontFaceNameElement.idl
    svg/SVGFontFaceSrcElement.idl
    svg/SVGFontFaceUriElement.idl
    svg/SVGForeignObjectElement.idl
    svg/SVGGElement.idl
    svg/SVGGeometryElement.idl
    svg/SVGGlyphElement.idl
    svg/SVGGlyphRefElement.idl
    svg/SVGGradientElement.idl
    svg/SVGGraphicsElement.idl
    svg/SVGHKernElement.idl
    svg/SVGImageElement.idl
    svg/SVGLength.idl
    svg/SVGLengthList.idl
    svg/SVGLineElement.idl
    svg/SVGLinearGradientElement.idl
    svg/SVGMPathElement.idl
    svg/SVGMarkerElement.idl
    svg/SVGMaskElement.idl
    svg/SVGMatrix.idl
    svg/SVGMetadataElement.idl
    svg/SVGMissingGlyphElement.idl
    svg/SVGNumber.idl
    svg/SVGNumberList.idl
    svg/SVGPathElement.idl
    svg/SVGPathSeg.idl
    svg/SVGPathSegArcAbs.idl
    svg/SVGPathSegArcRel.idl
    svg/SVGPathSegClosePath.idl
    svg/SVGPathSegCurvetoCubicAbs.idl
    svg/SVGPathSegCurvetoCubicRel.idl
    svg/SVGPathSegCurvetoCubicSmoothAbs.idl
    svg/SVGPathSegCurvetoCubicSmoothRel.idl
    svg/SVGPathSegCurvetoQuadraticAbs.idl
    svg/SVGPathSegCurvetoQuadraticRel.idl
    svg/SVGPathSegCurvetoQuadraticSmoothAbs.idl
    svg/SVGPathSegCurvetoQuadraticSmoothRel.idl
    svg/SVGPathSegLinetoAbs.idl
    svg/SVGPathSegLinetoHorizontalAbs.idl
    svg/SVGPathSegLinetoHorizontalRel.idl
    svg/SVGPathSegLinetoRel.idl
    svg/SVGPathSegLinetoVerticalAbs.idl
    svg/SVGPathSegLinetoVerticalRel.idl
    svg/SVGPathSegList.idl
    svg/SVGPathSegMovetoAbs.idl
    svg/SVGPathSegMovetoRel.idl
    svg/SVGPatternElement.idl
    svg/SVGPoint.idl
    svg/SVGPointList.idl
    svg/SVGPolygonElement.idl
    svg/SVGPolylineElement.idl
    svg/SVGPreserveAspectRatio.idl
    svg/SVGRadialGradientElement.idl
    svg/SVGRect.idl
    svg/SVGRectElement.idl
    svg/SVGRenderingIntent.idl
    svg/SVGSVGElement.idl
    svg/SVGScriptElement.idl
    svg/SVGSetElement.idl
    svg/SVGStopElement.idl
    svg/SVGStringList.idl
    svg/SVGStyleElement.idl
    svg/SVGSwitchElement.idl
    svg/SVGSymbolElement.idl
    svg/SVGTRefElement.idl
    svg/SVGTSpanElement.idl
    svg/SVGTests.idl
    svg/SVGTextContentElement.idl
    svg/SVGTextElement.idl
    svg/SVGTextPathElement.idl
    svg/SVGTextPositioningElement.idl
    svg/SVGTitleElement.idl
    svg/SVGTransform.idl
    svg/SVGTransformList.idl
    svg/SVGURIReference.idl
    svg/SVGUnitTypes.idl
    svg/SVGUseElement.idl
    svg/SVGVKernElement.idl
    svg/SVGViewElement.idl
    svg/SVGViewSpec.idl
    svg/SVGZoomAndPan.idl
    svg/SVGZoomEvent.idl
)

list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/Modules/gamepad"
)

list(APPEND WebCore_IDL_INCLUDES
    Modules/gamepad
)

list(APPEND WebCore_NON_SVG_IDL_FILES
    Modules/gamepad/Gamepad.idl
    Modules/gamepad/GamepadButton.idl
    Modules/gamepad/GamepadEvent.idl
    Modules/gamepad/Navigator+Gamepad.idl
)

if (ENABLE_GAMEPAD)
    list(APPEND WebCore_SOURCES
        Modules/gamepad/Gamepad.cpp
        Modules/gamepad/GamepadButton.cpp
        Modules/gamepad/GamepadEvent.cpp
        Modules/gamepad/GamepadManager.cpp
        Modules/gamepad/NavigatorGamepad.cpp

        platform/gamepad/EmptyGamepadProvider.cpp
        platform/gamepad/GamepadProvider.cpp
    )
endif ()

list(APPEND WebCore_NON_SVG_IDL_FILES
    Modules/webxr/Navigator+WebXR.idl
    Modules/webxr/WebXRBoundedReferenceSpace.idl
    Modules/webxr/WebXRFrame.idl
    Modules/webxr/WebXRInputSource.idl
    Modules/webxr/WebXRInputSourceArray.idl
    Modules/webxr/WebXRLayer.idl
    Modules/webxr/WebXRPose.idl
    Modules/webxr/WebXRReferenceSpace.idl
    Modules/webxr/WebXRRenderState.idl
    Modules/webxr/WebXRRigidTransform.idl
    Modules/webxr/WebXRSession.idl
    Modules/webxr/WebXRSpace.idl
    Modules/webxr/WebXRSystem.idl
    Modules/webxr/WebXRView.idl
    Modules/webxr/WebXRViewerPose.idl
    Modules/webxr/WebXRViewport.idl
    Modules/webxr/WebXRWebGLLayer.idl
    Modules/webxr/XREnvironmentBlendMode.idl
    Modules/webxr/XREye.idl
    Modules/webxr/XRFrameRequestCallback.idl
    Modules/webxr/XRHandedness.idl
    Modules/webxr/XRInputSourceEvent.idl
    Modules/webxr/XRInputSourcesChangeEvent.idl
    Modules/webxr/XRInteractionMode.idl
    Modules/webxr/XRReferenceSpaceEvent.idl
    Modules/webxr/XRReferenceSpaceType.idl
    Modules/webxr/XRRenderStateInit.idl
    Modules/webxr/XRSessionEvent.idl
    Modules/webxr/XRSessionInit.idl
    Modules/webxr/XRSessionMode.idl
    Modules/webxr/XRTargetRayMode.idl
    Modules/webxr/XRVisibilityState.idl
    Modules/webxr/XRWebGLLayerInit.idl
)

set(WebCore_IDL_FILES
    ${WebCore_NON_SVG_IDL_FILES}
    ${WebCore_SVG_IDL_FILES}
)

set(WebCore_CSS_PROPERTY_NAMES
    ${WEBCORE_DIR}/css/CSSProperties.json
)

set(WebCore_CSS_VALUE_KEYWORDS
    ${WEBCORE_DIR}/css/CSSValueKeywords.in
)

set(WebCore_USER_AGENT_STYLE_SHEETS
    ${WEBCORE_DIR}/css/dialog.css
    ${WEBCORE_DIR}/css/fullscreen.css
    ${WEBCORE_DIR}/css/html.css
    ${WEBCORE_DIR}/css/mathml.css
    ${WEBCORE_DIR}/css/mediaControls.css
    ${WEBCORE_DIR}/css/plugIns.css
    ${WEBCORE_DIR}/css/quirks.css
    ${WEBCORE_DIR}/css/svg.css
    ${WEBCORE_DIR}/html/shadow/meterElementShadow.css
)

set(WebCore_PLUG_INS_RESOURCES
    ${WEBCORE_DIR}/Resources/plugIns.js
)

set(WebCore_LIBRARIES
    LibXml2::LibXml2
    SQLite::SQLite3
    WebKit::JavaScriptCore
    WebKit::PAL
    ZLIB::ZLIB
)

if (ENABLE_XSLT)
    list(APPEND WebCore_LIBRARIES LibXslt::LibXslt)
endif ()

set(WebCoreTestSupport_LIBRARIES
    WebKit::JavaScriptCore
    WebKit::PAL
)
set(WebCoreTestSupport_DEPENDENCIES WebCoreTestSupportBindings)

if (ENABLE_LEGACY_ENCRYPTED_MEDIA)
    list(APPEND WebCore_SOURCES
        Modules/encryptedmedia/legacy/LegacyCDM.cpp
        Modules/encryptedmedia/legacy/LegacyCDMPrivateClearKey.cpp
        Modules/encryptedmedia/legacy/LegacyCDMPrivateMediaPlayer.cpp
        Modules/encryptedmedia/legacy/LegacyCDMSessionClearKey.cpp

        Modules/encryptedmedia/legacy/WebKitMediaKeyMessageEvent.cpp
        Modules/encryptedmedia/legacy/WebKitMediaKeyNeededEvent.cpp
        Modules/encryptedmedia/legacy/WebKitMediaKeySession.cpp
        Modules/encryptedmedia/legacy/WebKitMediaKeys.cpp
    )
    list(APPEND WebCoreTestSupport_SOURCES
        testing/LegacyMockCDM.cpp
    )
endif ()

list(APPEND WebCore_IDL_FILES
    Modules/encryptedmedia/legacy/WebKitMediaKeyMessageEvent.idl
    Modules/encryptedmedia/legacy/WebKitMediaKeyNeededEvent.idl
    Modules/encryptedmedia/legacy/WebKitMediaKeySession.idl
    Modules/encryptedmedia/legacy/WebKitMediaKeys.idl

    html/WebKitMediaKeyError.idl
)

if (ENABLE_ENCRYPTED_MEDIA)
    list(APPEND WebCore_SOURCES
        Modules/encryptedmedia/CDM.cpp
        Modules/encryptedmedia/InitDataRegistry.cpp
        Modules/encryptedmedia/MediaKeyMessageEvent.cpp
        Modules/encryptedmedia/MediaKeySession.cpp
        Modules/encryptedmedia/MediaKeyStatusMap.cpp
        Modules/encryptedmedia/MediaKeySystemAccess.cpp
        Modules/encryptedmedia/MediaKeys.cpp
        Modules/encryptedmedia/NavigatorEME.cpp

        html/MediaEncryptedEvent.cpp
    )
endif ()

list(APPEND WebCore_CSS_VALUE_KEYWORDS
    ${WEBCORE_DIR}/css/SVGCSSValueKeywords.in
)

if (ENABLE_WEBGL)
    list(APPEND WebCore_SOURCES
        html/canvas/ANGLEInstancedArrays.cpp
        html/canvas/EXTBlendMinMax.cpp
        html/canvas/EXTColorBufferFloat.cpp
        html/canvas/EXTColorBufferHalfFloat.cpp
        html/canvas/EXTFragDepth.cpp
        html/canvas/EXTShaderTextureLOD.cpp
        html/canvas/EXTTextureFilterAnisotropic.cpp
        html/canvas/EXTsRGB.cpp
        html/canvas/OESElementIndexUint.cpp
        html/canvas/OESStandardDerivatives.cpp
        html/canvas/OESTextureFloat.cpp
        html/canvas/OESTextureFloatLinear.cpp
        html/canvas/OESTextureHalfFloat.cpp
        html/canvas/OESTextureHalfFloatLinear.cpp
        html/canvas/OESVertexArrayObject.cpp
        html/canvas/WebGL2RenderingContext.cpp
        html/canvas/WebGLBuffer.cpp
        html/canvas/WebGLColorBufferFloat.cpp
        html/canvas/WebGLCompressedTextureASTC.cpp
        html/canvas/WebGLCompressedTextureATC.cpp
        html/canvas/WebGLCompressedTextureETC.cpp
        html/canvas/WebGLCompressedTextureETC1.cpp
        html/canvas/WebGLCompressedTexturePVRTC.cpp
        html/canvas/WebGLCompressedTextureS3TC.cpp
        html/canvas/WebGLCompressedTextureS3TCsRGB.cpp
        html/canvas/WebGLContextEvent.cpp
        html/canvas/WebGLContextGroup.cpp
        html/canvas/WebGLContextObject.cpp
        html/canvas/WebGLDebugRendererInfo.cpp
        html/canvas/WebGLDebugShaders.cpp
        html/canvas/WebGLDepthTexture.cpp
        html/canvas/WebGLDrawBuffers.cpp
        html/canvas/WebGLExtension.cpp
        html/canvas/WebGLFramebuffer.cpp
        html/canvas/WebGLLoseContext.cpp
        html/canvas/WebGLObject.cpp
        html/canvas/WebGLProgram.cpp
        html/canvas/WebGLQuery.cpp
        html/canvas/WebGLRenderbuffer.cpp
        html/canvas/WebGLRenderingContext.cpp
        html/canvas/WebGLRenderingContextBase.cpp
        html/canvas/WebGLSampler.cpp
        html/canvas/WebGLShader.cpp
        html/canvas/WebGLShaderPrecisionFormat.cpp
        html/canvas/WebGLSharedObject.cpp
        html/canvas/WebGLSync.cpp
        html/canvas/WebGLTexture.cpp
        html/canvas/WebGLTransformFeedback.cpp
        html/canvas/WebGLUniformLocation.cpp
        html/canvas/WebGLVertexArrayObject.cpp
        html/canvas/WebGLVertexArrayObjectBase.cpp
        html/canvas/WebGLVertexArrayObjectOES.cpp
    )
endif ()

list(APPEND WebCore_IDL_FILES
    html/canvas/ANGLEInstancedArrays.idl
    html/canvas/EXTBlendMinMax.idl
    html/canvas/EXTColorBufferFloat.idl
    html/canvas/EXTColorBufferHalfFloat.idl
    html/canvas/EXTFragDepth.idl
    html/canvas/EXTShaderTextureLOD.idl
    html/canvas/EXTTextureFilterAnisotropic.idl
    html/canvas/EXTsRGB.idl
    html/canvas/OESElementIndexUint.idl
    html/canvas/OESStandardDerivatives.idl
    html/canvas/OESTextureFloat.idl
    html/canvas/OESTextureFloatLinear.idl
    html/canvas/OESTextureHalfFloat.idl
    html/canvas/OESTextureHalfFloatLinear.idl
    html/canvas/OESVertexArrayObject.idl
    html/canvas/WebGL2RenderingContext.idl
    html/canvas/WebGLActiveInfo.idl
    html/canvas/WebGLBuffer.idl
    html/canvas/WebGLColorBufferFloat.idl
    html/canvas/WebGLCompressedTextureASTC.idl
    html/canvas/WebGLCompressedTextureATC.idl
    html/canvas/WebGLCompressedTextureETC.idl
    html/canvas/WebGLCompressedTextureETC1.idl
    html/canvas/WebGLCompressedTexturePVRTC.idl
    html/canvas/WebGLCompressedTextureS3TC.idl
    html/canvas/WebGLCompressedTextureS3TCsRGB.idl
    html/canvas/WebGLContextAttributes.idl
    html/canvas/WebGLContextEvent.idl
    html/canvas/WebGLDebugRendererInfo.idl
    html/canvas/WebGLDebugShaders.idl
    html/canvas/WebGLDepthTexture.idl
    html/canvas/WebGLDrawBuffers.idl
    html/canvas/WebGLFramebuffer.idl
    html/canvas/WebGLLoseContext.idl
    html/canvas/WebGLProgram.idl
    html/canvas/WebGLQuery.idl
    html/canvas/WebGLRenderbuffer.idl
    html/canvas/WebGLRenderingContext.idl
    html/canvas/WebGLRenderingContextBase.idl
    html/canvas/WebGLSampler.idl
    html/canvas/WebGLShader.idl
    html/canvas/WebGLShaderPrecisionFormat.idl
    html/canvas/WebGLSync.idl
    html/canvas/WebGLTexture.idl
    html/canvas/WebGLTransformFeedback.idl
    html/canvas/WebGLUniformLocation.idl
    html/canvas/WebGLVertexArrayObject.idl
    html/canvas/WebGLVertexArrayObjectOES.idl
)

if (ENABLE_QUOTA)
    list(APPEND WebCore_SOURCES
        Modules/quota/DOMWindowQuota.cpp
        Modules/quota/NavigatorStorageQuota.cpp
        Modules/quota/StorageErrorCallback.cpp
        Modules/quota/StorageInfo.cpp
        Modules/quota/StorageQuota.cpp
        Modules/quota/WorkerNavigatorStorageQuota.cpp
    )
endif ()

list(APPEND WebCore_IDL_FILES
    Modules/quota/DOMWindow+Quota.idl
    Modules/quota/Navigator+StorageQuota.idl
    Modules/quota/StorageErrorCallback.idl
    Modules/quota/StorageInfo.idl
    Modules/quota/StorageQuota.idl
    Modules/quota/StorageQuotaCallback.idl
    Modules/quota/StorageUsageCallback.idl
    Modules/quota/WorkerNavigator+StorageQuota.idl
)

if (ENABLE_GRAPHICS_CONTEXT_GL)
    list(APPEND WebCore_SOURCES
        platform/graphics/ANGLEWebKitBridge.cpp
        platform/graphics/FormatConverter.cpp
        platform/graphics/opengl/GraphicsContextGLOpenGL.cpp
    )

    list(APPEND WebCore_LIBRARIES ANGLEFramework)
    list(APPEND WebCoreTestSupport_LIBRARIES ANGLEFramework)

    # For platforms that want to use system-provided OpenGL (ES) / EGL headers,
    # these include directories, libraries or definitions need to be
    # added before the ANGLE directories.
    if (USE_LIBEPOXY)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${LIBEPOXY_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            ${LIBEPOXY_LIBRARIES}
        )
        add_definitions(${LIBEPOXY_DEFINITIONS})
    else ()
        if (USE_OPENGL)
            list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
                ${OPENGL_INCLUDE_DIRS}
            )
            list(APPEND WebCore_LIBRARIES
                ${OPENGL_LIBRARIES}
            )
            add_definitions(${OPENGL_DEFINITIONS})
        elseif (USE_OPENGL_ES)
            list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
                ${OPENGLES2_INCLUDE_DIRS}
            )
            list(APPEND WebCore_LIBRARIES
                ${OPENGLES2_LIBRARIES}
            )
            add_definitions(${OPENGLES2_DEFINITIONS})
        endif ()

        if (USE_EGL)
            list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
                ${EGL_INCLUDE_DIRS}
            )
            list(APPEND WebCore_LIBRARIES
                ${EGL_LIBRARIES}
            )
            add_definitions(${EGL_DEFINITIONS})
        endif ()
    endif ()

    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/platform/graphics/gpu"
    )
endif ()

list(APPEND WebCore_IDL_FILES
    page/UserMessageHandler.idl
    page/UserMessageHandlersNamespace.idl
    page/WebKitNamespace.idl
)

if (ENABLE_USER_MESSAGE_HANDLERS)
    list(APPEND WebCore_SOURCES
        page/UserMessageHandler.cpp
        page/UserMessageHandlerDescriptor.cpp
        page/UserMessageHandlersNamespace.cpp
        page/WebKitNamespace.cpp
    )
endif ()

if (USE_WOFF2)
  list(APPEND WebCore_LIBRARIES WOFF2::dec)
endif ()

if (USE_XDGMIME)
    list(APPEND WebCore_PRIVATE_INCLUDE_DIRECTORIES "${THIRDPARTY_DIR}/xdgmime/src")
    list(APPEND WebCore_LIBRARIES xdgmime)
endif ()

if (USE_LIBWEBRTC)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES "${THIRDPARTY_DIR}/libwebrtc/Source/"
        "${THIRDPARTY_DIR}/libwebrtc/Source/webrtc"
        "${THIRDPARTY_DIR}/libwebrtc/Source/third_party/abseil-cpp")
    list(APPEND WebCore_LIBRARIES webrtc)
    list(APPEND WebCore_SOURCES
        Modules/mediastream/libwebrtc/LibWebRTCDataChannelHandler.cpp
        Modules/mediastream/libwebrtc/LibWebRTCMediaEndpoint.cpp
        Modules/mediastream/libwebrtc/LibWebRTCPeerConnectionBackend.cpp
        Modules/mediastream/libwebrtc/LibWebRTCRtpReceiverBackend.cpp
        Modules/mediastream/libwebrtc/LibWebRTCRtpSenderBackend.cpp
        Modules/mediastream/libwebrtc/LibWebRTCRtpTransceiverBackend.cpp
        Modules/mediastream/libwebrtc/LibWebRTCStatsCollector.cpp
        Modules/mediastream/libwebrtc/LibWebRTCUtils.cpp
    )
endif ()

if (HAVE_OS_DARK_MODE_SUPPORT)
    set(FEATURE_DEFINES_WITH_SPACE_SEPARATOR "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR} HAVE_OS_DARK_MODE_SUPPORT")
endif ()

set(WebCoreTestSupport_PRIVATE_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/mock"
    "${WEBCORE_DIR}/testing"
    "${WEBCORE_DIR}/testing/js"
    "${WebCore_DERIVED_SOURCES_DIR}"
)

set(WebCoreTestSupport_IDL_INCLUDES
    testing
)

set(WebCoreTestSupport_IDL_FILES
    testing/GCObservation.idl
    testing/InternalSettings.idl
    testing/Internals.idl
    testing/InternalsMapLike.idl
    testing/InternalsSetLike.idl
    testing/MallocStatistics.idl
    testing/MemoryInfo.idl
    testing/MockCDMFactory.idl
    testing/MockContentFilterSettings.idl
    testing/MockPageOverlay.idl
    testing/ServiceWorkerInternals.idl
    testing/TypeConversions.idl
)

list(APPEND WebCoreTestSupport_SOURCES
    platform/mock/PlatformSpeechSynthesizerMock.cpp

    platform/mock/mediasource/MockBox.cpp
    platform/mock/mediasource/MockMediaPlayerMediaSource.cpp
    platform/mock/mediasource/MockMediaSourcePrivate.cpp
    platform/mock/mediasource/MockSourceBufferPrivate.cpp

    testing/GCObservation.cpp
    testing/InternalSettings.cpp
    testing/Internals.cpp
    testing/InternalsMapLike.cpp
    testing/InternalsSetLike.cpp
    testing/MockCDMFactory.cpp
    testing/MockGamepad.cpp
    testing/MockGamepadProvider.cpp
    testing/MockLibWebRTCPeerConnection.cpp
    testing/MockPageOverlay.cpp
    testing/MockPageOverlayClient.cpp
    testing/ServiceWorkerInternals.cpp
    testing/js/WebCoreTestSupport.cpp
)


if (ENABLE_WEBXR)
    list(APPEND WebCoreTestSupport_IDL_FILES
        testing/FakeXRBoundsPoint.idl
        testing/FakeXRButtonStateInit.idl
        testing/FakeXRInputSourceInit.idl
        testing/FakeXRRigidTransformInit.idl
        testing/FakeXRViewInit.idl
        testing/WebFakeXRDevice.idl
        testing/WebFakeXRInputController.idl
        testing/WebXRTest.idl
        testing/XRSimulateUserActivationFunction.idl
    )
    list(APPEND WebCoreTestSupport_SOURCES
        testing/WebFakeXRDevice.cpp
        testing/WebFakeXRInputController.cpp
        testing/WebXRTest.cpp
    )
endif ()

# Modules that the bindings generator scripts may use
set(SCRIPTS_RESOLVE_SUPPLEMENTAL
    ${WEBCORE_DIR}/bindings/scripts/IDLParser.pm
)
set(SCRIPTS_BINDINGS
    ${WEBCORE_DIR}/bindings/scripts/CodeGenerator.pm
    ${WEBCORE_DIR}/bindings/scripts/CodeGeneratorJS.pm
    ${WEBCORE_DIR}/bindings/scripts/IDLParser.pm
    ${WEBCORE_DIR}/bindings/scripts/InFilesParser.pm
    ${WEBCORE_DIR}/bindings/scripts/preprocessor.pm
)

set(MAKE_NAMES_DEPENDENCIES
    ${WEBCORE_DIR}/bindings/scripts/Hasher.pm
    ${WEBCORE_DIR}/bindings/scripts/StaticString.pm
)
set(SUPPLEMENTAL_DEPENDENCY_FILE ${WebCore_DERIVED_SOURCES_DIR}/supplemental_dependency.tmp)
set(ISO_SUBSPACES_HEADER_FILE ${WebCore_DERIVED_SOURCES_DIR}/DOMIsoSubspaces.h)
set(WINDOW_CONSTRUCTORS_FILE ${WebCore_DERIVED_SOURCES_DIR}/DOMWindowConstructors.idl)
set(WORKERGLOBALSCOPE_CONSTRUCTORS_FILE ${WebCore_DERIVED_SOURCES_DIR}/WorkerGlobalScopeConstructors.idl)
set(DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE ${WebCore_DERIVED_SOURCES_DIR}/DedicatedWorkerGlobalScopeConstructors.idl)
set(SERVICEWORKERGLOBALSCOPE_CONSTRUCTORS_FILE ${WebCore_DERIVED_SOURCES_DIR}/ServiceWorkerGlobalScopeConstructors.idl)
set(WORKLETGLOBALSCOPE_CONSTRUCTORS_FILE ${WebCore_DERIVED_SOURCES_DIR}/WorkletGlobalScopeConstructors.idl)
set(PAINTWORKLETGLOBALSCOPE_CONSTRUCTORS_FILE ${WebCore_DERIVED_SOURCES_DIR}/PaintWorkletGlobalScopeConstructors.idl)

set(WebCore_INTERFACE_LIBRARIES WebCore)
set(WebCore_INTERFACE_INCLUDE_DIRECTORIES ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR})

set(WebCoreTestSupport_INTERFACE_LIBRARIES WebCoreTestSupport)
set(WebCoreTestSupport_INTERFACE_INCLUDE_DIRECTORIES
    ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCoreTestSupport
)

WEBKIT_FRAMEWORK_DECLARE(WebCore)
WEBKIT_FRAMEWORK_DECLARE(WebCoreTestSupport)
WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

# Generate CommandLineAPIModuleSource.h
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/CommandLineAPIModuleSource.h ${WebCore_DERIVED_SOURCES_DIR}/CommandLineAPIModuleSource.min.js
    MAIN_DEPENDENCY inspector/CommandLineAPIModuleSource.js
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
    COMMAND ${CMAKE_COMMAND} -E echo "//# sourceURL=__InjectedScript_CommandLineAPIModuleSource.js" > ${WebCore_DERIVED_SOURCES_DIR}/CommandLineAPIModuleSource.min.js
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py < ${WEBCORE_DIR}/inspector/CommandLineAPIModuleSource.js >> ${WebCore_DERIVED_SOURCES_DIR}/CommandLineAPIModuleSource.min.js
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl CommandLineAPIModuleSource_js ${WebCore_DERIVED_SOURCES_DIR}/CommandLineAPIModuleSource.min.js ${WebCore_DERIVED_SOURCES_DIR}/CommandLineAPIModuleSource.h
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/CommandLineAPIModuleSource.h)


MAKE_HASH_TOOLS(${WEBCORE_DIR}/platform/ColorData)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/ColorData.cpp)


# Generate DOMJITAbstractHeapRepository.h
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/DOMJITAbstractHeapRepository.h
    MAIN_DEPENDENCY domjit/DOMJITAbstractHeapRepository.yaml
    DEPENDS ${WEBCORE_DIR}/domjit/generate-abstract-heap.rb
    COMMAND ${RUBY_EXECUTABLE} ${WEBCORE_DIR}/domjit/generate-abstract-heap.rb ${WEBCORE_DIR}/domjit/DOMJITAbstractHeapRepository.yaml ${WebCore_DERIVED_SOURCES_DIR}/DOMJITAbstractHeapRepository.h
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/DOMJITAbstractHeapRepository.h)

# Generate XMLViewerCSS.h
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/XMLViewerCSS.h ${WebCore_DERIVED_SOURCES_DIR}/XMLViewer.min.css
    MAIN_DEPENDENCY xml/XMLViewer.css
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/cssmin.py
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/cssmin.py < ${WEBCORE_DIR}/xml/XMLViewer.css > ${WebCore_DERIVED_SOURCES_DIR}/XMLViewer.min.css
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl XMLViewer_css ${WebCore_DERIVED_SOURCES_DIR}/XMLViewer.min.css ${WebCore_DERIVED_SOURCES_DIR}/XMLViewerCSS.h
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/XMLViewerCSS.h)

# Generate XMLViewerJS.h
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/XMLViewerJS.h ${WebCore_DERIVED_SOURCES_DIR}/XMLViewer.min.js
    MAIN_DEPENDENCY xml/XMLViewer.js
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py < ${WEBCORE_DIR}/xml/XMLViewer.js > ${WebCore_DERIVED_SOURCES_DIR}/XMLViewer.min.js
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl XMLViewer_js ${WebCore_DERIVED_SOURCES_DIR}/XMLViewer.min.js ${WebCore_DERIVED_SOURCES_DIR}/XMLViewerJS.h
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/XMLViewerJS.h)

# Generate HTML entity table
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/HTMLEntityTable.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/html/parser/HTMLEntityNames.in
    DEPENDS ${WEBCORE_DIR}/html/parser/create-html-entity-table
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/html/parser/create-html-entity-table -o ${WebCore_DERIVED_SOURCES_DIR}/HTMLEntityTable.cpp ${WEBCORE_DIR}/html/parser/HTMLEntityNames.in
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/HTMLEntityTable.cpp)

# Generate HTTP header names
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/HTTPHeaderNames.cpp ${WebCore_DERIVED_SOURCES_DIR}/HTTPHeaderNames.gperf ${WebCore_DERIVED_SOURCES_DIR}/HTTPHeaderNames.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/platform/network/HTTPHeaderNames.in
    DEPENDS ${WEBCORE_DIR}/platform/network/create-http-header-name-table
    WORKING_DIRECTORY ${WebCore_DERIVED_SOURCES_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/platform/network/create-http-header-name-table ${WEBCORE_DIR}/platform/network/HTTPHeaderNames.in "${GPERF_EXECUTABLE}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/HTTPHeaderNames.cpp)

# Generate CSS property names
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/CSSProperties.json ${WebCore_DERIVED_SOURCES_DIR}/CSSPropertyNames.h ${WebCore_DERIVED_SOURCES_DIR}/CSSPropertyNames.cpp ${WebCore_DERIVED_SOURCES_DIR}/CSSPropertyNames.gperf ${WebCore_DERIVED_SOURCES_DIR}/StyleBuilderGenerated.cpp ${WebCore_DERIVED_SOURCES_DIR}/StylePropertyShorthandFunctions.h ${WebCore_DERIVED_SOURCES_DIR}/StylePropertyShorthandFunctions.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makeprop.pl
    DEPENDS ${WebCore_CSS_PROPERTY_NAMES}
    WORKING_DIRECTORY ${WebCore_DERIVED_SOURCES_DIR}
    COMMAND ${PERL_EXECUTABLE} -ne "print" ${WebCore_CSS_PROPERTY_NAMES} > ${WebCore_DERIVED_SOURCES_DIR}/CSSProperties.json
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/makeprop.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --gperf-executable "${GPERF_EXECUTABLE}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/CSSPropertyNames.cpp)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/StyleBuilderGenerated.cpp)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/StylePropertyShorthandFunctions.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSParser.cpp CSSValueKeywords.h)

# Generate CSS value keywords
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/CSSValueKeywords.in ${WebCore_DERIVED_SOURCES_DIR}/CSSValueKeywords.h ${WebCore_DERIVED_SOURCES_DIR}/CSSValueKeywords.cpp ${WebCore_DERIVED_SOURCES_DIR}/CSSValueKeywords.gperf
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makevalues.pl
    DEPENDS ${WebCore_CSS_VALUE_KEYWORDS}
    WORKING_DIRECTORY ${WebCore_DERIVED_SOURCES_DIR}
    COMMAND ${PERL_EXECUTABLE} -ne "print" ${WebCore_CSS_VALUE_KEYWORDS} > ${WebCore_DERIVED_SOURCES_DIR}/CSSValueKeywords.in
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/makevalues.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR} ${CSS_VALUE_PLATFORM_DEFINES}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" --gperf-executable "${GPERF_EXECUTABLE}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/CSSValueKeywords.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSParser.cpp CSSValueKeywords.h)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSPrimitiveValueMappings.h CSSValueKeywords.h)

# Generate CSS Selector pseudo type name to value maps.
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/SelectorPseudoClassAndCompatibilityElementMap.gperf ${WebCore_DERIVED_SOURCES_DIR}/SelectorPseudoClassAndCompatibilityElementMap.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/SelectorPseudoClassAndCompatibilityElementMap.in
    DEPENDS ${WEBCORE_DIR}/css/makeSelectorPseudoClassAndCompatibilityElementMap.py
    WORKING_DIRECTORY ${WebCore_DERIVED_SOURCES_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/css/makeSelectorPseudoClassAndCompatibilityElementMap.py ${WEBCORE_DIR}/css/SelectorPseudoClassAndCompatibilityElementMap.in "${GPERF_EXECUTABLE}" "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/SelectorPseudoClassAndCompatibilityElementMap.cpp)
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/SelectorPseudoElementTypeMap.gperf ${WebCore_DERIVED_SOURCES_DIR}/SelectorPseudoElementTypeMap.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/SelectorPseudoElementTypeMap.in
    DEPENDS ${WEBCORE_DIR}/css/makeSelectorPseudoElementsMap.py
    WORKING_DIRECTORY ${WebCore_DERIVED_SOURCES_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/css/makeSelectorPseudoElementsMap.py ${WEBCORE_DIR}/css/SelectorPseudoElementTypeMap.in "${GPERF_EXECUTABLE}" "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}"
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/SelectorPseudoElementTypeMap.cpp)

# Generate user agent styles
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/UserAgentStyleSheetsData.cpp ${WebCore_DERIVED_SOURCES_DIR}/UserAgentStyleSheets.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/make-css-file-arrays.pl
    DEPENDS ${WebCore_USER_AGENT_STYLE_SHEETS} ${WEBCORE_DIR}/bindings/scripts/preprocessor.pm
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/make-css-file-arrays.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" ${WebCore_DERIVED_SOURCES_DIR}/UserAgentStyleSheets.h ${WebCore_DERIVED_SOURCES_DIR}/UserAgentStyleSheetsData.cpp ${WebCore_USER_AGENT_STYLE_SHEETS}
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/UserAgentStyleSheetsData.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/StyleResolver.cpp UserAgentStyleSheetsData.cpp UserAgentStyleSheets.h)

if (WebCore_USER_AGENT_SCRIPTS)
    # Necessary variables:
    # WebCore_USER_AGENT_SCRIPTS containing the JavaScript sources list
    # WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES containing the source file that will load the scripts to add the proper
    #   dependency and having them built at the right moment

    MAKE_JS_FILE_ARRAYS(
        ${WebCore_DERIVED_SOURCES_DIR}/UserAgentScriptsData.cpp
        ${WebCore_DERIVED_SOURCES_DIR}/UserAgentScripts.h
        WebCore
        WebCore_USER_AGENT_SCRIPTS
        WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES
    )
    list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/UserAgentScriptsData.cpp)
endif ()

# Generate plug-in resources
add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/PlugInsResourcesData.cpp ${WebCore_DERIVED_SOURCES_DIR}/PlugInsResources.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/make-css-file-arrays.pl
    DEPENDS ${WebCore_PLUG_INS_RESOURCES} ${WEBCORE_DIR}/bindings/scripts/preprocessor.pm
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/make-css-file-arrays.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" ${WebCore_DERIVED_SOURCES_DIR}/PlugInsResources.h ${WebCore_DERIVED_SOURCES_DIR}/PlugInsResourcesData.cpp ${WebCore_PLUG_INS_RESOURCES}
    VERBATIM)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/PlugInsResourcesData.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/StyleResolver.cpp PlugInsResourcesData.cpp PlugInsResources.h)

set(FEATURE_DEFINES_JAVASCRIPT "LANGUAGE_JAVASCRIPT ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")

GENERATE_SETTINGS_MACROS(${WEBCORE_DIR}/page/Settings.yaml Settings.h)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/Settings.h)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/Settings.cpp)
list(APPEND WebCoreTestSupport_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/InternalSettingsGenerated.cpp)
list(APPEND WebCoreTestSupport_IDL_FILES ${WebCore_DERIVED_SOURCES_DIR}/InternalSettingsGenerated.idl)

GENERATE_BINDINGS(WebCoreBindings
    OUTPUT_SOURCE WebCore_SOURCES
    INPUT_FILES ${WebCore_IDL_FILES}
    PP_INPUT_FILES ${WebCore_IDL_FILES} ${WebCoreTestSupport_IDL_FILES}
    BASE_DIR ${WEBCORE_DIR}
    IDL_INCLUDES ${WebCore_IDL_INCLUDES} ${WebCore_DERIVED_SOURCES_DIR}
    FEATURES ${FEATURE_DEFINES_JAVASCRIPT}
    DESTINATION ${WebCore_DERIVED_SOURCES_DIR}
    GENERATOR JS
    SUPPLEMENTAL_DEPFILE ${SUPPLEMENTAL_DEPENDENCY_FILE}
    PP_EXTRA_OUTPUT
        ${ISO_SUBSPACES_HEADER_FILE} ${WINDOW_CONSTRUCTORS_FILE} ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE} ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    PP_EXTRA_ARGS
        --isoSubspacesHeaderFile ${ISO_SUBSPACES_HEADER_FILE}
        --windowConstructorsFile ${WINDOW_CONSTRUCTORS_FILE}
        --workerGlobalScopeConstructorsFile ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
        --dedicatedWorkerGlobalScopeConstructorsFile ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
        --serviceWorkerGlobalScopeConstructorsFile ${SERVICEWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
        --workletGlobalScopeConstructorsFile ${WORKLETGLOBALSCOPE_CONSTRUCTORS_FILE}
        --paintWorkletGlobalScopeConstructorsFile ${PAINTWORKLETGLOBALSCOPE_CONSTRUCTORS_FILE})
list(APPEND WebCore_DEPENDENCIES WebCoreBindings)

if (ENABLE_WEBINSPECTORUI)
    list(APPEND WebCore_DEPENDENCIES WebInspectorUI)
endif ()

GENERATE_BINDINGS(WebCoreTestSupportBindings
    OUTPUT_SOURCE WebCoreTestSupport_SOURCES
    INPUT_FILES ${WebCoreTestSupport_IDL_FILES}
    BASE_DIR ${WEBCORE_DIR}
    IDL_INCLUDES ${WebCore_IDL_INCLUDES} ${WebCore_DERIVED_SOURCES_DIR} ${WebCoreTestSupport_IDL_INCLUDES}
    FEATURES ${FEATURE_DEFINES_JAVASCRIPT}
    DESTINATION ${WebCore_DERIVED_SOURCES_DIR}
    GENERATOR JS)

# WebCoreTestSupportBindings needs to have a direct or indirect
# dependency to WebCoreBindings for CMake Visual Studio generator to
# eliminate duplicated custom commands. Otherwise,
# GenerateSettings.rb will be triggered in both projects.
add_dependencies(WebCoreTestSupportBindings WebCoreBindings)

# WebCore JS Builtins

set(WebCore_BUILTINS_SOURCES
    ${WEBCORE_DIR}/Modules/streams/ByteLengthQueuingStrategy.js
    ${WEBCORE_DIR}/Modules/streams/CountQueuingStrategy.js
    ${WEBCORE_DIR}/Modules/streams/ReadableByteStreamController.js
    ${WEBCORE_DIR}/Modules/streams/ReadableByteStreamInternals.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStream.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamBYOBReader.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamBYOBRequest.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamDefaultController.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamInternals.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamDefaultReader.js
    ${WEBCORE_DIR}/Modules/streams/StreamInternals.js
    ${WEBCORE_DIR}/Modules/streams/TransformStream.js
    ${WEBCORE_DIR}/Modules/streams/TransformStreamDefaultController.js
    ${WEBCORE_DIR}/Modules/streams/TransformStreamInternals.js
    ${WEBCORE_DIR}/Modules/streams/WritableStream.js
    ${WEBCORE_DIR}/Modules/streams/WritableStreamDefaultController.js
    ${WEBCORE_DIR}/Modules/streams/WritableStreamDefaultWriter.js
    ${WEBCORE_DIR}/Modules/streams/WritableStreamInternals.js
    ${WEBCORE_DIR}/dom/TextDecoderStream.js
    ${WEBCORE_DIR}/dom/TextEncoderStream.js
    ${WEBCORE_DIR}/bindings/js/JSDOMBindingInternals.js
)

set(BUILTINS_GENERATOR_SCRIPTS
    ${JavaScriptCore_SCRIPTS_DIR}/wkbuiltins.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generator.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_model.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_templates.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_combined_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_combined_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_separate_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_separate_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_wrapper_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_wrapper_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_internals_wrapper_header.py
    ${JavaScriptCore_SCRIPTS_DIR}/builtins_generate_internals_wrapper_implementation.py
    ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py
    ${JavaScriptCore_SCRIPTS_DIR}/lazywriter.py
)

foreach (_builtinSource ${WebCore_BUILTINS_SOURCES})
    get_filename_component(_objectName ${_builtinSource} NAME_WE)
    add_custom_command(
        OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/${_objectName}Builtins.cpp
               ${WebCore_DERIVED_SOURCES_DIR}/${_objectName}Builtins.h
        MAIN_DEPENDENCY ${_builtinSource}
        DEPENDS ${BUILTINS_GENERATOR_SCRIPTS}
        COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py --framework WebCore --output-directory ${WebCore_DERIVED_SOURCES_DIR} ${_builtinSource}
        VERBATIM)
    list(APPEND WebCore_DERIVED_BUILTIN_HEADERS ${WebCore_DERIVED_SOURCES_DIR}/${_objectName}Builtins.h)
endforeach ()

add_custom_command(
    OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/WebCoreJSBuiltins.cpp
           ${WebCore_DERIVED_SOURCES_DIR}/WebCoreJSBuiltinInternals.cpp
           ${WebCore_DERIVED_SOURCES_DIR}/WebCoreJSBuiltins.h
           ${WebCore_DERIVED_SOURCES_DIR}/WebCoreJSBuiltinInternals.h
    MAIN_DEPENDENCY ${WebCore_BUILTINS_SOURCES}
    DEPENDS ${BUILTINS_GENERATOR_SCRIPTS} ${WebCore_DERIVED_BUILTIN_HEADERS}
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py --wrappers-only --framework WebCore --output-directory ${WebCore_DERIVED_SOURCES_DIR} ${WebCore_BUILTINS_SOURCES}
    VERBATIM)
list(APPEND WebCore_SOURCES
    ${WebCore_DERIVED_SOURCES_DIR}/WebCoreJSBuiltins.cpp
    ${WebCore_DERIVED_SOURCES_DIR}/WebCoreJSBuiltinInternals.cpp
    ${WebCore_DERIVED_SOURCES_DIR}/WebCoreJSBuiltins.h
    ${WebCore_DERIVED_SOURCES_DIR}/WebCoreJSBuiltinInternals.h)

ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/html/HTMLTreeBuilder.cpp MathMLNames.cpp)


GENERATE_DOM_NAMES(HTML ${WEBCORE_DIR}/html/HTMLAttributeNames.in ${WEBCORE_DIR}/html/HTMLTagNames.in)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/HTMLNames.cpp ${WebCore_DERIVED_SOURCES_DIR}/HTMLElementFactory.cpp ${WebCore_DERIVED_SOURCES_DIR}/JSHTMLElementWrapperFactory.cpp)


GENERATE_EVENT_FACTORY(${WEBCORE_DIR}/dom/EventNames.in Event)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/EventInterfaces.h)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/EventFactory.cpp)


GENERATE_EVENT_FACTORY(${WEBCORE_DIR}/dom/EventTargetFactory.in EventTarget)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/EventTargetInterfaces.h)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/EventTargetFactory.cpp)


GENERATE_FONT_NAMES(${WEBCORE_DIR}/css/WebKitFontFamilyNames.in)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/WebKitFontFamilyNames.cpp)


GENERATE_DOM_NAMES(MathML ${WEBCORE_DIR}/mathml/mathattrs.in ${WEBCORE_DIR}/mathml/mathtags.in)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/MathMLNames.cpp)
if (ENABLE_MATHML)
    list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/MathMLElementFactory.cpp)
endif ()

GENERATE_DOM_NAMES(SVG ${WEBCORE_DIR}/svg/svgattrs.in ${WEBCORE_DIR}/svg/svgtags.in)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/SVGNames.cpp)

list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/SVGElementFactory.cpp ${WebCore_DERIVED_SOURCES_DIR}/JSSVGElementWrapperFactory.cpp)

GENERATE_DOM_NAMES(XLink ${WEBCORE_DIR}/svg/xlinkattrs.in)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/XLinkNames.cpp)


GENERATE_DOM_NAMES(XMLNS ${WEBCORE_DIR}/xml/xmlnsattrs.in)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/XMLNSNames.cpp)

GENERATE_DOM_NAMES(XML ${WEBCORE_DIR}/xml/xmlattrs.in)
list(APPEND WebCore_SOURCES ${WebCore_DERIVED_SOURCES_DIR}/XMLNames.cpp)

WEBKIT_WRAP_SOURCELIST(${WebCore_IDL_FILES})
WEBKIT_COMPUTE_SOURCES(WebCore)
WEBKIT_WRAP_SOURCELIST(${WebCore_SOURCES})
WEBKIT_WRAP_SOURCELIST(${WebCoreTestSupport_IDL_FILES} ${WebCoreTestSupport_SOURCES})

if (MSVC)
    WEBKIT_ADD_PRECOMPILED_HEADER("WebCorePrefix.h" "WebCorePrefix.cpp" WebCore_SOURCES)
endif ()

WEBKIT_COPY_FILES(WebCore_CopyPrivateHeaders
    DESTINATION ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCore
    FILES ${WebCore_PRIVATE_FRAMEWORK_HEADERS}
    FLATTENED
)
list(APPEND WebCore_INTERFACE_DEPENDENCIES WebCore_CopyPrivateHeaders)
# WebCore_CopyPrivateHeaders needs to have a direct or indirect
# dependency of WebCore target for CMake Visual Studio generator to
# eliminate duplicated custom commands. Otherwise, CSSPropertyNames.h
# will be generated in both projects.
if (NOT INTERNAL_BUILD)
    add_dependencies(WebCore_CopyPrivateHeaders WebCore)
endif ()

WEBKIT_FRAMEWORK(WebCore)
WEBKIT_FRAMEWORK_TARGET(WebCore)

if (APPLE)
    set_target_properties(WebCore PROPERTIES LINK_FLAGS "-sub_library libobjc -umbrella WebKit -allowable_client WebCoreTestSupport -allowable_client WebKit2 -allowable_client WebKitLegacy")
endif ()

# The -ftree-sra optimization (implicit with -O2) causes crashes when
# allocating large chunks of memory using bmalloc on Intel 32bit.
# See https://bugs.webkit.org/show_bug.cgi?id=146440
string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR)
if (CMAKE_COMPILER_IS_GNUCXX AND "${LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR}" MATCHES "(i[3-6]86|x86)$")
    WEBKIT_ADD_TARGET_CXX_FLAGS(WebCore -fno-tree-sra)
endif ()

if (MSVC)
    WEBKIT_ADD_PRECOMPILED_HEADER("WebCoreTestSupportPrefix.h" "testing/js/WebCoreTestSupportPrefix.cpp" WebCoreTestSupport_SOURCES)
endif ()

WEBKIT_COPY_FILES(Copy_WebCoreTestSupportPrivateHeaders
    DESTINATION ${WebCore_PRIVATE_FRAMEWORK_HEADERS_DIR}/WebCoreTestSupport
    FILES testing/js/WebCoreTestSupport.h
    FLATTENED
)
list(APPEND WebCoreTestSupport_INTERFACE_DEPENDENCIES Copy_WebCoreTestSupportPrivateHeaders)

list(APPEND WebCoreTestSupport_INCLUDE_DIRECTORIES ${WebCore_INCLUDE_DIRECTORIES})
list(APPEND WebCoreTestSupport_PRIVATE_INCLUDE_DIRECTORIES ${WebCore_PRIVATE_INCLUDE_DIRECTORIES})
list(APPEND WebCoreTestSupport_SYSTEM_INCLUDE_DIRECTORIES ${WebCore_SYSTEM_INCLUDE_DIRECTORIES})

if (WebCore_LIBRARY_TYPE STREQUAL SHARED)
    list(APPEND WebCoreTestSupport_LIBRARIES WebCore)
else ()
    list(APPEND WebCoreTestSupport_LIBRARIES ${WebCore_LIBRARIES})
    list(APPEND WebCoreTestSupport_PRIVATE_LIBRARIES ${WebCore_PRIVATE_LIBRARIES})
    list(APPEND WebCoreTestSupport_DEPENDENCIES WebCore)
endif ()

WEBKIT_FRAMEWORK(WebCoreTestSupport)
WEBKIT_FRAMEWORK_TARGET(WebCoreTestSupport)

if (${WebCore_LIBRARY_TYPE} MATCHES "SHARED")
    set_target_properties(WebCore PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
    install(TARGETS WebCore DESTINATION "${LIB_INSTALL_DIR}")
endif ()

if (MSVC)
    set_target_properties(WebCore PROPERTIES
        COMPILE_PDB_NAME ${WebCore_OUTPUT_NAME}
    )
endif ()
