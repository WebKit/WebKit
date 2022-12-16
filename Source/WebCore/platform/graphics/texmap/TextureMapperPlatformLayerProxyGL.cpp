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

#include "BitmapTextureGL.h"
#include "TextureMapperGL.h"
#include "TextureMapperLayer.h"
#include "TextureMapperPlatformLayerBuffer.h"

#if USE(GLIB_EVENT_LOOP)
#include <wtf/glib/RunLoopSourcePriority.h>
#endif
#include <wtf/Scope.h>

static const Seconds releaseUnusedSecondsTolerance { 1_s };
static const Seconds releaseUnusedBuffersTimerInterval = { 500_ms };

namespace WebCore {

TextureMapperPlatformLayerProxyGL::TextureMapperPlatformLayerProxyGL() = default;

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

        m_releaseUnusedBuffersTimer = makeUnique<RunLoop::Timer>(RunLoop::current(), this, &TextureMapperPlatformLayerProxyGL::releaseUnusedBuffersTimerFired);
        m_compositorThreadUpdateTimer = makeUnique<RunLoop::Timer>(RunLoop::current(), this, &TextureMapperPlatformLayerProxyGL::compositorThreadUpdateTimerFired);

#if USE(GLIB_EVENT_LOOP)
        m_compositorThreadUpdateTimer->setPriority(RunLoopSourcePriority::CompositingThreadUpdateTimer);
        m_releaseUnusedBuffersTimer->setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
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

        m_currentBuffer = nullptr;
        m_pendingBuffer = nullptr;
        m_releaseUnusedBuffersTimer = nullptr;
        m_usedBuffers.clear();

        // Clear the timer and dispatch the update function manually now.
        m_compositorThreadUpdateTimer = nullptr;
        if (!m_compositorThreadUpdateFunction)
            return;
        updateFunction = WTFMove(m_compositorThreadUpdateFunction);
    }

    updateFunction();
}

void TextureMapperPlatformLayerProxyGL::pushNextBuffer(std::unique_ptr<TextureMapperPlatformLayerBuffer>&& newBuffer)
{
    ASSERT(m_lock.isHeld());
#if USE(ANGLE)
    // When the newBuffer overwrites m_pendingBuffer that is not swapped yet,
    // the layer related to m_pendingBuffer is flickering since its texture is destroyed.
    if (m_pendingBuffer && m_pendingBuffer->hasManagedTexture())
        return;
#endif
    m_pendingBuffer = WTFMove(newBuffer);
    m_wasBufferDropped = false;

    if (m_compositor)
        m_compositor->onNewBufferAvailable();
}

std::unique_ptr<TextureMapperPlatformLayerBuffer> TextureMapperPlatformLayerProxyGL::getAvailableBuffer(const IntSize& size, GLint internalFormat)
{
    ASSERT(m_lock.isHeld());
    ASSERT(m_compositorThread == &Thread::current());
    std::unique_ptr<TextureMapperPlatformLayerBuffer> availableBuffer;

    auto buffers = WTFMove(m_usedBuffers);
    for (auto& buffer : buffers) {
        if (!buffer)
            continue;

        if (!availableBuffer && buffer->canReuseWithoutReset(size, internalFormat)) {
            availableBuffer = WTFMove(buffer);
            availableBuffer->markUsed();
            continue;
        }
        m_usedBuffers.append(WTFMove(buffer));
    }

    if (!m_usedBuffers.isEmpty())
        scheduleReleaseUnusedBuffers();
    return availableBuffer;
}

void TextureMapperPlatformLayerProxyGL::appendToUnusedBuffers(std::unique_ptr<TextureMapperPlatformLayerBuffer> buffer)
{
    ASSERT(m_lock.isHeld());
    ASSERT(m_compositorThread == &Thread::current());
    m_usedBuffers.append(WTFMove(buffer));
    scheduleReleaseUnusedBuffers();
}

void TextureMapperPlatformLayerProxyGL::scheduleReleaseUnusedBuffers()
{
    if (!m_releaseUnusedBuffersTimer->isActive())
        m_releaseUnusedBuffersTimer->startOneShot(releaseUnusedBuffersTimerInterval);
}

void TextureMapperPlatformLayerProxyGL::releaseUnusedBuffersTimerFired()
{
    Locker locker { m_lock };
    if (m_usedBuffers.isEmpty())
        return;

    auto buffers = WTFMove(m_usedBuffers);
    MonotonicTime minUsedTime = MonotonicTime::now() - releaseUnusedSecondsTolerance;

    for (auto& buffer : buffers) {
        if (buffer && buffer->lastUsedTime() >= minUsedTime)
            m_usedBuffers.append(WTFMove(buffer));
    }
}

void TextureMapperPlatformLayerProxyGL::swapBuffer()
{
    ASSERT(m_compositorThread == &Thread::current());
    Locker locker { m_lock };
    if (!m_targetLayer || !m_pendingBuffer)
        return;

    auto prevBuffer = WTFMove(m_currentBuffer);

    m_currentBuffer = WTFMove(m_pendingBuffer);
    m_targetLayer->setContentsLayer(m_currentBuffer.get());

    if (prevBuffer && prevBuffer->hasManagedTexture())
        appendToUnusedBuffers(WTFMove(prevBuffer));
}

void TextureMapperPlatformLayerProxyGL::dropCurrentBufferWhilePreservingTexture(bool shouldWait)
{
    if (!shouldWait)
        ASSERT(m_lock.isHeld());

    if (m_pendingBuffer && m_pendingBuffer->hasManagedTexture()) {
        m_usedBuffers.append(WTFMove(m_pendingBuffer));
        scheduleReleaseUnusedBuffers();
    }

    if (!m_compositorThreadUpdateTimer)
        return;

    m_compositorThreadUpdateFunction =
        [this, shouldWait] {
            Locker locker { m_lock };

            auto maybeNotifySynchronousOperation = makeScopeExit([this, shouldWait]() {
                if (shouldWait) {
                    Locker locker { m_wasBufferDroppedLock };
                    m_wasBufferDropped = true;
                    m_wasBufferDroppedCondition.notifyAll();
                }
            });

            if (!m_compositor || !m_targetLayer || !m_currentBuffer)
                return;

            m_pendingBuffer = m_currentBuffer->clone();
            auto prevBuffer = WTFMove(m_currentBuffer);
            m_currentBuffer = WTFMove(m_pendingBuffer);
            m_targetLayer->setContentsLayer(m_currentBuffer.get());

            if (prevBuffer->hasManagedTexture())
                appendToUnusedBuffers(WTFMove(prevBuffer));
        };

    if (shouldWait) {
        Locker locker { m_wasBufferDroppedLock };
        m_wasBufferDropped = false;
    }

    m_compositorThreadUpdateTimer->startOneShot(0_s);
    if (shouldWait) {
        Locker locker { m_wasBufferDroppedLock };
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
