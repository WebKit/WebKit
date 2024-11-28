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
#include <WebCore/NotImplemented.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(VIDEO)
#include "RemoteVideoFrameObjectHeap.h"
#endif

#define MESSAGE_CHECK(assertion, connection) MESSAGE_CHECK_OPTIONAL_CONNECTION_BASE(assertion, connection)

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
    static LazyNeverDestroyed<IPC::StreamConnectionWorkQueue> instance;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        instance.construct("RemoteGraphicsContextGL work queue"_s); // LazyNeverDestroyed owns the initial ref.
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

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteGraphicsContextGL);

RemoteGraphicsContextGL::RemoteGraphicsContextGL(GPUConnectionToWebProcess& gpuConnectionToWebProcess, GraphicsContextGLIdentifier graphicsContextGLIdentifier, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& streamConnection)
    : m_gpuConnectionToWebProcess(gpuConnectionToWebProcess)
    , m_workQueue(remoteGraphicsContextGLStreamWorkQueueSingleton())
    , m_streamConnection(WTFMove(streamConnection))
    , m_graphicsContextGLIdentifier(graphicsContextGLIdentifier)
    , m_renderingBackend(renderingBackend)
    , m_sharedResourceCache(gpuConnectionToWebProcess.sharedResourceCache())
#if ENABLE(VIDEO)
    , m_videoFrameObjectHeap(gpuConnectionToWebProcess.videoFrameObjectHeap())
#if PLATFORM(COCOA)
    , m_sharedVideoFrameReader(m_videoFrameObjectHeap.ptr(), gpuConnectionToWebProcess.webProcessIdentity())
#endif
#endif
    , m_renderingResourcesRequest(ScopedWebGLRenderingResourcesRequest::acquire())
    , m_webProcessIdentifier(gpuConnectionToWebProcess.webProcessIdentifier())
    , m_sharedPreferencesForWebProcess(gpuConnectionToWebProcess.sharedPreferencesForWebProcessValue())
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
    protectedWorkQueue()->dispatch([attributes = WTFMove(attributes), protectedThis = Ref { *this }]() mutable {
        protectedThis->workQueueInitialize(WTFMove(attributes));
    });
}

void RemoteGraphicsContextGL::stopListeningForIPC(Ref<RemoteGraphicsContextGL>&& refFromConnection)
{
    assertIsMainRunLoop();
    protectedWorkQueue()->dispatch([protectedThis = WTFMove(refFromConnection)] {
        protectedThis->workQueueUninitialize();
    });
}

void RemoteGraphicsContextGL::workQueueInitialize(WebCore::GraphicsContextGLAttributes&& attributes)
{
    assertIsCurrent(workQueue());
    platformWorkQueueInitialize(WTFMove(attributes));
    m_streamConnection->open(protectedWorkQueue());
    if (RefPtr context = m_context) {
        context->setClient(this);
        String extensions = context->getString(GraphicsContextGL::EXTENSIONS);
        String requestableExtensions = context->getString(GraphicsContextGL::REQUESTABLE_EXTENSIONS_ANGLE);
        auto [externalImageTarget, externalImageBindingQuery] = context->externalImageTextureBindingPoint();
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

void RemoteGraphicsContextGL::addDebugMessage(GCGLenum type, GCGLenum id, GCGLenum severity, const String& message)
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

void RemoteGraphicsContextGL::ensureExtensionEnabled(String&& extension)
{
    assertIsCurrent(workQueue());
    protectedContext()->ensureExtensionEnabled(extension);
}

void RemoteGraphicsContextGL::drawSurfaceBufferToImageBuffer(WebCore::GraphicsContextGL::SurfaceBuffer buffer, WebCore::RenderingResourceIdentifier imageBufferIdentifier, CompletionHandler<void()>&& completionHandler)
{
    assertIsCurrent(workQueue());
    protectedContext()->withBufferAsNativeImage(buffer, [&](NativeImage& image) {
        paintNativeImageToImageBuffer(image, imageBufferIdentifier);
    });
    completionHandler();
}

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
void RemoteGraphicsContextGL::surfaceBufferToVideoFrame(WebCore::GraphicsContextGL::SurfaceBuffer buffer, CompletionHandler<void(std::optional<WebKit::RemoteVideoFrameProxy::Properties>&&)>&& completionHandler)
{
    assertIsCurrent(workQueue());
    std::optional<WebKit::RemoteVideoFrameProxy::Properties> result;
    if (auto videoFrame = protectedContext()->surfaceBufferToVideoFrame(buffer))
        result = protectedVideoFrameObjectHeap()->add(videoFrame.releaseNonNull());
    completionHandler(WTFMove(result));
}
#endif

void RemoteGraphicsContextGL::paintNativeImageToImageBuffer(NativeImage& image, RenderingResourceIdentifier imageBufferIdentifier)
{
    assertIsCurrent(workQueue());
    // FIXME: We do not have functioning read/write fences in RemoteRenderingBackend. Thus this is synchronous,
    // as are the messages that call these.
    Lock lock;
    Condition conditionVariable;
    bool isFinished = false;

    Ref renderingBackend = m_renderingBackend;
    renderingBackend->dispatch([&]() mutable {
        if (auto imageBuffer = renderingBackend->imageBuffer(imageBufferIdentifier)) {
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
        callOnMainRunLoop([gpuConnectionToWebProcess = m_gpuConnectionToWebProcess, identifier = m_graphicsContextGLIdentifier]() {
            if (auto connectionToWeb = gpuConnectionToWebProcess.get())
                connectionToWeb->releaseGraphicsContextGLForTesting(identifier);
        });
        return;
    }
    protectedContext()->simulateEventForTesting(event);
}

void RemoteGraphicsContextGL::getBufferSubDataInline(uint32_t target, uint64_t offset, size_t dataSize, CompletionHandler<void(std::span<const uint8_t>)>&& completionHandler)
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

void RemoteGraphicsContextGL::getBufferSubDataSharedMemory(uint32_t target, uint64_t offset, size_t dataSize, WebCore::SharedMemory::Handle handle, CompletionHandler<void(bool)>&& completionHandler)
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
    protectedContext()->multiDrawElementsANGLE(mode, GCGLSpanTuple { counts.data(), offsets, counts.size() }, type);
}

void RemoteGraphicsContextGL::multiDrawElementsInstancedANGLE(uint32_t mode, IPC::ArrayReferenceTuple<int32_t, int32_t, int32_t>&& countsOffsetsAndInstanceCounts, uint32_t type)
{
    assertIsCurrent(workQueue());
    // Copy the arrays. The contents are to be verified. The data might be in memory region shared by the caller.
    const Vector<GCGLsizei> counts = vectorCopyCast<GCGLsizei, 0>(countsOffsetsAndInstanceCounts);
    // Currently offsets are copied in the m_context.
    const GCGLsizei* offsets = reinterpret_cast<const GCGLsizei*>(countsOffsetsAndInstanceCounts.data<1>());
    const Vector<GCGLsizei> instanceCounts = vectorCopyCast<GCGLsizei, 2>(countsOffsetsAndInstanceCounts);
    protectedContext()->multiDrawElementsInstancedANGLE(mode, GCGLSpanTuple { counts.data(), offsets, instanceCounts.data(), counts.size() }, type);
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
    protectedContext()->multiDrawElementsInstancedBaseVertexBaseInstanceANGLE(mode, GCGLSpanTuple { counts.data(), offsets, instanceCounts.data(), baseVertices.data(), baseInstances.data(), counts.size() }, type);
}

RefPtr<RemoteGraphicsContextGL::GCGLContext> RemoteGraphicsContextGL::protectedContext()
{
    assertIsCurrent(workQueue());
    return m_context;
}

#if ENABLE(VIDEO)
Ref<RemoteVideoFrameObjectHeap> RemoteGraphicsContextGL::protectedVideoFrameObjectHeap() const
{
    return m_videoFrameObjectHeap;
}
#endif

void RemoteGraphicsContextGL::messageCheck(bool assertion)
{
    MESSAGE_CHECK(assertion, m_streamConnection);
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEBGL)
