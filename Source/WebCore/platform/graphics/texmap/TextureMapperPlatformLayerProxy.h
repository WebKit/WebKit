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

#ifndef TextureMapperPlatformLayerProxy_h
#define TextureMapperPlatformLayerProxy_h

#include "GraphicsTypes3D.h"
#include "TextureMapper.h"
#include "TransformationMatrix.h"
#include <wtf/Condition.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

#ifndef NDEBUG
#include <wtf/Threading.h>
#endif

namespace WebCore {

class TextureMapperLayer;
class TextureMapperPlatformLayerProxy;
class TextureMapperPlatformLayerBuffer;

class TextureMapperPlatformLayerProxyProvider {
public:
    virtual RefPtr<TextureMapperPlatformLayerProxy> proxy() const = 0;
    virtual void swapBuffersIfNeeded() = 0;
};

class TextureMapperPlatformLayerProxy : public ThreadSafeRefCounted<TextureMapperPlatformLayerProxy> {
    WTF_MAKE_FAST_ALLOCATED();
public:
    class Compositor {
    public:
        virtual void onNewBufferAvailable() = 0;
    };

    TextureMapperPlatformLayerProxy();
    virtual ~TextureMapperPlatformLayerProxy();

    // To avoid multiple lock/release situation to update a single frame,
    // the implementation of TextureMapperPlatformLayerProxyProvider should
    // aquire / release the lock explicitly to use below methods.
    Lock& lock() { return m_lock; }
    std::unique_ptr<TextureMapperPlatformLayerBuffer> getAvailableBuffer(const IntSize&, GC3Dint internalFormat);
    void pushNextBuffer(std::unique_ptr<TextureMapperPlatformLayerBuffer>);
    bool isActive();

    void activateOnCompositingThread(Compositor*, TextureMapperLayer*);
    void invalidate();

    void swapBuffer();

    bool scheduleUpdateOnCompositorThread(std::function<void()>&&);

private:
    void scheduleReleaseUnusedBuffers();
    void releaseUnusedBuffersTimerFired();

    Compositor* m_compositor;
    TextureMapperLayer* m_targetLayer;

    std::unique_ptr<TextureMapperPlatformLayerBuffer> m_currentBuffer;
    std::unique_ptr<TextureMapperPlatformLayerBuffer> m_pendingBuffer;

    Lock m_lock;

    Vector<std::unique_ptr<TextureMapperPlatformLayerBuffer>> m_usedBuffers;

    RunLoop::Timer<TextureMapperPlatformLayerProxy> m_releaseUnusedBuffersTimer;
#ifndef NDEBUG
    ThreadIdentifier m_compositorThreadID { 0 };
#endif

    void compositorThreadUpdateTimerFired();
    std::unique_ptr<RunLoop::Timer<TextureMapperPlatformLayerProxy>> m_compositorThreadUpdateTimer;
    std::function<void()> m_compositorThreadUpdateFunction;
};

} // namespace WebCore

#endif // TextureMapperPlatformLayerProxy_h
