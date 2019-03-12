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
#include "TextureMapperPlatformLayerProxy.h"

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

TextureMapperPlatformLayerProxy::TextureMapperPlatformLayerProxy()
    : m_compositor(nullptr)
    , m_targetLayer(nullptr)
{
}

TextureMapperPlatformLayerProxy::~TextureMapperPlatformLayerProxy()
{
    LockHolder locker(m_lock);
    if (m_targetLayer)
        m_targetLayer->setContentsLayer(nullptr);
}

void TextureMapperPlatformLayerProxy::activateOnCompositingThread(Compositor* compositor, TextureMapperLayer* targetLayer)
{
#ifndef NDEBUG
    if (!m_compositorThread)
        m_compositorThread = &Thread::current();
#endif
    ASSERT(m_compositorThread == &Thread::current());
    ASSERT(compositor);
    ASSERT(targetLayer);
    LockHolder locker(m_lock);
    m_compositor = compositor;
    m_targetLayer = targetLayer;
    if (m_targetLayer && m_currentBuffer)
        m_targetLayer->setContentsLayer(m_currentBuffer.get());

    m_releaseUnusedBuffersTimer = std::make_unique<RunLoop::Timer<TextureMapperPlatformLayerProxy>>(RunLoop::current(), this, &TextureMapperPlatformLayerProxy::releaseUnusedBuffersTimerFired);
    m_compositorThreadUpdateTimer = std::make_unique<RunLoop::Timer<TextureMapperPlatformLayerProxy>>(RunLoop::current(), this, &TextureMapperPlatformLayerProxy::compositorThreadUpdateTimerFired);

#if USE(GLIB_EVENT_LOOP)
    m_compositorThreadUpdateTimer->setPriority(RunLoopSourcePriority::CompositingThreadUpdateTimer);
    m_releaseUnusedBuffersTimer->setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
#endif
}

void TextureMapperPlatformLayerProxy::invalidate()
{
    ASSERT(m_compositorThread == &Thread::current());
    Function<void()> updateFunction;
    {
        LockHolder locker(m_lock);
        m_compositor = nullptr;
        m_targetLayer = nullptr;

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

bool TextureMapperPlatformLayerProxy::isActive()
{
    ASSERT(m_lock.isHeld());
    return !!m_targetLayer && !!m_compositor;
}

void TextureMapperPlatformLayerProxy::pushNextBuffer(std::unique_ptr<TextureMapperPlatformLayerBuffer> newBuffer)
{
    ASSERT(m_lock.isHeld());
    m_pendingBuffer = WTFMove(newBuffer);
    m_wasBufferDropped = false;

    if (m_compositor)
        m_compositor->onNewBufferAvailable();
}

std::unique_ptr<TextureMapperPlatformLayerBuffer> TextureMapperPlatformLayerProxy::getAvailableBuffer(const IntSize& size, GLint internalFormat)
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

void TextureMapperPlatformLayerProxy::appendToUnusedBuffers(std::unique_ptr<TextureMapperPlatformLayerBuffer> buffer)
{
    ASSERT(m_lock.isHeld());
    ASSERT(m_compositorThread == &Thread::current());
    m_usedBuffers.append(WTFMove(buffer));
    scheduleReleaseUnusedBuffers();
}

void TextureMapperPlatformLayerProxy::scheduleReleaseUnusedBuffers()
{
    if (!m_releaseUnusedBuffersTimer->isActive())
        m_releaseUnusedBuffersTimer->startOneShot(releaseUnusedBuffersTimerInterval);
}

void TextureMapperPlatformLayerProxy::releaseUnusedBuffersTimerFired()
{
    LockHolder locker(m_lock);
    if (m_usedBuffers.isEmpty())
        return;

    auto buffers = WTFMove(m_usedBuffers);
    MonotonicTime minUsedTime = MonotonicTime::now() - releaseUnusedSecondsTolerance;

    for (auto& buffer : buffers) {
        if (buffer && buffer->lastUsedTime() >= minUsedTime)
            m_usedBuffers.append(WTFMove(buffer));
    }
}

void TextureMapperPlatformLayerProxy::swapBuffer()
{
    ASSERT(m_compositorThread == &Thread::current());
    LockHolder locker(m_lock);
    if (!m_targetLayer || !m_pendingBuffer)
        return;

    auto prevBuffer = WTFMove(m_currentBuffer);

    m_currentBuffer = WTFMove(m_pendingBuffer);
    m_targetLayer->setContentsLayer(m_currentBuffer.get());

    if (prevBuffer && prevBuffer->hasManagedTexture())
        appendToUnusedBuffers(WTFMove(prevBuffer));
}

void TextureMapperPlatformLayerProxy::dropCurrentBufferWhilePreservingTexture(bool shouldWait)
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
            LockHolder locker(m_lock);

            auto maybeNotifySynchronousOperation = WTF::makeScopeExit([this, shouldWait]() {
                if (shouldWait) {
                    LockHolder holder(m_wasBufferDroppedLock);
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
        LockHolder holder(m_wasBufferDroppedLock);
        m_wasBufferDropped = false;
    }

    m_compositorThreadUpdateTimer->startOneShot(0_s);
    if (shouldWait) {
        LockHolder holder(m_wasBufferDroppedLock);
        m_wasBufferDroppedCondition.wait(m_wasBufferDroppedLock, [this] {
            return m_wasBufferDropped;
        });
    }
}

bool TextureMapperPlatformLayerProxy::scheduleUpdateOnCompositorThread(Function<void()>&& updateFunction)
{
    LockHolder locker(m_lock);
    if (!m_compositorThreadUpdateTimer)
        return false;

    m_compositorThreadUpdateFunction = WTFMove(updateFunction);
    m_compositorThreadUpdateTimer->startOneShot(0_s);
    return true;
}

void TextureMapperPlatformLayerProxy::compositorThreadUpdateTimerFired()
{
    Function<void()> updateFunction;
    {
        LockHolder locker(m_lock);
        if (!m_compositorThreadUpdateFunction)
            return;
        updateFunction = WTFMove(m_compositorThreadUpdateFunction);
    }

    updateFunction();
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
