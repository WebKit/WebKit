/*
 * Copyright (C) 2024 Igalia S.L.
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
#include <atomic>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
class CoordinatedPlatformLayer;
}

namespace WebKit {

class CoordinatedSceneState final : public ThreadSafeRefCounted<CoordinatedSceneState> {
    WTF_MAKE_TZONE_ALLOCATED(CoordinatedSceneState);
public:
    static Ref<CoordinatedSceneState> create()
    {
        return adoptRef(*new CoordinatedSceneState());
    }
    ~CoordinatedSceneState();

    WebCore::CoordinatedPlatformLayer& rootLayer() const { return m_rootLayer.get(); }

    void setRootLayerChildren(Vector<Ref<WebCore::CoordinatedPlatformLayer>>&&);
    void addLayer(WebCore::CoordinatedPlatformLayer&);
    void removeLayer(WebCore::CoordinatedPlatformLayer&);

    bool flush();
    void invalidate();

    const HashSet<Ref<WebCore::CoordinatedPlatformLayer>>& committedLayers();
    void invalidateCommittedLayers();

    bool layersDidChange() const { return m_didChangeLayers; }

    void waitUntilPaintingComplete();

    void willPaintTile();
    void didPaintTile();
    unsigned pendingTiles() const { return m_pendingTiles.load(); }

private:
    CoordinatedSceneState();

    const Ref<WebCore::CoordinatedPlatformLayer> m_rootLayer;
    HashSet<Ref<WebCore::CoordinatedPlatformLayer>> m_layers;
    Lock m_pendingLayersLock;
    HashSet<Ref<WebCore::CoordinatedPlatformLayer>> m_pendingLayers WTF_GUARDED_BY_LOCK(m_pendingLayersLock);
    std::atomic<bool> m_didChangeLayers { false };
    HashSet<Ref<WebCore::CoordinatedPlatformLayer>> m_committedLayers;
    std::atomic<unsigned> m_pendingTiles { 0 };
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

