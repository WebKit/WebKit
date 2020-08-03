# Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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

PYTHON = python
PERL = perl
RUBY = ruby
DELETE = rm -f

ifneq ($(SDKROOT),)
    SDK_FLAGS = -isysroot $(SDKROOT)
endif

ifeq ($(USE_LLVM_TARGET_TRIPLES_FOR_CLANG),YES)
    WK_CURRENT_ARCH = $(word 1, $(ARCHS))
    TARGET_TRIPLE_FLAGS = -target $(WK_CURRENT_ARCH)-$(LLVM_TARGET_TRIPLE_VENDOR)-$(LLVM_TARGET_TRIPLE_OS_VERSION)$(LLVM_TARGET_TRIPLE_SUFFIX)
endif

FRAMEWORK_FLAGS := $(shell echo $(BUILT_PRODUCTS_DIR) $(FRAMEWORK_SEARCH_PATHS) $(SYSTEM_FRAMEWORK_SEARCH_PATHS) | $(PERL) -e 'print "-F " . join(" -F ", split(" ", <>));')
HEADER_FLAGS := $(shell echo $(BUILT_PRODUCTS_DIR) $(HEADER_SEARCH_PATHS) $(SYSTEM_HEADER_SEARCH_PATHS) | $(PERL) -e 'print "-I" . join(" -I", split(" ", <>));')
FEATURE_AND_PLATFORM_DEFINES := $(shell $(CC) -std=gnu++1z -x c++ -E -P -dM $(SDK_FLAGS) $(TARGET_TRIPLE_FLAGS) $(FRAMEWORK_FLAGS) $(HEADER_FLAGS) -include "wtf/Platform.h" /dev/null | $(PERL) -ne "print if s/\#define ((HAVE_|USE_|ENABLE_|WTF_PLATFORM_)\w+) 1/\1/")

# FIXME: This should list Platform.h and all the things it includes. Could do that by using the -MD flag in the CC line above.
FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES = DerivedSources.make

# --------

VPATH = \
    $(WebCore) \
    $(WebCore)/Modules/airplay \
    $(WebCore)/Modules/applepay \
    $(WebCore)/Modules/applepay/paymentrequest \
    $(WebCore)/Modules/async-clipboard \
    $(WebCore)/Modules/beacon \
    $(WebCore)/Modules/cache \
    $(WebCore)/Modules/credentialmanagement \
    $(WebCore)/Modules/encryptedmedia \
    $(WebCore)/Modules/encryptedmedia/legacy \
    $(WebCore)/Modules/entriesapi \
    $(WebCore)/Modules/fetch \
    $(WebCore)/Modules/gamepad \
    $(WebCore)/Modules/geolocation \
    $(WebCore)/Modules/highlight \
    $(WebCore)/Modules/indexeddb \
    $(WebCore)/Modules/indieui \
    $(WebCore)/Modules/mediacapabilities \
    $(WebCore)/Modules/mediacontrols \
    $(WebCore)/Modules/mediarecorder \
    $(WebCore)/Modules/mediasession \
    $(WebCore)/Modules/mediasource \
    $(WebCore)/Modules/mediastream \
    $(WebCore)/Modules/notifications \
    $(WebCore)/Modules/paymentrequest \
    $(WebCore)/Modules/pictureinpicture \
    $(WebCore)/Modules/plugins \
    $(WebCore)/Modules/quota \
	$(WebCore)/Modules/remoteplayback \
    $(WebCore)/Modules/speech \
    $(WebCore)/Modules/streams \
    $(WebCore)/Modules/webaudio \
    $(WebCore)/Modules/webauthn \
    $(WebCore)/Modules/webdatabase \
    $(WebCore)/Modules/webdriver \
    $(WebCore)/Modules/webgpu \
    $(WebCore)/Modules/websockets \
    $(WebCore)/Modules/webxr \
    $(WebCore)/accessibility \
    $(WebCore)/animation \
    $(WebCore)/bindings/js \
    $(WebCore)/crypto \
    $(WebCore)/crypto/keys \
    $(WebCore)/crypto/parameters \
    $(WebCore)/css \
    $(WebCore)/css/typedom \
    $(WebCore)/dom \
    $(WebCore)/editing \
    $(WebCore)/fileapi \
    $(WebCore)/html \
    $(WebCore)/html/canvas \
    $(WebCore)/html/shadow \
    $(WebCore)/html/track \
    $(WebCore)/inspector \
    $(WebCore)/loader/appcache \
    $(WebCore)/mathml \
    $(WebCore)/page \
    $(WebCore)/platform/network \
    $(WebCore)/plugins \
    $(WebCore)/storage \
    $(WebCore)/svg \
    $(WebCore)/testing \
    $(WebCore)/websockets \
    $(WebCore)/workers \
    $(WebCore)/workers/service \
    $(WebCore)/worklets \
    $(WebCore)/xml \
#

JS_BINDING_IDLS = \
    $(WebCore)/Modules/airplay/WebKitPlaybackTargetAvailabilityEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayCancelEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayContactField.idl \
    $(WebCore)/Modules/applepay/ApplePayError.idl \
    $(WebCore)/Modules/applepay/ApplePayErrorCode.idl \
    $(WebCore)/Modules/applepay/ApplePayErrorContactField.idl \
    $(WebCore)/Modules/applepay/ApplePayInstallmentConfiguration.idl \
	$(WebCore)/Modules/applepay/ApplePayInstallmentItem.idl \
	$(WebCore)/Modules/applepay/ApplePayInstallmentItemType.idl \
	$(WebCore)/Modules/applepay/ApplePayInstallmentRetailChannel.idl \
    $(WebCore)/Modules/applepay/ApplePayLineItem.idl \
    $(WebCore)/Modules/applepay/ApplePayMerchantCapability.idl \
    $(WebCore)/Modules/applepay/ApplePayPayment.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentAuthorizationResult.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentAuthorizedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentContact.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentMethod.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentMethodSelectedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentMethodType.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentMethodUpdate.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentPass.idl \
    $(WebCore)/Modules/applepay/ApplePayPaymentRequest.idl \
    $(WebCore)/Modules/applepay/ApplePayRequestBase.idl \
    $(WebCore)/Modules/applepay/ApplePaySession.idl \
    $(WebCore)/Modules/applepay/ApplePaySessionError.idl \
    $(WebCore)/Modules/applepay/ApplePaySetup.idl \
    $(WebCore)/Modules/applepay/ApplePaySetupConfiguration.idl \
    $(WebCore)/Modules/applepay/ApplePaySetupFeature.idl \
    $(WebCore)/Modules/applepay/ApplePaySetupFeatureState.idl \
    $(WebCore)/Modules/applepay/ApplePaySetupFeatureType.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingContactSelectedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingContactUpdate.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingMethod.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingMethodSelectedEvent.idl \
    $(WebCore)/Modules/applepay/ApplePayShippingMethodUpdate.idl \
    $(WebCore)/Modules/applepay/ApplePayValidateMerchantEvent.idl \
    $(WebCore)/Modules/applepay/paymentrequest/ApplePayModifier.idl \
    $(WebCore)/Modules/applepay/paymentrequest/ApplePayRequest.idl \
    $(WebCore)/Modules/async-clipboard/Clipboard.idl \
    $(WebCore)/Modules/async-clipboard/ClipboardItem.idl \
    $(WebCore)/Modules/async-clipboard/NavigatorClipboard.idl \
    $(WebCore)/Modules/beacon/NavigatorBeacon.idl \
    $(WebCore)/Modules/cache/CacheQueryOptions.idl \
    $(WebCore)/Modules/cache/DOMCache.idl \
    $(WebCore)/Modules/cache/DOMCacheStorage.idl \
    $(WebCore)/Modules/cache/DOMWindowCaches.idl \
    $(WebCore)/Modules/cache/WorkerGlobalScopeCaches.idl \
    $(WebCore)/Modules/credentialmanagement/BasicCredential.idl \
    $(WebCore)/Modules/credentialmanagement/CredentialCreationOptions.idl \
    $(WebCore)/Modules/credentialmanagement/CredentialRequestOptions.idl \
    $(WebCore)/Modules/credentialmanagement/CredentialsContainer.idl \
    $(WebCore)/Modules/credentialmanagement/NavigatorCredentials.idl \
    $(WebCore)/Modules/encryptedmedia/MediaKeyEncryptionScheme.idl \
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
    $(WebCore)/Modules/entriesapi/DOMFileSystem.idl \
    $(WebCore)/Modules/entriesapi/ErrorCallback.idl \
    $(WebCore)/Modules/entriesapi/FileCallback.idl \
    $(WebCore)/Modules/entriesapi/FileSystemDirectoryEntry.idl \
    $(WebCore)/Modules/entriesapi/FileSystemDirectoryReader.idl \
    $(WebCore)/Modules/entriesapi/FileSystemEntriesCallback.idl \
    $(WebCore)/Modules/entriesapi/FileSystemEntry.idl \
    $(WebCore)/Modules/entriesapi/FileSystemEntryCallback.idl \
    $(WebCore)/Modules/entriesapi/FileSystemFileEntry.idl \
    $(WebCore)/Modules/entriesapi/HTMLInputElementEntriesAPI.idl \
    $(WebCore)/Modules/fetch/DOMWindowFetch.idl \
    $(WebCore)/Modules/fetch/FetchBody.idl \
    $(WebCore)/Modules/fetch/FetchHeaders.idl \
    $(WebCore)/Modules/fetch/FetchReferrerPolicy.idl \
    $(WebCore)/Modules/fetch/FetchRequest.idl \
    $(WebCore)/Modules/fetch/FetchRequestCache.idl \
    $(WebCore)/Modules/fetch/FetchRequestCredentials.idl \
    $(WebCore)/Modules/fetch/FetchRequestInit.idl \
    $(WebCore)/Modules/fetch/FetchRequestMode.idl \
    $(WebCore)/Modules/fetch/FetchRequestRedirect.idl \
    $(WebCore)/Modules/fetch/FetchResponse.idl \
    $(WebCore)/Modules/fetch/WorkerGlobalScopeFetch.idl \
    $(WebCore)/Modules/gamepad/Gamepad.idl \
    $(WebCore)/Modules/gamepad/GamepadButton.idl \
    $(WebCore)/Modules/gamepad/GamepadEvent.idl \
    $(WebCore)/Modules/gamepad/NavigatorGamepad.idl \
    $(WebCore)/Modules/geolocation/Geolocation.idl \
    $(WebCore)/Modules/geolocation/GeolocationCoordinates.idl \
    $(WebCore)/Modules/geolocation/GeolocationPosition.idl \
    $(WebCore)/Modules/geolocation/GeolocationPositionError.idl \
    $(WebCore)/Modules/geolocation/NavigatorGeolocation.idl \
    $(WebCore)/Modules/geolocation/PositionCallback.idl \
    $(WebCore)/Modules/geolocation/PositionErrorCallback.idl \
    $(WebCore)/Modules/geolocation/PositionOptions.idl \
    $(WebCore)/Modules/highlight/HighlightMap.idl \
    $(WebCore)/Modules/highlight/HighlightRangeGroup.idl \
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
    $(WebCore)/Modules/mediacapabilities/AudioConfiguration.idl \
    $(WebCore)/Modules/mediacapabilities/ColorGamut.idl \
    $(WebCore)/Modules/mediacapabilities/HdrMetadataType.idl \
    $(WebCore)/Modules/mediacapabilities/MediaCapabilities.idl \
    $(WebCore)/Modules/mediacapabilities/MediaCapabilitiesDecodingInfo.idl \
    $(WebCore)/Modules/mediacapabilities/MediaCapabilitiesEncodingInfo.idl \
    $(WebCore)/Modules/mediacapabilities/MediaCapabilitiesInfo.idl \
    $(WebCore)/Modules/mediacapabilities/MediaConfiguration.idl \
    $(WebCore)/Modules/mediacapabilities/MediaDecodingConfiguration.idl \
    $(WebCore)/Modules/mediacapabilities/MediaDecodingType.idl \
    $(WebCore)/Modules/mediacapabilities/MediaEncodingConfiguration.idl \
    $(WebCore)/Modules/mediacapabilities/MediaEncodingType.idl \
    $(WebCore)/Modules/mediacapabilities/NavigatorMediaCapabilities.idl \
    $(WebCore)/Modules/mediacapabilities/TransferFunction.idl \
    $(WebCore)/Modules/mediacapabilities/VideoConfiguration.idl \
    $(WebCore)/Modules/mediacontrols/MediaControlsHost.idl \
    $(WebCore)/Modules/mediarecorder/BlobEvent.idl \
    $(WebCore)/Modules/mediarecorder/MediaRecorder.idl \
    $(WebCore)/Modules/mediarecorder/MediaRecorderErrorEvent.idl \
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
    $(WebCore)/Modules/mediastream/CanvasCaptureMediaStreamTrack.idl \
    $(WebCore)/Modules/mediastream/DoubleRange.idl \
    $(WebCore)/Modules/mediastream/LongRange.idl \
    $(WebCore)/Modules/mediastream/MediaDeviceInfo.idl \
    $(WebCore)/Modules/mediastream/MediaDevices.idl \
    $(WebCore)/Modules/mediastream/MediaStream.idl \
    $(WebCore)/Modules/mediastream/MediaStreamTrack.idl \
    $(WebCore)/Modules/mediastream/MediaStreamTrackEvent.idl \
    $(WebCore)/Modules/mediastream/MediaTrackConstraints.idl \
    $(WebCore)/Modules/mediastream/MediaTrackSupportedConstraints.idl \
    $(WebCore)/Modules/mediastream/NavigatorMediaDevices.idl \
    $(WebCore)/Modules/mediastream/OverconstrainedError.idl \
    $(WebCore)/Modules/mediastream/OverconstrainedErrorEvent.idl \
    $(WebCore)/Modules/mediastream/RTCAnswerOptions.idl \
    $(WebCore)/Modules/mediastream/RTCCertificate.idl \
    $(WebCore)/Modules/mediastream/RTCConfiguration.idl \
    $(WebCore)/Modules/mediastream/RTCDTMFSender.idl \
    $(WebCore)/Modules/mediastream/RTCDTMFToneChangeEvent.idl \
    $(WebCore)/Modules/mediastream/RTCDataChannel.idl \
    $(WebCore)/Modules/mediastream/RTCDataChannelEvent.idl \
    $(WebCore)/Modules/mediastream/RTCDegradationPreference.idl \
    $(WebCore)/Modules/mediastream/RTCDtxStatus.idl \
    $(WebCore)/Modules/mediastream/RTCIceCandidate.idl \
    $(WebCore)/Modules/mediastream/RTCIceConnectionState.idl \
    $(WebCore)/Modules/mediastream/RTCIceGatheringState.idl \
    $(WebCore)/Modules/mediastream/RTCIceServer.idl \
    $(WebCore)/Modules/mediastream/RTCIceTransport.idl \
    $(WebCore)/Modules/mediastream/RTCIceTransportState.idl \
    $(WebCore)/Modules/mediastream/RTCOfferAnswerOptions.idl \
    $(WebCore)/Modules/mediastream/RTCOfferOptions.idl \
    $(WebCore)/Modules/mediastream/RTCPeerConnection.idl \
    $(WebCore)/Modules/mediastream/RTCPeerConnectionIceEvent.idl \
    $(WebCore)/Modules/mediastream/RTCPeerConnectionState.idl \
    $(WebCore)/Modules/mediastream/RTCPriorityType.idl \
    $(WebCore)/Modules/mediastream/RTCRtpCapabilities.idl \
    $(WebCore)/Modules/mediastream/RTCRtpCodecCapability.idl \
    $(WebCore)/Modules/mediastream/RTCRtpCodecParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpContributingSource.idl \
    $(WebCore)/Modules/mediastream/RTCRtpEncodingParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpFecParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpHeaderExtensionParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpReceiver.idl \
    $(WebCore)/Modules/mediastream/RTCRtpRtxParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpSendParameters.idl \
    $(WebCore)/Modules/mediastream/RTCRtpSender.idl \
    $(WebCore)/Modules/mediastream/RTCRtpSynchronizationSource.idl \
    $(WebCore)/Modules/mediastream/RTCRtpTransceiver.idl \
    $(WebCore)/Modules/mediastream/RTCRtpTransceiverDirection.idl \
    $(WebCore)/Modules/mediastream/RTCSessionDescription.idl \
    $(WebCore)/Modules/mediastream/RTCSignalingState.idl \
    $(WebCore)/Modules/mediastream/RTCStatsReport.idl \
    $(WebCore)/Modules/mediastream/RTCTrackEvent.idl \
    $(WebCore)/Modules/notifications/Notification.idl \
    $(WebCore)/Modules/notifications/NotificationPermission.idl \
    $(WebCore)/Modules/notifications/NotificationPermissionCallback.idl \
    $(WebCore)/Modules/paymentrequest/AddressErrors.idl \
    $(WebCore)/Modules/paymentrequest/MerchantValidationEvent.idl \
    $(WebCore)/Modules/paymentrequest/PayerErrorFields.idl \
    $(WebCore)/Modules/paymentrequest/PaymentAddress.idl \
    $(WebCore)/Modules/paymentrequest/PaymentComplete.idl \
    $(WebCore)/Modules/paymentrequest/PaymentCurrencyAmount.idl \
    $(WebCore)/Modules/paymentrequest/PaymentDetailsBase.idl \
    $(WebCore)/Modules/paymentrequest/PaymentDetailsInit.idl \
    $(WebCore)/Modules/paymentrequest/PaymentDetailsModifier.idl \
    $(WebCore)/Modules/paymentrequest/PaymentDetailsUpdate.idl \
    $(WebCore)/Modules/paymentrequest/PaymentItem.idl \
    $(WebCore)/Modules/paymentrequest/PaymentMethodChangeEvent.idl \
    $(WebCore)/Modules/paymentrequest/PaymentMethodData.idl \
    $(WebCore)/Modules/paymentrequest/PaymentOptions.idl \
    $(WebCore)/Modules/paymentrequest/PaymentRequest.idl \
    $(WebCore)/Modules/paymentrequest/PaymentRequestUpdateEvent.idl \
    $(WebCore)/Modules/paymentrequest/PaymentRequestUpdateEventInit.idl \
    $(WebCore)/Modules/paymentrequest/PaymentResponse.idl \
    $(WebCore)/Modules/paymentrequest/PaymentShippingOption.idl \
    $(WebCore)/Modules/paymentrequest/PaymentShippingType.idl \
    $(WebCore)/Modules/paymentrequest/PaymentValidationErrors.idl \
    $(WebCore)/Modules/pictureinpicture/DocumentPictureInPicture.idl \
    $(WebCore)/Modules/pictureinpicture/EnterPictureInPictureEvent.idl \
    $(WebCore)/Modules/pictureinpicture/HTMLVideoElementPictureInPicture.idl \
    $(WebCore)/Modules/pictureinpicture/PictureInPictureWindow.idl \
    $(WebCore)/Modules/plugins/QuickTimePluginReplacement.idl \
    $(WebCore)/Modules/quota/DOMWindowQuota.idl \
    $(WebCore)/Modules/quota/NavigatorStorageQuota.idl \
    $(WebCore)/Modules/quota/StorageErrorCallback.idl \
    $(WebCore)/Modules/quota/StorageInfo.idl \
    $(WebCore)/Modules/quota/StorageQuota.idl \
    $(WebCore)/Modules/quota/StorageQuotaCallback.idl \
    $(WebCore)/Modules/quota/StorageUsageCallback.idl \
    $(WebCore)/Modules/quota/WorkerNavigatorStorageQuota.idl \
	$(WebCore)/Modules/remoteplayback/HTMLMediaElementRemotePlayback.idl \
	$(WebCore)/Modules/remoteplayback/RemotePlayback.idl \
	$(WebCore)/Modules/remoteplayback/RemotePlaybackAvailabilityCallback.idl \
    $(WebCore)/Modules/speech/DOMWindowSpeechSynthesis.idl \
    $(WebCore)/Modules/speech/SpeechSynthesis.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisEvent.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisUtterance.idl \
    $(WebCore)/Modules/speech/SpeechSynthesisVoice.idl \
    $(WebCore)/Modules/streams/ByteLengthQueuingStrategy.idl \
    $(WebCore)/Modules/streams/CountQueuingStrategy.idl \
    $(WebCore)/Modules/streams/ReadableByteStreamController.idl \
    $(WebCore)/Modules/streams/ReadableStream.idl \
    $(WebCore)/Modules/streams/ReadableStreamBYOBReader.idl \
    $(WebCore)/Modules/streams/ReadableStreamBYOBRequest.idl \
    $(WebCore)/Modules/streams/ReadableStreamDefaultController.idl \
    $(WebCore)/Modules/streams/ReadableStreamDefaultReader.idl \
    $(WebCore)/Modules/streams/ReadableStreamSink.idl \
    $(WebCore)/Modules/streams/ReadableStreamSource.idl \
    $(WebCore)/Modules/streams/WritableStream.idl \
    $(WebCore)/Modules/webaudio/AnalyserNode.idl \
    $(WebCore)/Modules/webaudio/AnalyserOptions.idl \
    $(WebCore)/Modules/webaudio/AudioBuffer.idl \
    $(WebCore)/Modules/webaudio/AudioBufferCallback.idl \
    $(WebCore)/Modules/webaudio/AudioBufferOptions.idl \
    $(WebCore)/Modules/webaudio/AudioBufferSourceNode.idl \
    $(WebCore)/Modules/webaudio/AudioContext.idl \
    $(WebCore)/Modules/webaudio/AudioContextLatencyCategory.idl \
    $(WebCore)/Modules/webaudio/AudioContextOptions.idl \
    $(WebCore)/Modules/webaudio/AudioContextState.idl \
    $(WebCore)/Modules/webaudio/AudioDestinationNode.idl \
    $(WebCore)/Modules/webaudio/AudioListener.idl \
    $(WebCore)/Modules/webaudio/AudioNode.idl \
    $(WebCore)/Modules/webaudio/AudioNodeOptions.idl \
    $(WebCore)/Modules/webaudio/AudioParam.idl \
    $(WebCore)/Modules/webaudio/AudioProcessingEvent.idl \
    $(WebCore)/Modules/webaudio/AudioScheduledSourceNode.idl \
    $(WebCore)/Modules/webaudio/BaseAudioContext.idl \
    $(WebCore)/Modules/webaudio/BiquadFilterNode.idl \
    $(WebCore)/Modules/webaudio/ChannelCountMode.idl \
    $(WebCore)/Modules/webaudio/ChannelInterpretation.idl \
    $(WebCore)/Modules/webaudio/ChannelMergerNode.idl \
    $(WebCore)/Modules/webaudio/ChannelMergerOptions.idl \
    $(WebCore)/Modules/webaudio/ChannelSplitterNode.idl \
    $(WebCore)/Modules/webaudio/ChannelSplitterOptions.idl \
    $(WebCore)/Modules/webaudio/ConvolverNode.idl \
    $(WebCore)/Modules/webaudio/DelayNode.idl \
    $(WebCore)/Modules/webaudio/DelayOptions.idl \
    $(WebCore)/Modules/webaudio/DistanceModelType.idl \
    $(WebCore)/Modules/webaudio/DynamicsCompressorNode.idl \
    $(WebCore)/Modules/webaudio/GainNode.idl \
    $(WebCore)/Modules/webaudio/GainOptions.idl \
    $(WebCore)/Modules/webaudio/MediaElementAudioSourceNode.idl \
    $(WebCore)/Modules/webaudio/MediaStreamAudioDestinationNode.idl \
    $(WebCore)/Modules/webaudio/MediaStreamAudioSourceNode.idl \
    $(WebCore)/Modules/webaudio/OfflineAudioCompletionEvent.idl \
    $(WebCore)/Modules/webaudio/OfflineAudioCompletionEventInit.idl \
    $(WebCore)/Modules/webaudio/OfflineAudioContext.idl \
    $(WebCore)/Modules/webaudio/OfflineAudioContextOptions.idl \
    $(WebCore)/Modules/webaudio/OscillatorNode.idl \
    $(WebCore)/Modules/webaudio/OscillatorOptions.idl \
    $(WebCore)/Modules/webaudio/OscillatorType.idl \
    $(WebCore)/Modules/webaudio/OverSampleType.idl \
    $(WebCore)/Modules/webaudio/PannerNode.idl \
    $(WebCore)/Modules/webaudio/PannerOptions.idl \
    $(WebCore)/Modules/webaudio/PanningModelType.idl \
    $(WebCore)/Modules/webaudio/PeriodicWave.idl \
    $(WebCore)/Modules/webaudio/PeriodicWaveConstraints.idl \
    $(WebCore)/Modules/webaudio/PeriodicWaveOptions.idl \
    $(WebCore)/Modules/webaudio/ScriptProcessorNode.idl \
    $(WebCore)/Modules/webaudio/WaveShaperNode.idl \
    $(WebCore)/Modules/webaudio/WaveShaperOptions.idl \
    $(WebCore)/Modules/webaudio/WebKitAudioContext.idl \
    $(WebCore)/Modules/webaudio/WebKitAudioPannerNode.idl \
    $(WebCore)/Modules/webaudio/WebKitOfflineAudioContext.idl \
    $(WebCore)/Modules/webaudio/WebKitOscillatorNode.idl \
    $(WebCore)/Modules/webauthn/AttestationConveyancePreference.idl \
    $(WebCore)/Modules/webauthn/AuthenticationExtensionsClientInputs.idl \
    $(WebCore)/Modules/webauthn/AuthenticationExtensionsClientOutputs.idl \
    $(WebCore)/Modules/webauthn/AuthenticatorAssertionResponse.idl \
    $(WebCore)/Modules/webauthn/AuthenticatorAttestationResponse.idl \
    $(WebCore)/Modules/webauthn/AuthenticatorResponse.idl \
    $(WebCore)/Modules/webauthn/AuthenticatorTransport.idl \
    $(WebCore)/Modules/webauthn/PublicKeyCredential.idl \
    $(WebCore)/Modules/webauthn/PublicKeyCredentialCreationOptions.idl \
    $(WebCore)/Modules/webauthn/PublicKeyCredentialDescriptor.idl \
    $(WebCore)/Modules/webauthn/PublicKeyCredentialRequestOptions.idl \
    $(WebCore)/Modules/webauthn/PublicKeyCredentialType.idl \
    $(WebCore)/Modules/webauthn/UserVerificationRequirement.idl \
    $(WebCore)/Modules/webdatabase/DOMWindowWebDatabase.idl \
    $(WebCore)/Modules/webdatabase/Database.idl \
    $(WebCore)/Modules/webdatabase/DatabaseCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLError.idl \
    $(WebCore)/Modules/webdatabase/SQLResultSet.idl \
    $(WebCore)/Modules/webdatabase/SQLResultSetRowList.idl \
    $(WebCore)/Modules/webdatabase/SQLStatementCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLStatementErrorCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLTransaction.idl \
    $(WebCore)/Modules/webdatabase/SQLTransactionCallback.idl \
    $(WebCore)/Modules/webdatabase/SQLTransactionErrorCallback.idl \
    $(WebCore)/Modules/webdriver/NavigatorWebDriver.idl \
    $(WebCore)/Modules/webgpu/GPUCanvasContext.idl \
    $(WebCore)/Modules/webgpu/GPUColor.idl \
    $(WebCore)/Modules/webgpu/GPUColorStateDescriptor.idl \
    $(WebCore)/Modules/webgpu/GPUColorWrite.idl \
    $(WebCore)/Modules/webgpu/GPUBindGroupLayoutBinding.idl \
    $(WebCore)/Modules/webgpu/GPUBindGroupLayoutDescriptor.idl \
    $(WebCore)/Modules/webgpu/GPUBlendDescriptor.idl \
    $(WebCore)/Modules/webgpu/GPUBufferDescriptor.idl \
    $(WebCore)/Modules/webgpu/GPUBufferUsage.idl \
    $(WebCore)/Modules/webgpu/GPUCompareFunction.idl \
    $(WebCore)/Modules/webgpu/GPUDepthStencilStateDescriptor.idl \
    $(WebCore)/Modules/webgpu/GPUErrorFilter.idl \
    $(WebCore)/Modules/webgpu/GPUExtent3D.idl \
    $(WebCore)/Modules/webgpu/GPULoadOp.idl \
    $(WebCore)/Modules/webgpu/GPUOrigin3D.idl \
    $(WebCore)/Modules/webgpu/GPUOutOfMemoryError.idl \
    $(WebCore)/Modules/webgpu/GPURequestAdapterOptions.idl \
    $(WebCore)/Modules/webgpu/GPUSamplerDescriptor.idl \
    $(WebCore)/Modules/webgpu/GPUShaderStage.idl \
    $(WebCore)/Modules/webgpu/GPUStoreOp.idl \
    $(WebCore)/Modules/webgpu/GPUTextureDescriptor.idl \
    $(WebCore)/Modules/webgpu/GPUTextureFormat.idl \
    $(WebCore)/Modules/webgpu/GPUTextureUsage.idl \
    $(WebCore)/Modules/webgpu/GPUUncapturedErrorEvent.idl \
    $(WebCore)/Modules/webgpu/GPUValidationError.idl \
    $(WebCore)/Modules/webgpu/GPUVertexAttributeDescriptor.idl \
    $(WebCore)/Modules/webgpu/GPUVertexBufferDescriptor.idl \
    $(WebCore)/Modules/webgpu/GPUVertexInputDescriptor.idl \
    $(WebCore)/Modules/webgpu/NavigatorGPU.idl \
    $(WebCore)/Modules/webgpu/WebGPU.idl \
    $(WebCore)/Modules/webgpu/WebGPUAdapter.idl \
    $(WebCore)/Modules/webgpu/WebGPUBindGroup.idl \
    $(WebCore)/Modules/webgpu/WebGPUBindGroupBinding.idl \
    $(WebCore)/Modules/webgpu/WebGPUBindGroupDescriptor.idl \
    $(WebCore)/Modules/webgpu/WebGPUBindGroupLayout.idl \
    $(WebCore)/Modules/webgpu/WebGPUBuffer.idl \
    $(WebCore)/Modules/webgpu/WebGPUBufferBinding.idl \
    $(WebCore)/Modules/webgpu/WebGPUCommandBuffer.idl \
    $(WebCore)/Modules/webgpu/WebGPUCommandEncoder.idl \
    $(WebCore)/Modules/webgpu/WebGPUComputePassEncoder.idl \
    $(WebCore)/Modules/webgpu/WebGPUComputePipeline.idl \
    $(WebCore)/Modules/webgpu/WebGPUComputePipelineDescriptor.idl \
    $(WebCore)/Modules/webgpu/WebGPUDevice.idl \
    $(WebCore)/Modules/webgpu/WebGPUDeviceErrorScopes.idl \
    $(WebCore)/Modules/webgpu/WebGPUDeviceEventHandler.idl \
    $(WebCore)/Modules/webgpu/WebGPUQueue.idl \
    $(WebCore)/Modules/webgpu/WebGPUPipelineDescriptorBase.idl \
    $(WebCore)/Modules/webgpu/WebGPUPipelineLayout.idl \
    $(WebCore)/Modules/webgpu/WebGPUPipelineLayoutDescriptor.idl \
    $(WebCore)/Modules/webgpu/WebGPUProgrammableStageDescriptor.idl \
    $(WebCore)/Modules/webgpu/WebGPUProgrammablePassEncoder.idl \
    $(WebCore)/Modules/webgpu/WebGPURenderPassDescriptor.idl \
    $(WebCore)/Modules/webgpu/WebGPURenderPassEncoder.idl \
    $(WebCore)/Modules/webgpu/WebGPURenderPipeline.idl \
    $(WebCore)/Modules/webgpu/WebGPURenderPipelineDescriptor.idl \
    $(WebCore)/Modules/webgpu/WebGPUSampler.idl \
    $(WebCore)/Modules/webgpu/WebGPUShaderModule.idl \
    $(WebCore)/Modules/webgpu/WebGPUShaderModuleDescriptor.idl \
    $(WebCore)/Modules/webgpu/WebGPUSwapChain.idl \
    $(WebCore)/Modules/webgpu/WebGPUTexture.idl \
    $(WebCore)/Modules/webgpu/WebGPUTextureView.idl \
    $(WebCore)/Modules/webgpu/WorkerNavigatorGPU.idl \
    $(WebCore)/Modules/websockets/CloseEvent.idl \
    $(WebCore)/Modules/websockets/WebSocket.idl \
    $(WebCore)/Modules/webxr/NavigatorWebXR.idl \
    $(WebCore)/Modules/webxr/WebXRBoundedReferenceSpace.idl \
    $(WebCore)/Modules/webxr/WebXRFrame.idl \
    $(WebCore)/Modules/webxr/WebXRInputSourceArray.idl \
    $(WebCore)/Modules/webxr/WebXRInputSource.idl \
    $(WebCore)/Modules/webxr/WebXRLayer.idl \
    $(WebCore)/Modules/webxr/WebXRPose.idl \
    $(WebCore)/Modules/webxr/WebXRReferenceSpace.idl \
    $(WebCore)/Modules/webxr/WebXRRenderState.idl \
    $(WebCore)/Modules/webxr/WebXRRigidTransform.idl \
    $(WebCore)/Modules/webxr/WebXRSession.idl \
    $(WebCore)/Modules/webxr/WebXRSpace.idl \
    $(WebCore)/Modules/webxr/WebXRSystem.idl \
    $(WebCore)/Modules/webxr/WebXRViewerPose.idl \
    $(WebCore)/Modules/webxr/WebXRView.idl \
    $(WebCore)/Modules/webxr/WebXRViewport.idl \
    $(WebCore)/Modules/webxr/WebXRWebGLLayer.idl \
    $(WebCore)/Modules/webxr/XREnvironmentBlendMode.idl \
    $(WebCore)/Modules/webxr/XREye.idl \
    $(WebCore)/Modules/webxr/XRFrameRequestCallback.idl \
    $(WebCore)/Modules/webxr/XRHandedness.idl \
    $(WebCore)/Modules/webxr/XRInputSourceEvent.idl \
    $(WebCore)/Modules/webxr/XRInputSourcesChangeEvent.idl \
    $(WebCore)/Modules/webxr/XRInteractionMode.idl \
    $(WebCore)/Modules/webxr/XRReferenceSpaceEvent.idl \
    $(WebCore)/Modules/webxr/XRReferenceSpaceType.idl \
    $(WebCore)/Modules/webxr/XRRenderStateInit.idl \
    $(WebCore)/Modules/webxr/XRSessionEvent.idl \
    $(WebCore)/Modules/webxr/XRSessionInit.idl \
    $(WebCore)/Modules/webxr/XRSessionMode.idl \
    $(WebCore)/Modules/webxr/XRTargetRayMode.idl \
    $(WebCore)/Modules/webxr/XRVisibilityState.idl \
    $(WebCore)/Modules/webxr/XRWebGLLayerInit.idl \
    $(WebCore)/accessibility/AccessibilityRole.idl \
    $(WebCore)/accessibility/AriaAttributes.idl \
    $(WebCore)/animation/Animatable.idl \
    $(WebCore)/animation/AnimationEffect.idl \
    $(WebCore)/animation/AnimationPlaybackEvent.idl \
    $(WebCore)/animation/AnimationPlaybackEventInit.idl \
    $(WebCore)/animation/AnimationTimeline.idl \
    $(WebCore)/animation/CSSAnimation.idl \
    $(WebCore)/animation/CSSTransition.idl \
    $(WebCore)/animation/CompositeOperation.idl \
    $(WebCore)/animation/CompositeOperationOrAuto.idl \
    $(WebCore)/animation/ComputedEffectTiming.idl \
    $(WebCore)/animation/DocumentTimeline.idl \
    $(WebCore)/animation/DocumentTimelineOptions.idl \
    $(WebCore)/animation/EffectTiming.idl \
    $(WebCore)/animation/FillMode.idl \
    $(WebCore)/animation/GetAnimationsOptions.idl \
    $(WebCore)/animation/IterationCompositeOperation.idl \
    $(WebCore)/animation/KeyframeAnimationOptions.idl \
    $(WebCore)/animation/KeyframeEffect.idl \
    $(WebCore)/animation/KeyframeEffectOptions.idl \
    $(WebCore)/animation/OptionalEffectTiming.idl \
    $(WebCore)/animation/PlaybackDirection.idl \
    $(WebCore)/animation/WebAnimation.idl \
    $(WebCore)/crypto/CryptoAlgorithmParameters.idl \
    $(WebCore)/crypto/CryptoKey.idl \
    $(WebCore)/crypto/CryptoKeyPair.idl \
    $(WebCore)/crypto/CryptoKeyUsage.idl \
    $(WebCore)/crypto/JsonWebKey.idl \
    $(WebCore)/crypto/RsaOtherPrimesInfo.idl \
    $(WebCore)/crypto/SubtleCrypto.idl \
    $(WebCore)/crypto/keys/CryptoAesKeyAlgorithm.idl \
    $(WebCore)/crypto/keys/CryptoEcKeyAlgorithm.idl \
    $(WebCore)/crypto/keys/CryptoHmacKeyAlgorithm.idl \
    $(WebCore)/crypto/keys/CryptoKeyAlgorithm.idl \
    $(WebCore)/crypto/keys/CryptoRsaHashedKeyAlgorithm.idl \
    $(WebCore)/crypto/keys/CryptoRsaKeyAlgorithm.idl \
    $(WebCore)/crypto/parameters/AesCbcCfbParams.idl \
    $(WebCore)/crypto/parameters/AesCtrParams.idl \
    $(WebCore)/crypto/parameters/AesGcmParams.idl \
    $(WebCore)/crypto/parameters/AesKeyParams.idl \
    $(WebCore)/crypto/parameters/EcKeyParams.idl \
    $(WebCore)/crypto/parameters/EcdhKeyDeriveParams.idl \
    $(WebCore)/crypto/parameters/EcdsaParams.idl \
    $(WebCore)/crypto/parameters/HkdfParams.idl \
    $(WebCore)/crypto/parameters/HmacKeyParams.idl \
    $(WebCore)/crypto/parameters/Pbkdf2Params.idl \
    $(WebCore)/crypto/parameters/RsaHashedImportParams.idl \
    $(WebCore)/crypto/parameters/RsaHashedKeyGenParams.idl \
    $(WebCore)/crypto/parameters/RsaKeyGenParams.idl \
    $(WebCore)/crypto/parameters/RsaOaepParams.idl \
    $(WebCore)/crypto/parameters/RsaPssParams.idl \
    $(WebCore)/css/CSSFontFaceRule.idl \
    $(WebCore)/css/CSSImportRule.idl \
    $(WebCore)/css/CSSKeyframeRule.idl \
    $(WebCore)/css/CSSKeyframesRule.idl \
    $(WebCore)/css/CSSMediaRule.idl \
    $(WebCore)/css/CSSNamespaceRule.idl \
    $(WebCore)/css/CSSPageRule.idl \
    $(WebCore)/css/CSSPaintCallback.idl \
    $(WebCore)/css/CSSPaintSize.idl \
    $(WebCore)/css/CSSRule.idl \
    $(WebCore)/css/CSSRuleList.idl \
    $(WebCore)/css/CSSStyleDeclaration.idl \
    $(WebCore)/css/CSSStyleRule.idl \
    $(WebCore)/css/CSSStyleSheet.idl \
    $(WebCore)/css/CSSSupportsRule.idl \
    $(WebCore)/css/CSSUnknownRule.idl \
    $(WebCore)/css/DOMCSSCustomPropertyDescriptor.idl \
    $(WebCore)/css/DOMCSSNamespace.idl \
    $(WebCore)/css/DOMCSSPaintWorklet.idl \
    $(WebCore)/css/DOMCSSRegisterCustomProperty.idl \
    $(WebCore)/css/DOMMatrix.idl \
    $(WebCore)/css/DOMMatrix2DInit.idl \
    $(WebCore)/css/DOMMatrixInit.idl \
    $(WebCore)/css/DOMMatrixReadOnly.idl \
    $(WebCore)/css/DeprecatedCSSOMCounter.idl \
    $(WebCore)/css/DeprecatedCSSOMPrimitiveValue.idl \
    $(WebCore)/css/DeprecatedCSSOMRGBColor.idl \
    $(WebCore)/css/DeprecatedCSSOMRect.idl \
    $(WebCore)/css/DeprecatedCSSOMValue.idl \
    $(WebCore)/css/DeprecatedCSSOMValueList.idl \
    $(WebCore)/css/ElementCSSInlineStyle.idl \
    $(WebCore)/css/FontFace.idl \
    $(WebCore)/css/FontFaceSet.idl \
    $(WebCore)/css/MediaList.idl \
    $(WebCore)/css/MediaQueryList.idl \
    $(WebCore)/css/MediaQueryListEvent.idl \
    $(WebCore)/css/StyleMedia.idl \
    $(WebCore)/css/StyleSheet.idl \
    $(WebCore)/css/StyleSheetList.idl \
    $(WebCore)/css/typedom/StylePropertyMap.idl \
    $(WebCore)/css/typedom/StylePropertyMapReadOnly.idl \
    $(WebCore)/css/typedom/TypedOMCSSImageValue.idl \
    $(WebCore)/css/typedom/TypedOMCSSNumericValue.idl \
    $(WebCore)/css/typedom/TypedOMCSSStyleValue.idl \
    $(WebCore)/css/typedom/TypedOMCSSUnitValue.idl \
    $(WebCore)/css/typedom/TypedOMCSSUnparsedValue.idl \
    $(WebCore)/css/WebKitCSSMatrix.idl \
    $(WebCore)/dom/AbortController.idl \
    $(WebCore)/dom/AbortSignal.idl \
    $(WebCore)/dom/AnimationEvent.idl \
    $(WebCore)/dom/Attr.idl \
    $(WebCore)/dom/BeforeLoadEvent.idl \
    $(WebCore)/dom/BeforeUnloadEvent.idl \
    $(WebCore)/dom/CDATASection.idl \
    $(WebCore)/dom/CharacterData.idl \
    $(WebCore)/dom/ChildNode.idl \
    $(WebCore)/dom/ClipboardEvent.idl \
    $(WebCore)/dom/Comment.idl \
    $(WebCore)/dom/CompositionEvent.idl \
    $(WebCore)/dom/CustomElementRegistry.idl \
    $(WebCore)/dom/CustomEvent.idl \
    $(WebCore)/dom/DOMException.idl \
    $(WebCore)/dom/DOMImplementation.idl \
    $(WebCore)/dom/DOMPoint.idl \
    $(WebCore)/dom/DOMPointInit.idl \
    $(WebCore)/dom/DOMPointReadOnly.idl \
    $(WebCore)/dom/DOMQuad.idl \
    $(WebCore)/dom/DOMQuadInit.idl \
    $(WebCore)/dom/DOMRect.idl \
    $(WebCore)/dom/DOMRectInit.idl \
    $(WebCore)/dom/DOMRectList.idl \
    $(WebCore)/dom/DOMRectReadOnly.idl \
    $(WebCore)/dom/DOMStringList.idl \
    $(WebCore)/dom/DOMStringMap.idl \
    $(WebCore)/dom/DataTransfer.idl \
    $(WebCore)/dom/DataTransferItem.idl \
    $(WebCore)/dom/DataTransferItemList.idl \
    $(WebCore)/dom/DeviceMotionEvent.idl \
    $(WebCore)/dom/DeviceOrientationEvent.idl \
    $(WebCore)/dom/DeviceOrientationOrMotionEvent.idl \
    $(WebCore)/dom/DeviceOrientationOrMotionPermissionState.idl \
    $(WebCore)/dom/Document.idl \
    $(WebCore)/dom/DocumentAndElementEventHandlers.idl \
    $(WebCore)/dom/DocumentFullscreen.idl \
    $(WebCore)/dom/DocumentFragment.idl \
    $(WebCore)/dom/DocumentOrShadowRoot.idl \
    $(WebCore)/dom/DocumentStorageAccess.idl \
    $(WebCore)/dom/DocumentType.idl \
    $(WebCore)/dom/DragEvent.idl \
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
    $(WebCore)/dom/IdleDeadline.idl \
    $(WebCore)/dom/IdleRequestCallback.idl \
    $(WebCore)/dom/IdleRequestOptions.idl \
    $(WebCore)/dom/InputEvent.idl \
    $(WebCore)/dom/KeyboardEvent.idl \
    $(WebCore)/dom/MessageChannel.idl \
    $(WebCore)/dom/MessageEvent.idl \
    $(WebCore)/dom/MessagePort.idl \
    $(WebCore)/dom/MouseEvent.idl \
    $(WebCore)/dom/MouseEventInit.idl \
    $(WebCore)/dom/MutationCallback.idl \
    $(WebCore)/dom/MutationEvent.idl \
    $(WebCore)/dom/MutationObserver.idl \
    $(WebCore)/dom/MutationRecord.idl \
    $(WebCore)/dom/NamedNodeMap.idl \
    $(WebCore)/dom/NavigatorMaxTouchPoints.idl \
    $(WebCore)/dom/Node.idl \
    $(WebCore)/dom/NodeFilter.idl \
    $(WebCore)/dom/NodeIterator.idl \
    $(WebCore)/dom/NodeList.idl \
    $(WebCore)/dom/NonDocumentTypeChildNode.idl \
    $(WebCore)/dom/NonElementParentNode.idl \
    $(WebCore)/dom/OverflowEvent.idl \
    $(WebCore)/dom/PageTransitionEvent.idl \
    $(WebCore)/dom/ParentNode.idl \
    $(WebCore)/dom/PointerEvent.idl \
    $(WebCore)/dom/PopStateEvent.idl \
    $(WebCore)/dom/ProcessingInstruction.idl \
    $(WebCore)/dom/ProgressEvent.idl \
    $(WebCore)/dom/PromiseRejectionEvent.idl \
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
    $(WebCore)/dom/VisibilityState.idl \
    $(WebCore)/dom/WebKitAnimationEvent.idl \
    $(WebCore)/dom/WebKitTransitionEvent.idl \
    $(WebCore)/dom/WheelEvent.idl \
    $(WebCore)/dom/XMLDocument.idl \
    $(WebCore)/fileapi/Blob.idl \
    $(WebCore)/fileapi/BlobCallback.idl \
    $(WebCore)/fileapi/BlobPropertyBag.idl \
    $(WebCore)/fileapi/EndingType.idl \
    $(WebCore)/fileapi/File.idl \
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
    $(WebCore)/html/HTMLDialogElement.idl \
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
    $(WebCore)/html/HTMLMenuItemElement.idl \
    $(WebCore)/html/HTMLMetaElement.idl \
    $(WebCore)/html/HTMLMeterElement.idl \
    $(WebCore)/html/HTMLModElement.idl \
    $(WebCore)/html/HTMLOListElement.idl \
    $(WebCore)/html/HTMLObjectElement.idl \
    $(WebCore)/html/HTMLOptGroupElement.idl \
    $(WebCore)/html/HTMLOptionElement.idl \
    $(WebCore)/html/HTMLOptionsCollection.idl \
    $(WebCore)/html/HTMLOrForeignElement.idl \
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
    $(WebCore)/html/ImageBitmap.idl \
    $(WebCore)/html/ImageBitmapOptions.idl \
    $(WebCore)/html/ImageData.idl \
    $(WebCore)/html/MediaController.idl \
    $(WebCore)/html/MediaEncryptedEvent.idl \
    $(WebCore)/html/MediaError.idl \
    $(WebCore)/html/OffscreenCanvas.idl \
    $(WebCore)/html/RadioNodeList.idl \
    $(WebCore)/html/TextMetrics.idl \
    $(WebCore)/html/TimeRanges.idl \
    $(WebCore)/html/URLSearchParams.idl \
    $(WebCore)/html/ValidityState.idl \
    $(WebCore)/html/VoidCallback.idl \
    $(WebCore)/html/WebKitMediaKeyError.idl \
    $(WebCore)/html/canvas/ANGLEInstancedArrays.idl \
    $(WebCore)/html/canvas/CanvasCompositing.idl \
    $(WebCore)/html/canvas/CanvasDirection.idl \
    $(WebCore)/html/canvas/CanvasDrawImage.idl \
    $(WebCore)/html/canvas/CanvasDrawPath.idl \
    $(WebCore)/html/canvas/CanvasFillRule.idl \
    $(WebCore)/html/canvas/CanvasFillStrokeStyles.idl \
    $(WebCore)/html/canvas/CanvasFilters.idl \
    $(WebCore)/html/canvas/CanvasGradient.idl \
    $(WebCore)/html/canvas/CanvasImageData.idl \
    $(WebCore)/html/canvas/CanvasImageSmoothing.idl \
    $(WebCore)/html/canvas/CanvasLineCap.idl \
    $(WebCore)/html/canvas/CanvasLineJoin.idl \
    $(WebCore)/html/canvas/CanvasPath.idl \
    $(WebCore)/html/canvas/CanvasPathDrawingStyles.idl \
    $(WebCore)/html/canvas/CanvasPattern.idl \
    $(WebCore)/html/canvas/CanvasRect.idl \
    $(WebCore)/html/canvas/CanvasRenderingContext2D.idl \
    $(WebCore)/html/canvas/CanvasShadowStyles.idl \
    $(WebCore)/html/canvas/CanvasState.idl \
    $(WebCore)/html/canvas/CanvasText.idl \
    $(WebCore)/html/canvas/CanvasTextAlign.idl \
    $(WebCore)/html/canvas/CanvasTextBaseline.idl \
    $(WebCore)/html/canvas/CanvasTextDrawingStyles.idl \
    $(WebCore)/html/canvas/CanvasTransform.idl \
    $(WebCore)/html/canvas/CanvasUserInterface.idl \
    $(WebCore)/html/canvas/EXTBlendMinMax.idl \
    $(WebCore)/html/canvas/EXTColorBufferFloat.idl \
    $(WebCore)/html/canvas/EXTColorBufferHalfFloat.idl \
    $(WebCore)/html/canvas/EXTFragDepth.idl \
    $(WebCore)/html/canvas/EXTShaderTextureLOD.idl \
    $(WebCore)/html/canvas/EXTTextureFilterAnisotropic.idl \
    $(WebCore)/html/canvas/EXTsRGB.idl \
    $(WebCore)/html/canvas/ImageBitmapRenderingContext.idl \
    $(WebCore)/html/canvas/ImageBitmapRenderingContextSettings.idl \
    $(WebCore)/html/canvas/ImageSmoothingQuality.idl \
    $(WebCore)/html/canvas/OESElementIndexUint.idl \
    $(WebCore)/html/canvas/OESStandardDerivatives.idl \
    $(WebCore)/html/canvas/OESTextureFloat.idl \
    $(WebCore)/html/canvas/OESTextureFloatLinear.idl \
    $(WebCore)/html/canvas/OESTextureHalfFloat.idl \
    $(WebCore)/html/canvas/OESTextureHalfFloatLinear.idl \
    $(WebCore)/html/canvas/OESVertexArrayObject.idl \
    $(WebCore)/html/canvas/OffscreenCanvasRenderingContext2D.idl \
    $(WebCore)/html/canvas/PaintRenderingContext2D.idl \
    $(WebCore)/html/canvas/Path2D.idl \
    $(WebCore)/html/canvas/WebGL2RenderingContext.idl \
    $(WebCore)/html/canvas/WebGLActiveInfo.idl \
    $(WebCore)/html/canvas/WebGLBuffer.idl \
    $(WebCore)/html/canvas/WebGLColorBufferFloat.idl \
    $(WebCore)/html/canvas/WebGLCompressedTextureASTC.idl \
    $(WebCore)/html/canvas/WebGLCompressedTextureATC.idl \
    $(WebCore)/html/canvas/WebGLCompressedTextureETC.idl \
    $(WebCore)/html/canvas/WebGLCompressedTextureETC1.idl \
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
    $(WebCore)/html/track/TextTrackCueGeneric.idl \
    $(WebCore)/html/track/TextTrackCueList.idl \
    $(WebCore)/html/track/TextTrackList.idl \
    $(WebCore)/html/track/TrackEvent.idl \
    $(WebCore)/html/track/VTTCue.idl \
    $(WebCore)/html/track/VTTRegion.idl \
    $(WebCore)/html/track/VTTRegionList.idl \
    $(WebCore)/html/track/VideoTrack.idl \
    $(WebCore)/html/track/VideoTrackList.idl \
    $(WebCore)/mathml/MathMLElement.idl \
    $(WebCore)/mathml/MathMLMathElement.idl \
    $(WebCore)/inspector/CommandLineAPIHost.idl \
    $(WebCore)/inspector/InspectorAuditAccessibilityObject.idl \
    $(WebCore)/inspector/InspectorAuditDOMObject.idl \
    $(WebCore)/inspector/InspectorAuditResourcesObject.idl \
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
    $(WebCore)/page/NavigatorID.idl \
    $(WebCore)/page/NavigatorIsLoggedIn.idl \
    $(WebCore)/page/NavigatorLanguage.idl \
    $(WebCore)/page/NavigatorOnLine.idl \
    $(WebCore)/page/NavigatorPlugins.idl \
    $(WebCore)/page/NavigatorServiceWorker.idl \
    $(WebCore)/page/NavigatorShare.idl \
    $(WebCore)/page/Performance.idl \
    $(WebCore)/page/PerformanceEntry.idl \
    $(WebCore)/page/PerformanceMark.idl \
    $(WebCore)/page/PerformanceMeasure.idl \
    $(WebCore)/page/PerformanceNavigation.idl \
    $(WebCore)/page/PerformanceObserver.idl \
    $(WebCore)/page/PerformanceObserverCallback.idl \
    $(WebCore)/page/PerformanceObserverEntryList.idl \
    $(WebCore)/page/PerformancePaintTiming.idl \
    $(WebCore)/page/PerformanceResourceTiming.idl \
    $(WebCore)/page/PerformanceServerTiming.idl \
    $(WebCore)/page/PerformanceTiming.idl \
    $(WebCore)/page/PostMessageOptions.idl \
    $(WebCore)/page/RemoteDOMWindow.idl \
    $(WebCore)/page/ResizeObserver.idl \
    $(WebCore)/page/ResizeObserverCallback.idl \
    $(WebCore)/page/ResizeObserverEntry.idl \
    $(WebCore)/page/Screen.idl \
    $(WebCore)/page/ScrollBehavior.idl \
    $(WebCore)/page/ScrollIntoViewOptions.idl \
    $(WebCore)/page/ScrollLogicalPosition.idl \
    $(WebCore)/page/ScrollOptions.idl \
    $(WebCore)/page/ScrollToOptions.idl \
    $(WebCore)/page/ShareData.idl \
    $(WebCore)/page/UndoItem.idl \
    $(WebCore)/page/UndoManager.idl \
    $(WebCore)/page/UserMessageHandler.idl \
    $(WebCore)/page/UserMessageHandlersNamespace.idl \
    $(WebCore)/page/VisualViewport.idl \
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
    $(WebCore)/svg/SVGGeometryElement.idl \
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
    $(WebCore)/testing/InternalsMapLike.idl \
    $(WebCore)/testing/InternalsSetLike.idl \
    $(WebCore)/testing/MallocStatistics.idl \
    $(WebCore)/testing/MemoryInfo.idl \
    $(WebCore)/testing/MockCDMFactory.idl \
    $(WebCore)/testing/MockContentFilterSettings.idl \
    $(WebCore)/testing/MockPageOverlay.idl \
    $(WebCore)/testing/MockPaymentAddress.idl \
    $(WebCore)/testing/MockPaymentContactFields.idl \
    $(WebCore)/testing/MockPaymentCoordinator.idl \
    $(WebCore)/testing/MockPaymentError.idl \
    $(WebCore)/testing/MockWebAuthenticationConfiguration.idl \
    $(WebCore)/testing/ServiceWorkerInternals.idl \
    $(WebCore)/testing/TypeConversions.idl \
    $(WebCore)/testing/FakeXRBoundsPoint.idl \
    $(WebCore)/testing/FakeXRButtonStateInit.idl \
    $(WebCore)/testing/FakeXRInputSourceInit.idl \
    $(WebCore)/testing/FakeXRRigidTransformInit.idl \
    $(WebCore)/testing/FakeXRViewInit.idl \
    $(WebCore)/testing/WebFakeXRDevice.idl \
    $(WebCore)/testing/WebFakeXRInputController.idl \
    $(WebCore)/testing/WebXRTest.idl \
    $(WebCore)/testing/XRSimulateUserActivationFunction.idl \
    $(WebCore)/workers/AbstractWorker.idl \
    $(WebCore)/workers/DedicatedWorkerGlobalScope.idl \
    $(WebCore)/workers/Worker.idl \
    $(WebCore)/workers/WorkerGlobalScope.idl \
    $(WebCore)/workers/WorkerLocation.idl \
    $(WebCore)/workers/WorkerType.idl \
    $(WebCore)/workers/service/ExtendableEvent.idl \
    $(WebCore)/workers/service/ExtendableEventInit.idl \
    $(WebCore)/workers/service/ExtendableMessageEvent.idl \
    $(WebCore)/workers/service/FetchEvent.idl \
    $(WebCore)/workers/service/ServiceWorker.idl \
    $(WebCore)/workers/service/ServiceWorkerClient.idl \
    $(WebCore)/workers/service/ServiceWorkerClientType.idl \
    $(WebCore)/workers/service/ServiceWorkerClients.idl \
    $(WebCore)/workers/service/ServiceWorkerContainer.idl \
    $(WebCore)/workers/service/ServiceWorkerGlobalScope.idl \
    $(WebCore)/workers/service/ServiceWorkerRegistration.idl \
    $(WebCore)/workers/service/ServiceWorkerUpdateViaCache.idl \
    $(WebCore)/workers/service/ServiceWorkerWindowClient.idl \
    $(WebCore)/worklets/PaintWorkletGlobalScope.idl \
    $(WebCore)/worklets/Worklet.idl \
    $(WebCore)/worklets/WorkletGlobalScope.idl \
    $(WebCore)/xml/DOMParser.idl \
    $(WebCore)/xml/XMLHttpRequest.idl \
    $(WebCore)/xml/XMLHttpRequestEventTarget.idl \
    $(WebCore)/xml/XMLHttpRequestProgressEvent.idl \
    $(WebCore)/xml/XMLHttpRequestUpload.idl \
    $(WebCore)/xml/XMLSerializer.idl \
    $(WebCore)/xml/XPathEvaluator.idl \
    $(WebCore)/xml/XPathExpression.idl \
    $(WebCore)/xml/XPathNSResolver.idl \
    $(WebCore)/xml/XPathResult.idl \
    $(WebCore)/xml/XSLTProcessor.idl \
    InternalSettingsGenerated.idl \
#

# --------

ADDITIONAL_BINDING_IDLS =

ifeq ($(findstring ENABLE_IOS_GESTURE_EVENTS,$(FEATURE_AND_PLATFORM_DEFINES)), ENABLE_IOS_GESTURE_EVENTS)
ADDITIONAL_BINDING_IDLS += GestureEvent.idl
endif

ifeq ($(findstring ENABLE_IOS_TOUCH_EVENTS,$(FEATURE_AND_PLATFORM_DEFINES)), ENABLE_IOS_TOUCH_EVENTS)
ADDITIONAL_BINDING_IDLS += \
    DocumentTouch.idl \
    Touch.idl \
    TouchEvent.idl \
    TouchList.idl
endif

ifeq ($(findstring ENABLE_MAC_GESTURE_EVENTS,$(FEATURE_AND_PLATFORM_DEFINES)), ENABLE_MAC_GESTURE_EVENTS)
ADDITIONAL_BINDING_IDLS += GestureEvent.idl
endif

vpath %.in $(WEBKITADDITIONS_HEADER_SEARCH_PATHS)

ADDITIONAL_EVENT_NAMES =
ADDITIONAL_EVENT_TARGET_FACTORY =

-include WebCoreDerivedSourcesAdditions.make

JS_BINDING_IDLS += $(ADDITIONAL_BINDING_IDLS)

all : $(ADDITIONAL_BINDING_IDLS:%.idl=JS%.h)

vpath %.idl $(BUILT_PRODUCTS_DIR)/usr/local/include $(SDKROOT)/usr/local/include

$(ADDITIONAL_BINDING_IDLS) : % : WebKitAdditions/%
	cp $< .

ifneq ($(findstring ENABLE_IOS_TOUCH_EVENTS,$(FEATURE_AND_PLATFORM_DEFINES)), ENABLE_IOS_TOUCH_EVENTS)
JS_BINDING_IDLS += \
    $(WebCore)/dom/DocumentTouch.idl \
    $(WebCore)/dom/Touch.idl \
    $(WebCore)/dom/TouchEvent.idl \
    $(WebCore)/dom/TouchList.idl
endif

.PHONY : all

JS_DOM_CLASSES=$(basename $(notdir $(JS_BINDING_IDLS)))

JS_DOM_HEADERS=$(filter-out JSEventListener.h, $(JS_DOM_CLASSES:%=JS%.h))
JS_DOM_IMPLEMENTATIONS=$(filter-out JSEventListener.cpp, $(JS_DOM_CLASSES:%=JS%.cpp))

WEB_DOM_HEADERS :=

all : \
    $(JS_DOM_HEADERS) \
    $(JS_DOM_IMPLEMENTATIONS) \
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
    HTMLElementFactory.cpp \
    HTMLElementFactory.h \
    HTMLElementTypeHelpers.h \
    HTMLEntityTable.cpp \
    HTMLNames.cpp \
    HTMLNames.h \
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
    StyleBuilderGenerated.cpp \
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

# CSS property names and value keywords

WEBCORE_CSS_PROPERTY_NAMES := $(WebCore)/css/CSSProperties.json
WEBCORE_CSS_VALUE_KEYWORDS := $(WebCore)/css/CSSValueKeywords.in
WEBCORE_CSS_VALUE_KEYWORDS := $(WEBCORE_CSS_VALUE_KEYWORDS) $(WebCore)/css/SVGCSSValueKeywords.in

CSS_PROPERTY_NAME_FILES = \
    CSSPropertyNames.cpp \
    CSSPropertyNames.h \
    StyleBuilderGenerated.cpp \
    StylePropertyShorthandFunctions.cpp \
    StylePropertyShorthandFunctions.h \
#
CSS_PROPERTY_NAME_FILES_PATTERNS = $(subst .,%,$(CSS_PROPERTY_NAME_FILES))

all : $(CSS_PROPERTY_NAME_FILES)
$(CSS_PROPERTY_NAME_FILES_PATTERNS) : $(WEBCORE_CSS_PROPERTY_NAMES) css/makeprop.pl $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) -pe '' $(WEBCORE_CSS_PROPERTY_NAMES) > CSSProperties.json
	$(PERL) "$(WebCore)/css/makeprop.pl" --defines "$(FEATURE_AND_PLATFORM_DEFINES)"

CSS_VALUE_KEYWORD_FILES = \
    CSSValueKeywords.h \
    CSSValueKeywords.cpp \
#
CSS_VALUE_KEYWORD_FILES_PATTERNS = $(subst .,%,$(CSS_VALUE_KEYWORD_FILES))

all : $(CSS_VALUE_KEYWORD_FILES)
$(CSS_VALUE_KEYWORD_FILES_PATTERNS) : $(WEBCORE_CSS_VALUE_KEYWORDS) css/makevalues.pl bindings/scripts/preprocessor.pm $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) -pe '' $(WEBCORE_CSS_VALUE_KEYWORDS) > CSSValueKeywords.in
	$(PERL) "$(WebCore)/css/makevalues.pl" --defines "$(FEATURE_AND_PLATFORM_DEFINES)"

# --------

# CSS Selector pseudo type name to value map.

SelectorPseudoClassAndCompatibilityElementMap.cpp : $(WebCore)/css/makeSelectorPseudoClassAndCompatibilityElementMap.py $(WebCore)/css/SelectorPseudoClassAndCompatibilityElementMap.in $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PYTHON) "$(WebCore)/css/makeSelectorPseudoClassAndCompatibilityElementMap.py" $(WebCore)/css/SelectorPseudoClassAndCompatibilityElementMap.in gperf "$(FEATURE_AND_PLATFORM_DEFINES)"

SelectorPseudoElementTypeMap.cpp : $(WebCore)/css/makeSelectorPseudoElementsMap.py $(WebCore)/css/SelectorPseudoElementTypeMap.in $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PYTHON) "$(WebCore)/css/makeSelectorPseudoElementsMap.py" $(WebCore)/css/SelectorPseudoElementTypeMap.in gperf "$(FEATURE_AND_PLATFORM_DEFINES)"

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

HTTP_HEADER_NAMES_FILES = \
    HTTPHeaderNames.cpp \
    HTTPHeaderNames.gperf \
    HTTPHeaderNames.h \
#
HTTP_HEADER_NAMES_FILES_PATTERNS = $(subst .,%,$(HTTP_HEADER_NAMES_FILES))

all : $(HTTP_HEADER_NAMES_FILES)
$(HTTP_HEADER_NAMES_FILES_PATTERNS) : platform/network/HTTPHeaderNames.in $(WebCore)/platform/network/create-http-header-name-table
	$(PYTHON) $(WebCore)/platform/network/create-http-header-name-table $(WebCore)/platform/network/HTTPHeaderNames.in gperf

# --------

# color names

ColorData.cpp : platform/ColorData.gperf $(WebCore)/make-hash-tools.pl
	$(PERL) $(WebCore)/make-hash-tools.pl . $(WebCore)/platform/ColorData.gperf

# --------

# user agent style sheets

USER_AGENT_STYLE_SHEETS = $(WebCore)/css/html.css $(WebCore)/css/dialog.css $(WebCore)/css/quirks.css $(WebCore)/css/plugIns.css $(WebCore)/css/svg.css

ifeq ($(findstring ENABLE_MATHML,$(FEATURE_AND_PLATFORM_DEFINES)), ENABLE_MATHML)
    USER_AGENT_STYLE_SHEETS += $(WebCore)/css/mathml.css
endif

ifeq ($(findstring ENABLE_VIDEO,$(FEATURE_AND_PLATFORM_DEFINES)), ENABLE_VIDEO)
    USER_AGENT_STYLE_SHEETS += $(WebCore)/css/mediaControls.css
endif

ifeq ($(findstring ENABLE_FULLSCREEN_API,$(FEATURE_AND_PLATFORM_DEFINES)), ENABLE_FULLSCREEN_API)
    USER_AGENT_STYLE_SHEETS += $(WebCore)/css/fullscreen.css
endif

ifeq ($(findstring ENABLE_SERVICE_CONTROLS,$(FEATURE_AND_PLATFORM_DEFINES)), ENABLE_SERVICE_CONTROLS)
    USER_AGENT_STYLE_SHEETS += $(WebCore)/html/shadow/mac/imageControlsMac.css
endif

USER_AGENT_STYLE_SHEETS += $(WebCore)/Modules/plugins/QuickTimePluginReplacement.css

ifeq ($(findstring ENABLE_METER_ELEMENT,$(FEATURE_AND_PLATFORM_DEFINES)), ENABLE_METER_ELEMENT)
	USER_AGENT_STYLE_SHEETS += $(WebCore)/html/shadow/meterElementShadow.css
endif

UserAgentStyleSheets.h : css/make-css-file-arrays.pl bindings/scripts/preprocessor.pm $(USER_AGENT_STYLE_SHEETS) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) $< --defines "$(FEATURE_AND_PLATFORM_DEFINES)" $@ UserAgentStyleSheetsData.cpp $(USER_AGENT_STYLE_SHEETS)

# --------

# user agent scripts

USER_AGENT_SCRIPTS = $(WebCore)/Modules/plugins/QuickTimePluginReplacement.js

USER_AGENT_SCRIPTS_FILES = \
    UserAgentScripts.h \
    UserAgentScriptsData.cpp \
#
USER_AGENT_SCRIPTS_FILES_PATTERNS = $(subst .,%,$(USER_AGENT_SCRIPTS_FILES))

all : $(USER_AGENT_SCRIPTS_FILES)

$(USER_AGENT_SCRIPTS_FILES_PATTERNS) : $(JavaScriptCore_SCRIPTS_DIR)/make-js-file-arrays.py $(USER_AGENT_SCRIPTS)
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/make-js-file-arrays.py -n WebCore $(USER_AGENT_SCRIPTS_FILES) $(USER_AGENT_SCRIPTS)

# --------

# plug-ins resources

PLUG_INS_RESOURCES = $(WebCore)/Resources/plugIns.js

PlugInsResources.h : css/make-css-file-arrays.pl bindings/scripts/preprocessor.pm $(PLUG_INS_RESOURCES) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) $< --defines "$(FEATURE_AND_PLATFORM_DEFINES)" $@ PlugInsResourcesData.cpp $(PLUG_INS_RESOURCES)

# --------

WEBKIT_FONT_FAMILY_NAME_FILES = \
    WebKitFontFamilyNames.cpp \
    WebKitFontFamilyNames.h \
#
WEBKIT_FONT_FAMILY_NAME_FILES_PATTERNS = $(subst .,%,$(WEBKIT_FONT_FAMILY_NAME_FILES))

all : $(WEBKIT_FONT_FAMILY_NAME_FILES)
$(WEBKIT_FONT_FAMILY_NAME_FILES_PATTERNS): dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm css/WebKitFontFamilyNames.in
	$(PERL) $< --fonts $(WebCore)/css/WebKitFontFamilyNames.in

# HTML tag and attribute names

HTML_TAG_FILES = \
    JSHTMLElementWrapperFactory.cpp \
    JSHTMLElementWrapperFactory.h \
    HTMLElementFactory.cpp \
    HTMLElementFactory.h \
    HTMLElementTypeHelpers.h \
    HTMLNames.cpp \
    HTMLNames.h \
#
HTML_TAG_FILES_PATTERNS = $(subst .,%,$(HTML_TAG_FILES))

all : $(HTML_TAG_FILES)

$(HTML_TAG_FILES_PATTERNS) : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm html/HTMLTagNames.in html/HTMLAttributeNames.in $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) $< --tags $(WebCore)/html/HTMLTagNames.in --attrs $(WebCore)/html/HTMLAttributeNames.in --factory --wrapperFactory --extraDefines "$(FEATURE_AND_PLATFORM_DEFINES)"

XMLNSNames.cpp : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm xml/xmlnsattrs.in
	$(PERL) $< --attrs $(WebCore)/xml/xmlnsattrs.in

XMLNames.cpp : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm xml/xmlattrs.in
	$(PERL) $< --attrs $(WebCore)/xml/xmlattrs.in

# --------

# SVG tag and attribute names, and element factory

SVG_TAG_FILES = \
    JSSVGElementWrapperFactory.cpp \
    JSSVGElementWrapperFactory.h \
    SVGElementFactory.cpp \
    SVGElementFactory.h \
    SVGElementTypeHelpers.h \
    SVGNames.cpp \
    SVGNames.h \
#
SVG_TAG_FILES_PATTERNS = $(subst .,%,$(SVG_TAG_FILES))

all : $(SVG_TAG_FILES)

$(SVG_TAG_FILES_PATTERNS) : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm svg/svgtags.in svg/svgattrs.in $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) $< --tags $(WebCore)/svg/svgtags.in --attrs $(WebCore)/svg/svgattrs.in --factory --wrapperFactory --extraDefines "$(FEATURE_AND_PLATFORM_DEFINES)"

XLinkNames.cpp : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm svg/xlinkattrs.in
	$(PERL) $< --attrs $(WebCore)/svg/xlinkattrs.in

# --------

# Register event constructors and targets

EVENT_NAMES = EventNames.in $(ADDITIONAL_EVENT_NAMES)

EVENT_FACTORY_FILES = \
    EventFactory.cpp \
    EventHeaders.h \
    EventInterfaces.h \
#
EVENT_FACTORY_PATTERNS = $(subst .,%,$(EVENT_FACTORY_FILES))

all : $(EVENT_FACTORY_FILES)
$(EVENT_FACTORY_PATTERNS) : dom/make_event_factory.pl $(EVENT_NAMES)
	$(PERL) $< $(addprefix --input , $(filter-out $(WebCore)/dom/make_event_factory.pl, $^))

EVENT_TARGET_FACTORY = EventTargetFactory.in $(ADDITIONAL_EVENT_TARGET_FACTORY)

EVENT_TARGET_FACTORY_FILES = \
    EventTargetFactory.cpp \
    EventTargetHeaders.h \
    EventTargetInterfaces.h \
#
EVENT_TARGET_FACTORY_PATTERNS = $(subst .,%,$(EVENT_TARGET_FACTORY_FILES))

all : $(EVENT_TARGET_FACTORY_FILES)
$(EVENT_TARGET_FACTORY_PATTERNS) : dom/make_event_factory.pl $(EVENT_TARGET_FACTORY)
	$(PERL) $< $(addprefix --input , $(filter-out $(WebCore)/dom/make_event_factory.pl, $^))

# --------

# MathML tag and attribute names, and element factory

MATH_ML_GENERATED_FILES = \
    JSMathMLElementWrapperFactory.cpp \
    JSMathMLElementWrapperFactory.h \
    MathMLElementFactory.cpp \
    MathMLElementFactory.h \
    MathMLElementTypeHelpers.h \
    MathMLNames.cpp \
    MathMLNames.h \
#
MATH_ML_GENERATED_PATTERNS = $(subst .,%,$(MATH_ML_GENERATED_FILES))

all : $(MATH_ML_GENERATED_FILES)
$(MATH_ML_GENERATED_PATTERNS) : dom/make_names.pl bindings/scripts/Hasher.pm bindings/scripts/StaticString.pm mathml/mathtags.in mathml/mathattrs.in
	$(PERL) $< --tags $(WebCore)/mathml/mathtags.in --attrs $(WebCore)/mathml/mathattrs.in --factory --wrapperFactory

# --------

# Internal Settings

GENERATE_SETTINGS_TEMPLATES = \
    $(WebCore)/Scripts/SettingsTemplates/InternalSettingsGenerated.cpp.erb \
    $(WebCore)/Scripts/SettingsTemplates/InternalSettingsGenerated.idl.erb \
    $(WebCore)/Scripts/SettingsTemplates/InternalSettingsGenerated.h.erb \
    $(WebCore)/Scripts/SettingsTemplates/Settings.cpp.erb \
    $(WebCore)/Scripts/SettingsTemplates/Settings.h.erb \
#
GENERATE_SETTINGS_FILES = $(basename $(notdir $(GENERATE_SETTINGS_TEMPLATES)))
GENERATE_SETTINGS_PATTERNS = $(subst .,%,$(GENERATE_SETTINGS_FILES))

all : $(GENERATE_SETTINGS_FILES)
$(GENERATE_SETTINGS_PATTERNS) : $(WebCore)/Scripts/GenerateSettings.rb $(GENERATE_SETTINGS_TEMPLATES) page/Settings.yaml
	$(RUBY) $< --input $(WebCore)/page/Settings.yaml

# --------

# WHLSL Standard Library

all : WHLSLStandardLibrary.h WHLSLStandardLibraryFunctionMap.cpp

WHLSLStandardLibrary.h : $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl $(WebCore)/Modules/webgpu/WHLSL/WHLSLStandardLibrary.txt
	bash -c "$(PERL) $< WHLSLStandardLibrary <(gzip -cn $(WebCore)/Modules/webgpu/WHLSL/WHLSLStandardLibrary.txt) $@"

WHLSLStandardLibraryFunctionMap.cpp : $(WebCore)/Modules/webgpu/WHLSL/WHLSLBuildStandardLibraryFunctionMap.py $(WebCore)/Modules/webgpu/WHLSL/WHLSLStandardLibrary.txt
	$(PYTHON) $< $(WebCore)/Modules/webgpu/WHLSL/WHLSLStandardLibrary.txt $@

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
    $(WebCore)/accessibility \
    $(WebCore)/animation \
    $(WebCore)/css \
    $(WebCore)/css/typedom \
    $(WebCore)/crypto \
    $(WebCore)/dom \
    $(WebCore)/fileapi \
    $(WebCore)/html \
    $(WebCore)/html/canvas \
    $(WebCore)/html/shadow \
    $(WebCore)/html/track \
    $(WebCore)/inspector \
    $(WebCore)/loader/appcache \
    $(WebCore)/mathml \
    $(WebCore)/page \
    $(WebCore)/plugins \
    $(WebCore)/storage \
    $(WebCore)/svg \
    $(WebCore)/testing \
    $(WebCore)/workers \
    $(WebCore)/worklets \
    $(WebCore)/xml

IDL_COMMON_ARGS = $(IDL_INCLUDES:%=--include %) --write-dependencies --outputDir .

JS_BINDINGS_SCRIPTS = $(COMMON_BINDINGS_SCRIPTS) bindings/scripts/CodeGeneratorJS.pm

SUPPLEMENTAL_DEPENDENCY_FILE = SupplementalDependencies.txt
SUPPLEMENTAL_MAKEFILE_DEPS = SupplementalDependencies.dep
ISO_SUBSPACES_HEADER_FILE = DOMIsoSubspaces.h
WINDOW_CONSTRUCTORS_FILE = DOMWindowConstructors.idl
WORKERGLOBALSCOPE_CONSTRUCTORS_FILE = WorkerGlobalScopeConstructors.idl
DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE = DedicatedWorkerGlobalScopeConstructors.idl
SERVICEWORKERGLOBALSCOPE_CONSTRUCTORS_FILE = ServiceWorkerGlobalScopeConstructors.idl
WORKLETGLOBALSCOPE_CONSTRUCTORS_FILE = WorkletGlobalScopeConstructors.idl
PAINTWORKLETGLOBALSCOPE_CONSTRUCTORS_FILE = PaintWorkletGlobalScopeConstructors.idl
IDL_FILES_TMP = idl_files.tmp
IDL_ATTRIBUTES_FILE = $(WebCore)/bindings/scripts/IDLAttributes.json

# The following lines get a newline character stored in a variable.
# See <http://stackoverflow.com/questions/7039811/how-do-i-process-extremely-long-lists-of-files-in-a-make-recipe>.
define NL


endef


IDL_INTERMEDIATE_FILES = \
    $(SUPPLEMENTAL_MAKEFILE_DEPS) \
    $(SUPPLEMENTAL_DEPENDENCY_FILE) \
    $(ISO_SUBSPACES_HEADER_FILE) \
    $(WINDOW_CONSTRUCTORS_FILE) \
    $(WORKERGLOBALSCOPE_CONSTRUCTORS_FILE) \
    $(DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE) \
    $(SERVICEWORKERGLOBALSCOPE_CONSTRUCTORS_FILE) \
    $(WORKLETGLOBALSCOPE_CONSTRUCTORS_FILE) \
    $(PAINTWORKLETGLOBALSCOPE_CONSTRUCTORS_FILE)
#
IDL_INTERMEDIATE_PATTERNS = $(subst .,%,$(IDL_INTERMEDIATE_FILES))

$(IDL_INTERMEDIATE_PATTERNS) : $(PREPROCESS_IDLS_SCRIPTS) $(JS_BINDING_IDLS) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES) DerivedSources.make $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(foreach f,$(JS_BINDING_IDLS),@echo $(f)>>$(IDL_FILES_TMP)$(NL))
	$(PERL) $(WebCore)/bindings/scripts/preprocess-idls.pl --defines "$(FEATURE_AND_PLATFORM_DEFINES) LANGUAGE_JAVASCRIPT" --idlFilesList $(IDL_FILES_TMP) --supplementalDependencyFile $(SUPPLEMENTAL_DEPENDENCY_FILE) --isoSubspacesHeaderFile $(ISO_SUBSPACES_HEADER_FILE) --windowConstructorsFile $(WINDOW_CONSTRUCTORS_FILE) --workerGlobalScopeConstructorsFile $(WORKERGLOBALSCOPE_CONSTRUCTORS_FILE) --dedicatedWorkerGlobalScopeConstructorsFile $(DEDICATEDWORKERGLOBALSCOPE_CONSTRUCTORS_FILE) --serviceWorkerGlobalScopeConstructorsFile $(SERVICEWORKERGLOBALSCOPE_CONSTRUCTORS_FILE) --workletGlobalScopeConstructorsFile $(WORKLETGLOBALSCOPE_CONSTRUCTORS_FILE) --paintWorkletGlobalScopeConstructorsFile $(PAINTWORKLETGLOBALSCOPE_CONSTRUCTORS_FILE) --supplementalMakefileDeps $(SUPPLEMENTAL_MAKEFILE_DEPS)
	$(DELETE) $(IDL_FILES_TMP)

JS%.cpp JS%.h : %.idl $(JS_BINDINGS_SCRIPTS) $(IDL_ATTRIBUTES_FILE) $(IDL_INTERMEDIATE_FILES) $(FEATURE_AND_PLATFORM_DEFINE_DEPENDENCIES)
	$(PERL) $(WebCore)/bindings/scripts/generate-bindings.pl $(IDL_COMMON_ARGS) --defines "$(FEATURE_AND_PLATFORM_DEFINES) LANGUAGE_JAVASCRIPT" --generator JS --idlAttributesFile $(IDL_ATTRIBUTES_FILE) --supplementalDependencyFile $(SUPPLEMENTAL_DEPENDENCY_FILE) $<

ifneq ($(NO_SUPPLEMENTAL_FILES),1)
-include $(SUPPLEMENTAL_MAKEFILE_DEPS)
endif

ifneq ($(NO_SUPPLEMENTAL_FILES),1)
-include $(JS_DOM_HEADERS:.h=.dep)
endif

# Inspector interfaces

all : CommandLineAPIModuleSource.h

CommandLineAPIModuleSource.h : CommandLineAPIModuleSource.js
	echo "//# sourceURL=__InjectedScript_CommandLineAPIModuleSource.js" > ./CommandLineAPIModuleSource.min.js
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/jsmin.py <$(WebCore)/inspector/CommandLineAPIModuleSource.js >> ./CommandLineAPIModuleSource.min.js
	$(PERL) $(JavaScriptCore_SCRIPTS_DIR)/xxd.pl CommandLineAPIModuleSource_js ./CommandLineAPIModuleSource.min.js CommandLineAPIModuleSource.h
	$(DELETE) CommandLineAPIModuleSource.min.js

# WebCore JS Builtins

WebCore_BUILTINS_SOURCES = \
    $(WebCore)/Modules/mediastream/RTCPeerConnection.js \
    $(WebCore)/Modules/mediastream/RTCPeerConnectionInternals.js \
    $(WebCore)/Modules/streams/ByteLengthQueuingStrategy.js \
    $(WebCore)/Modules/streams/CountQueuingStrategy.js \
    $(WebCore)/Modules/streams/ReadableByteStreamController.js \
    $(WebCore)/Modules/streams/ReadableByteStreamInternals.js \
    $(WebCore)/Modules/streams/ReadableStream.js \
    $(WebCore)/Modules/streams/ReadableStreamBYOBRequest.js \
    $(WebCore)/Modules/streams/ReadableStreamDefaultController.js \
    $(WebCore)/Modules/streams/ReadableStreamInternals.js \
    $(WebCore)/Modules/streams/ReadableStreamBYOBReader.js \
    $(WebCore)/Modules/streams/ReadableStreamDefaultReader.js \
    $(WebCore)/Modules/streams/StreamInternals.js \
    $(WebCore)/Modules/streams/WritableStream.js \
    $(WebCore)/Modules/streams/WritableStreamInternals.js \
    $(WebCore)/bindings/js/JSDOMBindingInternals.js \
#

BUILTINS_GENERATOR_SCRIPTS = \
    $(JavaScriptCore_SCRIPTS_DIR)/wkbuiltins.py \
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
WebCore_BUILTINS_WRAPPERS_PATTERNS = $(subst .,%,$(WebCore_BUILTINS_WRAPPERS))

# Adding/removing scripts should trigger regeneration, but changing which builtins are
# generated should not affect other builtins when not passing '--combined' to the generator.

WebCore_BUILTINS_SOURCES_LIST : $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py DerivedSources.make
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py '$(WebCore_BUILTINS_SOURCES)' $@

WebCore_BUILTINS_DEPENDENCIES_LIST : $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py DerivedSources.make
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/UpdateContents.py '$(BUILTINS_GENERATOR_SCRIPTS)' $@

$(WebCore_BUILTINS_WRAPPERS_PATTERNS) : $(WebCore_BUILTINS_SOURCES) WebCore_BUILTINS_SOURCES_LIST $(BUILTINS_GENERATOR_SCRIPTS) WebCore_BUILTINS_DEPENDENCIES_LIST
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/generate-js-builtins.py --wrappers-only --output-directory . --framework WebCore $(WebCore_BUILTINS_SOURCES)

%Builtins.h: %.js $(BUILTINS_GENERATOR_SCRIPTS) WebCore_BUILTINS_DEPENDENCIES_LIST
	$(PYTHON) $(JavaScriptCore_SCRIPTS_DIR)/generate-js-builtins.py --output-directory . --framework WebCore $<

all : $(notdir $(WebCore_BUILTINS_SOURCES:%.js=%Builtins.h)) $(WebCore_BUILTINS_WRAPPERS)

# ------------------------

