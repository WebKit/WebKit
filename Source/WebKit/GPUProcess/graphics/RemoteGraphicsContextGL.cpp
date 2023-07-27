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
#include "QualifiedRenderingResourceIdentifier.h"
#include "RemoteGraphicsContextGLInitializationState.h"
#include "RemoteGraphicsContextGLMessages.h"
#include "RemoteGraphicsContextGLProxyMessages.h"
#include "StreamConnectionWorkQueue.h"
#include <WebCore/ByteArrayPixelBuffer.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/NotImplemented.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeap.h"
#endif

namespace WebKit {

using namespace WebCore;

namespace {
template<typename S, int I, typename T>
Vector<S> vectorCopyCast(const T& arrayReference)
{
    return { reinterpret_cast<const S*>(arrayReference.template data<I>()), arrayReference.size() };
}
}

// Currently we have one global WebGL processing instance.
IPC::StreamConnectionWorkQueue& remoteGraphicsContextGLStreamWorkQueue()
{
    static LazyNeverDestroyed<IPC::StreamConnectionWorkQueue> instance;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        instance.construct("RemoteGraphicsContextGL work queue"); // LazyNeverDestroyed owns the initial ref.
    });
    return instance.get();
}

#if !PLATFORM(COCOA) && !USE(GRAPHICS_LAYER_WC) && !USE(GBM)
Ref<RemoteGraphicsContextGL> RemoteGraphicsContextGL::create(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLAttributes&& attributes, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
{
    ASSERT_NOT_REACHED();
    auto instance = adoptRef(*new RemoteGraphicsContextGL(gpuConnectionToWebProcess, graphicsContextGLIdentifier, renderingBackend, WTFMove(streamConnection)));
    instance->initialize(WTFMove(attributes));
    return instance;
}
#endif

RemoteGraphicsContextGL::RemoteGraphicsContextGL(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_workQueue(remoteGraphicsContextGLStreamWorkQueue())
    , m_streamConnection(WTFMove(streamConnection))
    , m_graphicsContextGLIdentifier(graphicsContextGLIdentifier)
    , m_renderingBackend(renderingBackend)
#if ENABLE(VIDEO)
    , m_videoFrameObjectHeap(gpuConnectionToWebProcess.videoFrameObjectHeap())
#if PLATFORM(COCOA)
    , m_sharedVideoFrameReader(m_videoFrameObjectHeap.ptr(), gpuConnectionToWebProcess.webProcessIdentity())
#endif
#endif
    , m_renderingResourcesRequest(ScopedWebGLRenderingResourcesRequest::acquire())
    , m_webProcessIdentifier(gpuConnectionToWebProcess.webProcessIdentifier())
    , m_resourceOwner(gpuConnectionToWebProcess.webProcessIdentity())
{
    assertIsMainRunLoop();
}

RemoteGraphicsContextGL::~RemoteGraphicsContextGL()
{
    ASSERT(!m_streamConnection);
    ASSERT(!m_context);
}

void RemoteGraphicsContextGL::initialize(GraphicsContextGLAttributes&& attributes)
{
    assertIsMainRunLoop();
    workQueue().dispatch([attributes = WTFMove(attributes), protectedThis = Ref { *this }]() mutable {
        protectedThis->workQueueInitialize(WTFMove(attributes));
    });
}

void RemoteGraphicsContextGL::stopListeningForIPC(Ref<RemoteGraphicsContextGL>&& refFromConnection)
{
    assertIsMainRunLoop();
    workQueue().dispatch([protectedThis = WTFMove(refFromConnection)] {
        protectedThis->workQueueUninitialize();
    });
}

#if PLATFORM(MAC)
void RemoteGraphicsContextGL::displayWasReconfigured()
{
    assertIsMainRunLoop();
    workQueue().dispatch([protectedThis = Ref { *this }] {
        assertIsCurrent(protectedThis->workQueue());
        protectedThis->m_context->updateContextOnDisplayReconfiguration();
    });
}
#endif

void RemoteGraphicsContextGL::workQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    assertIsCurrent(workQueue());
    platformWorkQueueInitialize(WTFMove(attributes));
    m_streamConnection->open(workQueue());
    if (m_context) {
        m_context->setClient(this);
        String extensions = m_context->getString(GraphicsContextGL::EXTENSIONS);
        String requestableExtensions = m_context->getString(GraphicsContextGL::REQUESTABLE_EXTENSIONS_ANGLE);
        auto [externalImageTarget, externalImageBindingQuery] = m_context->externalImageTextureBindingPoint();
        RemoteGraphicsContextGLInitializationState initializationState { extensions, requestableExtensions, externalImageTarget, externalImageBindingQuery };

        send(Messages::RemoteGraphicsContextGLProxy::WasCreated(workQueue().wakeUpSemaphore(), m_streamConnection->clientWaitSemaphore(), { initializationState }));
        m_streamConnection->startReceivingMessages(*this, Messages::RemoteGraphicsContextGL::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64());
    } else
        send(Messages::RemoteGraphicsContextGLProxy::WasCreated({ }, { }, std::nullopt));
}

void RemoteGraphicsContextGL::workQueueUninitialize()
{
    assertIsCurrent(workQueue());
    if (m_context) {
        m_context->setClient(nullptr);
        m_context = nullptr;
        m_streamConnection->stopReceivingMessages(Messages::RemoteGraphicsContextGL::messageReceiverName(), m_graphicsContextGLIdentifier.toUInt64());
    }
    m_streamConnection->invalidate();
    m_streamConnection = nullptr;
    m_renderingResourcesRequest = { };
}

void RemoteGraphicsContextGL::forceContextLost()
{
    assertIsCurrent(workQueue());
    send(Messages::RemoteGraphicsContextGLProxy::WasLost());
}

void RemoteGraphicsContextGL::dispatchContextChangedNotification()
{
    assertIsCurrent(workQueue());
    send(Messages::RemoteGraphicsContextGLProxy::WasChanged());
}

void RemoteGraphicsContextGL::createAndBindEGLImage(GCGLenum target, WebCore::GraphicsContextGL::EGLImageSource source, CompletionHandler<void(uint64_t handle, WebCore::IntSize size)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    auto attachment = m_context->createAndBindEGLImage(target, WTFMove(source));
    auto [handle, size] = attachment.value_or(std::make_tuple(nullptr, IntSize { }));
    completionHandler(static_cast<uint64_t>(reinterpret_cast<intptr_t>(handle)), size);
}

void RemoteGraphicsContextGL::reshape(int32_t width, int32_t height)
{
    assertIsCurrent(workQueue());
    if (width && height)
        m_context->reshape(width, height);
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
    completionHandler(m_context->getErrors());
}

void RemoteGraphicsContextGL::ensureExtensionEnabled(String&& extension)
{
    assertIsCurrent(workQueue());
    m_context->ensureExtensionEnabled(extension);
}

void RemoteGraphicsContextGL::markContextChanged()
{
    assertIsCurrent(workQueue());
    m_context->markContextChanged();
}

void RemoteGraphicsContextGL::paintRenderingResultsToCanvas(WebCore::RenderingResourceIdentifier imageBuffer, CompletionHandler<void()>&& completionHandler)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    paintRenderingResultsToCanvasWithQualifiedIdentifier({ imageBuffer, m_webProcessIdentifier });
    completionHandler();
}

void RemoteGraphicsContextGL::paintRenderingResultsToCanvasWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier imageBuffer)
{
    assertIsCurrent(workQueue());
    m_context->withDrawingBufferAsNativeImage([&](NativeImage& image) {
        paintNativeImageToImageBuffer(image, imageBuffer);
    });
}

void RemoteGraphicsContextGL::paintCompositedResultsToCanvas(WebCore::RenderingResourceIdentifier imageBuffer, CompletionHandler<void()>&& completionHandler)
{
    // Immediately turn the RenderingResourceIdentifier (which is error-prone) to a QualifiedRenderingResourceIdentifier,
    // and use a helper function to make sure that don't accidentally use the RenderingResourceIdentifier (because the helper function can't see it).
    paintCompositedResultsToCanvasWithQualifiedIdentifier({ imageBuffer, m_webProcessIdentifier });
    completionHandler();
}

void RemoteGraphicsContextGL::paintCompositedResultsToCanvasWithQualifiedIdentifier(QualifiedRenderingResourceIdentifier imageBuffer)
{
    assertIsCurrent(workQueue());
    m_context->withDisplayBufferAsNativeImage([&](NativeImage& image) {
        paintNativeImageToImageBuffer(image, imageBuffer);
    });
}

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
void RemoteGraphicsContextGL::paintCompositedResultsToVideoFrame(CompletionHandler<void(std::optional<WebKit::RemoteVideoFrameProxy::Properties>&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    std::optional<WebKit::RemoteVideoFrameProxy::Properties> result;
    if (auto videoFrame = m_context->paintCompositedResultsToVideoFrame())
        result = m_videoFrameObjectHeap->add(videoFrame.releaseNonNull());
    completionHandler(WTFMove(result));
}
#endif

void RemoteGraphicsContextGL::paintNativeImageToImageBuffer(NativeImage& image, QualifiedRenderingResourceIdentifier target)
{
    assertIsCurrent(workQueue());
    // FIXME: We do not have functioning read/write fences in RemoteRenderingBackend. Thus this is synchronous,
    // as are the messages that call these.
    Lock lock;
    Condition conditionVariable;
    bool isFinished = false;

    m_renderingBackend->dispatch([&]() mutable {
        if (auto imageBuffer = m_renderingBackend->remoteResourceCache().cachedImageBuffer(target)) {
            // Here we do not try to play back pending commands for imageBuffer. Currently this call is only made for empty
            // image buffers and there's no good way to add display lists.
            GraphicsContextGL::paintToCanvas(image, imageBuffer->backendSize(), imageBuffer->context());


            // We know that the image might be updated afterwards, so flush the drawing so that read back does not occur.
            // Unfortunately "flush" implementation in RemoteRenderingBackend overloads ordering and effects.
            imageBuffer->flushDrawingContext();
        }
        Locker locker { lock };
        isFinished = true;
        conditionVariable.notifyOne();
    });
    Locker locker { lock };
    conditionVariable.wait(lock, [&] {
        return isFinished;
    });
}

void RemoteGraphicsContextGL::simulateEventForTesting(WebCore::GraphicsContextGL::SimulatedEventForTesting event)
{
    assertIsCurrent(workQueue());
    // FIXME: only run this in testing mode. https://bugs.webkit.org/show_bug.cgi?id=222544
    if (event == WebCore::GraphicsContextGL::SimulatedEventForTesting::Timeout) {
        // Simulate the timeout by just discarding the context. The subsequent messages act like
        // unauthorized or old messages from Web process, they are skipped.
        callOnMainRunLoop([gpuConnectionToWebProcess = m_gpuConnectionToWebProcess, identifier = m_graphicsContextGLIdentifier]() {
            if (auto connectionToWeb = gpuConnectionToWebProcess.get())
                connectionToWeb->releaseGraphicsContextGLForTesting(identifier);
        });
        return;
    }
    if (event == WebCore::GraphicsContextGL::SimulatedEventForTesting::ContextChange) {
#if PLATFORM(MAC)
        callOnMainRunLoop([weakConnection = m_gpuConnectionToWebProcess]() {
            if (auto connection = weakConnection.get())
                connection->dispatchDisplayWasReconfiguredForTesting();
        });
#endif
        return;
    }
    m_context->simulateEventForTesting(event);
}

void RemoteGraphicsContextGL::readPixelsInline(WebCore::IntRect rect, uint32_t format, uint32_t type, CompletionHandler<void(std::optional<WebCore::IntSize>, IPC::ArrayReference<uint8_t>)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    static constexpr size_t readPixelsInlineSizeLimit = 64 * KB; // NOTE: when changing, change the value in RemoteGraphicsContextGLProxy too.
    unsigned bytesPerGroup = GraphicsContextGL::computeBytesPerGroup(format, type);
    auto replyImageBytes = rect.area<RecordOverflow>() * bytesPerGroup;
    if (!bytesPerGroup || replyImageBytes.hasOverflowed()) {
        completionHandler(std::nullopt, { });
        return;
    }
    MallocPtr<uint8_t> pixelsStore;
    std::span<uint8_t> pixels;
    if (replyImageBytes && replyImageBytes <= readPixelsInlineSizeLimit) {
        pixelsStore = MallocPtr<uint8_t>::tryMalloc(replyImageBytes);
        if (pixelsStore)
            pixels = { pixelsStore.get(), replyImageBytes };
    }
    std::optional<WebCore::IntSize> readArea;
    if (pixels.size() == replyImageBytes)
        readArea = m_context->readPixelsWithStatus(rect, format, type, pixels);
    else
        m_context->addError(GCGLErrorCode::OutOfMemory);
    if (!readArea) {
        pixels = { };
        pixelsStore = { };
    }
    completionHandler(readArea, pixels);
}


void RemoteGraphicsContextGL::readPixelsSharedMemory(WebCore::IntRect rect, uint32_t format, uint32_t type, SharedMemory::Handle handle, CompletionHandler<void(std::optional<WebCore::IntSize>)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    std::optional<WebCore::IntSize> readArea;
    if (!handle.isNull()) {
        handle.setOwnershipOfMemory(m_resourceOwner, WebKit::MemoryLedger::Default);
        if (auto buffer = SharedMemory::map(WTFMove(handle), SharedMemory::Protection::ReadWrite))
            readArea = m_context->readPixelsWithStatus(rect, format, type, std::span<uint8_t>(static_cast<uint8_t*>(buffer->data()), buffer->size()));
        else
            m_context->addError(GCGLErrorCode::InvalidOperation);
    } else
        m_context->addError(GCGLErrorCode::InvalidOperation);
    completionHandler(readArea);
}

void RemoteGraphicsContextGL::multiDrawArraysANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t>&& firstsAndCounts)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    Vector<GCGLint> firsts = vectorCopyCast<GCGLint, 0>(firstsAndCounts);
    Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 1>(firstsAndCounts);
    m_context->multiDrawArraysANGLE(mode, GCGLSpanTuple { firsts, counts });
}

void RemoteGraphicsContextGL::multiDrawArraysInstancedANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t>&& firstsCountsAndInstanceCounts)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    Vector<GCGLint> firsts = vectorCopyCast<GCGLint, 0>(firstsCountsAndInstanceCounts);
    Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 1>(firstsCountsAndInstanceCounts);
    Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(firstsCountsAndInstanceCounts);
    m_context->multiDrawArraysInstancedANGLE(mode, GCGLSpanTuple { firsts, counts, instanceCounts });
}

void RemoteGraphicsContextGL::multiDrawElementsANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t>&& countsAndOffsets, uint32_t type)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    const Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 0>(countsAndOffsets);
    // Currently offsets are copied in the m_context.
    const GCGLsizei* offsets = reinterpret_cast<const GCGLsizei*>(countsAndOffsets.data<1>());
    m_context->multiDrawElementsANGLE(mode, GCGLSpanTuple { counts.data(), offsets, counts.size() }, type);
}

void RemoteGraphicsContextGL::multiDrawElementsInstancedANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t>&& countsOffsetsAndInstanceCounts, uint32_t type)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    const Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 0>(countsOffsetsAndInstanceCounts);
    // Currently offsets are copied in the m_context.
    const GCGLsizei* offsets = reinterpret_cast<const GCGLsizei*>(countsOffsetsAndInstanceCounts.data<1>());
    const Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(countsOffsetsAndInstanceCounts);
    m_context->multiDrawElementsInstancedANGLE(mode, GCGLSpanTuple { counts.data(), offsets, instanceCounts.data(), counts.size() }, type);
}

void RemoteGraphicsContextGL::multiDrawArraysInstancedBaseInstanceANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t, uint32_t>&& firstsCountsInstanceCountsAndBaseInstances)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    Vector<GCGLint> firsts = vectorCopyCast<GCGLint, 0>(firstsCountsInstanceCountsAndBaseInstances);
    Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 1>(firstsCountsInstanceCountsAndBaseInstances);
    Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(firstsCountsInstanceCountsAndBaseInstances);
    Vector<GCGLuint> baseInstances = vectorCopyCast<GCGLuint, 3>(firstsCountsInstanceCountsAndBaseInstances);
    m_context->multiDrawArraysInstancedBaseInstanceANGLE(mode, GCGLSpanTuple { firsts, counts, instanceCounts, baseInstances });
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
    m_context->multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(mode, GCGLSpanTuple { counts.data(), offsets, instanceCounts.data(), baseVertices.data(), baseInstances.data(), counts.size() }, type);
}

} // namespace WebKit

#endif
