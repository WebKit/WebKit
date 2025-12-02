/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteGraphicsContextGL.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL)

#include "GPUConnectionToWebProcess.h"
#include "Logging.h"
#include "RemoteGraphicsContextGLInitializationState.h"
#include "RemoteGraphicsContextGLMessages.h"
#include "RemoteGraphicsContextGLProxyMessages.h"
#include "RemoteSharedResourceCache.h"
#include "StreamConnectionWorkQueue.h"
#include <WebCore/ByteArrayPixelBuffer.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/NativeImage.h>
#include <WebCore/NotImplemented.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeap.h"
#endif

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_connection);

namespace WebKit {

using namespace WebCore;

namespace {
template<typename S, int I, typename T>
Vector<S> vectorCopyCast(const T& arrayReference)
{
    return Vector(spanReinterpretCast<const S>(arrayReference.template span<I>()));
}
}

// Currently we have one global WebGL processing instance.
IPC::StreamConnectionWorkQueue& remoteGraphicsContextGLStreamWorkQueueSingleton()
{
    // NeverDestroyed owns the initial ref.
    static NeverDestroyed<IPC::StreamConnectionWorkQueue> instance { "RemoteGraphicsContextGL work queue"_s };
    return instance.get();
}

#if !PLATFORM(COCOA) && !USE(GRAPHICS_LAYER_WC) && !USE(GBM)
Ref<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLAttributes&& attributes, RemoteGraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
{
    ASSERT_NOT_REACHED();
    auto instance = adoptRef(*new RemoteGraphicsContextGL(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(streamConnection)));
    instance->initialize(WTFMove(attributes));
    return instance;
}
#endif

RemoteGraphicsContextGL::RemoteGraphicsContextGL(GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteGraphicsContextGLIdentifier identifier, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_workQueue(remoteGraphicsContextGLStreamWorkQueueSingleton())
    , m_connection(WTFMove(streamConnection))
    , m_identifier(identifier)
    , m_renderingBackend(renderingBackend)
    , m_sharedResourceCache(gpuConnectionToWebProcess.sharedResourceCache())
#if ENABLE(VIDEO)
    , m_videoFrameObjectHeap(gpuConnectionToWebProcess.videoFrameObjectHeap())
#if PLATFORM(COCOA)
    , m_sharedVideoFrameReader(m_videoFrameObjectHeap.ptr(), gpuConnectionToWebProcess.webProcessIdentity())
#endif
#endif
    , m_renderingResourcesRequest(ScopedWebGLRenderingResourcesRequest::acquire())
    , m_sharedPreferencesForWebProcess(gpuConnectionToWebProcess.sharedPreferencesForWebProcessValue())
{
    assertIsMainRunLoop();
}

// <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=121717>
IGNORE_GCC_WARNINGS_BEGIN("free-nonheap-object")
RemoteGraphicsContextGL::~RemoteGraphicsContextGL()
{
    ASSERT(!m_context);
}
IGNORE_GCC_WARNINGS_END

void RemoteGraphicsContextGL::initialize(GraphicsContextGLAttributes&& attributes)
{
    assertIsMainRunLoop();
    m_workQueue->dispatch([attributes = WTFMove(attributes), protectedThis = Ref { *this }]() mutable {
        protectedThis->workQueueInitialize(WTFMove(attributes));
    });
}

void RemoteGraphicsContextGL::stopListeningForIPC(Ref<RemoteGraphicsContextGL>&& refFromConnection)
{
    assertIsMainRunLoop();
    m_workQueue->dispatch([protectedThis = WTFMove(refFromConnection)] {
        protectedThis->workQueueUninitialize();
    });
}

void RemoteGraphicsContextGL::workQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    assertIsCurrent(workQueue());
    platformWorkQueueInitialize(WTFMove(attributes));
    m_connection->open(*this, m_workQueue);
    if (RefPtr context = m_context) {
        context->setClient(this);
        auto knownActiveExtensions = context->knownActiveExtensions();
        auto requestableExtensions = context->requestableExtensions();
        auto [externalImageTarget, externalImageBindingQuery] = context->externalImageTextureBindingPoint();
        RemoteGraphicsContextGLInitializationState initializationState {
            .knownActiveExtensions = knownActiveExtensions.toRaw(),
            .requestableExtensions = requestableExtensions.toRaw(),
            .externalImageTarget = externalImageTarget,
            .externalImageBindingQuery = externalImageBindingQuery
        };
        send(Messages::RemoteGraphicsContextGLProxy::WasCreated(workQueue().wakeUpSemaphore(), m_connection->clientWaitSemaphore(), { initializationState }));
        m_connection->startReceivingMessages(*this, Messages::RemoteGraphicsContextGL::messageReceiverName(), m_identifier.toUInt64());
    } else
        send(Messages::RemoteGraphicsContextGLProxy::WasCreated({ }, { }, std::nullopt));
}

void RemoteGraphicsContextGL::workQueueUninitialize()
{
    assertIsCurrent(workQueue());
    if (m_context) {
        m_context->setClient(nullptr);
        m_context = nullptr;
        m_connection->stopReceivingMessages(Messages::RemoteGraphicsContextGL::messageReceiverName(), m_identifier.toUInt64());
    }
    m_connection->invalidate();
    m_renderingResourcesRequest = { };
}

void RemoteGraphicsContextGL::didReceiveInvalidMessage(IPC::StreamServerConnection&, IPC::MessageName messageName, const Vector<uint32_t>&)
{
    RELEASE_LOG_FAULT(IPC, "Received an invalid message '%" PUBLIC_LOG_STRING "' from WebContent process, requesting for it to be terminated.", description(messageName).characters());
    callOnMainRunLoop([weakGPUConnectionToWebProcess = m_gpuConnectionToWebProcess] {
        if (RefPtr gpuConnectionToWebProcess = weakGPUConnectionToWebProcess.get())
            gpuConnectionToWebProcess->terminateWebProcess();
    });
}

void RemoteGraphicsContextGL::forceContextLost()
{
    assertIsCurrent(workQueue());
    send(Messages::RemoteGraphicsContextGLProxy::WasLost());
}

void RemoteGraphicsContextGL::addDebugMessage(GCGLenum type, GCGLenum id, GCGLenum severity, const CString& message)
{
    assertIsCurrent(workQueue());
    send(Messages::RemoteGraphicsContextGLProxy::addDebugMessage(type, id, severity, message));
}

void RemoteGraphicsContextGL::reshape(int32_t width, int32_t height)
{
    assertIsCurrent(workQueue());
    if (width && height)
        protectedContext()->reshape(width, height);
    else
        forceContextLost();
}

#if !PLATFORM(COCOA) && !USE(GRAPHICS_LAYER_WC) && !USE(GBM)
void RemoteGraphicsContextGL::prepareForDisplay(CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    notImplemented();
    completionHandler();
}
#endif

void RemoteGraphicsContextGL::getErrors(CompletionHandler<void(GCGLErrorCodeSet)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    completionHandler(protectedContext()->getErrors());
}

void RemoteGraphicsContextGL::ensureExtensionEnabled(GCGLExtension extension)
{
    assertIsCurrent(workQueue());
    bool success = protectedContext()->enableExtension(extension);
    MESSAGE_CHECK(success);
}

void RemoteGraphicsContextGL::copyNativeImageYFlipped(WebCore::GraphicsContextGL::SurfaceBuffer buffer, WebCore::RenderingResourceIdentifier nativeImageIdentifier)
{
    assertIsCurrent(workQueue());
    RefPtr image = protectedContext()->copyNativeImageYFlipped(buffer);
    // FIXME: Handle OOM.
    MESSAGE_CHECK(image);
    bool success = m_sharedResourceCache->addNativeImage(nativeImageIdentifier, image.releaseNonNull());
    MESSAGE_CHECK(success);
}

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
void RemoteGraphicsContextGL::surfaceBufferToVideoFrame(WebCore::GraphicsContextGL::SurfaceBuffer buffer, CompletionHandler<void(std::optional<WebKit::RemoteVideoFrameProxy::Properties>&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    std::optional<WebKit::RemoteVideoFrameProxy::Properties> result;
    if (auto videoFrame = protectedContext()->surfaceBufferToVideoFrame(buffer))
        result = m_videoFrameObjectHeap->add(videoFrame.releaseNonNull());
    completionHandler(WTFMove(result));
}
#endif

bool RemoteGraphicsContextGL::webXREnabled() const
{
    RefPtr gpuConnectionToWebProcess = m_gpuConnectionToWebProcess.get();
    if (gpuConnectionToWebProcess)
        return gpuConnectionToWebProcess->isWebXREnabled();
    return false;
}

bool RemoteGraphicsContextGL::webXRPromptAccepted() const
{
#if ENABLE(WEBXR) && PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    auto currentAcceptedValue = GPUProcess::singleton().immersiveModeProcessIdentity();
    return currentAcceptedValue && m_sharedResourceCache->resourceOwner() == *currentAcceptedValue;
#else
    return webXREnabled();
#endif
}

void RemoteGraphicsContextGL::simulateEventForTesting(WebCore::GraphicsContextGL::SimulatedEventForTesting event)
{
    assertIsCurrent(workQueue());
    // FIXME: only run this in testing mode. https://bugs.webkit.org/show_bug.cgi?id=222544
    if (event == WebCore::GraphicsContextGL::SimulatedEventForTesting::Timeout) {
        // Simulate the timeout by just discarding the context. The subsequent messages act like
        // unauthorized or old messages from Web process, they are skipped.
        callOnMainRunLoop([gpuConnectionToWebProcess = m_gpuConnectionToWebProcess, identifier = m_identifier]() {
            if (auto connectionToWeb = gpuConnectionToWebProcess.get())
                connectionToWeb->releaseGraphicsContextGLForTesting(identifier);
        });
        return;
    }
    protectedContext()->simulateEventForTesting(event);
}

void RemoteGraphicsContextGL::getBufferSubDataInline(uint32_t target, uint64_t offset, uint64_t dataSize, CompletionHandler<void(std::span<const uint8_t>)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    static constexpr size_t getBufferSubDataInlineSizeLimit = 64 * KB; // NOTE: when changing, change the value in RemoteGraphicsContextGLProxy too.

    RefPtr context = m_context;
    if (!dataSize || dataSize > getBufferSubDataInlineSizeLimit) {
        context->addError(GCGLErrorCode::InvalidOperation);
        completionHandler({ });
        return;
    }

    MallocSpan<uint8_t> bufferStore;
    std::span<uint8_t> bufferData;
    bufferStore = MallocSpan<uint8_t>::tryMalloc(dataSize);
    if (bufferStore) {
        bufferData = bufferStore.mutableSpan();
        if (!context->getBufferSubDataWithStatus(target, offset, bufferData))
            bufferData = { };
    } else
        context->addError(GCGLErrorCode::OutOfMemory);

    completionHandler(bufferData);
}

void RemoteGraphicsContextGL::getBufferSubDataSharedMemory(uint32_t target, uint64_t offset, uint64_t dataSize, WebCore::SharedMemory::Handle handle, CompletionHandler<void(bool)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    bool validBufferData = false;

    static constexpr size_t readPixelsSharedMemorySizeLimit = 100 * MB; // NOTE: when changing, change the value in RemoteGraphicsContextGLProxy too.

    if (dataSize > readPixelsSharedMemorySizeLimit) {
        completionHandler({ });
        return;
    }

    RefPtr context = m_context;

    handle.setOwnershipOfMemory(m_sharedResourceCache->resourceOwner(), WebKit::MemoryLedger::Default);
    auto buffer = SharedMemory::map(WTFMove(handle), SharedMemory::Protection::ReadWrite);
    if (buffer && dataSize <= buffer->size())
        validBufferData = context->getBufferSubDataWithStatus(target, offset, buffer->mutableSpan().subspan(0, dataSize));
    else
        context->addError(GCGLErrorCode::InvalidOperation);

    completionHandler(validBufferData);
}

void RemoteGraphicsContextGL::readPixelsInline(WebCore::IntRect rect, uint32_t format, uint32_t type, bool packReverseRowOrder, CompletionHandler<void(std::optional<WebCore::IntSize>, std::span<const uint8_t>)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    static constexpr size_t readPixelsInlineSizeLimit = 64 * KB; // NOTE: when changing, change the value in RemoteGraphicsContextGLProxy too.
    unsigned bytesPerGroup = GraphicsContextGL::computeBytesPerGroup(format, type);
    auto replyImageBytes = rect.area<RecordOverflow>() * bytesPerGroup;
    if (!bytesPerGroup || replyImageBytes.hasOverflowed()) {
        completionHandler(std::nullopt, { });
        return;
    }
    MallocSpan<uint8_t> pixelsStore;
    std::span<uint8_t> pixels;
    if (replyImageBytes && replyImageBytes <= readPixelsInlineSizeLimit) {
        pixelsStore = MallocSpan<uint8_t>::tryMalloc(replyImageBytes);
        if (pixelsStore)
            pixels = pixelsStore.mutableSpan();
    }

    RefPtr context = m_context;
    std::optional<WebCore::IntSize> readArea;
    if (pixels.size() == replyImageBytes)
        readArea = context->readPixelsWithStatus(rect, format, type, packReverseRowOrder, pixels);
    else
        context->addError(GCGLErrorCode::OutOfMemory);
    if (!readArea) {
        pixels = { };
        pixelsStore = { };
    }
    completionHandler(readArea, pixels);
}


void RemoteGraphicsContextGL::readPixelsSharedMemory(WebCore::IntRect rect, uint32_t format, uint32_t type, bool packReverseRowOrder, SharedMemory::Handle handle, CompletionHandler<void(std::optional<WebCore::IntSize>)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    std::optional<WebCore::IntSize> readArea;

    RefPtr context = m_context;
    handle.setOwnershipOfMemory(m_sharedResourceCache->resourceOwner(), WebKit::MemoryLedger::Default);
    if (auto buffer = SharedMemory::map(WTFMove(handle), SharedMemory::Protection::ReadWrite))
        readArea = context->readPixelsWithStatus(rect, format, type, packReverseRowOrder, buffer->mutableSpan());
    else
        context->addError(GCGLErrorCode::InvalidOperation);

    completionHandler(readArea);
}

void RemoteGraphicsContextGL::multiDrawArraysANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t>&& firstsAndCounts)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    Vector<GCGLint> firsts = vectorCopyCast<GCGLint, 0>(firstsAndCounts);
    Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 1>(firstsAndCounts);
    protectedContext()->multiDrawArraysANGLE(mode, GCGLSpanTuple { firsts, counts });
}

void RemoteGraphicsContextGL::multiDrawArraysInstancedANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t>&& firstsCountsAndInstanceCounts)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    Vector<GCGLint> firsts = vectorCopyCast<GCGLint, 0>(firstsCountsAndInstanceCounts);
    Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 1>(firstsCountsAndInstanceCounts);
    Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(firstsCountsAndInstanceCounts);
    protectedContext()->multiDrawArraysInstancedANGLE(mode, GCGLSpanTuple { firsts, counts, instanceCounts });
}

void RemoteGraphicsContextGL::multiDrawElementsANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t>&& countsAndOffsets, uint32_t type)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    const Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 0>(countsAndOffsets);
    // Currently offsets are copied in the m_context.
    const GCGLsizei* offsets = reinterpret_cast<const GCGLsizei*>(countsAndOffsets.data<1>());
    protectedContext()->multiDrawElementsANGLE(mode, GCGLSpanTuple { counts.span().data(), offsets, counts.size() }, type);
}

void RemoteGraphicsContextGL::multiDrawElementsInstancedANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t>&& countsOffsetsAndInstanceCounts, uint32_t type)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    const Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 0>(countsOffsetsAndInstanceCounts);
    // Currently offsets are copied in the m_context.
    const GCGLsizei* offsets = reinterpret_cast<const GCGLsizei*>(countsOffsetsAndInstanceCounts.data<1>());
    const Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(countsOffsetsAndInstanceCounts);
    protectedContext()->multiDrawElementsInstancedANGLE(mode, GCGLSpanTuple { counts.span().data(), offsets, instanceCounts.span().data(), counts.size() }, type);
}

void RemoteGraphicsContextGL::multiDrawArraysInstancedBaseInstanceANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t, uint32_t>&& firstsCountsInstanceCountsAndBaseInstances)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    Vector<GCGLint> firsts = vectorCopyCast<GCGLint, 0>(firstsCountsInstanceCountsAndBaseInstances);
    Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 1>(firstsCountsInstanceCountsAndBaseInstances);
    Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(firstsCountsInstanceCountsAndBaseInstances);
    Vector<GCGLuint> baseInstances = vectorCopyCast<GCGLuint, 3>(firstsCountsInstanceCountsAndBaseInstances);
    protectedContext()->multiDrawArraysInstancedBaseInstanceANGLE(mode, GCGLSpanTuple { firsts, counts, instanceCounts, baseInstances });
}

void RemoteGraphicsContextGL::multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t, int32_t, uint32_t>&& countsOffsetsInstanceCountsBaseVerticesAndBaseInstances, uint32_t type)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    const Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 0>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances);
    // Currently offsets are copied in the m_context.
    const GCGLsizei* offsets = reinterpret_cast<const GCGLsizei*>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances.data<1>());
    const Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances);
    const Vector<GCGLint> baseVertices = vectorCopyCast<GCGLint, 3>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances);
    const Vector<GCGLuint> baseInstances = vectorCopyCast<GCGLuint, 4>(countsOffsetsInstanceCountsBaseVerticesAndBaseInstances);
    protectedContext()->multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(mode, GCGLSpanTuple { counts.span().data(), offsets, instanceCounts.span().data(), baseVertices.span().data(), baseInstances.span().data(), counts.size() }, type);
}

void RemoteGraphicsContextGL::drawBuffers(std::span<const uint32_t> bufs)
{
    assertIsCurrent(workQueue());
    protectedContext()->drawBuffers(Vector(bufs));
}

void RemoteGraphicsContextGL::drawBuffersEXT(std::span<const uint32_t> bufs)
{
    assertIsCurrent(workQueue());
    protectedContext()->drawBuffersEXT(Vector(bufs));
}

void RemoteGraphicsContextGL::invalidateFramebuffer(uint32_t target, std::span<const uint32_t> attachments)
{
    assertIsCurrent(workQueue());
    protectedContext()->invalidateFramebuffer(target, Vector(attachments));
}

void RemoteGraphicsContextGL::invalidateSubFramebuffer(uint32_t target, std::span<const uint32_t> attachments, int32_t x, int32_t y, int32_t width, int32_t height)
{
    assertIsCurrent(workQueue());
    protectedContext()->invalidateSubFramebuffer(target, Vector(attachments), x, y, width, height);
}

#if ENABLE(WEBXR)

void RemoteGraphicsContextGL::framebufferDiscard(uint32_t target, std::span<const uint32_t> attachments)
{
    assertIsCurrent(workQueue());
    MESSAGE_CHECK(webXRPromptAccepted());
    protectedContext()->framebufferDiscard(target, Vector(attachments));
}

#endif

void RemoteGraphicsContextGL::setDrawingBufferColorSpace(WebCore::DestinationColorSpace&& colorSpace)
{
    assertIsCurrent(workQueue());
    protectedContext()->setDrawingBufferColorSpace(colorSpace);
}

RefPtr<RemoteGraphicsContextGL::GCGLContext> RemoteGraphicsContextGL::protectedContext()
{
    assertIsCurrent(workQueue());
    return m_context;
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEBGL)
