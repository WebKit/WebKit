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
#include "CoordinatedSceneStatePlayStation.h"

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
    ASSERT(m_pendingLayersToRemove.isEmpty());
    ASSERT(m_committedLayers.isEmpty());
}

void CoordinatedSceneState::setRootLayerChildren(Vector<Ref<CoordinatedPlatformLayer>>&& children)
{
    ASSERT(isMainRunLoop());

    {
        Locker locker { m_rootLayer->lock() };
        m_rootLayer->setChildren(WTF::move(children));
    }
    m_didChangeLayers = true;
}

void CoordinatedSceneState::addLayer(CoordinatedPlatformLayer& layer)
{
    assertIsMainThread();

    m_layers.add(layer);
    m_didChangeLayers = true;
}

void CoordinatedSceneState::removeLayer(CoordinatedPlatformLayer& layer)
{
    assertIsMainThread();

    m_layers.remove(layer);
    m_layersToRemove.add(layer);
    m_didChangeLayers = true;
}

void CoordinatedSceneState::didChangeLayerPendingScrollingPosition(CoordinatedPlatformLayer& layer)
{
    Locker locker { m_layersWithPendingScrollingPositionLock };
    ASSERT(!m_layersWithPendingScrollingPosition.contains(Ref { layer }));
    m_layersWithPendingScrollingPosition.append(layer);
}

bool CoordinatedSceneState::flush()
{
    assertIsMainThread();

    bool didChangeLayers = m_didChangeLayers.exchange(false);
    if (didChangeLayers) {
        Locker pendingLayersLock { m_pendingLayersLock };
        m_pendingLayers = m_layers;

        if (m_pendingLayersToRemove.isEmpty())
            m_pendingLayersToRemove = WTF::move(m_layersToRemove);
        else
            m_pendingLayersToRemove.addAll(std::exchange(m_layersToRemove, { }));
    }

    return didChangeLayers;
}

void CoordinatedSceneState::commitPendingScrollingPositions()
{
    Locker stateLock { m_stateLock };

    Vector<Ref<CoordinatedPlatformLayer>> layers;
    {
        Locker pendingLayersLock { m_layersWithPendingScrollingPositionLock };
        layers = WTF::move(m_layersWithPendingScrollingPosition);
    }

    for (Ref layer : layers)
        layer->commitPendingScrollingPosition();
}

void CoordinatedSceneState::commitPendingLayers()
{
    ASSERT(!isMainRunLoop());
    Locker pendingLayersLock { m_pendingLayersLock };
    while (!m_pendingLayersToRemove.isEmpty()) {
        auto layer = m_pendingLayersToRemove.takeAny();
        layer->invalidateTarget();
    }

    if (!m_pendingLayers.isEmpty())
        m_committedLayers = WTF::move(m_pendingLayers);
}

CoordinatedSceneState::ChangedPreRenderedTiles CoordinatedSceneState::flushCompositingState(const OptionSet<CompositionReason>& reasons)
{
    commitPendingLayers();

    Vector<Ref<CoordinatedPlatformLayer>, 16> layersWithPendingTileUpdates;
    m_rootLayer->flushCompositingState(reasons);
    for (auto& layer : m_committedLayers) {
        layer->flushCompositingState(reasons);
        if (layer->hasPendingBackingStoreTileUpdates())
            layersWithPendingTileUpdates.append(Ref { layer });
    }

    {
        Locker stateLock { m_stateLock };
        m_rootLayer->flushScrollingPositionChanges(reasons);
        for (auto& layer : m_committedLayers)
            layer->flushScrollingPositionChanges(reasons);
    }

    if (layersWithPendingTileUpdates.isEmpty())
        return ChangedPreRenderedTiles::No;

    for (auto& layer : layersWithPendingTileUpdates)
        layer->processPendingBackingStoreTileUpdates();

    return ChangedPreRenderedTiles::No;
}

void CoordinatedSceneState::invalidateCommittedLayers()
{
    ASSERT(!isMainRunLoop());
    commitPendingLayers();
    m_rootLayer->invalidateTarget();
    while (!m_committedLayers.isEmpty()) {
        auto layer = m_committedLayers.takeAny();
        layer->invalidateTarget();
    }
}

void CoordinatedSceneState::invalidate()
{
    assertIsMainThread();

    // Root layer doesn't have client nor backing stores to invalidate.
    auto layers = WTF::move(m_layers);
    for (Ref layer : layers)
        layer->invalidateClient();

    {
        Locker pendingLayersLock { m_pendingLayersLock };
        m_pendingLayers = { };
        m_pendingLayersToRemove = { };
    }

    {
        Locker pendingLayersLock { m_layersWithPendingScrollingPositionLock };
        m_layersWithPendingScrollingPosition = { };
    }
}

void CoordinatedSceneState::waitUntilPaintingFinished()
{
    ASSERT(isMainRunLoop());
    Locker pendingLayersLock { m_pendingLayersLock };
    for (auto& layer : m_pendingLayers)
        layer->waitUntilPaintingFinished();
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
