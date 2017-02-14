# Copyright (C) 2006-2008, 2012, 2014-2015 Apple Inc. All rights reserved.
# Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
# Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

VPATH = \
    $(WebCore) \
    $(WebCore)/Modules/airplay \
    $(WebCore)/Modules/applepay \
    $(WebCore)/Modules/encryptedmedia \
    $(WebCore)/Modules/encryptedmedia/legacy \
    $(WebCore)/Modules/fetch \
    $(WebCore)/Modules/gamepad \
    $(WebCore)/Modules/geolocation \
    $(WebCore)/Modules/indexeddb \
    $(WebCore)/Modules/indieui \
    $(WebCore)/Modules/mediacontrols \
    $(WebCore)/Modules/mediasession \
    $(WebCore)/Modules/mediasource \
    $(WebCore)/Modules/mediastream \
    $(WebCore)/Modules/notifications \
    $(WebCore)/Modules/plugins \
    $(WebCore)/Modules/quota \
    $(WebCore)/Modules/speech \
    $(WebCore)/Modules/streams \
    $(WebCore)/Modules/webaudio \
    $(WebCore)/Modules/webdatabase \
    $(WebCore)/Modules/webdriver \
    $(WebCore)/Modules/websockets \
    $(WebCore)/animation \
    $(WebCore)/bindings/js \
    $(WebCore)/crypto \
    $(WebCore)/crypto/parameters \
    $(WebCore)/css \
    $(WebCore)/dom \
    $(WebCore)/editing \
    $(WebCore)/fetch \
    $(WebCore)/fileapi \
    $(WebCore)/html \
    $(WebCore)/html/canvas \
    $(WebCore)/html/shadow \
    $(WebCore)/html/track \
    $(WebCore)/inspector \
    $(WebCore)/loader/appcache \
    $(WebCore)/page \
    $(WebCore)/platform/network \
    $(WebCore)/plugins \
    $(WebCore)/storage \
    $(WebCore)/xml \
    $(WebCore)/workers \
    $(WebCore)/svg \
    $(WebCore)/testing \
    $(WebCore)/websockets \
#

JS_BINDING_IDLS = \
    $(WebCore)/Modules/airplay/WebKitPlaybackTargetAvailabilityEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayLineItem.idl \
    $(WebCore)/Modules/applepay/ApplePayPayment.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentAuthorizedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentContact.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentMethod.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentMethodSelectedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentPass.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentRequest.idl \
    $(WebCore)/Modules/applepay/ApplePaySession.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingContactSelectedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingMethod.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingMethodSelectedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayValidateMerchantEvent.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeyMessageEvent.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeySession.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeySessionType.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeyStatusMap.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeySystemAccess.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeySystemConfiguration.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeySystemMediaCapability.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeys.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeysRequirement.idl \
    $(WebCore)/Modules/encryptedmedia/NavigatorEME.idl \
    $(WebCore)/Modules/encryptedmedia/legacy/WebKitMediaKeyMessageEvent.idl \
    $(WebCore)/Modules/encryptedmedia/legacy/WebKitMediaKeyNeededEvent.idl \
    $(WebCore)/Modules/encryptedmedia/legacy/WebKitMediaKeySession.idl \
    $(WebCore)/Modules/encryptedmedia/legacy/WebKitMediaKeys.idl \
    $(WebCore)/Modules/fetch/DOMWindowFetch.idl \
    $(WebCore)/Modules/fetch/FetchBody.idl \
    $(WebCore)/Modules/fetch/FetchHeaders.idl \
    $(WebCore)/Modules/fetch/FetchRequest.idl \
    $(WebCore)/Modules/fetch/FetchResponse.idl \
    $(WebCore)/Modules/fetch/WorkerGlobalScopeFetch.idl \
    $(WebCore)/Modules/gamepad/Gamepad.idl \
    $(WebCore)/Modules/gamepad/GamepadButton.idl \
    $(WebCore)/Modules/gamepad/GamepadEvent.idl \
    $(WebCore)/Modules/gamepad/NavigatorGamepad.idl \
    $(WebCore)/Modules/geolocation/Coordinates.idl \
    $(WebCore)/Modules/geolocation/Geolocation.idl \
    $(WebCore)/Modules/geolocation/Geoposition.idl \
    $(WebCore)/Modules/geolocation/NavigatorGeolocation.idl \
    $(WebCore)/Modules/geolocation/PositionCallback.idl \
    $(WebCore)/Modules/geolocation/PositionError.idl \
    $(WebCore)/Modules/geolocation/PositionErrorCallback.idl \
    $(WebCore)/Modules/geolocation/PositionOptions.idl \
    $(WebCore)/Modules/indexeddb/DOMWindowIndexedDatabase.idl \
    $(WebCore)/Modules/indexeddb/IDBCursor.idl \
    $(WebCore)/Modules/indexeddb/IDBCursorDirection.idl \
    $(WebCore)/Modules/indexeddb/IDBCursorWithValue.idl \
    $(WebCore)/Modules/indexeddb/IDBDatabase.idl \
    $(WebCore)/Modules/indexeddb/IDBFactory.idl \
    $(WebCore)/Modules/indexeddb/IDBIndex.idl \
    $(WebCore)/Modules/indexeddb/IDBKeyRange.idl \
    $(WebCore)/Modules/indexeddb/IDBObjectStore.idl \
    $(WebCore)/Modules/indexeddb/IDBOpenDBRequest.idl \
    $(WebCore)/Modules/indexeddb/IDBRequest.idl \
    $(WebCore)/Modules/indexeddb/IDBTransaction.idl \
    $(WebCore)/Modules/indexeddb/IDBTransactionMode.idl \
    $(WebCore)/Modules/indexeddb/IDBVersionChangeEvent.idl \
    $(WebCore)/Modules/indexeddb/WorkerGlobalScopeIndexedDatabase.idl \
    $(WebCore)/Modules/mediacontrols/MediaControlsHost.idl \
    $(WebCore)/Modules/mediasession/HTMLMediaElementMediaSession.idl \
    $(WebCore)/Modules/mediasession/MediaRemoteControls.idl \
    $(WebCore)/Modules/mediasession/MediaSession.idl \
    $(WebCore)/Modules/mediasource/AudioTrackMediaSource.idl \
    $(WebCore)/Modules/mediasource/DOMURLMediaSource.idl \
    $(WebCore)/Modules/mediasource/MediaSource.idl \
    $(WebCore)/Modules/mediasource/SourceBuffer.idl \
    $(WebCore)/Modules/mediasource/SourceBufferList.idl \
    $(WebCore)/Modules/mediasource/TextTrackMediaSource.idl \
    $(WebCore)/Modules/mediasource/VideoPlaybackQuality.idl \
    $(WebCore)/Modules/mediasource/VideoTrackMediaSource.idl \
    $(WebCore)/Modules/mediastream/DOMURLMediaStream.idl \
    $(WebCore)/Modules/mediastream/DoubleRange.idl \
    $(WebCore)/Modules/mediastream/LongRange.idl \
    $(WebCore)/Modules/mediastream/MediaDeviceInfo.idl \
    $(WebCore)/Modules/mediastream/MediaDevices.idl \
    $(WebCore)/Modules/mediastream/MediaStream.idl \
    $(WebCore)/Modules/mediastream/MediaStreamEvent.idl \
    $(WebCore)/Modules/mediastream/MediaStreamTrack.idl \
    $(WebCore)/Modules/mediastream/MediaStreamTrackEvent.idl \
    $(WebCore)/Modules/mediastream/MediaTrackConstraints.idl \
    $(WebCore)/Modules/mediastream/MediaTrackSupportedConstraints.idl \
    $(WebCore)/Modules/mediastream/NavigatorMediaDevices.idl \
    $(WebCore)/Modules/mediastream/NavigatorUserMedia.idl \
    $(WebCore)/Modules/mediastream/OverconstrainedError.idl \
    $(WebCore)/Modules/mediastream/OverconstrainedErrorEvent.idl \
    $(WebCore)/Modules/mediastream/RTCConfiguration.idl \
    $(WebCore)/Modules/mediastream/RTCDTMFSender.idl \
    $(WebCore)/Modules/mediastream/RTCDTMFToneChangeEvent.idl \
    $(WebCore)/Modules/mediastream/RTCDataChannel.idl \
    $(WebCore)/Modules/mediastream/RTCDataChannelEvent.idl \
    $(WebCore)/Modules/mediastream/RTCIceCandidate.idl \
    $(WebCore)/Modules/mediastream/RTCIceCandidateEvent.idl \
    $(WebCore)/Modules/mediastream/RTCIceServer.idl \
    $(WebCore)/Modules/mediastream/RTCPeerConnection.idl \
    $(WebCore)/Modules/mediastream/RTCRtpReceiver.idl \
    $(WebCore)/Modules/mediastream/RTCRtpSender.idl \
    $(WebCore)/Modules/mediastream/RTCRtpTransceiver.idl \
    $(WebCore)/Modules/mediastream/RTCSessionDescription.idl \
    $(WebCore)/Modules/mediastream/RTCStatsReport.idl \
    $(WebCore)/Modules/mediastream/RTCTrackEvent.idl \
    $(WebCore)/Modules/notifications/DOMWindowNotifications.idl \
    $(WebCore)/Modules/notifications/Notification.idl \
    $(WebCore)/Modules/notifications/NotificationCenter.idl \
    $(WebCore)/Modules/notifications/NotificationPermissionCallback.idl \
    $(WebCore)/Modules/notifications/WorkerGlobalScopeNotifications.idl \
    $(WebCore)/Modules/plugins/QuickTimePluginReplacement.idl \
    $(WebCore)/Modules/quota/DOMWindowQuota.idl \
    $(WebCore)/Modules/quota/NavigatorStorageQuota.idl \
    $(WebCore)/Modules/quota/StorageErrorCallback.idl \
    $(WebCore)/Modules/quota/StorageInfo.idl \
    $(WebCore)/Modules/quota/StorageQuota.idl \
    $(WebCore)/Modules/quota/StorageQuotaCallback.idl \
    $(WebCore)/Modules/quota/StorageUsageCallback.idl \
    $(WebCore)/Modules/quota/WorkerNavigatorStorageQuota.idl \
    $(WebCore)/Modules/speech/DOMWindowSpeechSynthesis.idl \
    $(WebCore)/Modules/speech/SpeechSynthesis.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisEvent.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisUtterance.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisVoice.idl \
    $(WebCore)/Modules/streams/ByteLengthQueuingStrategy.idl \
    $(WebCore)/Modules/streams/CountQueuingStrategy.idl \
    $(WebCore)/Modules/streams/ReadableByteStreamController.idl \
    $(WebCore)/Modules/streams/ReadableStream.idl \
    $(WebCore)/Modules/streams/ReadableStreamDefaultController.idl \
    $(WebCore)/Modules/streams/ReadableStreamDefaultReader.idl \
    $(WebCore)/Modules/streams/ReadableStreamSource.idl \
    $(WebCore)/Modules/streams/WritableStream.idl \
    $(WebCore)/Modules/webaudio/AnalyserNode.idl \
    $(WebCore)/Modules/webaudio/AudioBuffer.idl \
    $(WebCore)/Modules/webaudio/AudioBufferCallback.idl \
    $(WebCore)/Modules/webaudio/AudioBufferSourceNode.idl \
    $(WebCore)/Modules/webaudio/AudioContext.idl \
    $(WebCore)/Modules/webaudio/AudioDestinationNode.idl \
    $(WebCore)/Modules/webaudio/AudioListener.idl \
    $(WebCore)/Modules/webaudio/AudioNode.idl \
    $(WebCore)/Modules/webaudio/AudioParam.idl \
    $(WebCore)/Modules/webaudio/AudioProcessingEvent.idl \
    $(WebCore)/Modules/webaudio/BiquadFilterNode.idl \
    $(WebCore)/Modules/webaudio/ChannelMergerNode.idl \
    $(WebCore)/Modules/webaudio/ChannelSplitterNode.idl \
    $(WebCore)/Modules/webaudio/ConvolverNode.idl \
    $(WebCore)/Modules/webaudio/DelayNode.idl \
    $(WebCore)/Modules/webaudio/DynamicsCompressorNode.idl \
    $(WebCore)/Modules/webaudio/GainNode.idl \
    $(WebCore)/Modules/webaudio/MediaElementAudioSourceNode.idl \
    $(WebCore)/Modules/webaudio/MediaStreamAudioDestinationNode.idl \
    $(WebCore)/Modules/webaudio/MediaStreamAudioSourceNode.idl \
    $(WebCore)/Modules/webaudio/OfflineAudioCompletionEvent.idl \
    $(WebCore)/Modules/webaudio/OfflineAudioContext.idl \
    $(WebCore)/Modules/webaudio/OscillatorNode.idl \
    $(WebCore)/Modules/webaudio/PannerNode.idl \
    $(WebCore)/Modules/webaudio/PeriodicWave.idl \
    $(WebCore)/Modules/webaudio/ScriptProcessorNode.idl \
    $(WebCore)/Modules/webaudio/WaveShaperNode.idl \
    $(WebCore)/Modules/webdatabase/DOMWindowWebDatabase.idl \
    $(WebCore)/Modules/webdatabase/Database.idl \
    $(WebCore)/Modules/webdatabase/DatabaseCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLError.idl \
    $(WebCore)/Modules/webdatabase/SQLException.idl \
    $(WebCore)/Modules/webdatabase/SQLResultSet.idl \
    $(WebCore)/Modules/webdatabase/SQLResultSetRowList.idl \
    $(WebCore)/Modules/webdatabase/SQLStatementCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLStatementErrorCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLTransaction.idl \
    $(WebCore)/Modules/webdatabase/SQLTransactionCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLTransactionErrorCallback.idl \
    $(WebCore)/Modules/webdriver/NavigatorWebDriver.idl \
    $(WebCore)/Modules/websockets/CloseEvent.idl \
    $(WebCore)/Modules/websockets/WebSocket.idl \
    $(WebCore)/animation/Animatable.idl \
    $(WebCore)/animation/AnimationEffect.idl \
    $(WebCore)/animation/AnimationTimeline.idl \
    $(WebCore)/animation/DocumentAnimation.idl \
    $(WebCore)/animation/DocumentTimeline.idl \
    $(WebCore)/animation/KeyframeEffect.idl \
    $(WebCore)/animation/WebAnimation.idl \
    $(WebCore)/crypto/CryptoAlgorithmParameters.idl \
    $(WebCore)/crypto/CryptoKey.idl \
    $(WebCore)/crypto/CryptoKeyPair.idl \
    $(WebCore)/crypto/CryptoKeyUsage.idl \
    $(WebCore)/crypto/JsonWebKey.idl \
    $(WebCore)/crypto/RsaOtherPrimesInfo.idl \
    $(WebCore)/crypto/SubtleCrypto.idl \
    $(WebCore)/crypto/WebKitSubtleCrypto.idl \
    $(WebCore)/crypto/parameters/AesCbcParams.idl \
    $(WebCore)/crypto/parameters/AesKeyGenParams.idl \
    $(WebCore)/crypto/parameters/HmacKeyParams.idl \
    $(WebCore)/crypto/parameters/RsaHashedImportParams.idl \
    $(WebCore)/crypto/parameters/RsaHashedKeyGenParams.idl \
    $(WebCore)/crypto/parameters/RsaKeyGenParams.idl \
    $(WebCore)/crypto/parameters/RsaOaepParams.idl \
    $(WebCore)/css/CSSFontFaceLoadEvent.idl \
    $(WebCore)/css/CSSFontFaceRule.idl \
    $(WebCore)/css/CSSImportRule.idl \
    $(WebCore)/css/CSSKeyframeRule.idl \
    $(WebCore)/css/CSSKeyframesRule.idl \
    $(WebCore)/css/CSSMediaRule.idl \
    $(WebCore)/css/CSSNamespaceRule.idl \
    $(WebCore)/css/CSSPageRule.idl \
    $(WebCore)/css/CSSRule.idl \
    $(WebCore)/css/CSSRuleList.idl \
    $(WebCore)/css/CSSStyleDeclaration.idl \
    $(WebCore)/css/CSSStyleRule.idl \
    $(WebCore)/css/CSSStyleSheet.idl \
    $(WebCore)/css/CSSSupportsRule.idl \
    $(WebCore)/css/CSSUnknownRule.idl \
    $(WebCore)/css/DeprecatedCSSOMCounter.idl \
	$(WebCore)/css/DeprecatedCSSOMPrimitiveValue.idl \
	$(WebCore)/css/DeprecatedCSSOMRGBColor.idl \
	$(WebCore)/css/DeprecatedCSSOMRect.idl \
	$(WebCore)/css/DeprecatedCSSOMValue.idl \
    $(WebCore)/css/DeprecatedCSSOMValueList.idl \
    $(WebCore)/css/DOMCSSNamespace.idl \
    $(WebCore)/css/FontFace.idl \
    $(WebCore)/css/FontFaceSet.idl \
    $(WebCore)/css/MediaList.idl \
    $(WebCore)/css/MediaQueryList.idl \
    $(WebCore)/css/MediaQueryListListener.idl \
    $(WebCore)/css/StyleMedia.idl \
    $(WebCore)/css/StyleSheet.idl \
    $(WebCore)/css/StyleSheetList.idl \
    $(WebCore)/css/WebKitCSSMatrix.idl \
    $(WebCore)/css/WebKitCSSRegionRule.idl \
    $(WebCore)/css/WebKitCSSViewportRule.idl \
    $(WebCore)/dom/AnimationEvent.idl \
    $(WebCore)/dom/Attr.idl \
    $(WebCore)/dom/AutocompleteErrorEvent.idl \
    $(WebCore)/dom/BeforeLoadEvent.idl \
    $(WebCore)/dom/BeforeUnloadEvent.idl \
    $(WebCore)/dom/CDATASection.idl \
    $(WebCore)/dom/CharacterData.idl \
    $(WebCore)/dom/ChildNode.idl \
    $(WebCore)/dom/ClientRect.idl \
    $(WebCore)/dom/ClientRectList.idl \
    $(WebCore)/dom/ClipboardEvent.idl \
    $(WebCore)/dom/Comment.idl \
    $(WebCore)/dom/CompositionEvent.idl \
    $(WebCore)/dom/CustomElementRegistry.idl \
    $(WebCore)/dom/CustomEvent.idl \
    $(WebCore)/dom/DOMCoreException.idl \
    $(WebCore)/dom/DOMError.idl \
    $(WebCore)/dom/DOMImplementation.idl \
    $(WebCore)/dom/DOMNamedFlowCollection.idl \
    $(WebCore)/dom/DOMPoint.idl \
    $(WebCore)/dom/DOMPointInit.idl \
    $(WebCore)/dom/DOMPointReadOnly.idl \
    $(WebCore)/dom/DOMRect.idl \
    $(WebCore)/dom/DOMRectInit.idl \
    $(WebCore)/dom/DOMRectReadOnly.idl \
    $(WebCore)/dom/DOMStringList.idl \
    $(WebCore)/dom/DOMStringMap.idl \
    $(WebCore)/dom/DataTransfer.idl \
    $(WebCore)/dom/DataTransferItem.idl \
    $(WebCore)/dom/DataTransferItemList.idl \
    $(WebCore)/dom/DeviceMotionEvent.idl \
    $(WebCore)/dom/DeviceOrientationEvent.idl \
    $(WebCore)/dom/Document.idl \
    $(WebCore)/dom/DocumentFragment.idl \
    $(WebCore)/dom/DocumentOrShadowRoot.idl \
    $(WebCore)/dom/DocumentType.idl \
    $(WebCore)/dom/Element.idl \
    $(WebCore)/dom/ErrorEvent.idl \
    $(WebCore)/dom/Event.idl \
    $(WebCore)/dom/EventInit.idl \
    $(WebCore)/dom/EventListener.idl \
    $(WebCore)/dom/EventModifierInit.idl \
    $(WebCore)/dom/EventTarget.idl \
    $(WebCore)/dom/FocusEvent.idl \
    $(WebCore)/dom/GlobalEventHandlers.idl \
    $(WebCore)/dom/HashChangeEvent.idl \
    $(WebCore)/dom/InputEvent.idl \
    $(WebCore)/dom/KeyboardEvent.idl \
    $(WebCore)/dom/MessageChannel.idl \
    $(WebCore)/dom/MessageEvent.idl \
    $(WebCore)/dom/MessagePort.idl \
    $(WebCore)/dom/MouseEvent.idl \
    $(WebCore)/dom/MouseEventInit.idl \
    $(WebCore)/dom/MutationEvent.idl \
    $(WebCore)/dom/MutationObserver.idl \
    $(WebCore)/dom/MutationRecord.idl \
    $(WebCore)/dom/NamedNodeMap.idl \
    $(WebCore)/dom/Node.idl \
    $(WebCore)/dom/NodeFilter.idl \
    $(WebCore)/dom/NodeIterator.idl \
    $(WebCore)/dom/NodeList.idl \
    $(WebCore)/dom/NonDocumentTypeChildNode.idl \
    $(WebCore)/dom/NonElementParentNode.idl \
    $(WebCore)/dom/OverflowEvent.idl \
    $(WebCore)/dom/PageTransitionEvent.idl \
    $(WebCore)/dom/ParentNode.idl \
    $(WebCore)/dom/PopStateEvent.idl \
    $(WebCore)/dom/ProcessingInstruction.idl \
    $(WebCore)/dom/ProgressEvent.idl \
    $(WebCore)/dom/ProgressEvent.idl \
    $(WebCore)/dom/Range.idl \
    $(WebCore)/dom/RequestAnimationFrameCallback.idl \
    $(WebCore)/dom/SecurityPolicyViolationEvent.idl \
    $(WebCore)/dom/ShadowRoot.idl \
    $(WebCore)/dom/ShadowRootMode.idl \
    $(WebCore)/dom/Slotable.idl \
    $(WebCore)/dom/StaticRange.idl \
    $(WebCore)/dom/StringCallback.idl \
    $(WebCore)/dom/Text.idl \
    $(WebCore)/dom/TextDecoder.idl \
    $(WebCore)/dom/TextEncoder.idl \
    $(WebCore)/dom/TextEvent.idl \
    $(WebCore)/dom/TransitionEvent.idl \
    $(WebCore)/dom/TreeWalker.idl \
    $(WebCore)/dom/UIEvent.idl \
    $(WebCore)/dom/UIEventInit.idl \
    $(WebCore)/dom/WebKitAnimationEvent.idl \
    $(WebCore)/dom/WebKitNamedFlow.idl \
    $(WebCore)/dom/WebKitTransitionEvent.idl \
    $(WebCore)/dom/WheelEvent.idl \
    $(WebCore)/dom/XMLDocument.idl \
    $(WebCore)/fileapi/Blob.idl \
    $(WebCore)/fileapi/BlobLineEndings.idl \
    $(WebCore)/fileapi/BlobPropertyBag.idl \
    $(WebCore)/fileapi/File.idl \
    $(WebCore)/fileapi/FileError.idl \
    $(WebCore)/fileapi/FileException.idl \
    $(WebCore)/fileapi/FileList.idl \
    $(WebCore)/fileapi/FileReader.idl \
    $(WebCore)/fileapi/FileReaderSync.idl \
    $(WebCore)/html/DOMFormData.idl \
    $(WebCore)/html/DOMTokenList.idl \
    $(WebCore)/html/DOMURL.idl \
    $(WebCore)/html/HTMLAllCollection.idl \
    $(WebCore)/html/HTMLAnchorElement.idl \
    $(WebCore)/html/HTMLAppletElement.idl \
    $(WebCore)/html/HTMLAreaElement.idl \
    $(WebCore)/html/HTMLAttachmentElement.idl \
    $(WebCore)/html/HTMLAudioElement.idl \
    $(WebCore)/html/HTMLBRElement.idl \
    $(WebCore)/html/HTMLBaseElement.idl \
    $(WebCore)/html/HTMLBodyElement.idl \
    $(WebCore)/html/HTMLButtonElement.idl \
    $(WebCore)/html/HTMLCanvasElement.idl \
    $(WebCore)/html/HTMLCollection.idl \
    $(WebCore)/html/HTMLDListElement.idl \
    $(WebCore)/html/HTMLDataElement.idl \
    $(WebCore)/html/HTMLDataListElement.idl \
    $(WebCore)/html/HTMLDetailsElement.idl \
    $(WebCore)/html/HTMLDirectoryElement.idl \
    $(WebCore)/html/HTMLDivElement.idl \
    $(WebCore)/html/HTMLDocument.idl \
    $(WebCore)/html/HTMLElement.idl \
    $(WebCore)/html/HTMLEmbedElement.idl \
    $(WebCore)/html/HTMLFieldSetElement.idl \
    $(WebCore)/html/HTMLFontElement.idl \
    $(WebCore)/html/HTMLFormControlsCollection.idl \
    $(WebCore)/html/HTMLFormElement.idl \
    $(WebCore)/html/HTMLFrameElement.idl \
    $(WebCore)/html/HTMLFrameSetElement.idl \
    $(WebCore)/html/HTMLHRElement.idl \
    $(WebCore)/html/HTMLHeadElement.idl \
    $(WebCore)/html/HTMLHeadingElement.idl \
    $(WebCore)/html/HTMLHtmlElement.idl \
    $(WebCore)/html/HTMLHyperlinkElementUtils.idl \
    $(WebCore)/html/HTMLIFrameElement.idl \
    $(WebCore)/html/HTMLImageElement.idl \
    $(WebCore)/html/HTMLInputElement.idl \
    $(WebCore)/html/HTMLKeygenElement.idl \
    $(WebCore)/html/HTMLLIElement.idl \
    $(WebCore)/html/HTMLLabelElement.idl \
    $(WebCore)/html/HTMLLegendElement.idl \
    $(WebCore)/html/HTMLLinkElement.idl \
    $(WebCore)/html/HTMLMapElement.idl \
    $(WebCore)/html/HTMLMarqueeElement.idl \
    $(WebCore)/html/HTMLMediaElement.idl \
    $(WebCore)/html/HTMLMenuElement.idl \
    $(WebCore)/html/HTMLMetaElement.idl \
    $(WebCore)/html/HTMLMeterElement.idl \
    $(WebCore)/html/HTMLModElement.idl \
    $(WebCore)/html/HTMLOListElement.idl \
    $(WebCore)/html/HTMLObjectElement.idl \
    $(WebCore)/html/HTMLOptGroupElement.idl \
    $(WebCore)/html/HTMLOptionElement.idl \
    $(WebCore)/html/HTMLOptionsCollection.idl \
    $(WebCore)/html/HTMLOutputElement.idl \
    $(WebCore)/html/HTMLParagraphElement.idl \
    $(WebCore)/html/HTMLParamElement.idl \
    $(WebCore)/html/HTMLPictureElement.idl \
    $(WebCore)/html/HTMLPreElement.idl \
    $(WebCore)/html/HTMLProgressElement.idl \
    $(WebCore)/html/HTMLQuoteElement.idl \
    $(WebCore)/html/HTMLScriptElement.idl \
    $(WebCore)/html/HTMLSelectElement.idl \
    $(WebCore)/html/HTMLSlotElement.idl \
    $(WebCore)/html/HTMLSourceElement.idl \
    $(WebCore)/html/HTMLSpanElement.idl \
    $(WebCore)/html/HTMLStyleElement.idl \
    $(WebCore)/html/HTMLTableCaptionElement.idl \
    $(WebCore)/html/HTMLTableCellElement.idl \
    $(WebCore)/html/HTMLTableColElement.idl \
    $(WebCore)/html/HTMLTableElement.idl \
    $(WebCore)/html/HTMLTableRowElement.idl \
    $(WebCore)/html/HTMLTableSectionElement.idl \
    $(WebCore)/html/HTMLTemplateElement.idl \
    $(WebCore)/html/HTMLTextAreaElement.idl \
    $(WebCore)/html/HTMLTimeElement.idl \
    $(WebCore)/html/HTMLTitleElement.idl \
    $(WebCore)/html/HTMLTrackElement.idl \
    $(WebCore)/html/HTMLUListElement.idl \
    $(WebCore)/html/HTMLUnknownElement.idl \
    $(WebCore)/html/HTMLVideoElement.idl \
    $(WebCore)/html/ImageData.idl \
    $(WebCore)/html/MediaController.idl \
    $(WebCore)/html/MediaEncryptedEvent.idl \
    $(WebCore)/html/MediaError.idl \
    $(WebCore)/html/RadioNodeList.idl \
    $(WebCore)/html/TextMetrics.idl \
    $(WebCore)/html/TimeRanges.idl \
    $(WebCore)/html/URLSearchParams.idl \
    $(WebCore)/html/ValidityState.idl \
    $(WebCore)/html/VoidCallback.idl \
    $(WebCore)/html/WebKitMediaKeyError.idl \
    $(WebCore)/html/canvas/ANGLEInstancedArrays.idl \
    $(WebCore)/html/canvas/CanvasGradient.idl \
    $(WebCore)/html/canvas/CanvasPath.idl \
    $(WebCore)/html/canvas/CanvasPattern.idl \
    $(WebCore)/html/canvas/CanvasProxy.idl \
    $(WebCore)/html/canvas/CanvasRenderingContext2D.idl \
    $(WebCore)/html/canvas/DOMPath.idl \
    $(WebCore)/html/canvas/EXTBlendMinMax.idl \
    $(WebCore)/html/canvas/EXTFragDepth.idl \
    $(WebCore)/html/canvas/EXTShaderTextureLOD.idl \
    $(WebCore)/html/canvas/EXTTextureFilterAnisotropic.idl \
    $(WebCore)/html/canvas/EXTsRGB.idl \
    $(WebCore)/html/canvas/OESElementIndexUint.idl \
    $(WebCore)/html/canvas/OESStandardDerivatives.idl \
    $(WebCore)/html/canvas/OESTextureFloat.idl \
    $(WebCore)/html/canvas/OESTextureFloatLinear.idl \
    $(WebCore)/html/canvas/OESTextureHalfFloat.idl \
    $(WebCore)/html/canvas/OESTextureHalfFloatLinear.idl \
    $(WebCore)/html/canvas/OESVertexArrayObject.idl \
    $(WebCore)/html/canvas/WebGL2RenderingContext.idl \
    $(WebCore)/html/canvas/WebGLActiveInfo.idl \
    $(WebCore)/html/canvas/WebGLBuffer.idl \
    $(WebCore)/html/canvas/WebGLCompressedTextureATC.idl \
    $(WebCore)/html/canvas/WebGLCompressedTexturePVRTC.idl \
    $(WebCore)/html/canvas/WebGLCompressedTextureS3TC.idl \
    $(WebCore)/html/canvas/WebGLContextAttributes.idl \
    $(WebCore)/html/canvas/WebGLContextEvent.idl \
    $(WebCore)/html/canvas/WebGLDebugRendererInfo.idl \
    $(WebCore)/html/canvas/WebGLDebugShaders.idl \
    $(WebCore)/html/canvas/WebGLDepthTexture.idl \
    $(WebCore)/html/canvas/WebGLDrawBuffers.idl \
    $(WebCore)/html/canvas/WebGLFramebuffer.idl \
    $(WebCore)/html/canvas/WebGLLoseContext.idl \
    $(WebCore)/html/canvas/WebGLProgram.idl \
    $(WebCore)/html/canvas/WebGLQuery.idl \
    $(WebCore)/html/canvas/WebGLRenderbuffer.idl \
    $(WebCore)/html/canvas/WebGLRenderingContext.idl \
    $(WebCore)/html/canvas/WebGLRenderingContextBase.idl \
    $(WebCore)/html/canvas/WebGLSampler.idl \
    $(WebCore)/html/canvas/WebGLShader.idl \
    $(WebCore)/html/canvas/WebGLShaderPrecisionFormat.idl \
    $(WebCore)/html/canvas/WebGLSync.idl \
    $(WebCore)/html/canvas/WebGLTexture.idl \
    $(WebCore)/html/canvas/WebGLTransformFeedback.idl \
    $(WebCore)/html/canvas/WebGLUniformLocation.idl \
    $(WebCore)/html/canvas/WebGLVertexArrayObject.idl \
    $(WebCore)/html/canvas/WebGLVertexArrayObjectOES.idl \
    $(WebCore)/html/track/AudioTrack.idl \
    $(WebCore)/html/track/AudioTrackList.idl \
    $(WebCore)/html/track/DataCue.idl \
    $(WebCore)/html/track/TextTrack.idl \
    $(WebCore)/html/track/TextTrackCue.idl \
    $(WebCore)/html/track/TextTrackCueList.idl \
    $(WebCore)/html/track/TextTrackList.idl \
    $(WebCore)/html/track/TrackEvent.idl \
    $(WebCore)/html/track/VTTCue.idl \
    $(WebCore)/html/track/VTTRegion.idl \
    $(WebCore)/html/track/VTTRegionList.idl \
    $(WebCore)/html/track/VideoTrack.idl \
    $(WebCore)/html/track/VideoTrackList.idl \
    $(WebCore)/inspector/CommandLineAPIHost.idl \
    $(WebCore)/inspector/InspectorFrontendHost.idl \
    $(WebCore)/loader/appcache/DOMApplicationCache.idl \
    $(WebCore)/page/BarProp.idl \
    $(WebCore)/page/Crypto.idl \
    $(WebCore)/page/DOMSelection.idl \
    $(WebCore)/page/DOMWindow.idl \
    $(WebCore)/page/EventSource.idl \
    $(WebCore)/page/GlobalCrypto.idl \
    $(WebCore)/page/GlobalPerformance.idl \
    $(WebCore)/page/History.idl \
    $(WebCore)/page/IntersectionObserver.idl \
    $(WebCore)/page/IntersectionObserverCallback.idl \
    $(WebCore)/page/IntersectionObserverEntry.idl \
    $(WebCore)/page/Location.idl \
    $(WebCore)/page/Navigator.idl \
    $(WebCore)/page/NavigatorConcurrentHardware.idl \
    $(WebCore)/page/NavigatorID.idl \
    $(WebCore)/page/NavigatorLanguage.idl \
    $(WebCore)/page/NavigatorOnLine.idl \
    $(WebCore)/page/Performance.idl \
    $(WebCore)/page/PerformanceEntry.idl \
    $(WebCore)/page/PerformanceMark.idl \
    $(WebCore)/page/PerformanceMeasure.idl \
    $(WebCore)/page/PerformanceNavigation.idl \
    $(WebCore)/page/PerformanceObserver.idl \
    $(WebCore)/page/PerformanceObserverCallback.idl \
    $(WebCore)/page/PerformanceObserverEntryList.idl \
    $(WebCore)/page/PerformanceResourceTiming.idl \
    $(WebCore)/page/PerformanceTiming.idl \
    $(WebCore)/page/Screen.idl \
    $(WebCore)/page/ScrollToOptions.idl \
    $(WebCore)/page/UserMessageHandler.idl \
    $(WebCore)/page/UserMessageHandlersNamespace.idl \
    $(WebCore)/page/WebKitNamespace.idl \
    $(WebCore)/page/WebKitPoint.idl \
    $(WebCore)/page/WindowEventHandlers.idl \
    $(WebCore)/page/WindowOrWorkerGlobalScope.idl \
    $(WebCore)/page/WorkerNavigator.idl \
    $(WebCore)/plugins/DOMMimeType.idl \
    $(WebCore)/plugins/DOMMimeTypeArray.idl \
    $(WebCore)/plugins/DOMPlugin.idl \
    $(WebCore)/plugins/DOMPluginArray.idl \
    $(WebCore)/storage/Storage.idl \
    $(WebCore)/storage/StorageEvent.idl \
    $(WebCore)/svg/SVGAElement.idl \
    $(WebCore)/svg/SVGAltGlyphDefElement.idl \
    $(WebCore)/svg/SVGAltGlyphElement.idl \
    $(WebCore)/svg/SVGAltGlyphItemElement.idl \
    $(WebCore)/svg/SVGAngle.idl \
    $(WebCore)/svg/SVGAnimateColorElement.idl \
    $(WebCore)/svg/SVGAnimateElement.idl \
    $(WebCore)/svg/SVGAnimateMotionElement.idl \
    $(WebCore)/svg/SVGAnimateTransformElement.idl \
    $(WebCore)/svg/SVGAnimatedAngle.idl \
    $(WebCore)/svg/SVGAnimatedBoolean.idl \
    $(WebCore)/svg/SVGAnimatedEnumeration.idl \
    $(WebCore)/svg/SVGAnimatedInteger.idl \
    $(WebCore)/svg/SVGAnimatedLength.idl \
    $(WebCore)/svg/SVGAnimatedLengthList.idl \
    $(WebCore)/svg/SVGAnimatedNumber.idl \
    $(WebCore)/svg/SVGAnimatedNumberList.idl \
    $(WebCore)/svg/SVGAnimatedPreserveAspectRatio.idl \
    $(WebCore)/svg/SVGAnimatedRect.idl \
    $(WebCore)/svg/SVGAnimatedString.idl \
    $(WebCore)/svg/SVGAnimatedTransformList.idl \
    $(WebCore)/svg/SVGAnimationElement.idl \
    $(WebCore)/svg/SVGCircleElement.idl \
    $(WebCore)/svg/SVGClipPathElement.idl \
    $(WebCore)/svg/SVGComponentTransferFunctionElement.idl \
    $(WebCore)/svg/SVGCursorElement.idl \
    $(WebCore)/svg/SVGDefsElement.idl \
    $(WebCore)/svg/SVGDescElement.idl \
    $(WebCore)/svg/SVGDocument.idl \
    $(WebCore)/svg/SVGElement.idl \
    $(WebCore)/svg/SVGEllipseElement.idl \
    $(WebCore)/svg/SVGException.idl \
    $(WebCore)/svg/SVGExternalResourcesRequired.idl \
    $(WebCore)/svg/SVGFEBlendElement.idl \
    $(WebCore)/svg/SVGFEColorMatrixElement.idl \
    $(WebCore)/svg/SVGFEComponentTransferElement.idl \
    $(WebCore)/svg/SVGFECompositeElement.idl \
    $(WebCore)/svg/SVGFEConvolveMatrixElement.idl \
    $(WebCore)/svg/SVGFEDiffuseLightingElement.idl \
    $(WebCore)/svg/SVGFEDisplacementMapElement.idl \
    $(WebCore)/svg/SVGFEDistantLightElement.idl \
    $(WebCore)/svg/SVGFEDropShadowElement.idl \
    $(WebCore)/svg/SVGFEFloodElement.idl \
    $(WebCore)/svg/SVGFEFuncAElement.idl \
    $(WebCore)/svg/SVGFEFuncBElement.idl \
    $(WebCore)/svg/SVGFEFuncGElement.idl \
    $(WebCore)/svg/SVGFEFuncRElement.idl \
    $(WebCore)/svg/SVGFEGaussianBlurElement.idl \
    $(WebCore)/svg/SVGFEImageElement.idl \
    $(WebCore)/svg/SVGFEMergeElement.idl \
    $(WebCore)/svg/SVGFEMergeNodeElement.idl \
    $(WebCore)/svg/SVGFEMorphologyElement.idl \
    $(WebCore)/svg/SVGFEOffsetElement.idl \
    $(WebCore)/svg/SVGFEPointLightElement.idl \
    $(WebCore)/svg/SVGFESpecularLightingElement.idl \
    $(WebCore)/svg/SVGFESpotLightElement.idl \
    $(WebCore)/svg/SVGFETileElement.idl \
    $(WebCore)/svg/SVGFETurbulenceElement.idl \
    $(WebCore)/svg/SVGFilterElement.idl \
    $(WebCore)/svg/SVGFilterPrimitiveStandardAttributes.idl \
    $(WebCore)/svg/SVGFitToViewBox.idl \
    $(WebCore)/svg/SVGFontElement.idl \
    $(WebCore)/svg/SVGFontFaceElement.idl \
    $(WebCore)/svg/SVGFontFaceFormatElement.idl \
    $(WebCore)/svg/SVGFontFaceNameElement.idl \
    $(WebCore)/svg/SVGFontFaceSrcElement.idl \
    $(WebCore)/svg/SVGFontFaceUriElement.idl \
    $(WebCore)/svg/SVGForeignObjectElement.idl \
    $(WebCore)/svg/SVGGElement.idl \
    $(WebCore)/svg/SVGGlyphElement.idl \
    $(WebCore)/svg/SVGGlyphRefElement.idl \
    $(WebCore)/svg/SVGGradientElement.idl \
    $(WebCore)/svg/SVGGraphicsElement.idl \
    $(WebCore)/svg/SVGHKernElement.idl \
    $(WebCore)/svg/SVGImageElement.idl \
    $(WebCore)/svg/SVGLength.idl \
    $(WebCore)/svg/SVGLengthList.idl \
    $(WebCore)/svg/SVGLineElement.idl \
    $(WebCore)/svg/SVGLinearGradientElement.idl \
    $(WebCore)/svg/SVGMPathElement.idl \
    $(WebCore)/svg/SVGMarkerElement.idl \
    $(WebCore)/svg/SVGMaskElement.idl \
    $(WebCore)/svg/SVGMatrix.idl \
    $(WebCore)/svg/SVGMetadataElement.idl \
    $(WebCore)/svg/SVGMissingGlyphElement.idl \
    $(WebCore)/svg/SVGNumber.idl \
    $(WebCore)/svg/SVGNumberList.idl \
    $(WebCore)/svg/SVGPathElement.idl \
    $(WebCore)/svg/SVGPathSeg.idl \
    $(WebCore)/svg/SVGPathSegArcAbs.idl \
    $(WebCore)/svg/SVGPathSegArcRel.idl \
    $(WebCore)/svg/SVGPathSegClosePath.idl \
    $(WebCore)/svg/SVGPathSegCurvetoCubicAbs.idl \
    $(WebCore)/svg/SVGPathSegCurvetoCubicRel.idl \
    $(WebCore)/svg/SVGPathSegCurvetoCubicSmoothAbs.idl \
    $(WebCore)/svg/SVGPathSegCurvetoCubicSmoothRel.idl \
    $(WebCore)/svg/SVGPathSegCurvetoQuadraticAbs.idl \
    $(WebCore)/svg/SVGPathSegCurvetoQuadraticRel.idl \
    $(WebCore)/svg/SVGPathSegCurvetoQuadraticSmoothAbs.idl \
    $(WebCore)/svg/SVGPathSegCurvetoQuadraticSmoothRel.idl \
    $(WebCore)/svg/SVGPathSegLinetoAbs.idl \
    $(WebCore)/svg/SVGPathSegLinetoHorizontalAbs.idl \
    $(WebCore)/svg/SVGPathSegLinetoHorizontalRel.idl \
    $(WebCore)/svg/SVGPathSegLinetoRel.idl \
    $(WebCore)/svg/SVGPathSegLinetoVerticalAbs.idl \
    $(WebCore)/svg/SVGPathSegLinetoVerticalRel.idl \
    $(WebCore)/svg/SVGPathSegList.idl \
    $(WebCore)/svg/SVGPathSegMovetoAbs.idl \
    $(WebCore)/svg/SVGPathSegMovetoRel.idl \
    $(WebCore)/svg/SVGPatternElement.idl \
    $(WebCore)/svg/SVGPoint.idl \
    $(WebCore)/svg/SVGPointList.idl \
    $(WebCore)/svg/SVGPolygonElement.idl \
    $(WebCore)/svg/SVGPolylineElement.idl \
    $(WebCore)/svg/SVGPreserveAspectRatio.idl \
    $(WebCore)/svg/SVGRadialGradientElement.idl \
    $(WebCore)/svg/SVGRect.idl \
    $(WebCore)/svg/SVGRectElement.idl \
    $(WebCore)/svg/SVGRenderingIntent.idl \
    $(WebCore)/svg/SVGSVGElement.idl \
    $(WebCore)/svg/SVGScriptElement.idl \
    $(WebCore)/svg/SVGSetElement.idl \
    $(WebCore)/svg/SVGStopElement.idl \
    $(WebCore)/svg/SVGStringList.idl \
    $(WebCore)/svg/SVGStyleElement.idl \
    $(WebCore)/svg/SVGSwitchElement.idl \
    $(WebCore)/svg/SVGSymbolElement.idl \
    $(WebCore)/svg/SVGTRefElement.idl \
    $(WebCore)/svg/SVGTSpanElement.idl \
    $(WebCore)/svg/SVGTests.idl \
    $(WebCore)/svg/SVGTextContentElement.idl \
    $(WebCore)/svg/SVGTextElement.idl \
    $(WebCore)/svg/SVGTextPathElement.idl \
    $(WebCore)/svg/SVGTextPositioningElement.idl \
    $(WebCore)/svg/SVGTitleElement.idl \
    $(WebCore)/svg/SVGTransform.idl \
    $(WebCore)/svg/SVGTransformList.idl \
    $(WebCore)/svg/SVGURIReference.idl \
    $(WebCore)/svg/SVGUnitTypes.idl \
    $(WebCore)/svg/SVGUseElement.idl \
    $(WebCore)/svg/SVGVKernElement.idl \
    $(WebCore)/svg/SVGViewElement.idl \
    $(WebCore)/svg/SVGViewSpec.idl \
    $(WebCore)/svg/SVGZoomAndPan.idl \
    $(WebCore)/svg/SVGZoomEvent.idl \
    $(WebCore)/testing/GCObservation.idl \
    $(WebCore)/testing/InternalSettings.idl \
    $(WebCore)/testing/Internals.idl \
    $(WebCore)/testing/MallocStatistics.idl \
    $(WebCore)/testing/MemoryInfo.idl \
    $(WebCore)/testing/MockCDMFactory.idl \
    $(WebCore)/testing/MockContentFilterSettings.idl \
    $(WebCore)/testing/MockPageOverlay.idl \
    $(WebCore)/testing/TypeConversions.idl \
    $(WebCore)/workers/AbstractWorker.idl \
    $(WebCore)/workers/DedicatedWorkerGlobalScope.idl \
    $(WebCore)/workers/Worker.idl \
    $(WebCore)/workers/WorkerGlobalScope.idl \
    $(WebCore)/workers/WorkerLocation.idl \
    $(WebCore)/xml/DOMParser.idl \
    $(WebCore)/xml/XMLHttpRequest.idl \
    $(WebCore)/xml/XMLHttpRequestEventTarget.idl \
    $(WebCore)/xml/XMLHttpRequestProgressEvent.idl \
    $(WebCore)/xml/XMLHttpRequestUpload.idl \
    $(WebCore)/xml/XMLSerializer.idl \
    $(WebCore)/xml/XPathEvaluator.idl \
    $(WebCore)/xml/XPathException.idl \
    $(WebCore)/xml/XPathExpression.idl \
    $(WebCore)/xml/XPathNSResolver.idl \
    $(WebCore)/xml/XPathResult.idl \
    $(WebCore)/xml/XSLTProcessor.idl \
    InternalSettingsGenerated.idl \
#

PYTHON = python
PERL = perl
RUBY = ruby

ifeq ($(OS),Windows_NT)
    DELETE = cmd //C del
else
    DELETE = rm -f
endif
# --------

ifeq ($(OS),MACOS)

FRAMEWORK_FLAGS = $(shell echo $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_SEARCH_PATHS) | perl -e 'print "-F " . join(" -F ", split(" ", <>));')
HEADER_FLAGS = $(shell echo $(BUILT_PRODUCTS_DIR) $(HEADER_SEARCH_PATHS) | perl -e 'print "-I" . join(" -I", split(" ", <>));')
TEXT_PREPROCESSOR_FLAGS=-E -P -x c -traditional

ifneq ($(SDKROOT),)
    SDK_FLAGS=-isysroot $(SDKROOT)
endif

ifeq ($(shell $(CC) -std=gnu++11 -x c++ -E -P -dM $(SDK_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) -include "wtf/Platform.h" /dev/null | grep ' WTF_PLATFORM_IOS ' | cut -d' ' -f3), 1)
    WTF_PLATFORM_IOS = 1
else
    WTF_PLATFORM_IOS = 0
endif

ifeq ($(shell $(CC) -std=gnu++11 -x c++ -E -P -dM $(SDK_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) -include "wtf/Platform.h" /dev/null | grep USE_APPLE_INTERNAL_SDK | cut -d' ' -f3), 1)
    USE_APPLE_INTERNAL_SDK = 1
else
    USE_APPLE_INTERNAL_SDK = 0
endif

ifeq ($(shell $(CC) -std=gnu++11 -x c++ -E -P -dM $(SDK_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) -include "wtf/Platform.h" /dev/null | grep ENABLE_ORIENTATION_EVENTS | cut -d' ' -f3), 1)
    ENABLE_ORIENTATION_EVENTS = 1
endif

ifeq ($(PLATFORM_FEATURE_DEFINES),)
ifeq ($(OS), Windows_NT)
PLATFORM_FEATURE_DEFINES = $(WEBKIT_LIBRARIES)/tools/vsprops/FeatureDefines.props
else
PLATFORM_FEATURE_DEFINES = Configurations/FeatureDefines.xcconfig
endif
endif

ADDITIONAL_BINDING_IDLS =
ifeq ($(findstring ENABLE_MAC_GESTURE_EVENTS,$(FEATURE_DEFINES)), ENABLE_MAC_GESTURE_EVENTS)
ADDITIONAL_BINDING_IDLS += GestureEvent.idl
endif

ifeq ($(findstring ENABLE_IOS_GESTURE_EVENTS,$(FEATURE_DEFINES)), ENABLE_IOS_GESTURE_EVENTS)
ADDITIONAL_BINDING_IDLS += GestureEvent.idl
endif

ifeq ($(WTF_PLATFORM_IOS), 1)
ifeq ($(findstring ENABLE_IOS_TOUCH_EVENTS,$(FEATURE_DEFINES)), ENABLE_IOS_TOUCH_EVENTS)
ADDITIONAL_BINDING_IDLS += \
    Touch.idl \
    TouchEvent.idl \
    TouchList.idl
endif
endif # IOS

vpath %.in $(WEBKITADDITIONS_HEADER_SEARCH_PATHS)

ADDITIONAL_EVENT_NAMES =
ADDITIONAL_EVENT_TARGET_FACTORY =

JS_BINDING_IDLS += $(ADDITIONAL_BINDING_IDLS)

all : $(ADDITIONAL_BINDING_IDLS:%.idl=JS%.h)

vpath %.idl $(BUILT_PRODUCTS_DIR)/usr/local/include $(SDKROOT)/usr/local/include

$(ADDITIONAL_BINDING_IDLS) : % : WebKitAdditions/%
	cp $< .

endif # MACOS

ifneq ($(WTF_PLATFORM_IOS), 1)
JS_BINDING_IDLS += \
    $(WebCore)/dom/Touch.idl \
    $(WebCore)/dom/TouchEvent.idl \
    $(WebCore)/dom/TouchList.idl
endif

.PHONY : all

JS_DOM_CLASSES=$(basename $(notdir $(JS_BINDING_IDLS)))

JS_DOM_HEADERS=$(filter-out JSEventListener.h, $(JS_DOM_CLASSES:%=JS%.h))

WEB_DOM_HEADERS :=

all : \
    $(SUPPLEMENTAL_DEPENDENCY_FILE) \
    $(WINDOW_CONSTRUCTORS_FILE) \
    $(WORKERGLOBALSCOPE_CONSTRUCTORS_FILE) \
    $(JS_DOM_HEADERS) \
    $(WEB_DOM_HEADERS) \
    \
    CSSPropertyNames.cpp \
    CSSPropertyNames.h \
    CSSValueKeywords.cpp \
    CSSValueKeywords.h \
    ColorData.cpp \
    DOMJITAbstractHeapRepository.h \
    EventInterfaces.h \
    EventTargetInterfaces.h \
    ExceptionCodeDescription.cpp \
    HTMLElementFactory.cpp \
    HTMLElementFactory.h \
    HTMLElementTypeHelpers.h \
    HTMLEntityTable.cpp \
    HTMLNames.cpp \
    HTMLNames.h \
    HTTPHeaderNames.h \
    JSHTMLElementWrapperFactory.cpp \
    JSHTMLElementWrapperFactory.h \
    JSMathMLElementWrapperFactory.cpp \
    JSMathMLElementWrapperFactory.h \
    JSSVGElementWrapperFactory.cpp \
    JSSVGElementWrapperFactory.h \
    PlugInsResources.h \
    SVGElementFactory.cpp \
    SVGElementFactory.h \
    SVGElementTypeHelpers.h \
    SVGNames.cpp \
    SVGNames.h \
    SelectorPseudoClassAndCompatibilityElementMap.cpp \
    SelectorPseudoElementTypeMap.cpp \
    StyleBuilder.cpp \
    StylePropertyShorthandFunctions.cpp \
    StylePropertyShorthandFunctions.h \
    UserAgentStyleSheets.h \
    WebKitFontFamilyNames.cpp \
    WebKitFontFamilyNames.h \
    XLinkNames.cpp \
    XMLNSNames.cpp \
    XMLNames.cpp \
    MathMLElementFactory.cpp \
    MathMLElementFactory.h \
    MathMLElementTypeHelpers.h \
    MathMLNames.cpp \
    MathMLNames.h \
#

# --------

ADDITIONAL_IDL_DEFINES :=

ifndef ENABLE_ORIENTATION_EVENTS
    ENABLE_ORIENTATION_EVENTS = 0
endif

ifeq ($(ENABLE_ORIENTATION_EVENTS), 1)
    ADDITIONAL_IDL_DEFINES := $(ADDITIONAL_IDL_DEFINES) ENABLE_ORIENTATION_EVENTS
endif

ifeq ($(USE_APPLE_INTERNAL_SDK), 1)
    ADDITIONAL_IDL_DEFINES := $(ADDITIONAL_IDL_DEFINES) USE_APPLE_INTERNAL_SDK
endif

# --------

ADDITIONAL_CSS_PROPERTIES_DEFINES :=

ifeq ($(WTF_PLATFORM_IOS), 1)
    ADDITIONAL_CSS_PROPERTIES_DEFINES := WTF_PLATFORM_IOS
endif

# --------

# CSS property names and value keywords

WEBCORE_CSS_PROPERTY_NAMES := $(WebCore)/css/CSSProperties.json
WEBCORE_CSS_VALUE_KEYWORDS := $(WebCore)/css/CSSValueKeywords.in
WEBCORE_CSS_VALUE_KEYWORDS := $(WEBCORE_CSS_VALUE_KEYWORDS) $(WebCore)/css/SVGCSSValueKeywords.in
WEBCORE_CSS_VALUE_KEYWORDS_DEFINES := $(FEATURE_DEFINES) $(ADDITIONAL_CSS_VALUE_KEYWORDS_DEFINES)
WEBCORE_CSS_PROPERTIES_DEFINES := $(FEATURE_DEFINES) $(ADDITIONAL_CSS_PROPERTIES_DEFINES)

all : CSSPropertyNames.h CSSPropertyNames.cpp StyleBuilder.cpp StylePropertyShorthandFunctions.h StylePropertyShorthandFunctions.cpp
CSSPropertyNames%h CSSPropertyNames%cpp StyleBuilder%cpp StylePropertyShorthandFunctions%h StylePropertyShorthandFunctions%cpp : $(WEBCORE_CSS_PROPERTY_NAMES) css/makeprop.pl $(PLATFORM_FEATURE_DEFINES)
	$(PERL) -pe '' $(WEBCORE_CSS_PROPERTY_NAMES) > CSSProperties.json
	$(PERL) "$(WebCore)/css/makeprop.pl" --defines "$(WEBCORE_CSS_PROPERTIES_DEFINES)"

all : CSSValueKeywords.h CSSValueKeywords.cpp
CSSValueKeywords%h CSSValueKeywords%cpp : $(WEBCORE_CSS_VALUE_KEYWORDS) css/makevalues.pl bindings/scripts/preprocessor.pm $(PLATFORM_FEATURE_DEFINES)
	$(PERL) -pe '' $(WEBCORE_CSS_VALUE_KEYWORDS) > CSSValueKeywords.in
	$(PERL) "$(WebCore)/css/makevalues.pl" --defines "$(WEBCORE_CSS_VALUE_KEYWORDS_DEFINES)"

# --------

# CSS Selector pseudo type name to value map.

SelectorPseudoClassAndCompatibilityElementMap.cpp : $(WebCore)/css/makeSelectorPseudoClassAndCompatibilityElementMap.py $(WebCore)/css/SelectorPseudoClassAndCompatibilityElementMap.in
	$(PYTHON) "$(WebCore)/css/makeSelectorPseudoClassAndCompatibilityElementMap.py" $(WebCore)/css/SelectorPseudoClassAndCompatibilityElementMap.in gperf "$(FEATURE_DEFINES)"

SelectorPseudoElementTypeMap.cpp : $(WebCore)/css/makeSelectorPseudoElementsMap.py $(WebCore)/css/SelectorPseudoElementTypeMap.in
	$(PYTHON) "$(WebCore)/css/makeSelectorPseudoElementsMap.py" $(WebCore)/css/SelectorPseudoElementTypeMap.in gperf "$(FEATURE_DEFINES)"

# --------

# DOMJIT Abstract Heap

all : DOMJITAbstractHeapRepository.h

DOMJITAbstractHeapRepository.h : $(WebCore)/domjit/generate-abstract-heap.rb $(WebCore)/domjit/DOMJITAbstractHeapRepository.yaml
	$(RUBY) "$(WebCore)/domjit/generate-abstract-heap.rb" $(WebCore)/domjit/DOMJITAbstractHeapRepository.yaml ./DOMJITAbstractHeapRepository.h

# --------

# XMLViewer CSS

all : XMLViewerCSS.h

XMLViewerCSS.h : xml/XMLViewer.css
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/cssmin.py <"$(WebCore)/xml/XMLViewer.css" > ./XMLViewer.min.css
	$(PERL) $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl XMLViewer_css ./XMLViewer.min.css XMLViewerCSS.h
	$(DELETE) XMLViewer.min.css

# --------

# XMLViewer JS

all : XMLViewerJS.h

XMLViewerJS.h : xml/XMLViewer.js
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/jsmin.py <"$(WebCore)/xml/XMLViewer.js" > ./XMLViewer.min.js
	$(PERL) $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl XMLViewer_js ./XMLViewer.min.js XMLViewerJS.h
	$(DELETE) XMLViewer.min.js

# --------

# HTML entity names

HTMLEntityTable.cpp : html/parser/HTMLEntityNames.in $(WebCore)/html/parser/create-html-entity-table
	$(PYTHON) $(WebCore)/html/parser/create-html-entity-table -o HTMLEntityTable.cpp $(WebCore)/html/parser/HTMLEntityNames.in

# --------

# HTTP header names

HTTPHeaderNames.h : platform/network/HTTPHeaderNames.in $(WebCore)/platform/network/create-http-header-name-table
	$(PYTHON) $(WebCore)/platform/network/create-http-header-name-table $(WebCore)/platform/network/HTTPHeaderNames.in gperf

# --------

# color names

ColorData.cpp : platform/ColorData.gperf $(WebCore)/make-hash-tools.pl
	$(PERL) $(WebCore)/make-hash-tools.pl . $(WebCore)/platform/ColorData.gperf

# --------

# WebRTC scripts

WEBCORE_SDP_PROCESSOR_SCRIPTS = 

ifeq ($(OS),MACOS)
    WEBCORE_SDP_PROCESSOR_SCRIPTS := $(WEBCORE_SDP_PROCESSOR_SCRIPTS) $(WebCore)/Modules/mediastream/sdp.js
endif

ifdef WEBCORE_SDP_PROCESSOR_SCRIPTS
all : SDPProcessorScriptsData.h

SDPProcessorScriptsData.h : Scripts/make-js-file-arrays.py $(WEBCORE_SDP_PROCESSOR_SCRIPTS)
	PYTHONPATH=$(JavaScriptCore_SCRIPTS_DIR) $(PYTHON) $< $@ SDPProcessorScriptsData.cpp $(WEBCORE_SDP_PROCESSOR_SCRIPTS)
endif

# --------

# user agent style sheets

USER_AGENT_STYLE_SHEETS = $(WebCore)/css/html.css $(WebCore)/css/quirks.css $(WebCore)/css/plugIns.css
USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/svg.css

ifeq ($(findstring ENABLE_MATHML,$(FEATURE_DEFINES)), ENABLE_MATHML)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/mathml.css
endif

ifeq ($(findstring ENABLE_VIDEO,$(FEATURE_DEFINES)), ENABLE_VIDEO)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/mediaControls.css
endif

ifeq ($(findstring ENABLE_FULLSCREEN_API,$(FEATURE_DEFINES)), ENABLE_FULLSCREEN_API)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/fullscreen.css
endif

ifeq ($(findstring ENABLE_SERVICE_CONTROLS,$(FEATURE_DEFINES)), ENABLE_SERVICE_CONTROLS)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/html/shadow/mac/imageControlsMac.css
endif

ifeq ($(OS),MACOS)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/Modules/plugins/QuickTimePluginReplacement.css
endif

ifeq ($(OS), Windows_NT)
    USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/css/themeWin.css $(WebCore)/css/themeWinQuirks.css
endif

ifeq ($(findstring ENABLE_METER_ELEMENT,$(FEATURE_DEFINES)), ENABLE_METER_ELEMENT)
	USER_AGENT_STYLE_SHEETS := $(USER_AGENT_STYLE_SHEETS) $(WebCore)/html/shadow/meterElementShadow.css
endif

UserAgentStyleSheets.h : css/make-css-file-arrays.pl bindings/scripts/preprocessor.pm $(USER_AGENT_STYLE_SHEETS) $(PLATFORM_FEATURE_DEFINES)
	$(PERL) $< --defines "$(FEATURE_DEFINES)" $@ UserAgentStyleSheetsData.cpp $(USER_AGENT_STYLE_SHEETS)

# --------

# user agent scripts

USER_AGENT_SCRIPTS = 

ifeq ($(OS),MACOS)
    USER_AGENT_SCRIPTS := $(USER_AGENT_SCRIPTS) $(WebCore)/Modules/plugins/QuickTimePluginReplacement.js
endif

ifdef USER_AGENT_SCRIPTS
all : UserAgentScripts.h

UserAgentScripts.h : Scripts/make-js-file-arrays.py $(USER_AGENT_SCRIPTS)
	PYTHONPATH=$(JavaScriptCore_SCRIPTS_DIR) $(PYTHON) $< $@ UserAgentScriptsData.cpp $(USER_AGENT_SCRIPTS)
endif

# --------

# plugIns resources

PLUG_INS_RESOURCES = $(WebCore)/Resources/plugIns.js

PlugInsResources.h : css/make-css-file-arrays.pl bindings/scripts/preprocessor.pm $(PLUG_INS_RESOURCES) $(PLATFORM_FEATURE_DEFINES)
	$(PERL) $< --defines "$(FEATURE_DEFINES)" $@ PlugInsResourcesData.cpp $(PLUG_INS_RESOURCES)

# --------

all : WebKitFontFamilyNames.cpp WebKitFontFamilyNames.h
WebKitFontFamilyNames%cpp WebKitFontFamilyNames%h: dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm css/WebKitFontFamilyNames.in
	$(PERL) $< --fonts $(WebCore)/css/WebKitFontFamilyNames.in

# HTML tag and attribute names

ifeq ($(findstring ENABLE_DATALIST_ELEMENT,$(FEATURE_DEFINES)), ENABLE_DATALIST_ELEMENT)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_DATALIST_ELEMENT=1
endif

ifeq ($(findstring ENABLE_METER_ELEMENT,$(FEATURE_DEFINES)), ENABLE_METER_ELEMENT)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_METER_ELEMENT=1
endif

ifeq ($(findstring ENABLE_VIDEO,$(FEATURE_DEFINES)), ENABLE_VIDEO)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_VIDEO=1
endif

ifeq ($(findstring ENABLE_VIDEO_TRACK,$(FEATURE_DEFINES)), ENABLE_VIDEO_TRACK)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_VIDEO_TRACK=0
endif

ifeq ($(findstring ENABLE_DATACUE_VALUE,$(FEATURE_DEFINES)), ENABLE_DATACUE_VALUE)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_DATACUE_VALUE=0
endif

ifeq ($(findstring ENABLE_MEDIA_STREAM,$(FEATURE_DEFINES)), ENABLE_MEDIA_STREAM)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_MEDIA_STREAM=1
endif

ifeq ($(findstring ENABLE_LEGACY_ENCRYPTED_MEDIA,$(FEATURE_DEFINES)), ENABLE_LEGACY_ENCRYPTED_MEDIA)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_LEGACY_ENCRYPTED_MEDIA=1
endif

ifeq ($(findstring ENABLE_ENCRYPTED_MEDIA,$(FEATURE_DEFINES)), ENABLE_ENCRYPTED_MEDIA)
    HTML_FLAGS := $(HTML_FLAGS) ENABLE_ENCRYPTED_MEDIA=1
endif

all : JSHTMLElementWrapperFactory.cpp JSHTMLElementWrapperFactory.h HTMLElementFactory.cpp HTMLElementFactory.h HTMLElementTypeHelpers.h HTMLNames.cpp HTMLNames.h

ifdef HTML_FLAGS

JSHTMLElementWrapperFactory%cpp JSHTMLElementWrapperFactory%h HTMLElementFactory%cpp HTMLElementFactory%h HTMLElementTypeHelpers%h HTMLNames%cpp HTMLNames%h : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm html/HTMLTagNames.in html/HTMLAttributeNames.in
	$(PERL) $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in --factory --wrapperFactory --extraDefines "$(HTML_FLAGS)"

else

JSHTMLElementWrapperFactory%cpp JSHTMLElementWrapperFactory%h HTMLElementFactory%cpp HTMLElementFactory%h HTMLElementTypeHelpers%h HTMLNames%cpp HTMLNames%h : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm html/HTMLTagNames.in html/HTMLAttributeNames.in
	$(PERL) $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in --factory --wrapperFactory

endif

XMLNSNames.cpp : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm xml/xmlnsattrs.in
	$(PERL) $< --attrs $(WebCore)/xml/xmlnsattrs.in

XMLNames.cpp : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm xml/xmlattrs.in
	$(PERL) $< --attrs $(WebCore)/xml/xmlattrs.in

# --------

# SVG tag and attribute names, and element factory

ifeq ($(findstring ENABLE_SVG_FONTS,$(FEATURE_DEFINES)), ENABLE_SVG_FONTS)
    SVG_FLAGS := $(SVG_FLAGS) ENABLE_SVG_FONTS=1
endif

# SVG tag and attribute names (need to pass an extra flag if svg experimental features are enabled)

all : JSSVGElementWrapperFactory.cpp JSSVGElementWrapperFactory.h SVGElementFactory.cpp SVGElementFactory.h SVGElementTypeHelpers.h SVGNames.cpp SVGNames.h

ifdef SVG_FLAGS
JSSVGElementWrapperFactory%cpp JSSVGElementWrapperFactory%h SVGElementFactory%cpp SVGElementFactory%h SVGElementTypeHelpers%h SVGNames%cpp SVGNames%h : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm svg/svgtags.in svg/svgattrs.in
	$(PERL) $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --extraDefines "$(SVG_FLAGS)" --factory --wrapperFactory
else

JSSVGElementWrapperFactory%cpp JSSVGElementWrapperFactory%h SVGElementFactory%cpp SVGElementFactory%h SVGElementTypeHelpers%h SVGNames%cpp SVGNames%h : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm svg/svgtags.in svg/svgattrs.in
	$(PERL) $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --factory --wrapperFactory

endif

XLinkNames.cpp : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm svg/xlinkattrs.in
	$(PERL) $< --attrs $(WebCore)/svg/xlinkattrs.in

# --------

# Register event constructors and targets

EVENT_NAMES = EventNames.in $(ADDITIONAL_EVENT_NAMES)

all : EventFactory.cpp EventHeaders.h EventInterfaces.h
EventFactory%cpp EventHeaders%h EventInterfaces%h : dom/make_event_factory.pl $(EVENT_NAMES)
	$(PERL) $< $(addprefix --input , $(filter-out $(WebCore)/dom/make_event_factory.pl, $^))

EVENT_TARGET_FACTORY = EventTargetFactory.in $(ADDITIONAL_EVENT_TARGET_FACTORY)

all : EventTargetHeaders.h EventTargetInterfaces.h
EventTargetHeaders%h EventTargetInterfaces%h : dom/make_event_factory.pl $(EVENT_TARGET_FACTORY)
	$(PERL) $< $(addprefix --input , $(filter-out $(WebCore)/dom/make_event_factory.pl, $^))

all : ExceptionCodeDescription.cpp ExceptionCodeDescription.h ExceptionHeaders.h ExceptionInterfaces.h
ExceptionCodeDescription%cpp ExceptionCodeDescription%h ExceptionHeaders%h ExceptionInterfaces%h : dom/make_dom_exceptions.pl dom/DOMExceptions.in
	$(PERL) $< --input $(WebCore)/dom/DOMExceptions.in

# --------

# MathML tag and attribute names, and element factory

all : JSMathMLElementWrapperFactory.cpp JSMathMLElementWrapperFactory.h MathMLElementFactory.cpp MathMLElementFactory.h MathMLElementTypeHelpers.h MathMLNames.cpp MathMLNames.h
JSMathMLElementWrapperFactory%cpp JSMathMLElementWrapperFactory%h MathMLElementFactory%cpp MathMLElementFactory%h MathMLElementTypeHelpers%h MathMLNames%cpp MathMLNames%h : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm mathml/mathtags.in mathml/mathattrs.in
	$(PERL) $< --tags $(WebCore)/mathml/mathtags.in --attrs $(WebCore)/mathml/mathattrs.in --factory --wrapperFactory

# --------

# Internal Settings

all : InternalSettingsGenerated.idl InternalSettingsGenerated.cpp InternalSettingsGenerated.h SettingsMacros.h
InternalSettingsGenerated%idl InternalSettingsGenerated%cpp InternalSettingsGenerated%h SettingsMacros%h : page/make_settings.pl page/Settings.in
	$(PERL) $< --input $(WebCore)/page/Settings.in

# --------

# Common generator things

COMMON_BINDINGS_SCRIPTS = \
    bindings/scripts/CodeGenerator.pm \
    bindings/scripts/IDLParser.pm \
    bindings/scripts/generate-bindings.pl \
    bindings/scripts/preprocessor.pm

PREPROCESS_IDLS_SCRIPTS = \
    bindings/scripts/preprocess-idls.pl

# JS bindings generator

IDL_INCLUDES = \
    $(WebCore)/Modules \
    $(WebCore)/animation \
    $(WebCore)/css \
    $(WebCore)/crypto \
    $(WebCore)/dom \
    $(WebCore)/fileapi \
    $(WebCore)/html \
    $(WebCore)/html/canvas \
    $(WebCore)/html/shadow \
    $(WebCore)/html/track \
    $(WebCore)/inspector \
    $(WebCore)/loader/appcache \
    $(WebCore)/page \
    $(WebCore)/plugins \
    $(WebCore)/storage \
    $(WebCore)/svg \
    $(WebCore)/testing \
    $(WebCore)/workers \
    $(WebCore)/xml

IDL_COMMON_ARGS = $(IDL_INCLUDES:%=--include %) --write-dependencies --outputDir .

JS_BINDINGS_SCRIPTS = $(COMMON_BINDINGS_SCRIPTS) bindings/scripts/CodeGeneratorJS.pm

SUPPLEMENTAL_DEPENDENCY_FILE = ./SupplementalDependencies.txt
SUPPLEMENTAL_MAKEFILE_DEPS = ./SupplementalDependencies.dep
WINDOW_CONSTRUCTORS_FILE = ./DOMWindowConstructors.idl
WORKERGLOBALSCOPE_CONSTRUCTORS_FILE = ./WorkerGlobalScopeConstructors.idl
DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE = ./DedicatedWorkerGlobalScopeConstructors.idl
IDL_FILES_TMP = ./idl_files.tmp
IDL_ATTRIBUTES_FILE = $(WebCore)/bindings/scripts/IDLAttributes.txt

# The following lines get a newline character stored in a variable.
# See <http://stackoverflow.com/questions/7039811/how-do-i-process-extremely-long-lists-of-files-in-a-make-recipe>.
define NL


endef

$(SUPPLEMENTAL_MAKEFILE_DEPS) : $(PREPROCESS_IDLS_SCRIPTS) $(JS_BINDING_IDLS) $(PLATFORM_FEATURE_DEFINES) DerivedSources.make
	$(foreach f,$(JS_BINDING_IDLS),@echo $(f)>>$(IDL_FILES_TMP)$(NL))
	$(PERL) $(WebCore)/bindings/scripts/preprocess-idls.pl --defines "$(FEATURE_DEFINES) $(ADDITIONAL_IDL_DEFINES) LANGUAGE_JAVASCRIPT" --idlFilesList $(IDL_FILES_TMP) --supplementalDependencyFile $(SUPPLEMENTAL_DEPENDENCY_FILE) --windowConstructorsFile $(WINDOW_CONSTRUCTORS_FILE) --workerGlobalScopeConstructorsFile $(WORKERGLOBALSCOPE_CONSTRUCTORS_FILE) --dedicatedWorkerGlobalScopeConstructorsFile $(DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE) --supplementalMakefileDeps $@
	$(DELETE) $(IDL_FILES_TMP)

JS%.cpp JS%.h : %.idl $(JS_BINDINGS_SCRIPTS) $(IDL_ATTRIBUTES_FILE) $(WINDOW_CONSTRUCTORS_FILE) $(WORKERGLOBALSCOPE_CONSTRUCTORS_FILE) $(PLATFORM_FEATURE_DEFINES)
	$(PERL) $(WebCore)/bindings/scripts/generate-bindings.pl $(IDL_COMMON_ARGS) --defines "$(FEATURE_DEFINES) $(ADDITIONAL_IDL_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --idlAttributesFile $(IDL_ATTRIBUTES_FILE) --supplementalDependencyFile $(SUPPLEMENTAL_DEPENDENCY_FILE) $<

-include $(SUPPLEMENTAL_MAKEFILE_DEPS)

# Inspector interfaces

all : InspectorOverlayPage.h

InspectorOverlayPage.h : InspectorOverlayPage.html InspectorOverlayPage.css InspectorOverlayPage.js
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/inline-and-minify-stylesheets-and-scripts.py $(WebCore)/inspector/InspectorOverlayPage.html ./InspectorOverlayPage.combined.html
	$(PERL) $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl InspectorOverlayPage_html ./InspectorOverlayPage.combined.html InspectorOverlayPage.h
	$(DELETE) InspectorOverlayPage.combined.html

all : CommandLineAPIModuleSource.h

CommandLineAPIModuleSource.h : CommandLineAPIModuleSource.js
	echo "//# sourceURL=__InjectedScript_CommandLineAPIModuleSource.js" > ./CommandLineAPIModuleSource.min.js
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/jsmin.py <$(WebCore)/inspector/CommandLineAPIModuleSource.js >> ./CommandLineAPIModuleSource.min.js
	$(PERL) $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl CommandLineAPIModuleSource_js ./CommandLineAPIModuleSource.min.js CommandLineAPIModuleSource.h
	$(DELETE) CommandLineAPIModuleSource.min.js

# Web Replay inputs generator

INPUT_GENERATOR_SCRIPTS = \
    $(JavaScriptCore_SCRIPTS_DIR)/CodeGeneratorReplayInputs.py \
    $(JavaScriptCore_SCRIPTS_DIR)/CodeGeneratorReplayInputsTemplates.py \
#

INPUT_GENERATOR_SPECIFICATIONS = \
    $(WebCore)/replay/WebInputs.json \
    $(JavaScriptCore_SCRIPTS_DIR)/JSInputs.json \
#

all : WebReplayInputs.h

WebReplayInputs.h : $(INPUT_GENERATOR_SPECIFICATIONS) $(INPUT_GENERATOR_SCRIPTS)
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/CodeGeneratorReplayInputs.py --outputDir . --framework WebCore $(INPUT_GENERATOR_SPECIFICATIONS)

-include $(JS_DOM_HEADERS:.h=.dep)

# WebCore JS Builtins

WebCore_BUILTINS_SOURCES = \
    ${WebCore}/Modules/fetch/DOMWindowFetch.js \
    $(WebCore)/Modules/fetch/FetchHeaders.js \
    $(WebCore)/Modules/fetch/FetchInternals.js \
    $(WebCore)/Modules/fetch/FetchRequest.js \
    $(WebCore)/Modules/fetch/FetchResponse.js \
    ${WebCore}/Modules/fetch/WorkerGlobalScopeFetch.js \
    $(WebCore)/Modules/mediastream/NavigatorUserMedia.js \
    $(WebCore)/Modules/mediastream/RTCPeerConnection.js \
    $(WebCore)/Modules/mediastream/RTCPeerConnectionInternals.js \
    $(WebCore)/Modules/streams/ByteLengthQueuingStrategy.js \
    $(WebCore)/Modules/streams/CountQueuingStrategy.js \
    $(WebCore)/Modules/streams/ReadableByteStreamController.js \
    $(WebCore)/Modules/streams/ReadableByteStreamInternals.js \
    $(WebCore)/Modules/streams/ReadableStream.js \
    $(WebCore)/Modules/streams/ReadableStreamDefaultController.js \
    $(WebCore)/Modules/streams/ReadableStreamInternals.js \
    $(WebCore)/Modules/streams/ReadableStreamDefaultReader.js \
    $(WebCore)/Modules/streams/StreamInternals.js \
    $(WebCore)/Modules/streams/WritableStream.js \
    $(WebCore)/Modules/streams/WritableStreamInternals.js \
    $(WebCore)/xml/XMLHttpRequest.js \
#

BUILTINS_GENERATOR_SCRIPTS = \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generator.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_model.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_templates.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_combined_header.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_combined_implementation.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_separate_header.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_separate_implementation.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_internals_wrapper_header.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_internals_wrapper_implementation.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_wrapper_header.py \
    $(JavaScriptCore_SCRIPTS_DIR)/builtins_generate_wrapper_implementation.py \
    $(JavaScriptCore_SCRIPTS_DIR)/generate-js-builtins.py \
    $(JavaScriptCore_SCRIPTS_DIR)/lazywriter.py \
#

WebCore_BUILTINS_WRAPPERS = \
    WebCoreJSBuiltins.h \
    WebCoreJSBuiltins.cpp \
    WebCoreJSBuiltinInternals.h \
    WebCoreJSBuiltinInternals.cpp \
#

# Adding/removing scripts should trigger regeneration, but changing which builtins are
# generated should not affect other builtins when not passing '--combined' to the generator.

WebCore_BUILTINS_SOURCES_LIST : $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py DerivedSources.make
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py '$(WebCore_BUILTINS_SOURCES)' $@

WebCore_BUILTINS_DEPENDENCIES_LIST : $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py DerivedSources.make
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py '$(BUILTINS_GENERATOR_SCRIPTS)' $@

$(firstword $(WebCore_BUILTINS_WRAPPERS)): $(WebCore_BUILTINS_SOURCES) WebCore_BUILTINS_SOURCES_LIST $(BUILTINS_GENERATOR_SCRIPTS) WebCore_BUILTINS_DEPENDENCIES_LIST
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/generate-js-builtins.py --wrappers-only --output-directory . --framework WebCore $(WebCore_BUILTINS_SOURCES)

%Builtins.h: %.js $(BUILTINS_GENERATOR_SCRIPTS) WebCore_BUILTINS_DEPENDENCIES_LIST
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/generate-js-builtins.py --output-directory . --framework WebCore $<

all : $(notdir $(WebCore_BUILTINS_SOURCES:%.js=%Builtins.h)) $(firstword $(WebCore_BUILTINS_WRAPPERS))

# ------------------------

# Mac-specific rules

ifeq ($(OS),MACOS)

all : CharsetData.cpp

# --------

# character set name table

ifeq ($(WTF_PLATFORM_IOS),1)
ENCODINGS_FILENAME := ios-encodings.txt
else
ENCODINGS_FILENAME := mac-encodings.txt
endif # WTF_PLATFORM_IOS

CharsetData.cpp : platform/text/mac/make-charset-table.pl platform/text/mac/character-sets.txt platform/text/mac/$(ENCODINGS_FILENAME)
	$(PERL) $^ kTextEncoding > $@

# --------

endif # MACOS

# ------------------------

# Header detection

ifeq ($(OS),Windows_NT)

all : WebCoreHeaderDetection.h

WebCoreHeaderDetection.h : $(WebCore)/AVFoundationSupport.py DerivedSources.make
	$(PYTHON) $(WebCore)/AVFoundationSupport.py $(WEBKIT_LIBRARIES) > $@

endif # Windows_NT
