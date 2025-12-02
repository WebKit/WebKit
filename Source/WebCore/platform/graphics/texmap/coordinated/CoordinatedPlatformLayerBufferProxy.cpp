/*
 * Copyright (C) 2015, 2025 Igalia S.L.
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
#include "CoordinatedPlatformLayerBufferProxy.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedPlatformLayer.h"
#include "CoordinatedPlatformLayerBuffer.h"
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {

Ref<CoordinatedPlatformLayerBufferProxy> CoordinatedPlatformLayerBufferProxy::create()
{
    return adoptRef(*new CoordinatedPlatformLayerBufferProxy());
}

CoordinatedPlatformLayerBufferProxy::CoordinatedPlatformLayerBufferProxy() = default;

CoordinatedPlatformLayerBufferProxy::~CoordinatedPlatformLayerBufferProxy()
{
    ASSERT(!m_layer);
#if ENABLE(VIDEO) && USE(GSTREAMER)
    ASSERT(!m_compositingRunLoop);
#endif
}

void CoordinatedPlatformLayerBufferProxy::setTargetLayer(CoordinatedPlatformLayer* layer)
{
    ASSERT(RunLoop::isMain());
    Locker locker { m_lock };
    if (m_layer == layer)
        return;

    m_layer = layer;
    if (m_layer) {
#if ENABLE(VIDEO) && USE(GSTREAMER)
        m_compositingRunLoop = m_layer->compositingRunLoop();
#endif
    } else {
        m_pendingBuffer = nullptr;
#if ENABLE(VIDEO) && USE(GSTREAMER)
        m_compositingRunLoop = nullptr;
#endif
    }
}

void CoordinatedPlatformLayerBufferProxy::consumePendingBufferIfNeeded()
{
    ASSERT(RunLoop::isMain());
    Locker locker { m_lock };
    if (!m_pendingBuffer)
        return;

    if (m_layer)
        m_layer->setContentsBuffer(WTFMove(m_pendingBuffer));
    else
        m_pendingBuffer = nullptr;
}

void CoordinatedPlatformLayerBufferProxy::setDisplayBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& buffer)
{
    RefPtr<CoordinatedPlatformLayer> layer;
    {
        Locker locker { m_lock };
        if (!m_layer) {
            m_pendingBuffer = WTFMove(buffer);
            return;
        }

        m_pendingBuffer = nullptr;
        layer = m_layer;
    }

    {
        Locker layerLocker { layer->lock() };
        layer->setContentsBuffer(WTFMove(buffer), std::nullopt, CoordinatedPlatformLayer::RequireComposition::No);
    }
    layer->requestComposition(CompositionReason::VideoFrame);
}

#if ENABLE(VIDEO) && USE(GSTREAMER)
void CoordinatedPlatformLayerBufferProxy::dropCurrentBufferWhilePreservingTexture(ShouldWait shouldWait)
{
    RefPtr<RunLoop> compositingRunLoop;
    {
        Locker locker { m_lock };
        if (!m_layer || !m_compositingRunLoop)
            return;

        compositingRunLoop = m_compositingRunLoop;
    }

    auto dropCurrentBuffer = [this, protectedThis = Ref { *this }] {
        Locker locker { m_lock };
        if (!m_layer)
            return;

        m_layer->replaceCurrentContentsBufferWithCopy();
    };

    if (shouldWait == ShouldWait::No) {
        compositingRunLoop->dispatch(WTFMove(dropCurrentBuffer));
        return;
    }

    BinarySemaphore semaphore;
    compositingRunLoop->dispatch([&semaphore, function = WTFMove(dropCurrentBuffer)]() mutable {
        function();
        semaphore.signal();
    });
    semaphore.wait();
}
#endif

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
