/*
 *  Copyright (c) 2015, Canon Inc. All rights reserved.
 *  Copyright (C) 2018-2020 Apple Inc. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1.  Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *  2.  Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *  3.  Neither the name of Canon Inc. nor the names of
 *      its contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *  THIS SOFTWARE IS PROVIDED BY CANON INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL CANON INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/BuiltinUtils.h>

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebCoreBuiltinNamesAdditions.h>
#endif

namespace WebCore {

#if !defined(WEBCORE_ADDITIONAL_PRIVATE_IDENTIFIERS)
#define WEBCORE_ADDITIONAL_PRIVATE_IDENTIFIERS(macro)
#endif

#define WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(macro) \
    macro(Animation) \
    macro(AnimationEffect) \
    macro(AnimationPlaybackEvent) \
    macro(AnimationTimeline) \
    macro(ApplePaySession) \
    macro(ApplePaySetup) \
    macro(ApplePaySetupFeature) \
    macro(AttachmentElement) \
    macro(Audio) \
    macro(AudioBufferSourceNode) \
    macro(AudioContext) \
    macro(AudioListener) \
    macro(AuthenticatorAssertionResponse) \
    macro(AuthenticatorAttestationResponse) \
    macro(AuthenticatorResponse) \
    macro(BaseAudioContext) \
    macro(BeforeLoadEvent) \
    macro(BlobEvent) \
    macro(Cache) \
    macro(CacheStorage) \
    macro(Client) \
    macro(Clients) \
    macro(Clipboard) \
    macro(ClipboardItem) \
    macro(Credential) \
    macro(CredentialsContainer) \
    macro(CSSAnimation) \
    macro(CSSImageValue) \
    macro(CSSNumericValue) \
    macro(CSSPaintSize) \
    macro(CSSStyleValue) \
    macro(CSSTransition) \
    macro(CSSUnitValue) \
    macro(CSSUnparsedValue) \
    macro(CustomElementRegistry) \
    macro(Database) \
    macro(DataTransferItem) \
    macro(DataTransferItemList) \
    macro(DocumentTimeline) \
    macro(EnterPictureInPictureEvent) \
    macro(ExtendableEvent) \
    macro(ExtendableMessageEvent) \
    macro(FakeXRDevice) \
    macro(FakeXRInputController) \
    macro(FetchEvent) \
    macro(FileSystem) \
    macro(FileSystemDirectoryEntry) \
    macro(FileSystemDirectoryReader) \
    macro(FileSystemEntry) \
    macro(FileSystemFileEntry) \
    macro(Gamepad) \
    macro(GamepadButton) \
    macro(GamepadEvent) \
    macro(GPU) \
    macro(GPUAdapter) \
    macro(GPUBindGroup) \
    macro(GPUBindGroupLayout) \
    macro(GPUBuffer) \
    macro(GPUBufferUsage) \
    macro(GPUCanvasContext) \
    macro(GPUColorWrite) \
    macro(GPUCommandBuffer) \
    macro(GPUCommandEncoder) \
    macro(GPUComputePassEncoder) \
    macro(GPUComputePipeline) \
    macro(GPUDevice) \
    macro(GPUOutOfMemoryError) \
    macro(GPUPipelineLayout) \
    macro(GPUProgrammablePassEncoder) \
    macro(GPUQueue) \
    macro(GPURenderPassEncoder) \
    macro(GPURenderPipeline) \
    macro(GPUSampler) \
    macro(GPUShaderModule) \
    macro(GPUShaderStage) \
    macro(GPUSwapChain) \
    macro(GPUTexture) \
    macro(GPUTextureUsage) \
    macro(GPUTextureView) \
    macro(GPUUncapturedErrorEvent) \
    macro(GPUValidationError) \
    macro(HighlightMap) \
    macro(HighlightRangeGroup) \
    macro(HTMLAttachmentElement) \
    macro(HTMLAudioElement) \
    macro(HTMLDialogElement) \
    macro(HTMLDataListElement) \
    macro(HTMLMenuItemElement) \
    macro(HTMLKeygenElement) \
    macro(HTMLSlotElement) \
    macro(Headers) \
    macro(IDBCursor) \
    macro(IDBCursorWithValue) \
    macro(IDBDatabase) \
    macro(IDBFactory) \
    macro(IDBIndex) \
    macro(IDBKeyRange) \
    macro(IDBObjectStore) \
    macro(IDBOpenDBRequest) \
    macro(IDBRequest) \
    macro(IDBTransaction) \
    macro(IDBVersionChangeEvent) \
    macro(ImageBitmap) \
    macro(ImageBitmapRenderingContext) \
    macro(IdleDeadline) \
    macro(InputEvent) \
    macro(IntersectionObserver) \
    macro(IntersectionObserverEntry) \
    macro(KeyframeEffect) \
    macro(MediaCapabilities) \
    macro(MediaCapabilitiesInfo) \
    macro(MediaDevices) \
    macro(MediaEncryptedEvent) \
    macro(MediaKeyMessageEvent) \
    macro(MediaKeySession) \
    macro(MediaKeyStatusMap) \
    macro(MediaKeySystemAccess) \
    macro(MediaKeys) \
    macro(MediaQueryListEvent) \
    macro(MediaRecorder) \
    macro(MediaRecorderErrorEvent) \
    macro(MediaSource) \
    macro(MediaStream) \
    macro(MediaStreamTrack) \
    macro(MerchantValidationEvent) \
    macro(ModernMediaControls) \
    macro(NavigatorCredentials) \
    macro(NavigatorMediaDevices) \
    macro(NavigatorUserMedia) \
    macro(OfflineAudioContext) \
    macro(OffscreenCanvas) \
    macro(OffscreenCanvasRenderingContext2D) \
    macro(OscillatorNode) \
    macro(PaintRenderingContext2D) \
    macro(PannerNode) \
    macro(PaymentAddress) \
    macro(PaymentMethodChangeEvent) \
    macro(PaymentRequest) \
    macro(PaymentRequestUpdateEvent) \
    macro(PaymentResponse) \
    macro(PictureInPictureWindow) \
    macro(SQLError) \
    macro(SQLResultSet) \
    macro(SQLResultSetRowList) \
    macro(SQLTransaction) \
    macro(PaintWorkletGlobalScope) \
    macro(PerformanceEntry) \
    macro(PerformanceEntryList) \
    macro(PerformanceMark) \
    macro(PerformanceMeasure) \
    macro(PerformanceObserver) \
    macro(PerformanceObserverEntryList) \
    macro(PerformancePaintTiming) \
    macro(PerformanceResourceTiming) \
    macro(PerformanceServerTiming) \
    macro(PointerEvent) \
    macro(PublicKeyCredential) \
    macro(ResizeObserver) \
    macro(ResizeObserverEntry) \
    macro(RTCCertificate) \
    macro(RTCDTMFSender) \
    macro(RTCDTMFToneChangeEvent) \
    macro(RTCDataChannel) \
    macro(RTCDataChannelEvent) \
    macro(RTCIceCandidate) \
    macro(RTCIceTransport) \
    macro(RTCPeerConnection) \
    macro(RTCPeerConnectionIceEvent) \
    macro(RTCRtpReceiver) \
    macro(RTCRtpSender) \
    macro(RTCRtpTransceiver) \
    macro(RTCSessionDescription) \
    macro(RTCStatsReport) \
    macro(RTCTrackEvent) \
    macro(ReadableByteStreamController) \
    macro(ReadableStream) \
    macro(ReadableStreamBYOBReader) \
    macro(ReadableStreamBYOBRequest) \
    macro(ReadableStreamDefaultController) \
    macro(ReadableStreamDefaultReader) \
    macro(RemotePlayback) \
    macro(Request) \
    macro(Response) \
    macro(ScreenLuminance) \
    macro(ServiceWorker) \
    macro(ServiceWorkerContainer) \
    macro(ServiceWorkerGlobalScope) \
    macro(ServiceWorkerRegistration) \
    macro(ShadowRoot) \
    macro(StaticRange) \
    macro(StylePropertyMapReadOnly) \
    macro(StylePropertyMap) \
    macro(TextTrackCue) \
    macro(UndoItem) \
    macro(UndoManager) \
    macro(VisualViewport) \
    macro(WebGL2RenderingContext) \
    macro(WebGLVertexArrayObject) \
    macro(WebGLTransformFeedback) \
    macro(WebKitMediaKeyError) \
    macro(WebKitMediaKeyMessageEvent) \
    macro(WebKitMediaKeyNeededEvent) \
    macro(WebKitMediaKeySession) \
    macro(WebKitMediaKeys) \
    macro(WebSocket) \
    macro(WindowClient) \
    macro(Worklet) \
    macro(WorkletGlobalScope) \
    macro(WritableStream) \
    macro(WritableStreamDefaultController) \
    macro(WritableStreamDefaultWriter) \
    macro(XMLHttpRequest) \
    macro(XRBoundedReferenceSpace) \
    macro(XRFrame) \
    macro(XRInputSource) \
    macro(XRInputSourceArray) \
    macro(XRInputSourceEvent) \
    macro(XRInputSourcesChangeEvent) \
    macro(XRLayer) \
    macro(XRPose) \
    macro(XRReferenceSpace) \
    macro(XRReferenceSpaceEvent) \
    macro(XRRenderState) \
    macro(XRRigidTransform) \
    macro(XRSession) \
    macro(XRSessionEvent) \
    macro(XRSpace) \
    macro(XRSystem) \
    macro(XRTest) \
    macro(XRView) \
    macro(XRViewerPose) \
    macro(XRViewport) \
    macro(XRWebGLLayer) \
    macro(abortAlgorithm) \
    macro(abortSteps) \
    macro(appendFromJS) \
    macro(associatedReadableByteStreamController) \
    macro(autoAllocateChunkSize) \
    macro(backingMap) \
    macro(backingSet) \
    macro(backpressure) \
    macro(blur) \
    macro(body) \
    macro(byobRequest) \
    macro(caches) \
    macro(cancel) \
    macro(cancelAlgorithm) \
    macro(cancelAnimationFrame) \
    macro(cancelIdleCallback) \
    macro(cloneArrayBuffer) \
    macro(close) \
    macro(closeAlgorithm) \
    macro(closeRequest) \
    macro(closeRequested) \
    macro(closed) \
    macro(closedPromise) \
    macro(closedPromiseCapability) \
    macro(collectMatchingElementsInFlatTree) \
    macro(consume) \
    macro(consumeChunk) \
    macro(controlledReadableStream) \
    macro(controller) \
    macro(createImageBitmap) \
    macro(createReadableStream) \
    macro(customElements) \
    macro(disturbed) \
    macro(document) \
    macro(errorSteps) \
    macro(failureKind) \
    macro(fetch) \
    macro(fetchRequest) \
    macro(fillFromJS) \
    macro(finishConsumingStream) \
    macro(focus) \
    macro(frames) \
    macro(getTracks) \
    macro(getUserMedia) \
    macro(gpu) \
    macro(header) \
    macro(href) \
    macro(inFlightCloseRequest) \
    macro(inFlightWriteRequest) \
    macro(indexedDB) \
    macro(initializeWith) \
    macro(isDisturbed) \
    macro(isLoading) \
    macro(isSecureContext) \
    macro(localStreams) \
    macro(location) \
    macro(makeGetterTypeError) \
    macro(makeThisTypeError) \
    macro(matchingElementInFlatTree) \
    macro(mediaStreamTrackConstraints) \
    macro(openDatabase) \
    macro(ontouchcancel) \
    macro(ontouchend) \
    macro(ontouchmove) \
    macro(ontouchstart) \
    macro(ontouchforcechange) \
    macro(onuncapturederror) \
    macro(onvrdisplayactivate) \
    macro(onvrdisplayblur) \
    macro(onvrdisplayconnect) \
    macro(onvrdisplaydeactivate) \
    macro(onvrdisplaydisconnect) \
    macro(onvrdisplayfocus) \
    macro(onvrdisplaypresentchange) \
    macro(opener) \
    macro(operations) \
    macro(ownerReadableStream) \
    macro(parent) \
    macro(pendingPullIntos) \
    macro(postMessage) \
    macro(privateGetStats) \
    macro(pull) \
    macro(pullAgain) \
    macro(pullAlgorithm) \
    macro(pulling) \
    macro(queue) \
    macro(queuedAddIceCandidate) \
    macro(queuedCreateAnswer) \
    macro(queuedCreateOffer) \
    macro(queuedSetLocalDescription) \
    macro(queuedSetRemoteDescription) \
    macro(readIntoRequests) \
    macro(readRequests) \
    macro(readableByteStreamAPIEnabled) \
    macro(readableStreamController) \
    macro(reader) \
    macro(readyPromise) \
    macro(readyPromiseCapability) \
    macro(requestAnimationFrame) \
    macro(requestIdleCallback) \
    macro(response) \
    macro(responseCacheIsValid) \
    macro(retrieveResponse) \
    macro(self) \
    macro(setBody) \
    macro(setBodyFromInputRequest) \
    macro(setStatus) \
    macro(showModalDialog) \
    macro(start) \
    macro(startConsumingStream) \
    macro(started) \
    macro(startedPromise) \
    macro(state) \
    macro(storedError) \
    macro(strategy) \
    macro(strategyHWM) \
    macro(strategySizeAlgorithm) \
    macro(stream) \
    macro(streamClosed) \
    macro(streamClosing) \
    macro(streamErrored) \
    macro(streamReadable) \
    macro(streamWaiting) \
    macro(streamWritable) \
    macro(structuredCloneArrayBuffer) \
    macro(structuredCloneArrayBufferView) \
    macro(timeline) \
    macro(top) \
    macro(underlyingByteSource) \
    macro(underlyingSink) \
    macro(underlyingSource) \
    macro(view) \
    macro(visualViewport) \
    macro(webkit) \
    macro(webkitAudioContext) \
    macro(webkitAudioPannerNode) \
    macro(webkitIDBCursor) \
    macro(webkitIDBDatabase) \
    macro(webkitIDBFactory) \
    macro(webkitIDBIndex) \
    macro(webkitIDBKeyRange) \
    macro(webkitIDBObjectStore) \
    macro(webkitIDBRequest) \
    macro(webkitIDBTransaction) \
    macro(webkitIndexedDB) \
    macro(webkitOfflineAudioContext) \
    macro(webkitOscillatorNode) \
    macro(window) \
    macro(writeAlgorithm) \
    macro(writing) \
    macro(writer) \
    macro(pendingAbortRequest) \
    macro(writeRequests) \
    WEBCORE_ADDITIONAL_PRIVATE_IDENTIFIERS(macro) \

class WebCoreBuiltinNames {
public:
    explicit WebCoreBuiltinNames(JSC::VM& vm)
        : m_vm(vm)
        WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(INITIALIZE_BUILTIN_NAMES)
    {
#define EXPORT_NAME(name) m_vm.propertyNames->appendExternalName(name##PublicName(), name##PrivateName());
        WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(EXPORT_NAME)
#undef EXPORT_NAME
    }

    WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_IDENTIFIER_ACCESSOR)

private:
    JSC::VM& m_vm;
    WEBCORE_COMMON_PRIVATE_IDENTIFIERS_EACH_PROPERTY_NAME(DECLARE_BUILTIN_NAMES)
};

} // namespace WebCore
