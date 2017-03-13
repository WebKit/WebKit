cmake_minimum_required(VERSION 2.8.12)
include(WebKitCommon)
set_property(DIRECTORY . PROPERTY FOLDER "WebCore")
add_subdirectory(PAL/pal)

set(WebCore_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${WEBCORE_DIR}"
    "${WEBCORE_DIR}/Modules/airplay"
    "${WEBCORE_DIR}/Modules/applepay"
    "${WEBCORE_DIR}/Modules/encryptedmedia"
    "${WEBCORE_DIR}/Modules/encryptedmedia/legacy"
    "${WEBCORE_DIR}/Modules/fetch"
    "${WEBCORE_DIR}/Modules/geolocation"
    "${WEBCORE_DIR}/Modules/indexeddb"
    "${WEBCORE_DIR}/Modules/indexeddb/client"
    "${WEBCORE_DIR}/Modules/indexeddb/server"
    "${WEBCORE_DIR}/Modules/indexeddb/shared"
    "${WEBCORE_DIR}/Modules/mediacontrols/"
    "${WEBCORE_DIR}/Modules/mediasession"
    "${WEBCORE_DIR}/Modules/mediasource"
    "${WEBCORE_DIR}/Modules/mediastream"
    "${WEBCORE_DIR}/Modules/navigatorcontentutils"
    "${WEBCORE_DIR}/Modules/notifications"
    "${WEBCORE_DIR}/Modules/plugins"
    "${WEBCORE_DIR}/Modules/proximity"
    "${WEBCORE_DIR}/Modules/quota"
    "${WEBCORE_DIR}/Modules/speech"
    "${WEBCORE_DIR}/Modules/streams"
    "${WEBCORE_DIR}/Modules/vibration"
    "${WEBCORE_DIR}/Modules/webaudio"
    "${WEBCORE_DIR}/Modules/webdatabase"
    "${WEBCORE_DIR}/Modules/webdriver"
    "${WEBCORE_DIR}/Modules/websockets"
    "${WEBCORE_DIR}/accessibility"
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
    "${WEBCORE_DIR}/cssjit"
    "${WEBCORE_DIR}/dom"
    "${WEBCORE_DIR}/dom/default"
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
    "${WEBCORE_DIR}/loader"
    "${WEBCORE_DIR}/loader/appcache"
    "${WEBCORE_DIR}/loader/archive"
    "${WEBCORE_DIR}/loader/archive/mhtml"
    "${WEBCORE_DIR}/loader/cache"
    "${WEBCORE_DIR}/loader/icon"
    "${WEBCORE_DIR}/mathml"
    "${WEBCORE_DIR}/page"
    "${WEBCORE_DIR}/page/animation"
    "${WEBCORE_DIR}/page/csp"
    "${WEBCORE_DIR}/page/scrolling"
    "${WEBCORE_DIR}/platform"
    "${WEBCORE_DIR}/platform/animation"
    "${WEBCORE_DIR}/platform/audio"
    "${WEBCORE_DIR}/platform/crypto"
    "${WEBCORE_DIR}/platform/gamepad"
    "${WEBCORE_DIR}/platform/gamepad/deprecated"
    "${WEBCORE_DIR}/platform/gamepad/linux"
    "${WEBCORE_DIR}/platform/graphics"
    "${WEBCORE_DIR}/platform/graphics/cpu/arm"
    "${WEBCORE_DIR}/platform/graphics/cpu/arm/filters"
    "${WEBCORE_DIR}/platform/graphics/displaylists"
    "${WEBCORE_DIR}/platform/graphics/filters"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz"
    "${WEBCORE_DIR}/platform/graphics/harfbuzz/ng"
    "${WEBCORE_DIR}/platform/graphics/opentype"
    "${WEBCORE_DIR}/platform/graphics/transforms"
    "${WEBCORE_DIR}/platform/mediastream"
    "${WEBCORE_DIR}/platform/mediastream/libwebrtc"
    "${WEBCORE_DIR}/platform/mock"
    "${WEBCORE_DIR}/platform/mock/mediasource"
    "${WEBCORE_DIR}/platform/network"
    "${WEBCORE_DIR}/platform/sql"
    "${WEBCORE_DIR}/platform/text"
    "${WEBCORE_DIR}/platform/text/icu"
    "${WEBCORE_DIR}/plugins"
    "${WEBCORE_DIR}/rendering"
    "${WEBCORE_DIR}/rendering/line"
    "${WEBCORE_DIR}/rendering/mathml"
    "${WEBCORE_DIR}/rendering/shapes"
    "${WEBCORE_DIR}/rendering/style"
    "${WEBCORE_DIR}/rendering/svg"
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
    "${WEBCORE_DIR}/xml"
    "${WEBCORE_DIR}/xml/parser"
    "${DERIVED_SOURCES_WEBCORE_DIR}"
    "${CMAKE_SOURCE_DIR}/Source"
)

set(WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${ICU_INCLUDE_DIRS}
)

set(WebCore_IDL_INCLUDES
    animation
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
    Modules/encryptedmedia/legacy
    Modules/fetch
    Modules/geolocation
    Modules/indexeddb
    Modules/mediasession
    Modules/mediastream
    Modules/notifications
    Modules/proximity
    Modules/quota
    Modules/speech
    Modules/streams
    Modules/vibration
    Modules/webaudio
    Modules/webdatabase
    Modules/websockets

    bindings/js

    html/canvas
    html/shadow
    html/track

    loader/appcache
)

set(WebCore_NON_SVG_IDL_FILES
    Modules/airplay/WebKitPlaybackTargetAvailabilityEvent.idl

    Modules/fetch/DOMWindowFetch.idl
    Modules/fetch/FetchBody.idl
    Modules/fetch/FetchHeaders.idl
    Modules/fetch/FetchRequest.idl
    Modules/fetch/FetchResponse.idl
    Modules/fetch/WorkerGlobalScopeFetch.idl

    Modules/geolocation/Coordinates.idl
    Modules/geolocation/Geolocation.idl
    Modules/geolocation/Geoposition.idl
    Modules/geolocation/NavigatorGeolocation.idl
    Modules/geolocation/PositionCallback.idl
    Modules/geolocation/PositionError.idl
    Modules/geolocation/PositionErrorCallback.idl
    Modules/geolocation/PositionOptions.idl

    Modules/indexeddb/DOMWindowIndexedDatabase.idl
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
    Modules/indexeddb/WorkerGlobalScopeIndexedDatabase.idl

    Modules/mediacontrols/MediaControlsHost.idl

    Modules/mediasession/HTMLMediaElementMediaSession.idl
    Modules/mediasession/MediaRemoteControls.idl
    Modules/mediasession/MediaSession.idl

    Modules/mediasource/DOMURLMediaSource.idl
    Modules/mediasource/MediaSource.idl
    Modules/mediasource/SourceBuffer.idl
    Modules/mediasource/SourceBufferList.idl
    Modules/mediasource/VideoPlaybackQuality.idl

    Modules/mediastream/DOMURLMediaStream.idl
    Modules/mediastream/DoubleRange.idl
    Modules/mediastream/LongRange.idl
    Modules/mediastream/MediaDeviceInfo.idl
    Modules/mediastream/MediaDevices.idl
    Modules/mediastream/MediaStream.idl
    Modules/mediastream/MediaStreamEvent.idl
    Modules/mediastream/MediaStreamTrack.idl
    Modules/mediastream/MediaStreamTrackEvent.idl
    Modules/mediastream/MediaTrackConstraints.idl
    Modules/mediastream/MediaTrackSupportedConstraints.idl
    Modules/mediastream/NavigatorMediaDevices.idl
    Modules/mediastream/NavigatorUserMedia.idl
    Modules/mediastream/OverconstrainedError.idl
    Modules/mediastream/OverconstrainedErrorEvent.idl
    Modules/mediastream/RTCConfiguration.idl
    Modules/mediastream/RTCDTMFSender.idl
    Modules/mediastream/RTCDTMFToneChangeEvent.idl
    Modules/mediastream/RTCDataChannel.idl
    Modules/mediastream/RTCDataChannelEvent.idl
    Modules/mediastream/RTCIceCandidate.idl
    Modules/mediastream/RTCIceCandidateEvent.idl
    Modules/mediastream/RTCIceServer.idl
    Modules/mediastream/RTCPeerConnection.idl
    Modules/mediastream/RTCRtpReceiver.idl
    Modules/mediastream/RTCRtpSender.idl
    Modules/mediastream/RTCRtpTransceiver.idl
    Modules/mediastream/RTCSessionDescription.idl
    Modules/mediastream/RTCStatsReport.idl
    Modules/mediastream/RTCTrackEvent.idl

    Modules/navigatorcontentutils/NavigatorContentUtils.idl

    Modules/notifications/DOMWindowNotifications.idl
    Modules/notifications/Notification.idl
    Modules/notifications/NotificationCenter.idl
    Modules/notifications/NotificationPermissionCallback.idl
    Modules/notifications/WorkerGlobalScopeNotifications.idl

    Modules/proximity/DeviceProximityEvent.idl

    Modules/speech/DOMWindowSpeechSynthesis.idl
    Modules/speech/SpeechSynthesis.idl
    Modules/speech/SpeechSynthesisEvent.idl
    Modules/speech/SpeechSynthesisUtterance.idl
    Modules/speech/SpeechSynthesisVoice.idl

    Modules/streams/ByteLengthQueuingStrategy.idl
    Modules/streams/CountQueuingStrategy.idl
    Modules/streams/ReadableByteStreamController.idl
    Modules/streams/ReadableStream.idl
    Modules/streams/ReadableStreamDefaultController.idl
    Modules/streams/ReadableStreamDefaultReader.idl
    Modules/streams/ReadableStreamSource.idl
    Modules/streams/WritableStream.idl

    Modules/vibration/NavigatorVibration.idl

    Modules/webaudio/AnalyserNode.idl
    Modules/webaudio/AudioBuffer.idl
    Modules/webaudio/AudioBufferCallback.idl
    Modules/webaudio/AudioBufferSourceNode.idl
    Modules/webaudio/AudioContext.idl
    Modules/webaudio/AudioDestinationNode.idl
    Modules/webaudio/AudioListener.idl
    Modules/webaudio/AudioNode.idl
    Modules/webaudio/AudioParam.idl
    Modules/webaudio/AudioProcessingEvent.idl
    Modules/webaudio/BiquadFilterNode.idl
    Modules/webaudio/ChannelMergerNode.idl
    Modules/webaudio/ChannelSplitterNode.idl
    Modules/webaudio/ConvolverNode.idl
    Modules/webaudio/DelayNode.idl
    Modules/webaudio/DynamicsCompressorNode.idl
    Modules/webaudio/GainNode.idl
    Modules/webaudio/MediaElementAudioSourceNode.idl
    Modules/webaudio/MediaStreamAudioDestinationNode.idl
    Modules/webaudio/MediaStreamAudioSourceNode.idl
    Modules/webaudio/OfflineAudioCompletionEvent.idl
    Modules/webaudio/OfflineAudioContext.idl
    Modules/webaudio/OscillatorNode.idl
    Modules/webaudio/PannerNode.idl
    Modules/webaudio/PeriodicWave.idl
    Modules/webaudio/ScriptProcessorNode.idl
    Modules/webaudio/WaveShaperNode.idl

    Modules/webdatabase/DOMWindowWebDatabase.idl
    Modules/webdatabase/Database.idl
    Modules/webdatabase/DatabaseCallback.idl
    Modules/webdatabase/SQLError.idl
    Modules/webdatabase/SQLException.idl
    Modules/webdatabase/SQLResultSet.idl
    Modules/webdatabase/SQLResultSetRowList.idl
    Modules/webdatabase/SQLStatementCallback.idl
    Modules/webdatabase/SQLStatementErrorCallback.idl
    Modules/webdatabase/SQLTransaction.idl
    Modules/webdatabase/SQLTransactionCallback.idl
    Modules/webdatabase/SQLTransactionErrorCallback.idl

    Modules/webdriver/NavigatorWebDriver.idl

    Modules/websockets/CloseEvent.idl
    Modules/websockets/WebSocket.idl

    animation/Animatable.idl
    animation/AnimationEffect.idl
    animation/AnimationTimeline.idl
    animation/DocumentAnimation.idl
    animation/DocumentTimeline.idl
    animation/KeyframeEffect.idl
    animation/WebAnimation.idl

    crypto/CryptoAlgorithmParameters.idl
    crypto/CryptoKey.idl
    crypto/CryptoKeyPair.idl
    crypto/CryptoKeyUsage.idl
    crypto/JsonWebKey.idl
    crypto/RsaOtherPrimesInfo.idl
    crypto/SubtleCrypto.idl
    crypto/WebKitSubtleCrypto.idl

    crypto/parameters/AesCbcParams.idl
    crypto/parameters/AesKeyGenParams.idl
    crypto/parameters/HmacKeyParams.idl
    crypto/parameters/RsaHashedImportParams.idl
    crypto/parameters/RsaHashedKeyGenParams.idl
    crypto/parameters/RsaKeyGenParams.idl
    crypto/parameters/RsaOaepParams.idl

    css/CSSFontFaceLoadEvent.idl
    css/CSSFontFaceRule.idl
    css/CSSImportRule.idl
    css/CSSKeyframeRule.idl
    css/CSSKeyframesRule.idl
    css/CSSMediaRule.idl
    css/CSSNamespaceRule.idl
    css/CSSPageRule.idl
    css/CSSRule.idl
    css/CSSRuleList.idl
    css/CSSStyleDeclaration.idl
    css/CSSStyleRule.idl
    css/CSSStyleSheet.idl
    css/CSSSupportsRule.idl
    css/CSSUnknownRule.idl
    css/DeprecatedCSSOMCounter.idl
    css/DeprecatedCSSOMPrimitiveValue.idl
    css/DeprecatedCSSOMRGBColor.idl
    css/DeprecatedCSSOMRect.idl
    css/DeprecatedCSSOMValue.idl
    css/DeprecatedCSSOMValueList.idl
    css/DOMCSSNamespace.idl
    css/FontFace.idl
    css/FontFaceSet.idl
    css/MediaList.idl
    css/MediaQueryList.idl
    css/MediaQueryListListener.idl
    css/StyleMedia.idl
    css/StyleSheet.idl
    css/StyleSheetList.idl
    css/WebKitCSSMatrix.idl
    css/WebKitCSSRegionRule.idl
    css/WebKitCSSViewportRule.idl

    dom/AnimationEvent.idl
    dom/Attr.idl
    dom/BeforeLoadEvent.idl
    dom/BeforeUnloadEvent.idl
    dom/CDATASection.idl
    dom/CharacterData.idl
    dom/ChildNode.idl
    dom/ClientRect.idl
    dom/ClientRectList.idl
    dom/ClipboardEvent.idl
    dom/Comment.idl
    dom/CompositionEvent.idl
    dom/CustomElementRegistry.idl
    dom/CustomEvent.idl
    dom/DOMCoreException.idl
    dom/DOMError.idl
    dom/DOMImplementation.idl
    dom/DOMNamedFlowCollection.idl
    dom/DOMPoint.idl
    dom/DOMPointInit.idl
    dom/DOMPointReadOnly.idl
    dom/DOMRect.idl
    dom/DOMRectInit.idl
    dom/DOMRectReadOnly.idl
    dom/DOMStringList.idl
    dom/DOMStringMap.idl
    dom/DataTransfer.idl
    dom/DataTransferItem.idl
    dom/DataTransferItemList.idl
    dom/DeviceMotionEvent.idl
    dom/DeviceOrientationEvent.idl
    dom/Document.idl
    dom/DocumentFragment.idl
    dom/DocumentOrShadowRoot.idl
    dom/DocumentType.idl
    dom/Element.idl
    dom/ErrorEvent.idl
    dom/Event.idl
    dom/EventInit.idl
    dom/EventModifierInit.idl
    dom/EventTarget.idl
    dom/FocusEvent.idl
    dom/GlobalEventHandlers.idl
    dom/HashChangeEvent.idl
    dom/InputEvent.idl
    dom/KeyboardEvent.idl
    dom/MessageChannel.idl
    dom/MessageEvent.idl
    dom/MessagePort.idl
    dom/MouseEvent.idl
    dom/MouseEventInit.idl
    dom/MutationEvent.idl
    dom/MutationObserver.idl
    dom/MutationRecord.idl
    dom/NamedNodeMap.idl
    dom/Node.idl
    dom/NodeFilter.idl
    dom/NodeIterator.idl
    dom/NodeList.idl
    dom/NonDocumentTypeChildNode.idl
    dom/NonElementParentNode.idl
    dom/OverflowEvent.idl
    dom/PageTransitionEvent.idl
    dom/ParentNode.idl
    dom/PopStateEvent.idl
    dom/ProcessingInstruction.idl
    dom/ProgressEvent.idl
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
    dom/TextEncoder.idl
    dom/TextEvent.idl
    dom/Touch.idl
    dom/TouchEvent.idl
    dom/TouchList.idl
    dom/TransitionEvent.idl
    dom/TreeWalker.idl
    dom/UIEvent.idl
    dom/UIEventInit.idl
    dom/WebKitAnimationEvent.idl
    dom/WebKitNamedFlow.idl
    dom/WebKitTransitionEvent.idl
    dom/WheelEvent.idl
    dom/XMLDocument.idl

    fileapi/Blob.idl
    fileapi/BlobLineEndings.idl
    fileapi/BlobPropertyBag.idl
    fileapi/File.idl
    fileapi/FileError.idl
    fileapi/FileException.idl
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
    html/HTMLMenuElement.idl
    html/HTMLMetaElement.idl
    html/HTMLMeterElement.idl
    html/HTMLModElement.idl
    html/HTMLOListElement.idl
    html/HTMLObjectElement.idl
    html/HTMLOptGroupElement.idl
    html/HTMLOptionElement.idl
    html/HTMLOptionsCollection.idl
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
    html/ImageData.idl
    html/MediaController.idl
    html/MediaError.idl
    html/RadioNodeList.idl
    html/TextMetrics.idl
    html/TimeRanges.idl
    html/URLSearchParams.idl
    html/ValidityState.idl
    html/VoidCallback.idl

    html/canvas/CanvasGradient.idl
    html/canvas/CanvasPath.idl
    html/canvas/CanvasPattern.idl
    html/canvas/CanvasProxy.idl
    html/canvas/CanvasRenderingContext2D.idl
    html/canvas/DOMPath.idl

    inspector/CommandLineAPIHost.idl
    inspector/InspectorFrontendHost.idl

    loader/appcache/DOMApplicationCache.idl

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
    page/NavigatorConcurrentHardware.idl
    page/NavigatorID.idl
    page/NavigatorLanguage.idl
    page/NavigatorOnLine.idl
    page/Performance.idl
    page/PerformanceEntry.idl
    page/PerformanceMark.idl
    page/PerformanceMeasure.idl
    page/PerformanceObserver.idl
    page/PerformanceObserverCallback.idl
    page/PerformanceObserverEntryList.idl
    page/PerformanceNavigation.idl
    page/PerformanceResourceTiming.idl
    page/PerformanceTiming.idl
    page/Screen.idl
    page/ScrollToOptions.idl
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

    xml/DOMParser.idl
    xml/XMLHttpRequest.idl
    xml/XMLHttpRequestEventTarget.idl
    xml/XMLHttpRequestProgressEvent.idl
    xml/XMLHttpRequestUpload.idl
    xml/XMLSerializer.idl
    xml/XPathEvaluator.idl
    xml/XPathException.idl
    xml/XPathExpression.idl
    xml/XPathNSResolver.idl
    xml/XPathResult.idl
    xml/XSLTProcessor.idl
)

set(WebCore_SVG_IDL_FILES
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
    svg/SVGDocument.idl
    svg/SVGElement.idl
    svg/SVGEllipseElement.idl
    svg/SVGException.idl
    svg/SVGExternalResourcesRequired.idl
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

if (WIN32 AND INTERNAL_BUILD)
    set(JavaScriptCore_SCRIPTS_DIR "${CMAKE_BINARY_DIR}/../include/private/JavaScriptCore/Scripts")
    set(ANGLE_FORWARDING_HEADERS_DIR "${CMAKE_BINARY_DIR}/../include/private/ANGLE")
else ()
    set(JavaScriptCore_SCRIPTS_DIR "${DERIVED_SOURCES_DIR}/ForwardingHeaders/JavaScriptCore/Scripts")
    set(ANGLE_FORWARDING_HEADERS_DIR "${DERIVED_SOURCES_DIR}/ForwardingHeaders/ANGLE")
endif ()

set(WebCore_SOURCES
    Modules/airplay/WebKitPlaybackTargetAvailabilityEvent.cpp

    Modules/fetch/DOMWindowFetch.cpp
    Modules/fetch/FetchBody.cpp
    Modules/fetch/FetchBodyConsumer.cpp
    Modules/fetch/FetchBodyOwner.cpp
    Modules/fetch/FetchHeaders.cpp
    Modules/fetch/FetchLoader.cpp
    Modules/fetch/FetchRequest.cpp
    Modules/fetch/FetchResponse.cpp
    Modules/fetch/FetchResponseSource.cpp
    Modules/fetch/WorkerGlobalScopeFetch.cpp

    Modules/geolocation/Coordinates.cpp
    Modules/geolocation/GeoNotifier.cpp
    Modules/geolocation/Geolocation.cpp
    Modules/geolocation/GeolocationController.cpp
    Modules/geolocation/NavigatorGeolocation.cpp

    Modules/indexeddb/DOMWindowIndexedDatabase.cpp
    Modules/indexeddb/IDBCursor.cpp
    Modules/indexeddb/IDBCursorWithValue.cpp
    Modules/indexeddb/IDBDatabase.cpp
    Modules/indexeddb/IDBDatabaseException.cpp
    Modules/indexeddb/IDBDatabaseIdentifier.cpp
    Modules/indexeddb/IDBEventDispatcher.cpp
    Modules/indexeddb/IDBFactory.cpp
    Modules/indexeddb/IDBGetAllResult.cpp
    Modules/indexeddb/IDBGetResult.cpp
    Modules/indexeddb/IDBIndex.cpp
    Modules/indexeddb/IDBKey.cpp
    Modules/indexeddb/IDBKeyData.cpp
    Modules/indexeddb/IDBKeyPath.cpp
    Modules/indexeddb/IDBKeyRange.cpp
    Modules/indexeddb/IDBKeyRangeData.cpp
    Modules/indexeddb/IDBObjectStore.cpp
    Modules/indexeddb/IDBOpenDBRequest.cpp
    Modules/indexeddb/IDBRequest.cpp
    Modules/indexeddb/IDBRequestCompletionEvent.cpp
    Modules/indexeddb/IDBTransaction.cpp
    Modules/indexeddb/IDBValue.cpp
    Modules/indexeddb/IDBVersionChangeEvent.cpp
    Modules/indexeddb/WorkerGlobalScopeIndexedDatabase.cpp

    Modules/indexeddb/client/IDBConnectionProxy.cpp
    Modules/indexeddb/client/IDBConnectionToServer.cpp
    Modules/indexeddb/client/TransactionOperation.cpp

    Modules/indexeddb/server/IDBConnectionToClient.cpp
    Modules/indexeddb/server/IDBSerialization.cpp
    Modules/indexeddb/server/IDBServer.cpp
    Modules/indexeddb/server/IndexValueEntry.cpp
    Modules/indexeddb/server/IndexValueStore.cpp
    Modules/indexeddb/server/MemoryBackingStoreTransaction.cpp
    Modules/indexeddb/server/MemoryCursor.cpp
    Modules/indexeddb/server/MemoryIDBBackingStore.cpp
    Modules/indexeddb/server/MemoryIndex.cpp
    Modules/indexeddb/server/MemoryIndexCursor.cpp
    Modules/indexeddb/server/MemoryObjectStore.cpp
    Modules/indexeddb/server/MemoryObjectStoreCursor.cpp
    Modules/indexeddb/server/SQLiteIDBBackingStore.cpp
    Modules/indexeddb/server/SQLiteIDBCursor.cpp
    Modules/indexeddb/server/SQLiteIDBTransaction.cpp
    Modules/indexeddb/server/ServerOpenDBRequest.cpp
    Modules/indexeddb/server/UniqueIDBDatabase.cpp
    Modules/indexeddb/server/UniqueIDBDatabaseConnection.cpp
    Modules/indexeddb/server/UniqueIDBDatabaseTransaction.cpp

    Modules/indexeddb/shared/IDBCursorInfo.cpp
    Modules/indexeddb/shared/IDBDatabaseInfo.cpp
    Modules/indexeddb/shared/IDBError.cpp
    Modules/indexeddb/shared/IDBGetAllRecordsData.cpp
    Modules/indexeddb/shared/IDBGetRecordData.cpp
    Modules/indexeddb/shared/IDBIndexInfo.cpp
    Modules/indexeddb/shared/IDBIterateCursorData.cpp
    Modules/indexeddb/shared/IDBObjectStoreInfo.cpp
    Modules/indexeddb/shared/IDBRequestData.cpp
    Modules/indexeddb/shared/IDBResourceIdentifier.cpp
    Modules/indexeddb/shared/IDBResultData.cpp
    Modules/indexeddb/shared/IDBTransactionInfo.cpp
    Modules/indexeddb/shared/InProcessIDBServer.cpp
    Modules/indexeddb/shared/IndexKey.cpp

    Modules/mediacontrols/MediaControlsHost.cpp

    Modules/mediasession/HTMLMediaElementMediaSession.cpp
    Modules/mediasession/MediaRemoteControls.cpp
    Modules/mediasession/MediaSession.cpp

    Modules/mediasource/DOMURLMediaSource.cpp
    Modules/mediasource/MediaSource.cpp
    Modules/mediasource/MediaSourceRegistry.cpp
    Modules/mediasource/SampleMap.cpp
    Modules/mediasource/SourceBuffer.cpp
    Modules/mediasource/SourceBufferList.cpp
    Modules/mediasource/VideoPlaybackQuality.cpp

    Modules/mediastream/DOMURLMediaStream.cpp
    Modules/mediastream/MediaConstraintsImpl.cpp
    Modules/mediastream/MediaDeviceInfo.cpp
    Modules/mediastream/MediaDevices.cpp
    Modules/mediastream/MediaDevicesEnumerationRequest.cpp
    Modules/mediastream/MediaDevicesRequest.cpp
    Modules/mediastream/MediaEndpointPeerConnection.cpp
    Modules/mediastream/MediaEndpointSessionDescription.cpp
    Modules/mediastream/MediaStream.cpp
    Modules/mediastream/MediaStreamEvent.cpp
    Modules/mediastream/MediaStreamRegistry.cpp
    Modules/mediastream/MediaStreamTrack.cpp
    Modules/mediastream/MediaStreamTrackEvent.cpp
    Modules/mediastream/MediaTrackConstraints.cpp
    Modules/mediastream/NavigatorMediaDevices.cpp
    Modules/mediastream/PeerConnectionBackend.cpp
    Modules/mediastream/RTCDTMFSender.cpp
    Modules/mediastream/RTCDTMFToneChangeEvent.cpp
    Modules/mediastream/RTCDataChannel.cpp
    Modules/mediastream/RTCDataChannelEvent.cpp
    Modules/mediastream/RTCIceCandidate.cpp
    Modules/mediastream/RTCIceCandidateEvent.cpp
    Modules/mediastream/RTCPeerConnection.cpp
    Modules/mediastream/RTCRtpReceiver.cpp
    Modules/mediastream/RTCRtpSender.cpp
    Modules/mediastream/RTCRtpTransceiver.cpp
    Modules/mediastream/RTCSessionDescription.cpp
    Modules/mediastream/RTCStatsReport.cpp
    Modules/mediastream/RTCTrackEvent.cpp
    Modules/mediastream/SDPProcessor.cpp
    Modules/mediastream/UserMediaController.cpp
    Modules/mediastream/UserMediaRequest.cpp

    Modules/navigatorcontentutils/NavigatorContentUtils.cpp

    Modules/notifications/DOMWindowNotifications.cpp
    Modules/notifications/Notification.cpp
    Modules/notifications/NotificationCenter.cpp
    Modules/notifications/NotificationController.cpp
    Modules/notifications/WorkerGlobalScopeNotifications.cpp

    Modules/proximity/DeviceProximityController.cpp
    Modules/proximity/DeviceProximityEvent.cpp

    Modules/speech/DOMWindowSpeechSynthesis.cpp
    Modules/speech/SpeechSynthesis.cpp
    Modules/speech/SpeechSynthesisEvent.cpp
    Modules/speech/SpeechSynthesisUtterance.cpp
    Modules/speech/SpeechSynthesisVoice.cpp

    Modules/vibration/NavigatorVibration.cpp
    Modules/vibration/Vibration.cpp

    Modules/webaudio/AnalyserNode.cpp
    Modules/webaudio/AsyncAudioDecoder.cpp
    Modules/webaudio/AudioBasicInspectorNode.cpp
    Modules/webaudio/AudioBasicProcessorNode.cpp
    Modules/webaudio/AudioBuffer.cpp
    Modules/webaudio/AudioBufferSourceNode.cpp
    Modules/webaudio/AudioContext.cpp
    Modules/webaudio/AudioDestinationNode.cpp
    Modules/webaudio/AudioListener.cpp
    Modules/webaudio/AudioNode.cpp
    Modules/webaudio/AudioNodeInput.cpp
    Modules/webaudio/AudioNodeOutput.cpp
    Modules/webaudio/AudioParam.cpp
    Modules/webaudio/AudioParamTimeline.cpp
    Modules/webaudio/AudioProcessingEvent.cpp
    Modules/webaudio/AudioScheduledSourceNode.cpp
    Modules/webaudio/AudioSummingJunction.cpp
    Modules/webaudio/BiquadDSPKernel.cpp
    Modules/webaudio/BiquadFilterNode.cpp
    Modules/webaudio/BiquadProcessor.cpp
    Modules/webaudio/ChannelMergerNode.cpp
    Modules/webaudio/ChannelSplitterNode.cpp
    Modules/webaudio/ConvolverNode.cpp
    Modules/webaudio/DefaultAudioDestinationNode.cpp
    Modules/webaudio/DelayDSPKernel.cpp
    Modules/webaudio/DelayNode.cpp
    Modules/webaudio/DelayProcessor.cpp
    Modules/webaudio/DynamicsCompressorNode.cpp
    Modules/webaudio/GainNode.cpp
    Modules/webaudio/MediaElementAudioSourceNode.cpp
    Modules/webaudio/MediaStreamAudioDestinationNode.cpp
    Modules/webaudio/MediaStreamAudioSource.cpp
    Modules/webaudio/MediaStreamAudioSourceNode.cpp
    Modules/webaudio/OfflineAudioCompletionEvent.cpp
    Modules/webaudio/OfflineAudioContext.cpp
    Modules/webaudio/OfflineAudioDestinationNode.cpp
    Modules/webaudio/OscillatorNode.cpp
    Modules/webaudio/PannerNode.cpp
    Modules/webaudio/PeriodicWave.cpp
    Modules/webaudio/RealtimeAnalyser.cpp
    Modules/webaudio/ScriptProcessorNode.cpp
    Modules/webaudio/WaveShaperDSPKernel.cpp
    Modules/webaudio/WaveShaperNode.cpp
    Modules/webaudio/WaveShaperProcessor.cpp

    Modules/webdatabase/ChangeVersionWrapper.cpp
    Modules/webdatabase/DOMWindowWebDatabase.cpp
    Modules/webdatabase/Database.cpp
    Modules/webdatabase/DatabaseAuthorizer.cpp
    Modules/webdatabase/DatabaseContext.cpp
    Modules/webdatabase/DatabaseManager.cpp
    Modules/webdatabase/DatabaseTask.cpp
    Modules/webdatabase/DatabaseThread.cpp
    Modules/webdatabase/DatabaseTracker.cpp
    Modules/webdatabase/OriginLock.cpp
    Modules/webdatabase/SQLException.cpp
    Modules/webdatabase/SQLResultSet.cpp
    Modules/webdatabase/SQLResultSetRowList.cpp
    Modules/webdatabase/SQLStatement.cpp
    Modules/webdatabase/SQLTransaction.cpp
    Modules/webdatabase/SQLTransactionBackend.cpp
    Modules/webdatabase/SQLTransactionCoordinator.cpp
    Modules/webdatabase/SQLTransactionStateMachine.cpp

    Modules/webdriver/NavigatorWebDriver.cpp

    Modules/websockets/ThreadableWebSocketChannel.cpp
    Modules/websockets/ThreadableWebSocketChannelClientWrapper.cpp
    Modules/websockets/WebSocket.cpp
    Modules/websockets/WebSocketChannel.cpp
    Modules/websockets/WebSocketDeflateFramer.cpp
    Modules/websockets/WebSocketDeflater.cpp
    Modules/websockets/WebSocketExtensionDispatcher.cpp
    Modules/websockets/WebSocketExtensionParser.cpp
    Modules/websockets/WebSocketFrame.cpp
    Modules/websockets/WebSocketHandshake.cpp
    Modules/websockets/WorkerThreadableWebSocketChannel.cpp

    accessibility/AXObjectCache.cpp
    accessibility/AccessibilityARIAGrid.cpp
    accessibility/AccessibilityARIAGridCell.cpp
    accessibility/AccessibilityARIAGridRow.cpp
    accessibility/AccessibilityAttachment.cpp
    accessibility/AccessibilityImageMapLink.cpp
    accessibility/AccessibilityLabel.cpp
    accessibility/AccessibilityList.cpp
    accessibility/AccessibilityListBox.cpp
    accessibility/AccessibilityListBoxOption.cpp
    accessibility/AccessibilityMathMLElement.cpp
    accessibility/AccessibilityMediaControls.cpp
    accessibility/AccessibilityMenuList.cpp
    accessibility/AccessibilityMenuListOption.cpp
    accessibility/AccessibilityMenuListPopup.cpp
    accessibility/AccessibilityMockObject.cpp
    accessibility/AccessibilityNodeObject.cpp
    accessibility/AccessibilityObject.cpp
    accessibility/AccessibilityProgressIndicator.cpp
    accessibility/AccessibilityRenderObject.cpp
    accessibility/AccessibilitySVGElement.cpp
    accessibility/AccessibilitySVGRoot.cpp
    accessibility/AccessibilityScrollView.cpp
    accessibility/AccessibilityScrollbar.cpp
    accessibility/AccessibilitySlider.cpp
    accessibility/AccessibilitySpinButton.cpp
    accessibility/AccessibilityTable.cpp
    accessibility/AccessibilityTableCell.cpp
    accessibility/AccessibilityTableColumn.cpp
    accessibility/AccessibilityTableHeaderContainer.cpp
    accessibility/AccessibilityTableRow.cpp
    accessibility/AccessibilityTree.cpp
    accessibility/AccessibilityTreeItem.cpp

    animation/AnimationEffect.cpp
    animation/AnimationTimeline.cpp
    animation/DocumentAnimation.cpp
    animation/DocumentTimeline.cpp
    animation/KeyframeEffect.cpp
    animation/WebAnimation.cpp

    bindings/js/CachedModuleScriptLoader.cpp
    bindings/js/CachedScriptFetcher.cpp
    bindings/js/CommonVM.cpp
    bindings/js/DOMWrapperWorld.cpp
    bindings/js/GCController.cpp
    bindings/js/IDBBindingUtilities.cpp
    bindings/js/JSAnimationTimelineCustom.cpp
    bindings/js/JSAttrCustom.cpp
    bindings/js/JSAudioTrackCustom.cpp
    bindings/js/JSAudioTrackListCustom.cpp
    bindings/js/JSBlobCustom.cpp
    bindings/js/JSCSSRuleCustom.cpp
    bindings/js/JSCSSRuleListCustom.cpp
    bindings/js/JSCSSStyleDeclarationCustom.cpp
    bindings/js/JSDOMConvertDate.cpp
    bindings/js/JSDOMConvertNumbers.cpp
    bindings/js/JSDOMConvertStrings.cpp
    bindings/js/JSDOMIterator.cpp
    bindings/js/JSFontFaceCustom.cpp
    bindings/js/JSFontFaceSetCustom.cpp
    bindings/js/JSCSSValueCustom.cpp
    bindings/js/JSCallbackData.cpp
    bindings/js/JSCanvasRenderingContext2DCustom.cpp
    bindings/js/JSCommandLineAPIHostCustom.cpp
    bindings/js/JSCryptoAlgorithmBuilder.cpp
    bindings/js/JSCryptoAlgorithmDictionary.cpp
    bindings/js/JSCryptoCustom.cpp
    bindings/js/JSCryptoKeyCustom.cpp
    bindings/js/JSCryptoKeySerializationJWK.cpp
    bindings/js/JSCryptoOperationData.cpp
    bindings/js/JSCustomElementInterface.cpp
    bindings/js/JSCustomElementRegistryCustom.cpp
    bindings/js/JSCustomEventCustom.cpp
    bindings/js/JSCustomXPathNSResolver.cpp
    bindings/js/JSDOMBindingSecurity.cpp
    bindings/js/JSDOMBuiltinConstructorBase.cpp
    bindings/js/JSDOMConstructorBase.cpp
    bindings/js/JSDOMConstructorWithDocument.cpp
    bindings/js/JSDOMExceptionHandling.cpp
    bindings/js/JSDOMGlobalObject.cpp
    bindings/js/JSDOMGlobalObjectTask.cpp
    bindings/js/JSDOMPromise.cpp
    bindings/js/JSDOMStringMapCustom.cpp
    bindings/js/JSDOMWindowBase.cpp
    bindings/js/JSDOMWindowCustom.cpp
    bindings/js/JSDOMWindowProperties.cpp
    bindings/js/JSDOMWindowShell.cpp
    bindings/js/JSDOMWrapper.cpp
    bindings/js/JSDOMWrapperCache.cpp
    bindings/js/JSDocumentCustom.cpp
    bindings/js/JSDocumentFragmentCustom.cpp
    bindings/js/JSElementCustom.cpp
    bindings/js/JSErrorHandler.cpp
    bindings/js/JSEventCustom.cpp
    bindings/js/JSEventListener.cpp
    bindings/js/JSEventTargetCustom.cpp
    bindings/js/JSExceptionBase.cpp
    bindings/js/JSHTMLAllCollectionCustom.cpp
    bindings/js/JSHTMLAppletElementCustom.cpp
    bindings/js/JSHTMLCanvasElementCustom.cpp
    bindings/js/JSHTMLCollectionCustom.cpp
    bindings/js/JSHTMLDocumentCustom.cpp
    bindings/js/JSHTMLElementCustom.cpp
    bindings/js/JSHTMLEmbedElementCustom.cpp
    bindings/js/JSHTMLFormControlsCollectionCustom.cpp
    bindings/js/JSHTMLFrameSetElementCustom.cpp
    bindings/js/JSHTMLObjectElementCustom.cpp
    bindings/js/JSHTMLOptionsCollectionCustom.cpp
    bindings/js/JSHTMLSelectElementCustom.cpp
    bindings/js/JSHTMLTemplateElementCustom.cpp
    bindings/js/JSHistoryCustom.cpp
    bindings/js/JSIDBCursorCustom.cpp
    bindings/js/JSIDBCursorWithValueCustom.cpp
    bindings/js/JSIDBIndexCustom.cpp
    bindings/js/JSIDBObjectStoreCustom.cpp
    bindings/js/JSIDBTransactionCustom.cpp
    bindings/js/JSImageDataCustom.cpp
    bindings/js/JSInspectorFrontendHostCustom.cpp
    bindings/js/JSLazyEventListener.cpp
    bindings/js/JSLocationCustom.cpp
    bindings/js/JSMainThreadExecState.cpp
    bindings/js/JSMediaKeySessionCustom.cpp
    bindings/js/JSMessageChannelCustom.cpp
    bindings/js/JSMessageEventCustom.cpp
    bindings/js/JSMessagePortCustom.cpp
    bindings/js/JSMutationCallback.cpp
    bindings/js/JSMutationObserverCustom.cpp
    bindings/js/JSNodeCustom.cpp
    bindings/js/JSNodeFilterCustom.cpp
    bindings/js/JSNodeIteratorCustom.cpp
    bindings/js/JSNodeListCustom.cpp
    bindings/js/JSPerformanceEntryCustom.cpp
    bindings/js/JSPluginElementFunctions.cpp
    bindings/js/JSPopStateEventCustom.cpp
    bindings/js/JSReadableStreamPrivateConstructors.cpp
    bindings/js/JSReadableStreamSourceCustom.cpp
    bindings/js/JSSQLStatementErrorCallbackCustom.cpp
    bindings/js/JSSVGPathSegCustom.cpp
    bindings/js/JSStorageCustom.cpp
    bindings/js/JSStyleSheetCustom.cpp
    bindings/js/JSSubtleCryptoCustom.cpp
    bindings/js/JSTextCustom.cpp
    bindings/js/JSTextTrackCueCustom.cpp
    bindings/js/JSTextTrackCustom.cpp
    bindings/js/JSTextTrackListCustom.cpp
    bindings/js/JSTrackCustom.cpp
    bindings/js/JSTreeWalkerCustom.cpp
    bindings/js/JSVideoTrackCustom.cpp
    bindings/js/JSVideoTrackListCustom.cpp
    bindings/js/JSWebGL2RenderingContextCustom.cpp
    bindings/js/JSWebGLRenderingContextBaseCustom.cpp
    bindings/js/JSWebGLRenderingContextCustom.cpp
    bindings/js/JSWebKitSubtleCryptoCustom.cpp
    bindings/js/JSWorkerCustom.cpp
    bindings/js/JSWorkerGlobalScopeBase.cpp
    bindings/js/JSWorkerGlobalScopeCustom.cpp
    bindings/js/JSXMLDocumentCustom.cpp
    bindings/js/JSXMLHttpRequestCustom.cpp
    bindings/js/JSXPathNSResolverCustom.cpp
    bindings/js/JSXPathResultCustom.cpp
    bindings/js/ReadableStreamDefaultController.cpp
    bindings/js/ScheduledAction.cpp
    bindings/js/ScriptCachedFrameData.cpp
    bindings/js/ScriptController.cpp
    bindings/js/ScriptGlobalObject.cpp
    bindings/js/ScriptModuleLoader.cpp
    bindings/js/ScriptState.cpp
    bindings/js/StructuredClone.cpp
    bindings/js/SerializedScriptValue.cpp
    bindings/js/WebCoreJSClientData.cpp
    bindings/js/WebCoreTypedArrayController.cpp
    bindings/js/WorkerScriptController.cpp

    bridge/IdentifierRep.cpp
    bridge/NP_jsobject.cpp
    bridge/npruntime.cpp
    bridge/runtime_array.cpp
    bridge/runtime_method.cpp
    bridge/runtime_object.cpp
    bridge/runtime_root.cpp

    bridge/c/CRuntimeObject.cpp
    bridge/c/c_class.cpp
    bridge/c/c_instance.cpp
    bridge/c/c_runtime.cpp
    bridge/c/c_utility.cpp

    bridge/jsc/BridgeJSC.cpp

    contentextensions/CombinedFiltersAlphabet.cpp
    contentextensions/CombinedURLFilters.cpp
    contentextensions/CompiledContentExtension.cpp
    contentextensions/ContentExtension.cpp
    contentextensions/ContentExtensionCompiler.cpp
    contentextensions/ContentExtensionError.cpp
    contentextensions/ContentExtensionParser.cpp
    contentextensions/ContentExtensionRule.cpp
    contentextensions/ContentExtensionStyleSheet.cpp
    contentextensions/ContentExtensionsBackend.cpp
    contentextensions/DFA.cpp
    contentextensions/DFABytecodeCompiler.cpp
    contentextensions/DFABytecodeInterpreter.cpp
    contentextensions/DFACombiner.cpp
    contentextensions/DFAMinimizer.cpp
    contentextensions/DFANode.cpp
    contentextensions/NFA.cpp
    contentextensions/NFAToDFA.cpp
    contentextensions/URLFilterParser.cpp

    css/BasicShapeFunctions.cpp
    css/CSSAspectRatioValue.cpp
    css/CSSBasicShapes.cpp
    css/CSSBorderImage.cpp
    css/CSSBorderImageSliceValue.cpp
    css/CSSCalculationValue.cpp
    css/CSSCanvasValue.cpp
    css/CSSComputedStyleDeclaration.cpp
    css/CSSContentDistributionValue.cpp
    css/CSSCrossfadeValue.cpp
    css/CSSCursorImageValue.cpp
    css/CSSCustomIdentValue.cpp
    css/CSSCustomPropertyValue.cpp
    css/CSSDefaultStyleSheets.cpp
    css/CSSFilterImageValue.cpp
    css/FontFaceSet.cpp
    css/FontFace.cpp
    css/CSSFontFace.cpp
    css/CSSFontFaceSet.cpp
    css/CSSFontFaceLoadEvent.cpp
    css/CSSFontFaceRule.cpp
    css/CSSFontFaceSource.cpp
    css/CSSFontFaceSrcValue.cpp
    css/CSSFontFeatureValue.cpp
    css/CSSFontSelector.cpp
    css/CSSFontValue.cpp
    css/CSSFontVariationValue.cpp
    css/CSSFunctionValue.cpp
    css/CSSGradientValue.cpp
    css/CSSGridAutoRepeatValue.cpp
    css/CSSGridLineNamesValue.cpp
    css/CSSGridTemplateAreasValue.cpp
    css/CSSGroupingRule.cpp
    css/CSSImageGeneratorValue.cpp
    css/CSSImageSetValue.cpp
    css/CSSImageValue.cpp
    css/CSSImportRule.cpp
    css/CSSInheritedValue.cpp
    css/CSSInitialValue.cpp
    css/CSSKeyframeRule.cpp
    css/CSSKeyframesRule.cpp
    css/CSSLineBoxContainValue.cpp
    css/CSSMarkup.cpp
    css/CSSMediaRule.cpp
    css/CSSNamedImageValue.cpp
    css/CSSNamespaceRule.cpp
    css/CSSPageRule.cpp
    css/CSSPendingSubstitutionValue.cpp
    css/CSSPrimitiveValue.cpp
    css/CSSProperty.cpp
    css/CSSPropertySourceData.cpp
    css/CSSReflectValue.cpp
    css/CSSRevertValue.cpp
    css/CSSRule.cpp
    css/CSSRuleList.cpp
    css/CSSSegmentedFontFace.cpp
    css/CSSSelector.cpp
    css/CSSSelectorList.cpp
    css/CSSShadowValue.cpp
    css/CSSStyleRule.cpp
    css/CSSStyleSheet.cpp
    css/CSSSupportsRule.cpp
    css/CSSTimingFunctionValue.cpp
    css/CSSToLengthConversionData.cpp
    css/CSSToStyleMap.cpp
    css/CSSUnicodeRangeValue.cpp
    css/CSSUnsetValue.cpp
    css/CSSValue.cpp
    css/CSSValueList.cpp
    css/CSSValuePool.cpp
    css/CSSVariableData.cpp
    css/CSSVariableReferenceValue.cpp
    css/DOMCSSNamespace.cpp
    css/DeprecatedCSSOMPrimitiveValue.cpp
    css/DeprecatedCSSOMValue.cpp
    css/DeprecatedCSSOMValueList.cpp
    css/DocumentRuleSets.cpp
    css/ElementRuleCollector.cpp
    css/FontVariantBuilder.cpp
    css/InspectorCSSOMWrappers.cpp
    css/LengthFunctions.cpp
    css/MediaFeatureNames.cpp
    css/MediaList.cpp
    css/MediaQuery.cpp
    css/MediaQueryEvaluator.cpp
    css/MediaQueryExpression.cpp
    css/MediaQueryList.cpp
    css/MediaQueryMatcher.cpp
    css/PageRuleCollector.cpp
    css/PropertySetCSSStyleDeclaration.cpp
    css/RGBColor.cpp
    css/RuleFeature.cpp
    css/RuleSet.cpp
    css/SVGCSSComputedStyleDeclaration.cpp
    css/SelectorChecker.cpp
    css/SelectorFilter.cpp
    css/StyleColor.cpp
    css/StyleInvalidationAnalysis.cpp
    css/StyleMedia.cpp
    css/StyleProperties.cpp
    css/StylePropertyShorthand.cpp
    css/StyleResolver.cpp
    css/StyleRule.cpp
    css/StyleRuleImport.cpp
    css/StyleSheet.cpp
    css/StyleSheetContents.cpp
    css/StyleSheetList.cpp
    css/TransformFunctions.cpp
    css/ViewportStyleResolver.cpp
    css/WebKitCSSMatrix.cpp
    css/WebKitCSSRegionRule.cpp
    css/WebKitCSSViewportRule.cpp

    css/parser/CSSAtRuleID.cpp
    css/parser/CSSDeferredParser.cpp
    css/parser/CSSParser.cpp
    css/parser/CSSParserFastPaths.cpp
    css/parser/CSSParserIdioms.cpp
    css/parser/CSSParserImpl.cpp
    css/parser/CSSParserObserverWrapper.cpp
    css/parser/CSSParserToken.cpp
    css/parser/CSSParserTokenRange.cpp
    css/parser/CSSParserSelector.cpp
    css/parser/CSSPropertyParser.cpp
    css/parser/CSSPropertyParserHelpers.cpp
    css/parser/CSSSelectorParser.cpp
    css/parser/CSSSupportsParser.cpp
    css/parser/CSSTokenizer.cpp
    css/parser/CSSTokenizerInputStream.cpp
    css/parser/CSSVariableParser.cpp
    css/parser/MediaQueryBlockWatcher.cpp
    css/parser/MediaQueryParser.cpp
    css/parser/SizesAttributeParser.cpp
    css/parser/SizesCalcParser.cpp

    cssjit/SelectorCompiler.cpp

    dom/ActiveDOMCallback.cpp
    dom/ActiveDOMCallbackMicrotask.cpp
    dom/ActiveDOMObject.cpp
    dom/AnimationEvent.cpp
    dom/Attr.cpp
    dom/BeforeTextInsertedEvent.cpp
    dom/BeforeUnloadEvent.cpp
    dom/CDATASection.cpp
    dom/CharacterData.cpp
    dom/ChildListMutationScope.cpp
    dom/ChildNodeList.cpp
    dom/ClassCollection.cpp
    dom/ClientRect.cpp
    dom/ClientRectList.cpp
    dom/ClipboardEvent.cpp
    dom/CollectionIndexCache.cpp
    dom/Comment.cpp
    dom/ComposedTreeIterator.cpp
    dom/CompositionEvent.cpp
    dom/ContainerNode.cpp
    dom/ContainerNodeAlgorithms.cpp
    dom/ContextDestructionObserver.cpp
    dom/CustomElementReactionQueue.cpp
    dom/CustomElementRegistry.cpp
    dom/CustomEvent.cpp
    dom/DOMCoreException.cpp
    dom/DOMError.cpp
    dom/DOMImplementation.cpp
    dom/DOMNamedFlowCollection.cpp
    dom/DOMStringList.cpp
    dom/DataTransfer.cpp
    dom/DataTransferItem.cpp
    dom/DatasetDOMStringMap.cpp
    dom/DecodedDataDocumentParser.cpp
    dom/DeviceMotionController.cpp
    dom/DeviceMotionData.cpp
    dom/DeviceMotionEvent.cpp
    dom/DeviceOrientationController.cpp
    dom/DeviceOrientationData.cpp
    dom/DeviceOrientationEvent.cpp
    dom/Document.cpp
    dom/DocumentEventQueue.cpp
    dom/DocumentFragment.cpp
    dom/DocumentMarkerController.cpp
    dom/DocumentOrderedMap.cpp
    dom/DocumentParser.cpp
    dom/DocumentSharedObjectPool.cpp
    dom/DocumentType.cpp
    dom/Element.cpp
    dom/ElementData.cpp
    dom/ElementRareData.cpp
    dom/ErrorEvent.cpp
    dom/Event.cpp
    dom/EventContext.cpp
    dom/EventDispatcher.cpp
    dom/EventListenerMap.cpp
    dom/EventNames.cpp
    dom/EventPath.cpp
    dom/EventTarget.cpp
    dom/ExceptionBase.cpp
    dom/ExtensionStyleSheets.cpp
    dom/FocusEvent.cpp
    dom/GenericEventQueue.cpp
    dom/IdTargetObserver.cpp
    dom/IdTargetObserverRegistry.cpp
    dom/InlineClassicScript.cpp
    dom/InlineStyleSheetOwner.cpp
    dom/InputEvent.cpp
    dom/KeyboardEvent.cpp
    dom/LiveNodeList.cpp
    dom/LoadableClassicScript.cpp
    dom/LoadableModuleScript.cpp
    dom/LoadableScript.cpp
    dom/MessageChannel.cpp
    dom/MessageEvent.cpp
    dom/MessagePort.cpp
    dom/Microtasks.cpp
    dom/MouseEvent.cpp
    dom/MouseRelatedEvent.cpp
    dom/MutationEvent.cpp
    dom/MutationObserver.cpp
    dom/MutationObserverInterestGroup.cpp
    dom/MutationObserverRegistration.cpp
    dom/MutationRecord.cpp
    dom/NameNodeList.cpp
    dom/NamedFlowCollection.cpp
    dom/NamedNodeMap.cpp
    dom/NativeNodeFilter.cpp
    dom/Node.cpp
    dom/NodeFilterCondition.cpp
    dom/NodeIterator.cpp
    dom/NodeRareData.cpp
    dom/NodeTraversal.cpp
    dom/OverflowEvent.cpp
    dom/PageTransitionEvent.cpp
    dom/PendingScript.cpp
    dom/PopStateEvent.cpp
    dom/Position.cpp
    dom/PositionIterator.cpp
    dom/ProcessingInstruction.cpp
    dom/ProgressEvent.cpp
    dom/PseudoElement.cpp
    dom/QualifiedName.cpp
    dom/RadioButtonGroups.cpp
    dom/Range.cpp
    dom/ScopedEventQueue.cpp
    dom/ScriptElement.cpp
    dom/ScriptElementCachedScriptFetcher.cpp
    dom/ScriptExecutionContext.cpp
    dom/ScriptRunner.cpp
    dom/ScriptableDocumentParser.cpp
    dom/ScriptedAnimationController.cpp
    dom/SecurityContext.cpp
    dom/SecurityOriginPolicy.cpp
    dom/SelectorQuery.cpp
    dom/ShadowRoot.cpp
    dom/SimulatedClick.cpp
    dom/SlotAssignment.cpp
    dom/SpaceSplitString.cpp
    dom/StaticNodeList.cpp
    dom/StaticRange.cpp
    dom/StringCallback.cpp
    dom/StyledElement.cpp
    dom/TagCollection.cpp
    dom/Text.cpp
    dom/TextDecoder.cpp
    dom/TextEncoder.cpp
    dom/TextEvent.cpp
    dom/TextNodeTraversal.cpp
    dom/Touch.cpp
    dom/TouchEvent.cpp
    dom/TouchList.cpp
    dom/TransformSourceLibxslt.cpp
    dom/TransitionEvent.cpp
    dom/Traversal.cpp
    dom/TreeScope.cpp
    dom/TreeScopeAdopter.cpp
    dom/TreeWalker.cpp
    dom/UIEvent.cpp
    dom/UIEventWithKeyState.cpp
    dom/UserActionElementSet.cpp
    dom/UserGestureIndicator.cpp
    dom/UserTypingGestureIndicator.cpp
    dom/ViewportArguments.cpp
    dom/VisitedLinkState.cpp
    dom/WebKitAnimationEvent.cpp
    dom/WebKitNamedFlow.cpp
    dom/WebKitTransitionEvent.cpp
    dom/WheelEvent.cpp

    dom/default/PlatformMessagePortChannel.cpp

    domjit/DOMJITHelpers.cpp
    domjit/JSDocumentDOMJIT.cpp
    domjit/JSNodeDOMJIT.cpp

    editing/AlternativeTextController.cpp
    editing/AppendNodeCommand.cpp
    editing/ApplyBlockElementCommand.cpp
    editing/ApplyStyleCommand.cpp
    editing/BreakBlockquoteCommand.cpp
    editing/CompositeEditCommand.cpp
    editing/CreateLinkCommand.cpp
    editing/DeleteFromTextNodeCommand.cpp
    editing/DeleteSelectionCommand.cpp
    editing/DictationAlternative.cpp
    editing/DictationCommand.cpp
    editing/EditCommand.cpp
    editing/EditingStyle.cpp
    editing/Editor.cpp
    editing/EditorCommand.cpp
    editing/FormatBlockCommand.cpp
    editing/FrameSelection.cpp
    editing/HTMLInterchange.cpp
    editing/IndentOutdentCommand.cpp
    editing/InsertIntoTextNodeCommand.cpp
    editing/InsertLineBreakCommand.cpp
    editing/InsertListCommand.cpp
    editing/InsertNodeBeforeCommand.cpp
    editing/InsertParagraphSeparatorCommand.cpp
    editing/InsertTextCommand.cpp
    editing/MarkupAccumulator.cpp
    editing/MergeIdenticalElementsCommand.cpp
    editing/ModifySelectionListLevel.cpp
    editing/MoveSelectionCommand.cpp
    editing/RemoveCSSPropertyCommand.cpp
    editing/RemoveFormatCommand.cpp
    editing/RemoveNodeCommand.cpp
    editing/RemoveNodePreservingChildrenCommand.cpp
    editing/RenderedPosition.cpp
    editing/ReplaceNodeWithSpanCommand.cpp
    editing/ReplaceRangeWithTextCommand.cpp
    editing/ReplaceSelectionCommand.cpp
    editing/SetNodeAttributeCommand.cpp
    editing/SetSelectionCommand.cpp
    editing/SimplifyMarkupCommand.cpp
    editing/SmartReplace.cpp
    editing/SpellChecker.cpp
    editing/SpellingCorrectionCommand.cpp
    editing/SplitElementCommand.cpp
    editing/SplitTextNodeCommand.cpp
    editing/SplitTextNodeContainingElementCommand.cpp
    editing/TextCheckingHelper.cpp
    editing/TextInsertionBaseCommand.cpp
    editing/TextIterator.cpp
    editing/TypingCommand.cpp
    editing/UnlinkCommand.cpp
    editing/VisiblePosition.cpp
    editing/VisibleSelection.cpp
    editing/VisibleUnits.cpp
    editing/WrapContentsInDummySpanCommand.cpp
    editing/htmlediting.cpp
    editing/markup.cpp

    fileapi/AsyncFileStream.cpp
    fileapi/Blob.cpp
    fileapi/BlobBuilder.cpp
    fileapi/BlobURL.cpp
    fileapi/File.cpp
    fileapi/FileException.cpp
    fileapi/FileList.cpp
    fileapi/FileReader.cpp
    fileapi/FileReaderLoader.cpp
    fileapi/FileReaderSync.cpp
    fileapi/ThreadableBlobRegistry.cpp

    history/BackForwardController.cpp
    history/CachedFrame.cpp
    history/CachedPage.cpp
    history/HistoryItem.cpp
    history/PageCache.cpp

    html/Autofill.cpp
    html/BaseButtonInputType.cpp
    html/BaseCheckableInputType.cpp
    html/BaseChooserOnlyDateAndTimeInputType.cpp
    html/BaseClickableWithKeyInputType.cpp
    html/BaseDateAndTimeInputType.cpp
    html/BaseTextInputType.cpp
    html/ButtonInputType.cpp
    html/CheckboxInputType.cpp
    html/ColorInputType.cpp
    html/DOMFormData.cpp
    html/DOMTokenList.cpp
    html/DOMURL.cpp
    html/DateInputType.cpp
    html/DateTimeInputType.cpp
    html/DateTimeLocalInputType.cpp
    html/EmailInputType.cpp
    html/FTPDirectoryDocument.cpp
    html/FileInputType.cpp
    html/FormAssociatedElement.cpp
    html/FormController.cpp
    html/FormDataList.cpp
    html/GenericCachedHTMLCollection.cpp
    html/HTMLAllCollection.cpp
    html/HTMLAnchorElement.cpp
    html/HTMLAppletElement.cpp
    html/HTMLAreaElement.cpp
    html/HTMLAttachmentElement.cpp
    html/HTMLAudioElement.cpp
    html/HTMLBRElement.cpp
    html/HTMLBaseElement.cpp
    html/HTMLBodyElement.cpp
    html/HTMLButtonElement.cpp
    html/HTMLCanvasElement.cpp
    html/HTMLCollection.cpp
    html/HTMLDListElement.cpp
    html/HTMLDataElement.cpp
    html/HTMLDataListElement.cpp
    html/HTMLDetailsElement.cpp
    html/HTMLDirectoryElement.cpp
    html/HTMLDivElement.cpp
    html/HTMLDocument.cpp
    html/HTMLElement.cpp
    html/HTMLEmbedElement.cpp
    html/HTMLFieldSetElement.cpp
    html/HTMLFontElement.cpp
    html/HTMLFormControlElement.cpp
    html/HTMLFormControlElementWithState.cpp
    html/HTMLFormControlsCollection.cpp
    html/HTMLFormElement.cpp
    html/HTMLFrameElement.cpp
    html/HTMLFrameElementBase.cpp
    html/HTMLFrameOwnerElement.cpp
    html/HTMLFrameSetElement.cpp
    html/HTMLHRElement.cpp
    html/HTMLHeadElement.cpp
    html/HTMLHeadingElement.cpp
    html/HTMLHtmlElement.cpp
    html/HTMLIFrameElement.cpp
    html/HTMLImageElement.cpp
    html/HTMLImageLoader.cpp
    html/HTMLInputElement.cpp
    html/HTMLKeygenElement.cpp
    html/HTMLLIElement.cpp
    html/HTMLLabelElement.cpp
    html/HTMLLegendElement.cpp
    html/HTMLLinkElement.cpp
    html/HTMLMapElement.cpp
    html/HTMLMarqueeElement.cpp
    html/HTMLMediaElement.cpp
    html/HTMLMenuElement.cpp
    html/HTMLMetaElement.cpp
    html/HTMLMeterElement.cpp
    html/HTMLModElement.cpp
    html/HTMLNameCollection.cpp
    html/HTMLOListElement.cpp
    html/HTMLObjectElement.cpp
    html/HTMLOptGroupElement.cpp
    html/HTMLOptionElement.cpp
    html/HTMLOptionsCollection.cpp
    html/HTMLOutputElement.cpp
    html/HTMLParagraphElement.cpp
    html/HTMLParamElement.cpp
    html/HTMLPictureElement.cpp
    html/HTMLPlugInElement.cpp
    html/HTMLPlugInImageElement.cpp
    html/HTMLPreElement.cpp
    html/HTMLProgressElement.cpp
    html/HTMLQuoteElement.cpp
    html/HTMLScriptElement.cpp
    html/HTMLSelectElement.cpp
    html/HTMLSlotElement.cpp
    html/HTMLSourceElement.cpp
    html/HTMLSpanElement.cpp
    html/HTMLStyleElement.cpp
    html/HTMLSummaryElement.cpp
    html/HTMLTableCaptionElement.cpp
    html/HTMLTableCellElement.cpp
    html/HTMLTableColElement.cpp
    html/HTMLTableElement.cpp
    html/HTMLTablePartElement.cpp
    html/HTMLTableRowElement.cpp
    html/HTMLTableRowsCollection.cpp
    html/HTMLTableSectionElement.cpp
    html/HTMLTemplateElement.cpp
    html/HTMLTextAreaElement.cpp
    html/HTMLTextFormControlElement.cpp
    html/HTMLTimeElement.cpp
    html/HTMLTitleElement.cpp
    html/HTMLTrackElement.cpp
    html/HTMLUListElement.cpp
    html/HTMLVideoElement.cpp
    html/HTMLWBRElement.cpp
    html/HiddenInputType.cpp
    html/ImageData.cpp
    html/ImageDocument.cpp
    html/ImageInputType.cpp
    html/InputType.cpp
    html/InputTypeNames.cpp
    html/LabelableElement.cpp
    html/LabelsNodeList.cpp
    html/LinkIconCollector.cpp
    html/LinkRelAttribute.cpp
    html/MediaController.cpp
    html/MediaDocument.cpp
    html/MediaElementSession.cpp
    html/MediaFragmentURIParser.cpp
    html/MonthInputType.cpp
    html/NumberInputType.cpp
    html/PasswordInputType.cpp
    html/PluginDocument.cpp
    html/PublicURLManager.cpp
    html/RadioInputType.cpp
    html/RadioNodeList.cpp
    html/RangeInputType.cpp
    html/ResetInputType.cpp
    html/RubyElement.cpp
    html/RubyTextElement.cpp
    html/SearchInputType.cpp
    html/StepRange.cpp
    html/SubmitInputType.cpp
    html/TelephoneInputType.cpp
    html/TextDocument.cpp
    html/TextFieldInputType.cpp
    html/TextInputType.cpp
    html/TimeInputType.cpp
    html/TimeRanges.cpp
    html/TypeAhead.cpp
    html/URLInputType.cpp
    html/URLSearchParams.cpp
    html/ValidationMessage.cpp
    html/WeekInputType.cpp

    html/canvas/CanvasGradient.cpp
    html/canvas/CanvasPath.cpp
    html/canvas/CanvasPattern.cpp
    html/canvas/CanvasProxy.cpp
    html/canvas/CanvasRenderingContext.cpp
    html/canvas/CanvasRenderingContext2D.cpp
    html/canvas/CanvasStyle.cpp
    html/canvas/DOMPath.cpp

    html/forms/FileIconLoader.cpp

    html/parser/CSSPreloadScanner.cpp
    html/parser/HTMLConstructionSite.cpp
    html/parser/HTMLDocumentParser.cpp
    html/parser/HTMLElementStack.cpp
    html/parser/HTMLEntityParser.cpp
    html/parser/HTMLEntitySearch.cpp
    html/parser/HTMLFormattingElementList.cpp
    html/parser/HTMLMetaCharsetParser.cpp
    html/parser/HTMLParserIdioms.cpp
    html/parser/HTMLParserOptions.cpp
    html/parser/HTMLParserScheduler.cpp
    html/parser/HTMLPreloadScanner.cpp
    html/parser/HTMLResourcePreloader.cpp
    html/parser/HTMLScriptRunner.cpp
    html/parser/HTMLSourceTracker.cpp
    html/parser/HTMLSrcsetParser.cpp
    html/parser/HTMLTokenizer.cpp
    html/parser/HTMLTreeBuilder.cpp
    html/parser/TextDocumentParser.cpp
    html/parser/XSSAuditor.cpp
    html/parser/XSSAuditorDelegate.cpp

    html/shadow/AutoFillButtonElement.cpp
    html/shadow/DetailsMarkerControl.cpp
    html/shadow/MediaControlElementTypes.cpp
    html/shadow/MediaControlElements.cpp
    html/shadow/MediaControls.cpp
    html/shadow/ProgressShadowElement.cpp
    html/shadow/SliderThumbElement.cpp
    html/shadow/SpinButtonElement.cpp
    html/shadow/TextControlInnerElements.cpp

    inspector/CommandLineAPIHost.cpp
    inspector/CommandLineAPIModule.cpp
    inspector/DOMEditor.cpp
    inspector/DOMPatchSupport.cpp
    inspector/InspectorApplicationCacheAgent.cpp
    inspector/InspectorCSSAgent.cpp
    inspector/InspectorClient.cpp
    inspector/InspectorController.cpp
    inspector/InspectorDOMAgent.cpp
    inspector/InspectorDOMDebuggerAgent.cpp
    inspector/InspectorDOMStorageAgent.cpp
    inspector/InspectorDatabaseAgent.cpp
    inspector/InspectorDatabaseResource.cpp
    inspector/InspectorFrontendClientLocal.cpp
    inspector/InspectorFrontendHost.cpp
    inspector/InspectorHistory.cpp
    inspector/InspectorIndexedDBAgent.cpp
    inspector/InspectorInstrumentation.cpp
    inspector/InspectorInstrumentationCookie.cpp
    inspector/InspectorLayerTreeAgent.cpp
    inspector/InspectorMemoryAgent.cpp
    inspector/InspectorNetworkAgent.cpp
    inspector/InspectorNodeFinder.cpp
    inspector/InspectorOverlay.cpp
    inspector/InspectorPageAgent.cpp
    inspector/InspectorStyleSheet.cpp
    inspector/InspectorTimelineAgent.cpp
    inspector/InspectorWorkerAgent.cpp
    inspector/InstrumentingAgents.cpp
    inspector/NetworkResourcesData.cpp
    inspector/PageConsoleAgent.cpp
    inspector/PageDebuggerAgent.cpp
    inspector/PageHeapAgent.cpp
    inspector/PageRuntimeAgent.cpp
    inspector/PageScriptDebugServer.cpp
    inspector/TimelineRecordFactory.cpp
    inspector/WebConsoleAgent.cpp
    inspector/WebDebuggerAgent.cpp
    inspector/WebHeapAgent.cpp
    inspector/WebInjectedScriptHost.cpp
    inspector/WebInjectedScriptManager.cpp
    inspector/WorkerConsoleAgent.cpp
    inspector/WorkerDebuggerAgent.cpp
    inspector/WorkerInspectorController.cpp
    inspector/WorkerRuntimeAgent.cpp
    inspector/WorkerScriptDebugServer.cpp

    loader/ContentFilter.cpp
    loader/CookieJar.cpp
    loader/CrossOriginAccessControl.cpp
    loader/CrossOriginPreflightChecker.cpp
    loader/CrossOriginPreflightResultCache.cpp
    loader/DocumentLoader.cpp
    loader/DocumentThreadableLoader.cpp
    loader/DocumentWriter.cpp
    loader/EmptyClients.cpp
    loader/FTPDirectoryParser.cpp
    loader/FormState.cpp
    loader/FormSubmission.cpp
    loader/FrameLoadRequest.cpp
    loader/FrameLoader.cpp
    loader/FrameLoaderStateMachine.cpp
    loader/HistoryController.cpp
    loader/ImageLoader.cpp
    loader/LinkHeader.cpp
    loader/LinkLoader.cpp
    loader/LinkPreloadResourceClients.cpp
    loader/LoadTiming.cpp
    loader/LoaderStrategy.cpp
    loader/MediaResourceLoader.cpp
    loader/MixedContentChecker.cpp
    loader/NavigationAction.cpp
    loader/NavigationScheduler.cpp
    loader/NetscapePlugInStreamLoader.cpp
    loader/PingLoader.cpp
    loader/PolicyCallback.cpp
    loader/PolicyChecker.cpp
    loader/ProgressTracker.cpp
    loader/ResourceLoadNotifier.cpp
    loader/ResourceLoadObserver.cpp
    loader/ResourceLoadStatistics.cpp
    loader/ResourceLoadStatisticsStore.cpp
    loader/ResourceLoader.cpp
    loader/ResourceTiming.cpp
    loader/ResourceTimingInformation.cpp
    loader/SinkDocument.cpp
    loader/SubframeLoader.cpp
    loader/SubresourceLoader.cpp
    loader/TextResourceDecoder.cpp
    loader/TextTrackLoader.cpp
    loader/ThreadableLoader.cpp
    loader/WorkerThreadableLoader.cpp

    loader/appcache/ApplicationCache.cpp
    loader/appcache/ApplicationCacheGroup.cpp
    loader/appcache/ApplicationCacheHost.cpp
    loader/appcache/ApplicationCacheResource.cpp
    loader/appcache/ApplicationCacheStorage.cpp
    loader/appcache/DOMApplicationCache.cpp
    loader/appcache/ManifestParser.cpp

    loader/archive/Archive.cpp
    loader/archive/ArchiveFactory.cpp
    loader/archive/ArchiveResource.cpp
    loader/archive/ArchiveResourceCollection.cpp

    loader/archive/mhtml/MHTMLArchive.cpp
    loader/archive/mhtml/MHTMLParser.cpp

    loader/cache/CachedCSSStyleSheet.cpp
    loader/cache/CachedFont.cpp
    loader/cache/CachedImage.cpp
    loader/cache/CachedRawResource.cpp
    loader/cache/CachedResource.cpp
    loader/cache/CachedResourceHandle.cpp
    loader/cache/CachedResourceLoader.cpp
    loader/cache/CachedResourceRequest.cpp
    loader/cache/CachedResourceRequestInitiators.cpp
    loader/cache/CachedSVGDocument.cpp
    loader/cache/CachedSVGDocumentReference.cpp
    loader/cache/CachedSVGFont.cpp
    loader/cache/CachedScript.cpp
    loader/cache/CachedXSLStyleSheet.cpp
    loader/cache/MemoryCache.cpp

    loader/icon/IconController.cpp
    loader/icon/IconDatabase.cpp
    loader/icon/IconDatabaseBase.cpp
    loader/icon/IconLoader.cpp
    loader/icon/IconRecord.cpp
    loader/icon/PageURLRecord.cpp

    mathml/MathMLAnnotationElement.cpp
    mathml/MathMLElement.cpp
    mathml/MathMLFractionElement.cpp
    mathml/MathMLMathElement.cpp
    mathml/MathMLMencloseElement.cpp
    mathml/MathMLOperatorDictionary.cpp
    mathml/MathMLOperatorElement.cpp
    mathml/MathMLPaddedElement.cpp
    mathml/MathMLPresentationElement.cpp
    mathml/MathMLRowElement.cpp
    mathml/MathMLScriptsElement.cpp
    mathml/MathMLSelectElement.cpp
    mathml/MathMLSpaceElement.cpp
    mathml/MathMLTokenElement.cpp
    mathml/MathMLUnderOverElement.cpp

    page/AutoscrollController.cpp
    page/BarProp.cpp
    page/Base64Utilities.cpp
    page/CaptionUserPreferences.cpp
    page/Chrome.cpp
    page/ContextMenuContext.cpp
    page/ContextMenuController.cpp
    page/Crypto.cpp
    page/DOMSelection.cpp
    page/DOMTimer.cpp
    page/DOMWindow.cpp
    page/DOMWindowExtension.cpp
    page/DOMWindowProperty.cpp
    page/DatabaseProvider.cpp
    page/DebugPageOverlays.cpp
    page/DeviceController.cpp
    page/DiagnosticLoggingKeys.cpp
    page/DragController.cpp
    page/EventHandler.cpp
    page/EventSource.cpp
    page/FocusController.cpp
    page/Frame.cpp
    page/FrameDestructionObserver.cpp
    page/FrameSnapshotting.cpp
    page/FrameTree.cpp
    page/FrameView.cpp
    page/History.cpp
    page/IntersectionObserver.cpp
    page/IntersectionObserverEntry.cpp
    page/Location.cpp
    page/MainFrame.cpp
    page/MemoryRelease.cpp
    page/MouseEventWithHitTestResults.cpp
    page/Navigator.cpp
    page/NavigatorBase.cpp
    page/OriginAccessEntry.cpp
    page/Page.cpp
    page/PageConfiguration.cpp
    page/PageConsoleClient.cpp
    page/PageGroup.cpp
    page/PageGroupLoadDeferrer.cpp
    page/PageOverlay.cpp
    page/PageOverlayController.cpp
    page/PageSerializer.cpp
    page/Performance.cpp
    page/PerformanceEntry.cpp
    page/PerformanceLogging.cpp
    page/PerformanceMonitor.cpp
    page/PerformanceNavigation.cpp
    page/PerformanceObserver.cpp
    page/PerformanceObserverEntryList.cpp
    page/PerformanceResourceTiming.cpp
    page/PerformanceTiming.cpp
    page/PerformanceUserTiming.cpp
    page/PointerLockController.cpp
    page/PrintContext.cpp
    page/ResourceUsageData.cpp
    page/ResourceUsageOverlay.cpp
    page/ResourceUsageThread.cpp
    page/RuntimeEnabledFeatures.cpp
    page/Screen.cpp
    page/SecurityOrigin.cpp
    page/SecurityOriginData.cpp
    page/SecurityPolicy.cpp
    page/Settings.cpp
    page/SocketProvider.cpp
    page/SpatialNavigation.cpp
    page/SuspendableTimer.cpp
    page/TextIndicator.cpp
    page/UserContentProvider.cpp
    page/UserContentController.cpp
    page/UserContentURLPattern.cpp
    page/VisitedLinkStore.cpp
    page/WheelEventDeltaFilter.cpp
    page/WheelEventTestTrigger.cpp
    page/WindowFeatures.cpp
    page/WindowFocusAllowedIndicator.cpp
    page/WorkerNavigator.cpp

    page/animation/AnimationBase.cpp
    page/animation/CSSAnimationController.cpp
    page/animation/CSSPropertyAnimation.cpp
    page/animation/CompositeAnimation.cpp
    page/animation/ImplicitAnimation.cpp
    page/animation/KeyframeAnimation.cpp

    page/csp/ContentSecurityPolicy.cpp
    page/csp/ContentSecurityPolicyDirective.cpp
    page/csp/ContentSecurityPolicyDirectiveList.cpp
    page/csp/ContentSecurityPolicyDirectiveNames.cpp
    page/csp/ContentSecurityPolicyMediaListDirective.cpp
    page/csp/ContentSecurityPolicyResponseHeaders.cpp
    page/csp/ContentSecurityPolicySource.cpp
    page/csp/ContentSecurityPolicySourceList.cpp
    page/csp/ContentSecurityPolicySourceListDirective.cpp

    page/scrolling/AxisScrollSnapOffsets.cpp
    page/scrolling/ScrollLatchingState.cpp
    page/scrolling/ScrollingConstraints.cpp
    page/scrolling/ScrollingCoordinator.cpp
    page/scrolling/ScrollingStateFixedNode.cpp
    page/scrolling/ScrollingStateFrameScrollingNode.cpp
    page/scrolling/ScrollingStateNode.cpp
    page/scrolling/ScrollingStateOverflowScrollingNode.cpp
    page/scrolling/ScrollingStateScrollingNode.cpp
    page/scrolling/ScrollingStateStickyNode.cpp
    page/scrolling/ScrollingStateTree.cpp
    page/scrolling/ScrollingThread.cpp
    page/scrolling/ScrollingTree.cpp
    page/scrolling/ScrollingTreeFrameScrollingNode.cpp
    page/scrolling/ScrollingTreeNode.cpp
    page/scrolling/ScrollingTreeOverflowScrollingNode.cpp
    page/scrolling/ScrollingTreeScrollingNode.cpp
    page/scrolling/ThreadedScrollingTree.cpp

    platform/CPUTime.cpp
    platform/CalculationValue.cpp
    platform/Clock.cpp
    platform/ClockGeneric.cpp
    platform/ContentType.cpp
    platform/ContextMenu.cpp
    platform/ContextMenuItem.cpp
    platform/Cursor.cpp
    platform/DateComponents.cpp
    platform/Decimal.cpp
    platform/DisplaySleepDisabler.cpp
    platform/DragData.cpp
    platform/DragImage.cpp
    platform/EventTrackingRegions.cpp
    platform/FileChooser.cpp
    platform/FileHandle.cpp
    platform/FileStream.cpp
    platform/FileSystem.cpp
    platform/GenericTaskQueue.cpp
    platform/Language.cpp
    platform/Length.cpp
    platform/LengthBox.cpp
    platform/LengthPoint.cpp
    platform/LengthSize.cpp
    platform/LinkHash.cpp
    platform/Logging.cpp
    platform/MIMETypeRegistry.cpp
    platform/MainThreadSharedTimer.cpp
    platform/MemoryPressureHandler.cpp
    platform/NotImplemented.cpp
    platform/PasteboardWriterData.cpp
    platform/PlatformSpeechSynthesisUtterance.cpp
    platform/PlatformSpeechSynthesisVoice.cpp
    platform/PlatformSpeechSynthesizer.cpp
    platform/PlatformStrategies.cpp
    platform/RemoteCommandListener.cpp
    platform/SchemeRegistry.cpp
    platform/ScrollAnimator.cpp
    platform/ScrollView.cpp
    platform/ScrollableArea.cpp
    platform/Scrollbar.cpp
    platform/ScrollbarTheme.cpp
    platform/ScrollbarThemeComposite.cpp
    platform/SharedBuffer.cpp
    platform/SharedBufferChunkReader.cpp
    platform/StaticPasteboard.cpp
    platform/SystemSleepListener.cpp
    platform/Theme.cpp
    platform/ThreadGlobalData.cpp
    platform/ThreadTimers.cpp
    platform/Timer.cpp
    platform/URL.cpp
    platform/URLParser.cpp
    platform/UUID.cpp
    platform/UserActivity.cpp
    platform/WebCoreCrossThreadCopier.cpp
    platform/WebGLStateTracker.cpp
    platform/Widget.cpp

    platform/animation/Animation.cpp
    platform/animation/AnimationList.cpp
    platform/animation/TimingFunction.cpp

    platform/audio/AudioBus.cpp
    platform/audio/AudioChannel.cpp
    platform/audio/AudioDSPKernelProcessor.cpp
    platform/audio/AudioFIFO.cpp
    platform/audio/AudioHardwareListener.cpp
    platform/audio/AudioPullFIFO.cpp
    platform/audio/AudioResampler.cpp
    platform/audio/AudioResamplerKernel.cpp
    platform/audio/AudioUtilities.cpp
    platform/audio/Biquad.cpp
    platform/audio/Cone.cpp
    platform/audio/DirectConvolver.cpp
    platform/audio/Distance.cpp
    platform/audio/DownSampler.cpp
    platform/audio/DynamicsCompressor.cpp
    platform/audio/DynamicsCompressorKernel.cpp
    platform/audio/EqualPowerPanner.cpp
    platform/audio/FFTConvolver.cpp
    platform/audio/FFTFrame.cpp
    platform/audio/HRTFDatabase.cpp
    platform/audio/HRTFDatabaseLoader.cpp
    platform/audio/HRTFElevation.cpp
    platform/audio/HRTFKernel.cpp
    platform/audio/HRTFPanner.cpp
    platform/audio/MultiChannelResampler.cpp
    platform/audio/Panner.cpp
    platform/audio/PlatformMediaSession.cpp
    platform/audio/PlatformMediaSessionManager.cpp
    platform/audio/Reverb.cpp
    platform/audio/ReverbAccumulationBuffer.cpp
    platform/audio/ReverbConvolver.cpp
    platform/audio/ReverbConvolverStage.cpp
    platform/audio/ReverbInputBuffer.cpp
    platform/audio/SincResampler.cpp
    platform/audio/UpSampler.cpp
    platform/audio/VectorMath.cpp
    platform/audio/ZeroPole.cpp

    platform/graphics/BitmapImage.cpp
    platform/graphics/Color.cpp
    platform/graphics/ComplexTextController.cpp
    platform/graphics/CrossfadeGeneratedImage.cpp
    platform/graphics/DisplayRefreshMonitorClient.cpp
    platform/graphics/ExtendedColor.cpp
    platform/graphics/FloatPoint.cpp
    platform/graphics/FloatPoint3D.cpp
    platform/graphics/FloatPolygon.cpp
    platform/graphics/FloatQuad.cpp
    platform/graphics/FloatRect.cpp
    platform/graphics/FloatRoundedRect.cpp
    platform/graphics/FloatSize.cpp
    platform/graphics/Font.cpp
    platform/graphics/FontCache.cpp
    platform/graphics/FontCascade.cpp
    platform/graphics/FontCascadeFonts.cpp
    platform/graphics/FontDescription.cpp
    platform/graphics/FontTaggedSettings.cpp
    platform/graphics/FontGenericFamilies.cpp
    platform/graphics/FontPlatformData.cpp
    platform/graphics/FontRanges.cpp
    platform/graphics/GeneratedImage.cpp
    platform/graphics/GeometryUtilities.cpp
    platform/graphics/Gradient.cpp
    platform/graphics/GradientImage.cpp
    platform/graphics/GraphicsContext.cpp
    platform/graphics/GraphicsLayer.cpp
    platform/graphics/GraphicsLayerTransform.cpp
    platform/graphics/GraphicsLayerUpdater.cpp
    platform/graphics/GraphicsTypes.cpp
    platform/graphics/ISOVTTCue.cpp
    platform/graphics/Image.cpp
    platform/graphics/ImageBuffer.cpp
    platform/graphics/ImageFrame.cpp
    platform/graphics/ImageFrameCache.cpp
    platform/graphics/ImageOrientation.cpp
    platform/graphics/ImageSource.cpp
    platform/graphics/IntPoint.cpp
    platform/graphics/IntRect.cpp
    platform/graphics/IntSize.cpp
    platform/graphics/LayoutPoint.cpp
    platform/graphics/LayoutRect.cpp
    platform/graphics/LayoutSize.cpp
    platform/graphics/MediaPlayer.cpp
    platform/graphics/NamedImageGeneratedImage.cpp
    platform/graphics/Path.cpp
    platform/graphics/PathTraversalState.cpp
    platform/graphics/PathUtilities.cpp
    platform/graphics/Pattern.cpp
    platform/graphics/PlatformTimeRanges.cpp
    platform/graphics/Region.cpp
    platform/graphics/RoundedRect.cpp
    platform/graphics/ShadowBlur.cpp
    platform/graphics/StringTruncator.cpp
    platform/graphics/SurrogatePairAwareTextIterator.cpp
    platform/graphics/TextRun.cpp
    platform/graphics/WOFFFileFormat.cpp
    platform/graphics/WidthIterator.cpp

    platform/graphics/cpu/arm/filters/FELightingNEON.cpp

    platform/graphics/displaylists/DisplayList.cpp
    platform/graphics/displaylists/DisplayListItems.cpp
    platform/graphics/displaylists/DisplayListRecorder.cpp
    platform/graphics/displaylists/DisplayListReplayer.cpp

    platform/graphics/filters/DistantLightSource.cpp
    platform/graphics/filters/FEBlend.cpp
    platform/graphics/filters/FEColorMatrix.cpp
    platform/graphics/filters/FEComponentTransfer.cpp
    platform/graphics/filters/FEComposite.cpp
    platform/graphics/filters/FEConvolveMatrix.cpp
    platform/graphics/filters/FEDiffuseLighting.cpp
    platform/graphics/filters/FEDisplacementMap.cpp
    platform/graphics/filters/FEDropShadow.cpp
    platform/graphics/filters/FEFlood.cpp
    platform/graphics/filters/FEGaussianBlur.cpp
    platform/graphics/filters/FELighting.cpp
    platform/graphics/filters/FEMerge.cpp
    platform/graphics/filters/FEMorphology.cpp
    platform/graphics/filters/FEOffset.cpp
    platform/graphics/filters/FESpecularLighting.cpp
    platform/graphics/filters/FETile.cpp
    platform/graphics/filters/FETurbulence.cpp
    platform/graphics/filters/FilterEffect.cpp
    platform/graphics/filters/FilterOperation.cpp
    platform/graphics/filters/FilterOperations.cpp
    platform/graphics/filters/PointLightSource.cpp
    platform/graphics/filters/SourceAlpha.cpp
    platform/graphics/filters/SourceGraphic.cpp
    platform/graphics/filters/SpotLightSource.cpp

    platform/graphics/opentype/OpenTypeMathData.cpp

    platform/graphics/transforms/AffineTransform.cpp
    platform/graphics/transforms/Matrix3DTransformOperation.cpp
    platform/graphics/transforms/MatrixTransformOperation.cpp
    platform/graphics/transforms/PerspectiveTransformOperation.cpp
    platform/graphics/transforms/RotateTransformOperation.cpp
    platform/graphics/transforms/ScaleTransformOperation.cpp
    platform/graphics/transforms/SkewTransformOperation.cpp
    platform/graphics/transforms/TransformOperation.cpp
    platform/graphics/transforms/TransformOperations.cpp
    platform/graphics/transforms/TransformState.cpp
    platform/graphics/transforms/TransformationMatrix.cpp
    platform/graphics/transforms/TranslateTransformOperation.cpp

    platform/mediastream/MediaConstraints.cpp
    platform/mediastream/MediaEndpointConfiguration.cpp
    platform/mediastream/MediaStreamPrivate.cpp
    platform/mediastream/MediaStreamTrackPrivate.cpp
    platform/mediastream/RealtimeMediaSourceSupportedConstraints.cpp
    platform/mediastream/RTCIceCandidateDescriptor.cpp
    platform/mediastream/RTCSessionDescriptionDescriptor.cpp
    platform/mediastream/RealtimeMediaSource.cpp
    platform/mediastream/RealtimeMediaSourceCenter.cpp
    platform/mediastream/RealtimeMediaSourceSettings.cpp
    platform/mediastream/SDPProcessorScriptResource.cpp

    platform/mock/DeviceOrientationClientMock.cpp
    platform/mock/GeolocationClientMock.cpp
    platform/mock/MockMediaEndpoint.cpp
    platform/mock/MockRealtimeAudioSource.cpp
    platform/mock/MockRealtimeMediaSource.cpp
    platform/mock/MockRealtimeMediaSourceCenter.cpp
    platform/mock/MockRealtimeVideoSource.cpp
    platform/mock/RTCDataChannelHandlerMock.cpp
    platform/mock/RTCNotifiersMock.cpp
    platform/mock/ScrollAnimatorMock.cpp
    platform/mock/ScrollbarThemeMock.cpp

    platform/network/AuthenticationChallengeBase.cpp
    platform/network/BlobData.cpp
    platform/network/BlobDataFileReference.cpp
    platform/network/BlobRegistry.cpp
    platform/network/BlobRegistryImpl.cpp
    platform/network/BlobResourceHandle.cpp
    platform/network/CacheValidation.cpp
    platform/network/CredentialBase.cpp
    platform/network/CredentialStorage.cpp
    platform/network/DNSResolveQueue.cpp
    platform/network/DataURLDecoder.cpp
    platform/network/FormData.cpp
    platform/network/FormDataBuilder.cpp
    platform/network/HTTPHeaderMap.cpp
    platform/network/HTTPHeaderValues.cpp
    platform/network/HTTPParsers.cpp
    platform/network/MIMEHeader.cpp
    platform/network/NetworkStateNotifier.cpp
    platform/network/NetworkStorageSession.cpp
    platform/network/ParsedContentRange.cpp
    platform/network/ParsedContentType.cpp
    platform/network/ProtectionSpaceBase.cpp
    platform/network/ProxyServer.cpp
    platform/network/ResourceErrorBase.cpp
    platform/network/ResourceHandle.cpp
    platform/network/ResourceHandleClient.cpp
    platform/network/ResourceRequestBase.cpp
    platform/network/ResourceResponseBase.cpp
    platform/network/SocketStreamHandle.cpp
    platform/network/SynchronousLoaderClient.cpp

    platform/sql/SQLiteAuthorizer.cpp
    platform/sql/SQLiteDatabase.cpp
    platform/sql/SQLiteDatabaseTracker.cpp
    platform/sql/SQLiteFileSystem.cpp
    platform/sql/SQLiteStatement.cpp
    platform/sql/SQLiteTransaction.cpp

    platform/text/BidiContext.cpp
    platform/text/DateTimeFormat.cpp
    platform/text/LineEnding.cpp
    platform/text/LocaleToScriptMappingDefault.cpp
    platform/text/PlatformLocale.cpp
    platform/text/QuotedPrintable.cpp
    platform/text/SegmentedString.cpp
    platform/text/TextBoundaries.cpp
    platform/text/TextCodec.cpp
    platform/text/TextCodecICU.cpp
    platform/text/TextCodecLatin1.cpp
    platform/text/TextCodecReplacement.cpp
    platform/text/TextCodecUTF16.cpp
    platform/text/TextCodecUTF8.cpp
    platform/text/TextCodecUserDefined.cpp
    platform/text/TextEncoding.cpp
    platform/text/TextEncodingDetectorICU.cpp
    platform/text/TextEncodingRegistry.cpp
    platform/text/TextStream.cpp

    plugins/DOMMimeType.cpp
    plugins/DOMMimeTypeArray.cpp
    plugins/DOMPlugin.cpp
    plugins/DOMPluginArray.cpp
    plugins/PluginData.cpp
    plugins/PluginInfoProvider.cpp

    rendering/AutoTableLayout.cpp
    rendering/BidiRun.cpp
    rendering/BorderEdge.cpp
    rendering/BreakLines.cpp
    rendering/ClipRect.cpp
    rendering/CounterNode.cpp
    rendering/EllipsisBox.cpp
    rendering/FilterEffectRenderer.cpp
    rendering/FixedTableLayout.cpp
    rendering/FloatingObjects.cpp
    rendering/FlowThreadController.cpp
    rendering/Grid.cpp
    rendering/GridTrackSizingAlgorithm.cpp
    rendering/HitTestLocation.cpp
    rendering/HitTestResult.cpp
    rendering/HitTestingTransformState.cpp
    rendering/ImageQualityController.cpp
    rendering/InlineBox.cpp
    rendering/InlineElementBox.cpp
    rendering/InlineFlowBox.cpp
    rendering/InlineIterator.cpp
    rendering/InlineTextBox.cpp
    rendering/LayoutRepainter.cpp
    rendering/LayoutState.cpp
    rendering/OrderIterator.cpp
    rendering/PointerEventsHitRules.cpp
    rendering/RenderAttachment.cpp
    rendering/RenderBlock.cpp
    rendering/RenderBlockFlow.cpp
    rendering/RenderBlockLineLayout.cpp
    rendering/RenderBox.cpp
    rendering/RenderBoxModelObject.cpp
    rendering/RenderButton.cpp
    rendering/RenderCombineText.cpp
    rendering/RenderCounter.cpp
    rendering/RenderDeprecatedFlexibleBox.cpp
    rendering/RenderDetailsMarker.cpp
    rendering/RenderElement.cpp
    rendering/RenderEmbeddedObject.cpp
    rendering/RenderFieldset.cpp
    rendering/RenderFileUploadControl.cpp
    rendering/RenderFlexibleBox.cpp
    rendering/RenderFlowThread.cpp
    rendering/RenderFrame.cpp
    rendering/RenderFrameBase.cpp
    rendering/RenderFrameSet.cpp
    rendering/RenderFullScreen.cpp
    rendering/RenderGeometryMap.cpp
    rendering/RenderGrid.cpp
    rendering/RenderHTMLCanvas.cpp
    rendering/RenderIFrame.cpp
    rendering/RenderImage.cpp
    rendering/RenderImageResource.cpp
    rendering/RenderImageResourceStyleImage.cpp
    rendering/RenderInline.cpp
    rendering/RenderLayer.cpp
    rendering/RenderLayerBacking.cpp
    rendering/RenderLayerCompositor.cpp
    rendering/RenderLayerFilterInfo.cpp
    rendering/RenderLayerModelObject.cpp
    rendering/RenderLineBoxList.cpp
    rendering/RenderLineBreak.cpp
    rendering/RenderListBox.cpp
    rendering/RenderListItem.cpp
    rendering/RenderListMarker.cpp
    rendering/RenderMarquee.cpp
    rendering/RenderMedia.cpp
    rendering/RenderMediaControlElements.cpp
    rendering/RenderMediaControls.cpp
    rendering/RenderMenuList.cpp
    rendering/RenderMeter.cpp
    rendering/RenderMultiColumnFlowThread.cpp
    rendering/RenderMultiColumnSet.cpp
    rendering/RenderMultiColumnSpannerPlaceholder.cpp
    rendering/RenderNamedFlowFragment.cpp
    rendering/RenderNamedFlowThread.cpp
    rendering/RenderObject.cpp
    rendering/RenderProgress.cpp
    rendering/RenderQuote.cpp
    rendering/RenderRegion.cpp
    rendering/RenderRegionSet.cpp
    rendering/RenderReplaced.cpp
    rendering/RenderReplica.cpp
    rendering/RenderRuby.cpp
    rendering/RenderRubyBase.cpp
    rendering/RenderRubyRun.cpp
    rendering/RenderRubyText.cpp
    rendering/RenderScrollbar.cpp
    rendering/RenderScrollbarPart.cpp
    rendering/RenderScrollbarTheme.cpp
    rendering/RenderSearchField.cpp
    rendering/RenderSelectionInfo.cpp
    rendering/RenderSlider.cpp
    rendering/RenderSnapshottedPlugIn.cpp
    rendering/RenderTable.cpp
    rendering/RenderTableCaption.cpp
    rendering/RenderTableCell.cpp
    rendering/RenderTableCol.cpp
    rendering/RenderTableRow.cpp
    rendering/RenderTableSection.cpp
    rendering/RenderText.cpp
    rendering/RenderTextControl.cpp
    rendering/RenderTextControlMultiLine.cpp
    rendering/RenderTextControlSingleLine.cpp
    rendering/RenderTextFragment.cpp
    rendering/RenderTextLineBoxes.cpp
    rendering/RenderTheme.cpp
    rendering/RenderTreeAsText.cpp
    rendering/RenderVTTCue.cpp
    rendering/RenderVideo.cpp
    rendering/RenderView.cpp
    rendering/RenderWidget.cpp
    rendering/RootInlineBox.cpp
    rendering/ScrollAlignment.cpp
    rendering/SelectionSubtreeRoot.cpp
    rendering/SimpleLineLayout.cpp
    rendering/SimpleLineLayoutCoverage.cpp
    rendering/SimpleLineLayoutFlowContents.cpp
    rendering/SimpleLineLayoutFunctions.cpp
    rendering/SimpleLineLayoutPagination.cpp
    rendering/SimpleLineLayoutResolver.cpp
    rendering/SimpleLineLayoutTextFragmentIterator.cpp
    rendering/TextDecorationPainter.cpp
    rendering/TextPaintStyle.cpp
    rendering/TextPainter.cpp

    rendering/line/LineBreaker.cpp
    rendering/line/LineInfo.cpp
    rendering/line/LineWidth.cpp
    rendering/line/TrailingObjects.cpp

    rendering/mathml/MathMLStyle.cpp
    rendering/mathml/MathOperator.cpp
    rendering/mathml/RenderMathMLBlock.cpp
    rendering/mathml/RenderMathMLFenced.cpp
    rendering/mathml/RenderMathMLFencedOperator.cpp
    rendering/mathml/RenderMathMLFraction.cpp
    rendering/mathml/RenderMathMLMath.cpp
    rendering/mathml/RenderMathMLMenclose.cpp
    rendering/mathml/RenderMathMLOperator.cpp
    rendering/mathml/RenderMathMLPadded.cpp
    rendering/mathml/RenderMathMLRoot.cpp
    rendering/mathml/RenderMathMLRow.cpp
    rendering/mathml/RenderMathMLScripts.cpp
    rendering/mathml/RenderMathMLSpace.cpp
    rendering/mathml/RenderMathMLToken.cpp
    rendering/mathml/RenderMathMLUnderOver.cpp

    rendering/shapes/BoxShape.cpp
    rendering/shapes/PolygonShape.cpp
    rendering/shapes/RasterShape.cpp
    rendering/shapes/RectangleShape.cpp
    rendering/shapes/Shape.cpp
    rendering/shapes/ShapeOutsideInfo.cpp

    rendering/style/BasicShapes.cpp
    rendering/style/ContentData.cpp
    rendering/style/CounterDirectives.cpp
    rendering/style/FillLayer.cpp
    rendering/style/GridPositionsResolver.cpp
    rendering/style/KeyframeList.cpp
    rendering/style/NinePieceImage.cpp
    rendering/style/QuotesData.cpp
    rendering/style/RenderStyle.cpp
    rendering/style/RenderStyleConstants.cpp
    rendering/style/SVGRenderStyle.cpp
    rendering/style/SVGRenderStyleDefs.cpp
    rendering/style/ShadowData.cpp
    rendering/style/ShapeValue.cpp
    rendering/style/StyleBackgroundData.cpp
    rendering/style/StyleBoxData.cpp
    rendering/style/StyleCachedImage.cpp
    rendering/style/StyleDeprecatedFlexibleBoxData.cpp
    rendering/style/StyleFilterData.cpp
    rendering/style/StyleFlexibleBoxData.cpp
    rendering/style/StyleGeneratedImage.cpp
    rendering/style/StyleGridData.cpp
    rendering/style/StyleGridItemData.cpp
    rendering/style/StyleInheritedData.cpp
    rendering/style/StyleMarqueeData.cpp
    rendering/style/StyleMultiColData.cpp
    rendering/style/StyleRareInheritedData.cpp
    rendering/style/StyleRareNonInheritedData.cpp
    rendering/style/StyleScrollSnapPoints.cpp
    rendering/style/StyleSurroundData.cpp
    rendering/style/StyleTransformData.cpp
    rendering/style/StyleVisualData.cpp
    rendering/style/WillChangeData.cpp

    rendering/svg/RenderSVGBlock.cpp
    rendering/svg/RenderSVGContainer.cpp
    rendering/svg/RenderSVGEllipse.cpp
    rendering/svg/RenderSVGForeignObject.cpp
    rendering/svg/RenderSVGGradientStop.cpp
    rendering/svg/RenderSVGHiddenContainer.cpp
    rendering/svg/RenderSVGImage.cpp
    rendering/svg/RenderSVGInline.cpp
    rendering/svg/RenderSVGInlineText.cpp
    rendering/svg/RenderSVGModelObject.cpp
    rendering/svg/RenderSVGPath.cpp
    rendering/svg/RenderSVGRect.cpp
    rendering/svg/RenderSVGResource.cpp
    rendering/svg/RenderSVGResourceClipper.cpp
    rendering/svg/RenderSVGResourceContainer.cpp
    rendering/svg/RenderSVGResourceFilter.cpp
    rendering/svg/RenderSVGResourceFilterPrimitive.cpp
    rendering/svg/RenderSVGResourceGradient.cpp
    rendering/svg/RenderSVGResourceLinearGradient.cpp
    rendering/svg/RenderSVGResourceMarker.cpp
    rendering/svg/RenderSVGResourceMasker.cpp
    rendering/svg/RenderSVGResourcePattern.cpp
    rendering/svg/RenderSVGResourceRadialGradient.cpp
    rendering/svg/RenderSVGResourceSolidColor.cpp
    rendering/svg/RenderSVGRoot.cpp
    rendering/svg/RenderSVGShape.cpp
    rendering/svg/RenderSVGText.cpp
    rendering/svg/RenderSVGTextPath.cpp
    rendering/svg/RenderSVGTransformableContainer.cpp
    rendering/svg/RenderSVGViewportContainer.cpp
    rendering/svg/SVGInlineFlowBox.cpp
    rendering/svg/SVGInlineTextBox.cpp
    rendering/svg/SVGPathData.cpp
    rendering/svg/SVGRenderSupport.cpp
    rendering/svg/SVGRenderTreeAsText.cpp
    rendering/svg/SVGRenderingContext.cpp
    rendering/svg/SVGResources.cpp
    rendering/svg/SVGResourcesCache.cpp
    rendering/svg/SVGResourcesCycleSolver.cpp
    rendering/svg/SVGRootInlineBox.cpp
    rendering/svg/SVGTextChunk.cpp
    rendering/svg/SVGTextChunkBuilder.cpp
    rendering/svg/SVGTextLayoutAttributes.cpp
    rendering/svg/SVGTextLayoutAttributesBuilder.cpp
    rendering/svg/SVGTextLayoutEngine.cpp
    rendering/svg/SVGTextLayoutEngineBaseline.cpp
    rendering/svg/SVGTextLayoutEngineSpacing.cpp
    rendering/svg/SVGTextMetrics.cpp
    rendering/svg/SVGTextMetricsBuilder.cpp
    rendering/svg/SVGTextQuery.cpp

    replay/UserInputBridge.cpp

    storage/Storage.cpp
    storage/StorageEvent.cpp
    storage/StorageEventDispatcher.cpp
    storage/StorageMap.cpp
    storage/StorageNamespaceProvider.cpp

    style/AttributeChangeInvalidation.cpp
    style/ClassChangeInvalidation.cpp
    style/IdChangeInvalidation.cpp
    style/InlineTextBoxStyle.cpp
    style/RenderTreePosition.cpp
    style/RenderTreeUpdater.cpp
    style/StyleChange.cpp
    style/StyleFontSizeFunctions.cpp
    style/StylePendingResources.cpp
    style/StyleRelations.cpp
    style/StyleResolveForDocument.cpp
    style/StyleScope.cpp
    style/StyleSharingResolver.cpp
    style/StyleTreeResolver.cpp
    style/StyleUpdate.cpp

    svg/SVGAElement.cpp
    svg/SVGAltGlyphDefElement.cpp
    svg/SVGAltGlyphElement.cpp
    svg/SVGAltGlyphItemElement.cpp
    svg/SVGAngleValue.cpp
    svg/SVGAnimateColorElement.cpp
    svg/SVGAnimateElement.cpp
    svg/SVGAnimateElementBase.cpp
    svg/SVGAnimateMotionElement.cpp
    svg/SVGAnimateTransformElement.cpp
    svg/SVGAnimatedAngle.cpp
    svg/SVGAnimatedBoolean.cpp
    svg/SVGAnimatedColor.cpp
    svg/SVGAnimatedEnumeration.cpp
    svg/SVGAnimatedInteger.cpp
    svg/SVGAnimatedIntegerOptionalInteger.cpp
    svg/SVGAnimatedLength.cpp
    svg/SVGAnimatedLengthList.cpp
    svg/SVGAnimatedNumber.cpp
    svg/SVGAnimatedNumberList.cpp
    svg/SVGAnimatedNumberOptionalNumber.cpp
    svg/SVGAnimatedPath.cpp
    svg/SVGAnimatedPointList.cpp
    svg/SVGAnimatedPreserveAspectRatio.cpp
    svg/SVGAnimatedRect.cpp
    svg/SVGAnimatedString.cpp
    svg/SVGAnimatedTransformList.cpp
    svg/SVGAnimatedType.cpp
    svg/SVGAnimatedTypeAnimator.cpp
    svg/SVGAnimationElement.cpp
    svg/SVGCircleElement.cpp
    svg/SVGClipPathElement.cpp
    svg/SVGComponentTransferFunctionElement.cpp
    svg/SVGCursorElement.cpp
    svg/SVGDefsElement.cpp
    svg/SVGDescElement.cpp
    svg/SVGDocument.cpp
    svg/SVGDocumentExtensions.cpp
    svg/SVGElement.cpp
    svg/SVGEllipseElement.cpp
    svg/SVGException.cpp
    svg/SVGExternalResourcesRequired.cpp
    svg/SVGFEBlendElement.cpp
    svg/SVGFEColorMatrixElement.cpp
    svg/SVGFEComponentTransferElement.cpp
    svg/SVGFECompositeElement.cpp
    svg/SVGFEConvolveMatrixElement.cpp
    svg/SVGFEDiffuseLightingElement.cpp
    svg/SVGFEDisplacementMapElement.cpp
    svg/SVGFEDistantLightElement.cpp
    svg/SVGFEDropShadowElement.cpp
    svg/SVGFEFloodElement.cpp
    svg/SVGFEFuncAElement.cpp
    svg/SVGFEFuncBElement.cpp
    svg/SVGFEFuncGElement.cpp
    svg/SVGFEFuncRElement.cpp
    svg/SVGFEGaussianBlurElement.cpp
    svg/SVGFEImageElement.cpp
    svg/SVGFELightElement.cpp
    svg/SVGFEMergeElement.cpp
    svg/SVGFEMergeNodeElement.cpp
    svg/SVGFEMorphologyElement.cpp
    svg/SVGFEOffsetElement.cpp
    svg/SVGFEPointLightElement.cpp
    svg/SVGFESpecularLightingElement.cpp
    svg/SVGFESpotLightElement.cpp
    svg/SVGFETileElement.cpp
    svg/SVGFETurbulenceElement.cpp
    svg/SVGFilterElement.cpp
    svg/SVGFilterPrimitiveStandardAttributes.cpp
    svg/SVGFitToViewBox.cpp
    svg/SVGFontElement.cpp
    svg/SVGFontFaceElement.cpp
    svg/SVGFontFaceFormatElement.cpp
    svg/SVGFontFaceNameElement.cpp
    svg/SVGFontFaceSrcElement.cpp
    svg/SVGFontFaceUriElement.cpp
    svg/SVGForeignObjectElement.cpp
    svg/SVGGElement.cpp
    svg/SVGGlyphElement.cpp
    svg/SVGGlyphRefElement.cpp
    svg/SVGGradientElement.cpp
    svg/SVGGraphicsElement.cpp
    svg/SVGHKernElement.cpp
    svg/SVGImageElement.cpp
    svg/SVGImageLoader.cpp
    svg/SVGLangSpace.cpp
    svg/SVGLengthContext.cpp
    svg/SVGLengthListValues.cpp
    svg/SVGLengthValue.cpp
    svg/SVGLineElement.cpp
    svg/SVGLinearGradientElement.cpp
    svg/SVGLocatable.cpp
    svg/SVGMPathElement.cpp
    svg/SVGMarkerElement.cpp
    svg/SVGMaskElement.cpp
    svg/SVGMetadataElement.cpp
    svg/SVGMissingGlyphElement.cpp
    svg/SVGNumberListValues.cpp
    svg/SVGParserUtilities.cpp
    svg/SVGPathBlender.cpp
    svg/SVGPathBuilder.cpp
    svg/SVGPathByteStreamBuilder.cpp
    svg/SVGPathByteStreamSource.cpp
    svg/SVGPathElement.cpp
    svg/SVGPathParser.cpp
    svg/SVGPathSegList.cpp
    svg/SVGPathSegListBuilder.cpp
    svg/SVGPathSegListSource.cpp
    svg/SVGPathSegListValues.cpp
    svg/SVGPathStringBuilder.cpp
    svg/SVGPathStringSource.cpp
    svg/SVGPathTraversalStateBuilder.cpp
    svg/SVGPathUtilities.cpp
    svg/SVGPatternElement.cpp
    svg/SVGPointListValues.cpp
    svg/SVGPolyElement.cpp
    svg/SVGPolygonElement.cpp
    svg/SVGPolylineElement.cpp
    svg/SVGPreserveAspectRatioValue.cpp
    svg/SVGRadialGradientElement.cpp
    svg/SVGRectElement.cpp
    svg/SVGSVGElement.cpp
    svg/SVGScriptElement.cpp
    svg/SVGSetElement.cpp
    svg/SVGStopElement.cpp
    svg/SVGStringListValues.cpp
    svg/SVGStyleElement.cpp
    svg/SVGSwitchElement.cpp
    svg/SVGSymbolElement.cpp
    svg/SVGTRefElement.cpp
    svg/SVGTSpanElement.cpp
    svg/SVGTests.cpp
    svg/SVGTextContentElement.cpp
    svg/SVGTextElement.cpp
    svg/SVGTextPathElement.cpp
    svg/SVGTextPositioningElement.cpp
    svg/SVGTitleElement.cpp
    svg/SVGToOTFFontConversion.cpp
    svg/SVGTransform.cpp
    svg/SVGTransformDistance.cpp
    svg/SVGTransformListValues.cpp
    svg/SVGTransformValue.cpp
    svg/SVGTransformable.cpp
    svg/SVGURIReference.cpp
    svg/SVGUseElement.cpp
    svg/SVGVKernElement.cpp
    svg/SVGViewElement.cpp
    svg/SVGViewSpec.cpp
    svg/SVGZoomAndPan.cpp
    svg/SVGZoomEvent.cpp

    svg/animation/SMILTime.cpp
    svg/animation/SMILTimeContainer.cpp
    svg/animation/SVGSMILElement.cpp

    svg/graphics/SVGImage.cpp
    svg/graphics/SVGImageCache.cpp
    svg/graphics/SVGImageForContainer.cpp

    svg/graphics/filters/SVGFEImage.cpp
    svg/graphics/filters/SVGFilter.cpp
    svg/graphics/filters/SVGFilterBuilder.cpp

    svg/properties/SVGAnimatedProperty.cpp
    svg/properties/SVGAttributeToPropertyMap.cpp
    svg/properties/SVGPropertyInfo.cpp

    workers/AbstractWorker.cpp
    workers/DedicatedWorkerGlobalScope.cpp
    workers/DedicatedWorkerThread.cpp
    workers/Worker.cpp
    workers/WorkerConsoleClient.cpp
    workers/WorkerEventQueue.cpp
    workers/WorkerGlobalScope.cpp
    workers/WorkerInspectorProxy.cpp
    workers/WorkerLocation.cpp
    workers/WorkerMessagingProxy.cpp
    workers/WorkerRunLoop.cpp
    workers/WorkerScriptLoader.cpp
    workers/WorkerThread.cpp

    xml/DOMParser.cpp
    xml/NativeXPathNSResolver.cpp
    xml/XMLErrors.cpp
    xml/XMLHttpRequest.cpp
    xml/XMLHttpRequestProgressEventThrottle.cpp
    xml/XMLHttpRequestUpload.cpp
    xml/XMLSerializer.cpp
    xml/XMLTreeViewer.cpp
    xml/XPathEvaluator.cpp
    xml/XPathException.cpp
    xml/XPathExpression.cpp
    xml/XPathExpressionNode.cpp
    xml/XPathFunctions.cpp
    xml/XPathGrammar.cpp
    xml/XPathNSResolver.cpp
    xml/XPathNodeSet.cpp
    xml/XPathParser.cpp
    xml/XPathPath.cpp
    xml/XPathPredicate.cpp
    xml/XPathResult.cpp
    xml/XPathStep.cpp
    xml/XPathUtil.cpp
    xml/XPathValue.cpp
    xml/XPathVariableReference.cpp
    xml/XSLImportRule.cpp
    xml/XSLStyleSheetLibxslt.cpp
    xml/XSLTExtensions.cpp
    xml/XSLTProcessor.cpp
    xml/XSLTProcessorLibxslt.cpp
    xml/XSLTUnicodeSort.cpp

    xml/parser/XMLDocumentParser.cpp
    xml/parser/XMLDocumentParserLibxml2.cpp
    xml/parser/XMLDocumentParserScope.cpp
)

if (ENABLE_GAMEPAD)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/gamepad"
    )

    list(APPEND WebCore_IDL_INCLUDES
        Modules/gamepad
    )

    list(APPEND WebCore_NON_SVG_IDL_FILES
        Modules/gamepad/Gamepad.idl
        Modules/gamepad/GamepadButton.idl
        Modules/gamepad/GamepadEvent.idl
        Modules/gamepad/NavigatorGamepad.idl
    )

    list(APPEND WebCore_SOURCES
        Modules/gamepad/Gamepad.cpp
        Modules/gamepad/GamepadButton.cpp
        Modules/gamepad/GamepadEvent.cpp
        Modules/gamepad/GamepadManager.cpp
        Modules/gamepad/NavigatorGamepad.cpp

        platform/gamepad/EmptyGamepadProvider.cpp
        platform/gamepad/GamepadProvider.cpp
    )
elseif (ENABLE_GAMEPAD_DEPRECATED)
    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${WEBCORE_DIR}/Modules/gamepad/deprecated"
    )

    list(APPEND WebCore_IDL_INCLUDES
        Modules/gamepad/deprecated
    )

    list(APPEND WebCore_NON_SVG_IDL_FILES
        Modules/gamepad/deprecated/Gamepad.idl
        Modules/gamepad/deprecated/GamepadList.idl
        Modules/gamepad/deprecated/NavigatorGamepad.idl
    )

    list(APPEND WebCore_SOURCES
        Modules/gamepad/deprecated/Gamepad.cpp
        Modules/gamepad/deprecated/GamepadList.cpp
        Modules/gamepad/deprecated/NavigatorGamepad.cpp
    )
endif ()

set(WebCore_IDL_FILES
    ${WebCore_NON_SVG_IDL_FILES}
    ${WebCore_SVG_IDL_FILES}
)

if (ENABLE_ALLINONE_BUILD)
    message(STATUS "All-in-One build is enabled.")
    set(allInOnes
        accessibility/AccessibilityAllInOne.cpp

        bindings/js/JSBindingsAllInOne.cpp

        css/CSSAllInOne.cpp

        dom/DOMAllInOne.cpp

        editing/EditingAllInOne.cpp

        html/HTMLElementsAllInOne.cpp

        inspector/InspectorAllInOne.cpp

        loader/appcache/ApplicationCacheAllInOne.cpp

        mathml/MathMLAllInOne.cpp

        platform/text/TextAllInOne.cpp

        rendering/RenderingAllInOne.cpp

        rendering/style/StyleAllInOne.cpp

        rendering/svg/RenderSVGAllInOne.cpp

        svg/SVGAllInOne.cpp
    )
    foreach (allInOne ${allInOnes})
        PROCESS_ALLINONE_FILE("${WebCore_SOURCES}" ${allInOne} newSources FALSE)
        set(WebCore_SOURCES
            ${allInOne}
            ${newSources}
        )
    endforeach ()
endif ()

set(WebCore_CSS_PROPERTY_NAMES
    ${WEBCORE_DIR}/css/CSSProperties.json
)

set(WebCore_CSS_VALUE_KEYWORDS
    ${WEBCORE_DIR}/css/CSSValueKeywords.in
)

set(WebCore_USER_AGENT_STYLE_SHEETS
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
    ${ICU_LIBRARIES}
    JavaScriptCore${DEBUG_SUFFIX}
    PAL${DEBUG_SUFFIX}
)

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
    list(APPEND WebCore_IDL_FILES
        Modules/encryptedmedia/legacy/WebKitMediaKeyMessageEvent.idl
        Modules/encryptedmedia/legacy/WebKitMediaKeyNeededEvent.idl
        Modules/encryptedmedia/legacy/WebKitMediaKeySession.idl
        Modules/encryptedmedia/legacy/WebKitMediaKeys.idl

        html/WebKitMediaKeyError.idl
    )
endif ()

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
    list(APPEND WebCore_IDL_FILES
    )
    list(APPEND WebCore_IDL_FILES
        Modules/encryptedmedia/MediaKeyMessageEvent.idl
        Modules/encryptedmedia/MediaKeySession.idl
        Modules/encryptedmedia/MediaKeySessionType.idl
        Modules/encryptedmedia/MediaKeyStatusMap.idl
        Modules/encryptedmedia/MediaKeySystemAccess.idl
        Modules/encryptedmedia/MediaKeySystemConfiguration.idl
        Modules/encryptedmedia/MediaKeySystemMediaCapability.idl
        Modules/encryptedmedia/MediaKeys.idl
        Modules/encryptedmedia/MediaKeysRequirement.idl
        Modules/encryptedmedia/NavigatorEME.idl

        html/MediaEncryptedEvent.idl
    )
endif ()

list(APPEND WebCore_CSS_VALUE_KEYWORDS
    ${WEBCORE_DIR}/css/SVGCSSValueKeywords.in
)

if (ENABLE_WEBGL)
    list(APPEND WebCore_SOURCES
        html/canvas/ANGLEInstancedArrays.cpp
        html/canvas/EXTBlendMinMax.cpp
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
        html/canvas/WebGLAny.cpp
        html/canvas/WebGLBuffer.cpp
        html/canvas/WebGLCompressedTextureATC.cpp
        html/canvas/WebGLCompressedTexturePVRTC.cpp
        html/canvas/WebGLCompressedTextureS3TC.cpp
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
    list(APPEND WebCore_IDL_FILES
        html/canvas/ANGLEInstancedArrays.idl
        html/canvas/EXTBlendMinMax.idl
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
        html/canvas/WebGLCompressedTextureATC.idl
        html/canvas/WebGLCompressedTexturePVRTC.idl
        html/canvas/WebGLCompressedTextureS3TC.idl
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
endif ()

if (ENABLE_VIDEO_TRACK)
    list(APPEND WebCore_IDL_FILES
        html/track/AudioTrack.idl
        html/track/AudioTrackList.idl
        html/track/DataCue.idl
        html/track/TextTrack.idl
        html/track/TextTrackCue.idl
        html/track/TextTrackCueList.idl
        html/track/TextTrackList.idl
        html/track/TrackEvent.idl
        html/track/VTTCue.idl
        html/track/VTTRegion.idl
        html/track/VTTRegionList.idl
        html/track/VideoTrack.idl
        html/track/VideoTrackList.idl
    )

    list(APPEND WebCore_SOURCES
        html/track/AudioTrack.cpp
        html/track/AudioTrackList.cpp
        html/track/BufferedLineReader.cpp
        html/track/DataCue.cpp
        html/track/InbandDataTextTrack.cpp
        html/track/InbandGenericTextTrack.cpp
        html/track/InbandTextTrack.cpp
        html/track/InbandWebVTTTextTrack.cpp
        html/track/LoadableTextTrack.cpp
        html/track/TextTrack.cpp
        html/track/TextTrackCue.cpp
        html/track/TextTrackCueGeneric.cpp
        html/track/TextTrackCueList.cpp
        html/track/TextTrackList.cpp
        html/track/TrackBase.cpp
        html/track/TrackEvent.cpp
        html/track/TrackListBase.cpp
        html/track/VTTCue.cpp
        html/track/VTTRegion.cpp
        html/track/VTTRegionList.cpp
        html/track/VTTScanner.cpp
        html/track/VideoTrack.cpp
        html/track/VideoTrackList.cpp
        html/track/WebVTTElement.cpp
        html/track/WebVTTParser.cpp
        html/track/WebVTTTokenizer.cpp

        loader/cache/CachedTextTrack.cpp

        platform/graphics/TextTrackRepresentation.cpp
    )
endif ()

if (ENABLE_QUOTA)
    list(APPEND WebCore_SOURCES
        Modules/quota/DOMWindowQuota.cpp
        Modules/quota/NavigatorStorageQuota.cpp
        Modules/quota/StorageErrorCallback.cpp
        Modules/quota/StorageInfo.cpp
        Modules/quota/StorageQuota.cpp
        Modules/quota/WorkerNavigatorStorageQuota.cpp
    )

    list(APPEND WebCore_IDL_FILES
        Modules/quota/DOMWindowQuota.idl
        Modules/quota/NavigatorStorageQuota.idl
        Modules/quota/StorageErrorCallback.idl
        Modules/quota/StorageInfo.idl
        Modules/quota/StorageQuota.idl
        Modules/quota/StorageQuotaCallback.idl
        Modules/quota/StorageUsageCallback.idl
        Modules/quota/WorkerNavigatorStorageQuota.idl
    )
endif ()

if (ENABLE_GRAPHICS_CONTEXT_3D)
    # For platforms that want to use system-provided OpenGL (ES) / EGL headers,
    # these include directories, libraries or definitions need to be
    # added before the ANGLE directories.
    if (USE_OPENGL)
        list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
            ${OPENGL_INCLUDE_DIRS}
        )
        list(APPEND WebCore_LIBRARIES
            ${OPENGL_LIBRARIES}
        )
        add_definitions(${OPENGL_DEFINITIONS})
    elseif (USE_OPENGL_ES_2)
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

    list(APPEND WebCore_INCLUDE_DIRECTORIES
        "${ANGLE_FORWARDING_HEADERS_DIR}"
        "${WEBCORE_DIR}/platform/graphics/gpu"
    )

    list(APPEND ANGLESupport_SOURCES
        ${THIRDPARTY_DIR}/ANGLE/src/common/Float16ToFloat32.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/angleutils.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/debug.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/event_tracer.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/mathutil.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/MemoryBuffer.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/utilities.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/common/tls.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/AddAndTrueToLoopCondition.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ArrayReturnValueToOutParameter.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ASTMetadataHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/blocklayout.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/blocklayoutHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/BuiltInFunctionEmulator.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/BuiltInFunctionEmulatorGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/BuiltInFunctionEmulatorHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Cache.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/CallDAG.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/CodeGen.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Compiler.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ConstantUnion.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/DeferGlobalInitializers.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Diagnostics.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/DirectiveHandler.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/EmulateGLFragColorBroadcast.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/EmulatePrecision.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ExtensionGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/glslang_lex.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/glslang_tab.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ForLoopUnroll.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/FlagStd140Structs.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/InfoSink.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Initialize.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/InitializeDll.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/InitializeParseContext.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/InitializeVariables.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Intermediate.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/IntermTraverse.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/IntermNode.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/intermOut.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/LoopInfo.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Operator.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputESSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputGLSLBase.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/OutputHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ParseContext.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/PoolAlloc.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/PruneEmptyDeclarations.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/QualifierTypes.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RecordConstantPrecision.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RegenerateStructNames.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemoveDynamicIndexing.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemovePow.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RemoveSwitchFallThrough.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RewriteDoWhile.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RewriteElseBlocks.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/RewriteTexelFetchOffset.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ScalarizeVecAndMatConstructorArgs.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SearchSymbol.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SeparateArrayInitialization.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SeparateExpressionsReturningArrays.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SeparateDeclarations.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ShaderLang.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ShaderVars.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/StructureHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/SymbolTable.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/TranslatorESSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/TranslatorGLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/TranslatorHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/Types.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UnfoldShortCircuitAST.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UnfoldShortCircuitToIf.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UniformHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UseInterfaceBlockFields.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/util.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/UtilsHLSL.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateGlobalInitializer.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateLimitations.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateMaxParameters.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateOutputs.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/ValidateSwitch.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/VariableInfo.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/VariablePacker.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/translator/VersionGLSL.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/DiagnosticsBase.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/DirectiveHandlerBase.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/DirectiveParser.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/ExpressionParser.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Input.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Lexer.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Macro.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/MacroExpander.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Preprocessor.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Token.cpp
        ${THIRDPARTY_DIR}/ANGLE/src/compiler/preprocessor/Tokenizer.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/libANGLE/Platform.cpp

        ${THIRDPARTY_DIR}/ANGLE/src/third_party/compiler/ArrayBoundsClamper.cpp
    )

    list(APPEND WebCore_SOURCES
        platform/graphics/ANGLEWebKitBridge.cpp
        platform/graphics/FormatConverter.cpp
        platform/graphics/GraphicsContext3D.cpp
    )
endif ()

if (ENABLE_WEB_REPLAY)
    set(WebCore_WEB_REPLAY_INPUTS ${CMAKE_CURRENT_SOURCE_DIR}/replay/WebInputs.json ${JAVASCRIPTCORE_DIR}/replay/JSInputs.json)
    add_custom_command(
        OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/WebReplayInputs.h ${DERIVED_SOURCES_WEBCORE_DIR}/WebReplayInputs.cpp
        MAIN_DEPENDENCY ${JAVASCRIPTCORE_DIR}/replay/scripts/CodeGeneratorReplayInputs.py
        DEPENDS ${WebCore_WEB_REPLAY_INPUTS}
        COMMAND ${PYTHON_EXECUTABLE} ${JAVASCRIPTCORE_DIR}/replay/scripts/CodeGeneratorReplayInputs.py --outputDir ${DERIVED_SOURCES_WEBCORE_DIR}/ --framework WebCore ${WebCore_WEB_REPLAY_INPUTS}
        VERBATIM)

    list(APPEND WebCore_SOURCES
        inspector/InspectorReplayAgent.cpp

        replay/CapturingInputCursor.cpp
        replay/EventLoopInput.cpp
        replay/EventLoopInputDispatcher.cpp
        replay/MemoizedDOMResult.cpp
        replay/ReplayController.cpp
        replay/ReplayInputCreationMethods.cpp
        replay/ReplayInputDispatchMethods.cpp
        replay/ReplaySession.cpp
        replay/ReplaySessionSegment.cpp
        replay/ReplayingInputCursor.cpp
        replay/SegmentedInputStorage.cpp
        replay/SerializationMethods.cpp
        replay/UserInputBridge.cpp

        ${DERIVED_SOURCES_WEBCORE_DIR}/WebReplayInputs.cpp
    )
endif ()

if (ENABLE_USER_MESSAGE_HANDLERS)
    list(APPEND WebCore_IDL_FILES
        page/UserMessageHandler.idl
        page/UserMessageHandlersNamespace.idl
        page/WebKitNamespace.idl
    )
    list(APPEND WebCore_SOURCES
        bindings/js/JSUserMessageHandlersNamespaceCustom.cpp

        page/UserMessageHandler.cpp
        page/UserMessageHandlerDescriptor.cpp
        page/UserMessageHandlersNamespace.cpp
        page/WebKitNamespace.cpp
    )
endif ()

if (USE_WOFF2)
  list(APPEND WebCore_INCLUDE_DIRECTORIES "${THIRDPARTY_DIR}/woff2/src")
  list(APPEND WebCore_LIBRARIES woff2)
endif ()

set(WebCoreTestSupport_INCLUDE_DIRECTORIES
    "${WEBCORE_DIR}/platform/mock"
    "${WEBCORE_DIR}/testing"
    "${WEBCORE_DIR}/testing/js"
    "${DERIVED_SOURCES_WEBCORE_DIR}"
)

set(WebCoreTestSupport_IDL_INCLUDES
    testing
)

set(WebCoreTestSupport_IDL_FILES
    testing/GCObservation.idl
    testing/InternalSettings.idl
    testing/Internals.idl
    testing/MallocStatistics.idl
    testing/MemoryInfo.idl
    testing/MockCDMFactory.idl
    testing/MockContentFilterSettings.idl
    testing/MockPageOverlay.idl
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
    testing/MockCDMFactory.cpp
    testing/MockGamepad.cpp
    testing/MockGamepadProvider.cpp
    testing/MockPageOverlay.cpp
    testing/MockPageOverlayClient.cpp

    testing/js/WebCoreTestSupport.cpp
)

set(WebCoreTestSupport_LIBRARIES
    PRIVATE JavaScriptCore${DEBUG_SUFFIX}
)

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
set(SUPPLEMENTAL_DEPENDENCY_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/supplemental_dependency.tmp)
set(WINDOW_CONSTRUCTORS_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/DOMWindowConstructors.idl)
set(WORKERGLOBALSCOPE_CONSTRUCTORS_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/WorkerGlobalScopeConstructors.idl)
set(DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE ${DERIVED_SOURCES_WEBCORE_DIR}/DedicatedWorkerGlobalScopeConstructors.idl)

WEBKIT_INCLUDE_CONFIG_FILES_IF_EXISTS()

# Generate InspectorOverlayPage.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.h ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.combined.html
    MAIN_DEPENDENCY inspector/InspectorOverlayPage.html
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/inline-and-minify-stylesheets-and-scripts.py
    DEPENDS inspector/InspectorOverlayPage.css inspector/InspectorOverlayPage.js
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/inline-and-minify-stylesheets-and-scripts.py ${WEBCORE_DIR}/inspector/InspectorOverlayPage.html ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.combined.html
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl InspectorOverlayPage_html ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.combined.html ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.h
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/InspectorOverlayPage.h)


# Generate CommandLineAPIModuleSource.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.h ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.min.js
    MAIN_DEPENDENCY inspector/CommandLineAPIModuleSource.js
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
    COMMAND ${CMAKE_COMMAND} -E echo "//# sourceURL=__InjectedScript_CommandLineAPIModuleSource.js" > ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.min.js
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py < ${WEBCORE_DIR}/inspector/CommandLineAPIModuleSource.js >> ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.min.js
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl CommandLineAPIModuleSource_js ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.min.js ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.h
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/CommandLineAPIModuleSource.h)


MAKE_HASH_TOOLS(${WEBCORE_DIR}/platform/ColorData)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/ColorData.cpp)


# Generate DOMJITAbstractHeapRepository.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/DOMJITAbstractHeapRepository.h
    MAIN_DEPENDENCY domjit/DOMJITAbstractHeapRepository.yaml
    DEPENDS ${WEBCORE_DIR}/domjit/generate-abstract-heap.rb
    COMMAND ${RUBY_EXECUTABLE} ${WEBCORE_DIR}/domjit/generate-abstract-heap.rb ${WEBCORE_DIR}/domjit/DOMJITAbstractHeapRepository.yaml ${DERIVED_SOURCES_WEBCORE_DIR}/DOMJITAbstractHeapRepository.h
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/DOMJITAbstractHeapRepository.h)

# Generate XMLViewerCSS.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerCSS.h ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.css
    MAIN_DEPENDENCY xml/XMLViewer.css
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/cssmin.py
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/cssmin.py < ${WEBCORE_DIR}/xml/XMLViewer.css > ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.css
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl XMLViewer_css ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.css ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerCSS.h
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerCSS.h)

# Generate XMLViewerJS.h
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerJS.h ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.js
    MAIN_DEPENDENCY xml/XMLViewer.js
    DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/jsmin.py < ${WEBCORE_DIR}/xml/XMLViewer.js > ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.js
    COMMAND ${PERL_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/xxd.pl XMLViewer_js ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewer.min.js ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerJS.h
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLViewerJS.h)

# Generate HTML entity table
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLEntityTable.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/html/parser/HTMLEntityNames.in
    DEPENDS ${WEBCORE_DIR}/html/parser/create-html-entity-table
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/html/parser/create-html-entity-table -o ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLEntityTable.cpp ${WEBCORE_DIR}/html/parser/HTMLEntityNames.in
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLEntityTable.cpp)

# Generate HTTP header names
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/platform/network/HTTPHeaderNames.in
    DEPENDS ${WEBCORE_DIR}/platform/network/create-http-header-name-table
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/platform/network/create-http-header-name-table ${WEBCORE_DIR}/platform/network/HTTPHeaderNames.in "${GPERF_EXECUTABLE}"
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/HTTPHeaderNames.cpp)

# Generate CSS property names
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/CSSProperties.json ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.h ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/StyleBuilder.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/StylePropertyShorthandFunctions.h ${DERIVED_SOURCES_WEBCORE_DIR}/StylePropertyShorthandFunctions.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makeprop.pl
    DEPENDS ${WebCore_CSS_PROPERTY_NAMES}
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PERL_EXECUTABLE} -ne "print" ${WebCore_CSS_PROPERTY_NAMES} > ${DERIVED_SOURCES_WEBCORE_DIR}/CSSProperties.json
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/makeprop.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --gperf-executable "${GPERF_EXECUTABLE}"
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/CSSPropertyNames.cpp)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/StyleBuilder.cpp)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/StylePropertyShorthandFunctions.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSParser.cpp CSSValueKeywords.h)

# Generate CSS value keywords
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.in ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.h ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.gperf
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makevalues.pl
    DEPENDS ${WebCore_CSS_VALUE_KEYWORDS}
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PERL_EXECUTABLE} -ne "print" ${WebCore_CSS_VALUE_KEYWORDS} > ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.in
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/makevalues.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}"
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/CSSValueKeywords.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSParser.cpp CSSValueKeywords.h)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/CSSPrimitiveValueMappings.h CSSValueKeywords.h)

# Generate CSS Selector pseudo type name to value maps.
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoClassAndCompatibilityElementMap.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoClassAndCompatibilityElementMap.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makeSelectorPseudoClassAndCompatibilityElementMap.py
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/css/makeSelectorPseudoClassAndCompatibilityElementMap.py ${WEBCORE_DIR}/css/SelectorPseudoClassAndCompatibilityElementMap.in "${GPERF_EXECUTABLE}" "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}"
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoClassAndCompatibilityElementMap.cpp)
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoElementTypeMap.gperf ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoElementTypeMap.cpp
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/makeSelectorPseudoElementsMap.py
    WORKING_DIRECTORY ${DERIVED_SOURCES_WEBCORE_DIR}
    COMMAND ${PYTHON_EXECUTABLE} ${WEBCORE_DIR}/css/makeSelectorPseudoElementsMap.py ${WEBCORE_DIR}/css/SelectorPseudoElementTypeMap.in "${GPERF_EXECUTABLE}" "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}"
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SelectorPseudoElementTypeMap.cpp)

# Generate user agent styles
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheetsData.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheets.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/make-css-file-arrays.pl
    DEPENDS ${WebCore_USER_AGENT_STYLE_SHEETS} ${WEBCORE_DIR}/bindings/scripts/preprocessor.pm
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/make-css-file-arrays.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheets.h ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheetsData.cpp ${WebCore_USER_AGENT_STYLE_SHEETS}
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentStyleSheetsData.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/StyleResolver.cpp UserAgentStyleSheetsData.cpp UserAgentStyleSheets.h)

if (WebCore_USER_AGENT_SCRIPTS)
    # Necessary variables:
    # WebCore_USER_AGENT_SCRIPTS containing the JavaScript sources list
    # WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES containing the source file that will load the scripts to add the proper
    #   dependency and having them built at the right moment

    MAKE_JS_FILE_ARRAYS(
        ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScriptsData.cpp
        ${DERIVED_SOURCES_WEBCORE_DIR}/UserAgentScripts.h
        WebCore_USER_AGENT_SCRIPTS
        WebCore_USER_AGENT_SCRIPTS_DEPENDENCIES
    )
endif ()

if (ENABLE_WEB_RTC)
    # Necessary variables:
    # WebCore_SDP_PROCESSOR_SCRIPTS containing the JavaScript sources list
    # WebCore_SDP_PROCESSOR_SCRIPTS_DEPENDENCIES containing the source file that will load the scripts to add the proper
    #   dependency and having them built at the right moment
    set(WebCore_SDP_PROCESSOR_SCRIPTS ${WEBCORE_DIR}/Modules/mediastream/sdp.js)
    set(WebCore_SDP_PROCESSOR_SCRIPTS_DEPENDENCIES ${WEBCORE_DIR}/platform/mediastream/SDPProcessorScriptResource.cpp)

    MAKE_JS_FILE_ARRAYS(
        ${DERIVED_SOURCES_WEBCORE_DIR}/SDPProcessorScriptsData.cpp
        ${DERIVED_SOURCES_WEBCORE_DIR}/SDPProcessorScriptsData.h
        WebCore_SDP_PROCESSOR_SCRIPTS
        WebCore_SDP_PROCESSOR_SCRIPTS_DEPENDENCIES
    )
endif ()

# Generate plug-in resources
add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/PlugInsResourcesData.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/PlugInsResources.h
    MAIN_DEPENDENCY ${WEBCORE_DIR}/css/make-css-file-arrays.pl
    DEPENDS ${WebCore_PLUG_INS_RESOURCES} ${WEBCORE_DIR}/bindings/scripts/preprocessor.pm
    COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/css/make-css-file-arrays.pl --defines "${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}" --preprocessor "${CODE_GENERATOR_PREPROCESSOR}" ${DERIVED_SOURCES_WEBCORE_DIR}/PlugInsResources.h ${DERIVED_SOURCES_WEBCORE_DIR}/PlugInsResourcesData.cpp ${WebCore_PLUG_INS_RESOURCES}
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/PlugInsResourcesData.cpp)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/css/StyleResolver.cpp PlugInsResourcesData.cpp PlugInsResources.h)

set(FEATURE_DEFINES_JAVASCRIPT "LANGUAGE_JAVASCRIPT ${FEATURE_DEFINES_WITH_SPACE_SEPARATOR}")
list(APPEND WebCoreTestSupport_IDL_FILES ${DERIVED_SOURCES_WEBCORE_DIR}/InternalSettingsGenerated.idl)

GENERATE_BINDINGS(WebCoreBindings
    OUTPUT_SOURCE WebCore_DERIVED_SOURCES
    INPUT_FILES ${WebCore_IDL_FILES}
    BASE_DIR ${WEBCORE_DIR}
    IDL_INCLUDES ${WebCore_IDL_INCLUDES} ${DERIVED_SOURCES_WEBCORE_DIR}
    FEATURES ${FEATURE_DEFINES_JAVASCRIPT}
    DESTINATION ${DERIVED_SOURCES_WEBCORE_DIR}
    GENERATOR JS
    SUPPLEMENTAL_DEPFILE ${SUPPLEMENTAL_DEPENDENCY_FILE}
    PP_EXTRA_OUTPUT
        ${WINDOW_CONSTRUCTORS_FILE} ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE} ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
    PP_EXTRA_ARGS
        --windowConstructorsFile ${WINDOW_CONSTRUCTORS_FILE}
        --workerGlobalScopeConstructorsFile ${WORKERGLOBALSCOPE_CONSTRUCTORS_FILE}
        --dedicatedWorkerGlobalScopeConstructorsFile ${DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE})

GENERATE_BINDINGS(WebCoreTestSupportBindings
    OUTPUT_SOURCE WebCoreTestSupport_SOURCES
    INPUT_FILES ${WebCoreTestSupport_IDL_FILES}
    BASE_DIR ${WEBCORE_DIR}
    IDL_INCLUDES ${WebCore_IDL_INCLUDES} ${DERIVED_SOURCES_WEBCORE_DIR} ${WebCoreTestSupport_IDL_INCLUDES}
    FEATURES ${FEATURE_DEFINES_JAVASCRIPT}
    DESTINATION ${DERIVED_SOURCES_WEBCORE_DIR}
    GENERATOR JS)

# WebCoreTestSupportBindings needs InternalSettingsGenerated.idl
add_dependencies(WebCoreTestSupportBindings WebCoreDerivedSources)

# WebCore JS Builtins

set(WebCore_BUILTINS_SOURCES
    ${WEBCORE_DIR}/Modules/fetch/DOMWindowFetch.js
    ${WEBCORE_DIR}/Modules/fetch/FetchHeaders.js
    ${WEBCORE_DIR}/Modules/fetch/FetchInternals.js
    ${WEBCORE_DIR}/Modules/fetch/FetchRequest.js
    ${WEBCORE_DIR}/Modules/fetch/FetchResponse.js
    ${WEBCORE_DIR}/Modules/fetch/WorkerGlobalScopeFetch.js
    ${WEBCORE_DIR}/Modules/mediastream/NavigatorUserMedia.js
    ${WEBCORE_DIR}/Modules/mediastream/RTCPeerConnection.js
    ${WEBCORE_DIR}/Modules/mediastream/RTCPeerConnectionInternals.js
    ${WEBCORE_DIR}/Modules/streams/ByteLengthQueuingStrategy.js
    ${WEBCORE_DIR}/Modules/streams/CountQueuingStrategy.js
    ${WEBCORE_DIR}/Modules/streams/ReadableByteStreamController.js
    ${WEBCORE_DIR}/Modules/streams/ReadableByteStreamInternals.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStream.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamDefaultController.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamInternals.js
    ${WEBCORE_DIR}/Modules/streams/ReadableStreamDefaultReader.js
    ${WEBCORE_DIR}/Modules/streams/StreamInternals.js
    ${WEBCORE_DIR}/Modules/streams/WritableStream.js
    ${WEBCORE_DIR}/Modules/streams/WritableStreamInternals.js
    ${WEBCORE_DIR}/xml/XMLHttpRequest.js
)

set(BUILTINS_GENERATOR_SCRIPTS
    ${JavaScriptCore_SCRIPTS_DIR}/builtins.py
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
        OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/${_objectName}Builtins.cpp
               ${DERIVED_SOURCES_WEBCORE_DIR}/${_objectName}Builtins.h
        MAIN_DEPENDENCY ${_builtinSource}
        DEPENDS ${BUILTINS_GENERATOR_SCRIPTS}
        COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py --framework WebCore --output-directory ${DERIVED_SOURCES_WEBCORE_DIR} ${_builtinSource}
        VERBATIM)
    list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/${_objectName}Builtins.cpp)
    list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/${_objectName}Builtins.h)
    list(APPEND WebCore_DERIVED_BUILTIN_HEADERS ${DERIVED_SOURCES_WEBCORE_DIR}/${_objectName}Builtins.h)
endforeach ()

add_custom_command(
    OUTPUT ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltinInternals.cpp
           ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltins.h
           ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltinInternals.h
    MAIN_DEPENDENCY ${WebCore_BUILTINS_SOURCES}
    DEPENDS ${BUILTINS_GENERATOR_SCRIPTS} ${WebCore_DERIVED_BUILTIN_HEADERS}
    COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/generate-js-builtins.py --wrappers-only --framework WebCore --output-directory ${DERIVED_SOURCES_WEBCORE_DIR} ${WebCore_BUILTINS_SOURCES}
    VERBATIM)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltinInternals.cpp)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltins.h)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/WebCoreJSBuiltinInternals.h)

ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/html/HTMLTreeBuilder.cpp MathMLNames.cpp)


GENERATE_DOM_NAMES(HTML ${WEBCORE_DIR}/html/HTMLAttributeNames.in ${WEBCORE_DIR}/html/HTMLTagNames.in "")
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLNames.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/HTMLElementFactory.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/JSHTMLElementWrapperFactory.cpp)


GENERATE_EVENT_FACTORY(${WEBCORE_DIR}/dom/EventNames.in EventInterfaces.h)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/EventInterfaces.h)


GENERATE_EVENT_FACTORY(${WEBCORE_DIR}/dom/EventTargetFactory.in EventTargetInterfaces.h)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/dom/EventNames.cpp EventTargetInterfaces.h)
ADD_SOURCE_WEBCORE_DERIVED_DEPENDENCIES(${WEBCORE_DIR}/dom/DOMAllInOne.cpp EventTargetInterfaces.h)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/EventTargetInterfaces.h)


GENERATE_EXCEPTION_CODE_DESCRIPTION(${WEBCORE_DIR}/dom/DOMExceptions.in ExceptionCodeDescription.cpp)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/ExceptionCodeDescription.cpp)


GENERATE_SETTINGS_MACROS(${WEBCORE_DIR}/page/Settings.in SettingsMacros.h)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SettingsMacros.h)
list(APPEND WebCoreTestSupport_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/InternalSettingsGenerated.cpp)


GENERATE_FONT_NAMES(${WEBCORE_DIR}/css/WebKitFontFamilyNames.in)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/WebKitFontFamilyNames.cpp)


GENERATE_DOM_NAMES(MathML ${WEBCORE_DIR}/mathml/mathattrs.in ${WEBCORE_DIR}/mathml/mathtags.in "")
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/MathMLNames.cpp)
if (ENABLE_MATHML)
    list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/MathMLElementFactory.cpp)
endif ()

# SVG extra defines need to map to a numerical value for correct preprocessing of svgtags.in.
set(SVG_EXTRA_DEFINES "")
if (ENABLE_SVG_FONTS)
    list(APPEND SVG_EXTRA_DEFINES "ENABLE_SVG_FONTS=1")
endif ()

GENERATE_DOM_NAMES(SVG ${WEBCORE_DIR}/svg/svgattrs.in ${WEBCORE_DIR}/svg/svgtags.in "${SVG_EXTRA_DEFINES}")
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SVGNames.cpp)

list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/SVGElementFactory.cpp ${DERIVED_SOURCES_WEBCORE_DIR}/JSSVGElementWrapperFactory.cpp)

GENERATE_DOM_NAMES(XLink ${WEBCORE_DIR}/svg/xlinkattrs.in)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XLinkNames.cpp)


GENERATE_DOM_NAMES(XMLNS ${WEBCORE_DIR}/xml/xmlnsattrs.in)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLNSNames.cpp)


GENERATE_DOM_NAMES(XML ${WEBCORE_DIR}/xml/xmlattrs.in)
list(APPEND WebCore_DERIVED_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/XMLNames.cpp)

if (ENABLE_ALLINONE_BUILD AND WIN32)
    message(STATUS "All-in-One DerivedSources build is enabled.")
    PROCESS_ALLINONE_FILE("${WebCore_DERIVED_SOURCES}" DerivedSources.cpp newSources TRUE)
    set(WebCore_DERIVED_SOURCES DerivedSources.cpp ${newSources})
endif ()

WEBKIT_WRAP_SOURCELIST(${WebCore_IDL_FILES} ${WebCore_DERIVED_SOURCES})
WEBKIT_WRAP_SOURCELIST(${WebCore_SOURCES})
WEBKIT_WRAP_SOURCELIST(${WebCoreTestSupport_IDL_FILES} ${WebCoreTestSupport_SOURCES})

if (MSVC)
    ADD_PRECOMPILED_HEADER("WebCorePrefix.h" "WebCorePrefix.cpp" WebCore_SOURCES)
    ADD_PRECOMPILED_HEADER("WebCorePrefix.h" "WebCoreDerivedSourcesPrefix.cpp" WebCore_DERIVED_SOURCES)
endif ()

# This is split into a separate library as a workaround for command line length
# limits. This should no longer be needed when CMake supports Ninja response
# files on OS X.
add_library(WebCoreDerivedSources STATIC ${WebCore_DERIVED_SOURCES})
set_target_properties(WebCoreDerivedSources PROPERTIES OUTPUT_NAME WebCoreDerivedSources${DEBUG_SUFFIX})
target_link_libraries(WebCoreDerivedSources ${WebCore_LIBRARIES})
target_include_directories(WebCoreDerivedSources PUBLIC ${WebCore_INCLUDE_DIRECTORIES})
add_dependencies(WebCoreDerivedSources WebCoreBindings)
if (NOT WIN32)
    list(APPEND WebCore_LIBRARIES WebCoreDerivedSources)
endif ()

WEBKIT_FRAMEWORK(WebCore)

if (APPLE)
    set_target_properties(WebCore PROPERTIES LINK_FLAGS "-sub_library libobjc -umbrella WebKit -allowable_client WebCoreTestSupport -allowable_client WebKit2 -allowable_client WebKitLegacy")
endif ()

if (WIN32)
    add_custom_target(WebCoreDerivedSources_PreBuild COMMAND ${WebCore_DERIVED_SOURCES_PRE_BUILD_COMMAND} VERBATIM)
    add_dependencies(WebCoreDerivedSources WebCoreDerivedSources_PreBuild)
    add_dependencies(WebCore WebCoreDerivedSources)
endif ()

# The -ftree-sra optimization (implicit with -O2) causes crashes when
# allocating large chunks of memory using bmalloc on Intel 32bit.
# See https://bugs.webkit.org/show_bug.cgi?id=146440
string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR)
if (CMAKE_COMPILER_IS_GNUCXX AND "${LOWERCASE_CMAKE_HOST_SYSTEM_PROCESSOR}" MATCHES "(i[3-6]86|x86)$")
    ADD_TARGET_PROPERTIES(WebCore COMPILE_FLAGS "-fno-tree-sra")
endif ()

if (MSVC)
    ADD_PRECOMPILED_HEADER("WebCoreTestSupportPrefix.h" "testing/js/WebCoreTestSupportPrefix.cpp" WebCoreTestSupport_SOURCES)
endif ()

add_library(WebCoreTestSupport ${WebCoreTestSupport_LIBRARY_TYPE} ${WebCoreTestSupport_SOURCES})
add_dependencies(WebCoreTestSupport WebCore WebCoreTestSupportBindings)
target_include_directories(WebCoreTestSupport PUBLIC ${WebCoreTestSupport_INCLUDE_DIRECTORIES} ${WebCore_INCLUDE_DIRECTORIES})
target_link_libraries(WebCoreTestSupport ${WebCoreTestSupport_LIBRARIES})

if (WebCoreTestSupport_OUTPUT_NAME)
    set_target_properties(WebCoreTestSupport PROPERTIES OUTPUT_NAME ${WebCoreTestSupport_OUTPUT_NAME})
    if (MSVC)
        add_compile_options(/GF- /WX-)
    endif ()
endif ()

if (ENABLE_GRAPHICS_CONTEXT_3D AND NOT WIN32)
    add_library(ANGLESupport STATIC ${ANGLESupport_SOURCES})

    # Suppress null conversion warnings for sources in Source/ThirdParty/ANGLE
    if (COMPILER_IS_CLANG)
        ADD_TARGET_PROPERTIES(ANGLESupport COMPILE_FLAGS "-Wno-null-conversion")
    endif ()

    # Enable the ESSL and GLSL translators.
    set_property(TARGET ANGLESupport
        PROPERTY COMPILE_DEFINITIONS
        ANGLE_ENABLE_ESSL
        ANGLE_ENABLE_GLSL)

    target_include_directories(ANGLESupport PRIVATE
        "${THIRDPARTY_DIR}/ANGLE"
        "${THIRDPARTY_DIR}/ANGLE/include"
        "${THIRDPARTY_DIR}/ANGLE/include/KHR"
        "${THIRDPARTY_DIR}/ANGLE/src"
        "${THIRDPARTY_DIR}/ANGLE/src/common/third_party/numerics"
    )
    list(APPEND WebCore_LIBRARIES ANGLESupport)
    WEBKIT_SET_EXTRA_COMPILER_FLAGS(ANGLESupport IGNORECXX_WARNINGS)
endif ()

target_link_libraries(WebCore ${WebCore_LIBRARIES})

if (SHARED_CORE)
    set_target_properties(WebCore PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
    install(TARGETS WebCore DESTINATION "${LIB_INSTALL_DIR}")
endif ()

# [ARM] Build SVGPathElement.cpp with -O2 due to a GCC bug
# https://bugs.webkit.org/show_bug.cgi?id=145377
if ((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") AND WTF_CPU_ARM AND (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "4.9") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9.3"))
    set_source_files_properties(svg/SVGPathElement.cpp PROPERTIES COMPILE_FLAGS "-O2")
endif ()

if (MSVC)
    set_target_properties(WebCore PROPERTIES
        COMPILE_PDB_NAME ${WebCore_OUTPUT_NAME}
    )
endif ()
