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

#pragma once

#if USE(COORDINATED_GRAPHICS)
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
class CoordinatedPlatformLayer;
class CoordinatedPlatformLayerBuffer;
class IntSize;
class TextureMapperLayer;

class CoordinatedPlatformLayerBufferProxy final : public ThreadSafeRefCounted<CoordinatedPlatformLayerBufferProxy> {
public:
    static Ref<CoordinatedPlatformLayerBufferProxy> create();
#if ENABLE(VIDEO) && USE(GSTREAMER)
    // layerAttachedCallback is invoked whenever a new (non-null) target layer is attached,
    // e.g. because the previous layer was destroyed and recreated, which happens whenever
    // the render tree is rebuilt, such as when a page is restored from the back/forward cache.
    // This lets the owning media player re-deliver its current frame so the new layer isn't
    // left blank until the next decoded sample arrives (which, for a paused player, may never happen).
    static Ref<CoordinatedPlatformLayerBufferProxy> create(Function<void()>&& layerAttachedCallback);
#endif
    ~CoordinatedPlatformLayerBufferProxy();

    void setTargetLayer(CoordinatedPlatformLayer*);
    void consumePendingBufferIfNeeded();
    void setDisplayBuffer(std::unique_ptr<CoordinatedPlatformLayerBuffer>&&);

#if ENABLE(VIDEO) && USE(GSTREAMER)
    enum class ShouldWait : bool { No, Yes };
    void dropCurrentBufferWhilePreservingTexture(ShouldWait);
#endif

private:
    CoordinatedPlatformLayerBufferProxy();
#if ENABLE(VIDEO) && USE(GSTREAMER)
    CoordinatedPlatformLayerBufferProxy(Function<void()>&& layerAttachedCallback);
#endif

    Lock m_lock;
    RefPtr<CoordinatedPlatformLayer> m_layer WTF_GUARDED_BY_LOCK(m_lock);
    std::unique_ptr<CoordinatedPlatformLayerBuffer> m_pendingBuffer WTF_GUARDED_BY_LOCK(m_lock);
#if ENABLE(VIDEO) && USE(GSTREAMER)
    RefPtr<RunLoop> m_compositingRunLoop WTF_GUARDED_BY_LOCK(m_lock);
    Function<void()> m_layerAttachedCallback;
#endif
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
