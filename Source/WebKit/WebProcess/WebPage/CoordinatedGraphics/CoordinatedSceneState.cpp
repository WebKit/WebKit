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

#include "config.h"
#include "CoordinatedSceneState.h"

#if USE(COORDINATED_GRAPHICS)
#include <WebCore/CoordinatedPlatformLayer.h>
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(CoordinatedSceneState);

CoordinatedSceneState::CoordinatedSceneState()
    : m_rootLayer(CoordinatedPlatformLayer::create())
{
    ASSERT(isMainRunLoop());
}

CoordinatedSceneState::~CoordinatedSceneState()
{
    ASSERT(m_layers.isEmpty());
    ASSERT(m_pendingLayers.isEmpty());
    ASSERT(m_committedLayers.isEmpty());
}

void CoordinatedSceneState::setRootLayerChildren(Vector<Ref<CoordinatedPlatformLayer>>&& children)
{
    ASSERT(isMainRunLoop());

    {
        Locker locker { m_rootLayer->lock() };
        m_rootLayer->setChildren(WTFMove(children));
    }
    m_didChangeLayers = true;
}

void CoordinatedSceneState::addLayer(CoordinatedPlatformLayer& layer)
{
    ASSERT(isMainRunLoop());
    m_layers.add(layer);
    m_didChangeLayers = true;
}

void CoordinatedSceneState::removeLayer(CoordinatedPlatformLayer& layer)
{
    ASSERT(isMainRunLoop());
    m_layers.remove(layer);
    m_didChangeLayers = true;
}

bool CoordinatedSceneState::flush()
{
    ASSERT(isMainRunLoop());
    if (!m_didChangeLayers)
        return false;

    m_didChangeLayers = false;

    Locker pendingLayersLock { m_pendingLayersLock };
    m_pendingLayers = m_layers;
    return true;
}

const HashSet<Ref<CoordinatedPlatformLayer>>& CoordinatedSceneState::committedLayers()
{
    ASSERT(!isMainRunLoop());
    Locker pendingLayersLock { m_pendingLayersLock };
    if (!m_pendingLayers.isEmpty()) {
        auto removedLayers = m_committedLayers.differenceWith(m_pendingLayers);
        m_committedLayers = WTFMove(m_pendingLayers);
        for (auto& layer : removedLayers)
            layer->invalidateTarget();
    }
    return m_committedLayers;
}

void CoordinatedSceneState::invalidateCommittedLayers()
{
    ASSERT(!isMainRunLoop());
    m_rootLayer->invalidateTarget();
    while (!m_committedLayers.isEmpty()) {
        auto layer = m_committedLayers.takeAny();
        layer->invalidateTarget();
    }
}

void CoordinatedSceneState::invalidate()
{
    ASSERT(isMainRunLoop());
    // Root layer doesn't have client nor backing stores to invalidate.
    while (!m_layers.isEmpty()) {
        auto layer = m_layers.takeAny();
        layer->invalidateClient();
    }

    Locker pendingLayersLock { m_pendingLayersLock };
    m_pendingLayers = { };
}

void CoordinatedSceneState::waitUntilPaintingComplete()
{
    ASSERT(isMainRunLoop());
    Locker pendingLayersLock { m_pendingLayersLock };
    for (auto& layer : m_pendingLayers)
        layer->waitUntilPaintingComplete();
}

void CoordinatedSceneState::willPaintTile()
{
    m_pendingTiles++;
}

void CoordinatedSceneState::didPaintTile()
{
    ASSERT(m_pendingTiles.load() > 0);
    m_pendingTiles--;
}

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
