/*
 *  Copyright (c) 2015, Canon Inc. All rights reserved.
 *  Copyright (C) 2018 Apple Inc. All rights reserved.
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
    macro(AttachmentElement) \
    macro(Audio) \
    macro(AuthenticatorAssertionResponse) \
    macro(AuthenticatorAttestationResponse) \
    macro(AuthenticatorResponse) \
    macro(BlobEvent) \
    macro(Cache) \
    macro(CacheStorage) \
    macro(Client) \
    macro(Clients) \
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
    macro(ExtendableEvent) \
    macro(ExtendableMessageEvent) \
    macro(FetchEvent) \
    macro(FileSystem) \
    macro(FileSystemDirectoryEntry) \
    macro(FileSystemDirectoryReader) \
    macro(FileSystemEntry) \
    macro(FileSystemFileEntry) \
    macro(Gamepad) \
    macro(GamepadButton) \
    macro(GamepadEvent) \
    macro(GPUBufferUsage) \
    macro(GPUShaderStageBit) \
    macro(GPUTextureUsage) \
    macro(HTMLAttachmentElement) \
    macro(HTMLAudioElement) \
    macro(HTMLDataListElement) \
    macro(HTMLMenuItemElement) \
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
    macro(InputEvent) \
    macro(IntersectionObserver) \
    macro(IntersectionObserverEntry) \
    macro(KeyframeEffect) \
    macro(MediaCapabilities) \
    macro(MediaCapabilitiesInfo) \
    macro(MediaEncryptedEvent) \
    macro(MediaKeyMessageEvent) \
    macro(MediaKeySession) \
    macro(MediaKeyStatusMap) \
    macro(MediaKeySystemAccess) \
    macro(MediaKeys) \
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
    macro(OffscreenCanvas) \
    macro(OffscreenCanvasRenderingContext2D) \
    macro(PaintRenderingContext2D) \
    macro(PaymentAddress) \
    macro(PaymentMethodChangeEvent) \
    macro(PaymentRequest) \
    macro(PaymentRequestUpdateEvent) \
    macro(PaymentResponse) \
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
    macro(PerformanceResourceTiming) \
    macro(PerformanceServerTiming) \
    macro(PointerEvent) \
    macro(PublicKeyCredential) \
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
    macro(Request) \
    macro(Response) \
    macro(ScreenLuminance) \
    macro(ServiceWorker) \
    macro(ServiceWorkerContainer) \
    macro(ServiceWorkerGlobalScope) \
    macro(ServiceWorkerRegistration) \
    macro(ShadowRoot) \
    macro(SpectreGadget) \
    macro(StaticRange) \
    macro(StylePropertyMapReadOnly) \
    macro(StylePropertyMap) \
    macro(UndoItem) \
    macro(UndoManager) \
    macro(VRDisplay) \
    macro(VRDisplayCapabilities) \
    macro(VRDisplayEvent) \
    macro(VREyeParameters) \
    macro(VRFieldOfView) \
    macro(VRFrameData) \
    macro(VRStageParameters) \
    macro(VisualViewport) \
    macro(WebGL2RenderingContext) \
    macro(WebGLVertexArrayObject) \
    macro(WebGPU) \
    macro(WebGPUAdapter) \
    macro(WebGPUBindGroup) \
    macro(WebGPUBindGroupLayout) \
    macro(WebGPUBuffer) \
    macro(WebGPUCommandBuffer) \
    macro(WebGPUDevice) \
    macro(WebGPUIndexFormat) \
    macro(WebGPUInputStepMode) \
    macro(WebGPUQueue) \
    macro(WebGPUPipelineLayout) \
    macro(WebGPUProgrammablePassEncoder) \
    macro(WebGPURenderingContext) \
    macro(WebGPURenderPassEncoder) \
    macro(WebGPURenderPipeline) \
    macro(WebGPUShaderModule) \
    macro(WebGPUSwapChain) \
    macro(WebGPUTexture) \
    macro(WebGPUTextureView) \
    macro(WebGPUVertexFormat) \
    macro(WebMetalBuffer) \
    macro(WebMetalCommandBuffer) \
    macro(WebMetalCommandQueue) \
    macro(WebMetalComputeCommandEncoder) \
    macro(WebMetalComputePipelineState) \
    macro(WebMetalDepthStencilDescriptor) \
    macro(WebMetalDepthStencilState) \
    macro(WebMetalDrawable) \
    macro(WebMetalFunction) \
    macro(WebMetalLibrary) \
    macro(WebMetalRenderCommandEncoder) \
    macro(WebMetalRenderPassAttachmentDescriptor) \
    macro(WebMetalRenderPassColorAttachmentDescriptor) \
    macro(WebMetalRenderPassDepthAttachmentDescriptor) \
    macro(WebMetalRenderPassDescriptor) \
    macro(WebMetalRenderPipelineColorAttachmentDescriptor) \
    macro(WebMetalRenderPipelineDescriptor) \
    macro(WebMetalRenderPipelineState) \
    macro(WebMetalRenderingContext) \
    macro(WebMetalSize) \
    macro(WebMetalTexture) \
    macro(WebMetalTextureDescriptor) \
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
    macro(XMLHttpRequest) \
    macro(appendFromJS) \
    macro(associatedReadableByteStreamController) \
    macro(autoAllocateChunkSize) \
    macro(backingMap) \
    macro(blur) \
    macro(body) \
    macro(byobRequest) \
    macro(caches) \
    macro(cancel) \
    macro(cloneArrayBuffer) \
    macro(close) \
    macro(closeRequested) \
    macro(closed) \
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
    macro(failureKind) \
    macro(fetch) \
    macro(fetchRequest) \
    macro(fillFromJS) \
    macro(finishConsumingStream) \
    macro(focus) \
    macro(frames) \
    macro(getTracks) \
    macro(getUserMedia) \
    macro(header) \
    macro(href) \
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
    macro(readyPromiseCapability) \
    macro(response) \
    macro(responseCacheIsValid) \
    macro(retrieveResponse) \
    macro(self) \
    macro(setBody) \
    macro(setBodyFromInputRequest) \
    macro(setStatus) \
    macro(showModalDialog) \
    macro(startConsumingStream) \
    macro(started) \
    macro(startedPromise) \
    macro(state) \
    macro(storedError) \
    macro(strategy) \
    macro(strategyHWM) \
    macro(streamClosed) \
    macro(streamClosing) \
    macro(streamErrored) \
    macro(streamReadable) \
    macro(streamWaiting) \
    macro(streamWritable) \
    macro(structuredCloneArrayBuffer) \
    macro(structuredCloneArrayBufferView) \
    macro(top) \
    macro(underlyingByteSource) \
    macro(underlyingSink) \
    macro(underlyingSource) \
    macro(view) \
    macro(visualViewport) \
    macro(webkit) \
    macro(webkitAudioContext) \
    macro(webkitIDBCursor) \
    macro(webkitIDBDatabase) \
    macro(webkitIDBFactory) \
    macro(webkitIDBIndex) \
    macro(webkitIDBKeyRange) \
    macro(webkitIDBObjectStore) \
    macro(webkitIDBRequest) \
    macro(webkitIDBTransaction) \
    macro(webkitIndexedDB) \
    macro(webgpu) \
    macro(window) \
    macro(writing) \
    WEBCORE_ADDITIONAL_PRIVATE_IDENTIFIERS(macro) \

class WebCoreBuiltinNames {
public:
    explicit WebCoreBuiltinNames(JSC::VM* vm)
        : m_vm(*vm)
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
