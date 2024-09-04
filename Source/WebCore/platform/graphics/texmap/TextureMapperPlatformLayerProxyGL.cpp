/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "TextureMapperPlatformLayerProxyGL.h"

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedPlatformLayerBuffer.h"
#include "TextureMapperLayer.h"
#include <wtf/Scope.h>

#if ENABLE(VIDEO) && USE(GSTREAMER)
#include "CoordinatedPlatformLayerBufferVideo.h"
#endif

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif

namespace WebCore {

TextureMapperPlatformLayerProxyGL::TextureMapperPlatformLayerProxyGL(ContentType contentType)
    : TextureMapperPlatformLayerProxy(contentType)
{
}

TextureMapperPlatformLayerProxyGL::~TextureMapperPlatformLayerProxyGL()
{
    Locker locker { m_lock };
    if (m_targetLayer)
        m_targetLayer->setContentsLayer(nullptr);
}

void TextureMapperPlatformLayerProxyGL::activateOnCompositingThread(Compositor* compositor, TextureMapperLayer* targetLayer)
{
#if ASSERT_ENABLED
    if (!m_compositorThread)
        m_compositorThread = &Thread::current();
#endif
    ASSERT(m_compositorThread == &Thread::current());
    ASSERT(compositor);
    ASSERT(targetLayer);
    Function<void()> updateFunction;
    {
        Locker locker { m_lock };
        m_compositor = compositor;
        // If the proxy is already active on another layer, remove the layer's reference to the current buffer.
        if (m_targetLayer)
            m_targetLayer->setContentsLayer(nullptr);
        m_targetLayer = targetLayer;
        if (m_targetLayer && m_currentBuffer)
            m_targetLayer->setContentsLayer(m_currentBuffer.get());

        m_compositorThreadUpdateTimer = makeUnique<RunLoop::Timer>(RunLoop::current(), this, &TextureMapperPlatformLayerProxyGL::compositorThreadUpdateTimerFired);
#if USE(GLIB_EVENT_LOOP)
        m_compositorThreadUpdateTimer->setPriority(RunLoopSourcePriority::CompositingThreadUpdateTimer);
#endif

        if (!m_compositorThreadUpdateFunction)
            return;
        updateFunction = WTFMove(m_compositorThreadUpdateFunction);
    }
    updateFunction();
}

void TextureMapperPlatformLayerProxyGL::invalidate()
{
    ASSERT(m_compositorThread == &Thread::current());
#if ASSERT_ENABLED
    m_compositorThread = nullptr;
#endif
    Function<void()> updateFunction;
    {
        Locker locker { m_lock };
        m_compositor = nullptr;
        if (m_targetLayer) {
            m_targetLayer->setContentsLayer(nullptr);
            m_targetLayer = nullptr;
        }

        if (contentType() != ContentType::HolePunch) {
            m_currentBuffer = nullptr;
            m_pendingBuffer = nullptr;
        }

        // Clear the timer and dispatch the update function manually now.
        m_compositorThreadUpdateTimer = nullptr;
        if (!m_compositorThreadUpdateFunction)
            return;
        updateFunction = WTFMove(m_compositorThreadUpdateFunction);
    }

    updateFunction();
}

void TextureMapperPlatformLayerProxyGL::pushNextBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&& newBuffer)
{
    ASSERT(m_lock.isHeld());
    m_pendingBuffer = WTFMove(newBuffer);
    m_wasBufferDropped = false;

#if HAVE(DISPLAY_LINK)
    // WebGL and Canvas changes will cause a composition request during layerFlush. We cannot request
    // a new compostion here as well or we may trigger two compositions instead of one.
    switch (contentType()) {
    case ContentType::WebGL:
    case ContentType::Canvas:
        return;
    default:
        break;
    }
#endif

    if (m_compositor)
        m_compositor->onNewBufferAvailable();
}

void TextureMapperPlatformLayerProxyGL::swapBuffer()
{
    ASSERT(m_compositorThread == &Thread::current());
    Locker locker { m_lock };
    if (!m_targetLayer || !m_pendingBuffer)
        return;

    m_currentBuffer = WTFMove(m_pendingBuffer);
    m_targetLayer->setContentsLayer(m_currentBuffer.get());
}

void TextureMapperPlatformLayerProxyGL::dropCurrentBufferWhilePreservingTexture(bool shouldWait)
{
    {
        Locker locker { m_lock };
        if (!isActive())
            return;
    }

    auto dropBufferFunction = [this, protectedThis = Ref { *this }, shouldWait] {
        Locker locker { m_lock };

        auto maybeNotifySynchronousOperation = makeScopeExit([this, shouldWait]() {
            if (shouldWait) {
                Locker locker { m_wasBufferDroppedLock };
                m_wasBufferDropped = true;
                m_wasBufferDroppedCondition.notifyAll();
            }
        });

        if (!isActive() || !m_currentBuffer)
            return;

        m_pendingBuffer = nullptr;
#if ENABLE(VIDEO) && USE(GSTREAMER)
        if (is<CoordinatedPlatformLayerBufferVideo>(*m_currentBuffer))
            m_pendingBuffer = downcast<CoordinatedPlatformLayerBufferVideo>(*m_currentBuffer).copyBuffer();
#endif

        m_currentBuffer = WTFMove(m_pendingBuffer);
        m_targetLayer->setContentsLayer(m_currentBuffer.get());
    };

    if (!scheduleUpdateOnCompositorThread(WTFMove(dropBufferFunction)))
        return;

    if (shouldWait) {
        Locker locker { m_wasBufferDroppedLock };
        m_wasBufferDropped = false;
        m_wasBufferDroppedCondition.wait(m_wasBufferDroppedLock, [this] {
            assertIsHeld(m_wasBufferDroppedLock);
            return m_wasBufferDropped;
        });
    }
}

bool TextureMapperPlatformLayerProxyGL::scheduleUpdateOnCompositorThread(Function<void()>&& updateFunction)
{
    Locker locker { m_lock };
    m_compositorThreadUpdateFunction = WTFMove(updateFunction);

    if (!m_compositorThreadUpdateTimer)
        return false;

    m_compositorThreadUpdateTimer->startOneShot(0_s);
    return true;
}

void TextureMapperPlatformLayerProxyGL::compositorThreadUpdateTimerFired()
{
    Function<void()> updateFunction;
    {
        Locker locker { m_lock };
        if (!m_compositorThreadUpdateFunction)
            return;
        updateFunction = WTFMove(m_compositorThreadUpdateFunction);
    }

    updateFunction();
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
