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

#pragma once

#if USE(COORDINATED_GRAPHICS)
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
class CoordinatedPlatformLayer;
class CoordinatedPlatformLayerBuffer;
class IntSize;
class TextureMapperLayer;

class TextureMapperPlatformLayerProxy final : public ThreadSafeRefCounted<TextureMapperPlatformLayerProxy> {
public:
    enum class ContentType : uint8_t {
        WebGL,
        Video,
        OffscreenCanvas,
        HolePunch,
        Canvas
    };

    static Ref<TextureMapperPlatformLayerProxy> create(ContentType contentType)
    {
        return adoptRef(*new TextureMapperPlatformLayerProxy(contentType));
    }

    virtual ~TextureMapperPlatformLayerProxy();

    ContentType contentType() const { return m_contentType; }

    bool isActive();
    void activateOnCompositingThread(CoordinatedPlatformLayer&);
    void invalidate();
    void swapBuffer();

    void pushNextBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&);
    void dropCurrentBufferWhilePreservingTexture(bool shouldWait);
    void setSwapBuffersFunction(Function<void(TextureMapperPlatformLayerProxy&)>&& function) { m_swapBuffersFunction = WTFMove(function); }
    void swapBuffersIfNeeded();

private:
    explicit TextureMapperPlatformLayerProxy(ContentType);

    bool isActiveLocked() const;

    bool scheduleUpdateOnCompositorThread(Function<void()>&&);
    void compositorThreadUpdateTimerFired();

    Lock m_lock;
    RefPtr<CoordinatedPlatformLayer> m_targetLayer;
#if ASSERT_ENABLED
    RefPtr<Thread> m_compositorThread;
#endif
    std::unique_ptr<RunLoop::Timer> m_compositorThreadUpdateTimer;
    Function<void()> m_compositorThreadUpdateFunction;

    ContentType m_contentType;
    std::unique_ptr<CoordinatedPlatformLayerBuffer> m_currentBuffer;
    std::unique_ptr<CoordinatedPlatformLayerBuffer> m_pendingBuffer;
    Function<void(TextureMapperPlatformLayerProxy&)> m_swapBuffersFunction;

    Lock m_wasBufferDroppedLock;
    Condition m_wasBufferDroppedCondition;
    bool m_wasBufferDropped WTF_GUARDED_BY_LOCK(m_wasBufferDroppedLock) { false };
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
