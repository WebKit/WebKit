/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BufferIdentifierSet.h"
#include "MarkSurfacesAsVolatileRequestIdentifier.h"
#include "PrepareBackingStoreBuffersData.h"
#include "RemoteDisplayListRecorderProxy.h"
#include "RemoteImageBufferSetIdentifier.h"
#include "RenderingUpdateID.h"
#include "WorkQueueMessageReceiver.h"
#include <wtf/Identified.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(GPU_PROCESS)

namespace IPC {
class Connection;
class Decoder;
class StreamClientConnection;
}

namespace WebKit {

class RemoteImageBufferSetProxyFlushFence;
struct BufferSetBackendHandle;

// FIXME: We should have a generic 'ImageBufferSet' class that contains
// the code that isn't specific to being remote, and this helper belongs
// there.
class ThreadSafeImageBufferSetFlusher {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(ThreadSafeImageBufferSetFlusher);
    WTF_MAKE_NONCOPYABLE(ThreadSafeImageBufferSetFlusher);
public:
    enum class FlushType {
        BackendHandlesOnly,
        BackendHandlesAndDrawing,
    };

    ThreadSafeImageBufferSetFlusher() = default;
    virtual ~ThreadSafeImageBufferSetFlusher() = default;
    // Returns true if flush succeeded, false if it failed.
    virtual bool flushAndCollectHandles(HashMap<RemoteImageBufferSetIdentifier, std::unique_ptr<BufferSetBackendHandle>>&) = 0;
};

// A RemoteImageBufferSet is a set of three ImageBuffers (front, back,
// secondary back) owned by the GPU process, for the purpose of drawing
// successive (layer) frames.
// To draw a frame, the consumer allocates a new RemoteDisplayListRecorderProxy and
// asks the RemoteImageBufferSet set to map it to an appropriate new front
// buffer (either by picking one of the back buffers, or by allocating a new
// one). It then copies across the pixels from the previous front buffer,
// clips to the dirty region and clears that region.
// Usage is done through RemoteRenderingBackendProxy::prepareImageBufferSetsForDisplay,
// so that a Vector of RemoteImageBufferSets can be used with a single
// IPC call.
class RemoteImageBufferSetProxy : public IPC::WorkQueueMessageReceiver, public Identified<RemoteImageBufferSetIdentifier> {
public:
    RemoteImageBufferSetProxy(RemoteRenderingBackendProxy&);
    ~RemoteImageBufferSetProxy();

    OptionSet<BufferInSetType> requestedVolatility() { return m_requestedVolatility; }
    OptionSet<BufferInSetType> confirmedVolatility() { return m_confirmedVolatility; }
    void clearVolatilityUntilAfter(MarkSurfacesAsVolatileRequestIdentifier previousVolatilityRequest);
    void addRequestedVolatility(OptionSet<BufferInSetType> request);
    void setConfirmedVolatility(MarkSurfacesAsVolatileRequestIdentifier, OptionSet<BufferInSetType> types);

#if PLATFORM(COCOA)
    void didPrepareForDisplay(ImageBufferSetPrepareBufferForDisplayOutputData, RenderingUpdateID);
#endif

    WebCore::GraphicsContext& context();
    bool hasContext() const { return !!m_displayListRecorder; }

    WebCore::RenderingResourceIdentifier displayListResourceIdentifier() const { return m_displayListIdentifier; }

    std::unique_ptr<ThreadSafeImageBufferSetFlusher> flushFrontBufferAsync(ThreadSafeImageBufferSetFlusher::FlushType);

    void setConfiguration(WebCore::FloatSize, float, const WebCore::DestinationColorSpace&, WebCore::ImageBufferPixelFormat, WebCore::RenderingMode, WebCore::RenderingPurpose);
    void willPrepareForDisplay();
    void remoteBufferSetWasDestroyed();

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    std::optional<WebCore::DynamicContentScalingDisplayList> dynamicContentScalingDisplayList();
#endif

    unsigned generation() const { return m_generation; }

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    void close();

private:
    template<typename T> auto send(T&& message);
    template<typename T> auto sendSync(T&& message);
    RefPtr<IPC::StreamClientConnection> connection() const;
    void didBecomeUnresponsive() const;

    WeakPtr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;

    WebCore::RenderingResourceIdentifier m_displayListIdentifier;
    std::unique_ptr<RemoteDisplayListRecorderProxy> m_displayListRecorder;

    MarkSurfacesAsVolatileRequestIdentifier m_minimumVolatilityRequest;
    OptionSet<BufferInSetType> m_requestedVolatility;
    OptionSet<BufferInSetType> m_confirmedVolatility;

    WebCore::FloatSize m_size;
    float m_scale { 1.0f };
    WebCore::DestinationColorSpace m_colorSpace { WebCore::DestinationColorSpace::SRGB() };
    WebCore::ImageBufferPixelFormat m_pixelFormat;
    WebCore::RenderingMode m_renderingMode { WebCore::RenderingMode::Unaccelerated };
    WebCore::RenderingPurpose m_renderingPurpose { WebCore::RenderingPurpose::Unspecified };
    unsigned m_generation { 0 };
    bool m_remoteNeedsConfigurationUpdate { false };

    Lock m_lock;
    RefPtr<RemoteImageBufferSetProxyFlushFence> m_pendingFlush WTF_GUARDED_BY_LOCK(m_lock);
    RefPtr<IPC::StreamClientConnection> m_streamConnection  WTF_GUARDED_BY_LOCK(m_lock);
    bool m_prepareForDisplayIsPending WTF_GUARDED_BY_LOCK(m_lock) { false };
    bool m_closed WTF_GUARDED_BY_LOCK(m_lock) { false };
};

inline TextStream& operator<<(TextStream& ts, RemoteImageBufferSetProxy& bufferSet)
{
    ts << bufferSet.identifier();
    return ts;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
