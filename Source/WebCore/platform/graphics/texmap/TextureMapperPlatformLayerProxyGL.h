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

#include "TextureMapperPlatformLayerProxy.h"

#if USE(COORDINATED_GRAPHICS)

#include "TextureMapperGLHeaders.h"
#include <wtf/Condition.h>
#include <wtf/Function.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>

#ifndef NDEBUG
#include <wtf/Threading.h>
#endif

namespace WebCore {

class TextureMapperPlatformLayerProxyGL final : public TextureMapperPlatformLayerProxy {
    WTF_MAKE_FAST_ALLOCATED();
public:
    TextureMapperPlatformLayerProxyGL(bool disableBufferInvalidation = false);
    virtual ~TextureMapperPlatformLayerProxyGL();

    bool isGLBased() const override { return true; }

    WEBCORE_EXPORT void activateOnCompositingThread(Compositor*, TextureMapperLayer*) override;
    WEBCORE_EXPORT void invalidate() override;
    WEBCORE_EXPORT void swapBuffer() override;

    std::unique_ptr<TextureMapperPlatformLayerBuffer> getAvailableBuffer(const IntSize&, GLint internalFormat);
    void pushNextBuffer(std::unique_ptr<TextureMapperPlatformLayerBuffer>&&);

    void dropCurrentBufferWhilePreservingTexture(bool shouldWait = false);

    bool scheduleUpdateOnCompositorThread(Function<void()>&&);

private:
    void appendToUnusedBuffers(std::unique_ptr<TextureMapperPlatformLayerBuffer>);
    void scheduleReleaseUnusedBuffers();
    void releaseUnusedBuffersTimerFired();

    std::unique_ptr<TextureMapperPlatformLayerBuffer> m_currentBuffer;
    std::unique_ptr<TextureMapperPlatformLayerBuffer> m_pendingBuffer;
    bool m_disableBufferInvalidation;

    Lock m_wasBufferDroppedLock;
    Condition m_wasBufferDroppedCondition;
    bool m_wasBufferDropped WTF_GUARDED_BY_LOCK(m_wasBufferDroppedLock) { false };

    Vector<std::unique_ptr<TextureMapperPlatformLayerBuffer>> m_usedBuffers;
    std::unique_ptr<RunLoop::Timer> m_releaseUnusedBuffersTimer;

#if ASSERT_ENABLED
    RefPtr<Thread> m_compositorThread;
#endif

    void compositorThreadUpdateTimerFired();
    std::unique_ptr<RunLoop::Timer> m_compositorThreadUpdateTimer;
    Function<void()> m_compositorThreadUpdateFunction;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_TEXTUREMAPPER_PLATFORMLAYERPROXY(TextureMapperPlatformLayerProxyGL, isGLBased());

#endif // USE(COORDINATED_GRAPHICS)
