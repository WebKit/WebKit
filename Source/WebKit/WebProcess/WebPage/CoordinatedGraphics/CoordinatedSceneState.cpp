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
    ASSERT(isMainRunLoop());
    {
        Locker locker { m_layersLock };
        m_layers.add(layer);
    }
    m_didChangeLayers = true;
}

void CoordinatedSceneState::removeLayer(CoordinatedPlatformLayer& layer)
{
    ASSERT(isMainRunLoop());
    {
        Locker locker { m_layersLock };
        m_layers.remove(layer);
    }
    m_layersToRemove.add(layer);
    m_didChangeLayers = true;
}

bool CoordinatedSceneState::flush()
{
    ASSERT(isMainRunLoop());

    bool didChangeLayers = m_didChangeLayers.exchange(false);
    if (didChangeLayers) {
        Locker pendingLayersLock { m_pendingLayersLock };
        {
            Locker locker { m_layersLock };
            m_pendingLayers = m_layers;
        }
        if (m_pendingLayersToRemove.isEmpty())
            m_pendingLayersToRemove = WTF::move(m_layersToRemove);
        else
            m_pendingLayersToRemove.addAll(std::exchange(m_layersToRemove, { }));
    }

    flushPendingState();

    return didChangeLayers;
}

void CoordinatedSceneState::flushPendingState()
{
    Locker stateLock { m_stateLock };
    Locker layersLock { m_layersLock };
    for (Ref layer : m_layers)
        layer->flushPendingState();
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

    {
        Locker stateLock { m_stateLock };
        m_rootLayer->flushPositionChanges(reasons);
        for (auto& layer : m_committedLayers)
            layer->flushPositionChanges(reasons);
    }

    Vector<Ref<CoordinatedPlatformLayer>, 16> layersWithPendingTileUpdates;
    m_rootLayer->flushCompositingState(reasons);
    for (auto& layer : m_committedLayers) {
        layer->flushCompositingState(reasons);
        if (layer->hasPendingBackingStoreTileUpdates())
            layersWithPendingTileUpdates.append(Ref { layer });
    }

    if (layersWithPendingTileUpdates.isEmpty())
        return ChangedPreRenderedTiles::No;

    for (auto& layer : layersWithPendingTileUpdates)
        layer->processPendingBackingStoreTileUpdates();

#if USE(TEXTURE_MAPPER)
    return ChangedPreRenderedTiles::No;
#else
    Locker locker { m_layersWithPreRenderedTilesLock };
    bool wasEmpty = m_layersWithPreRenderedTiles.isEmpty();
    for (auto& layer : layersWithPendingTileUpdates) {
        if (layer->hasPendingBackingStoreTileUpdates())
            m_layersWithPreRenderedTiles.add(layer);
    }
    return wasEmpty && !m_layersWithPreRenderedTiles.isEmpty() ? ChangedPreRenderedTiles::Yes : ChangedPreRenderedTiles::No;
#endif
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
    ASSERT(isMainRunLoop());
    // Root layer doesn't have client nor backing stores to invalidate.
    HashSet<Ref<CoordinatedPlatformLayer>> layers;
    {
        Locker locker { m_layersLock };
        layers = WTF::move(m_layers);
    }
    for (Ref layer : layers)
        layer->invalidateClient();

    {
        Locker pendingLayersLock { m_pendingLayersLock };
        m_pendingLayers = { };
        m_pendingLayersToRemove = { };
    }

#if !USE(TEXTURE_MAPPER)
    {
        Locker locker { m_layersWithPreRenderedTilesLock };
        m_layersWithPreRenderedTiles = { };
    }
#endif
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

#if !USE(TEXTURE_MAPPER)
CoordinatedSceneState::ChangedPreRenderedTiles CoordinatedSceneState::didPaintPreRenderedTile(WebCore::CoordinatedPlatformLayer& layer)
{
    Locker locker { m_layersWithPreRenderedTilesLock };
    bool wasEmpty = m_layersWithPreRenderedTiles.isEmpty();
    m_layersWithPreRenderedTiles.add(layer);
    return wasEmpty ? ChangedPreRenderedTiles::Yes : ChangedPreRenderedTiles::No;
}

CoordinatedSceneState::ChangedPreRenderedTiles CoordinatedSceneState::processLayersWithPreRenderedTiles(const Function<bool()>& shouldStopProcessingTiles)
{
    // Copy the list of layers because it can change while processing them.
    HashSet<Ref<CoordinatedPlatformLayer>> layers;
    {
        Locker locker { m_layersWithPreRenderedTilesLock };
        layers = WTF::move(m_layersWithPreRenderedTiles);
    }

    Vector<Ref<CoordinatedPlatformLayer>, 16> layersWithDelayedTileUpdates;
    for (Ref layer : layers) {
        // If layersWithDelayedTileUpdates is not empty, it's because we stopped processing pre-rendered tiles,
        // so just add the pending layers to layersWithDelayedTileUpdates to be re-added to m_layersWithPreRenderedTiles.
        if (!layersWithDelayedTileUpdates.isEmpty()) {
            layersWithDelayedTileUpdates.append(WTF::move(layer));
            continue;
        }

        bool wasStopped = layer->processPendingBackingStoreTileUpdatesIncludingPreRendered(shouldStopProcessingTiles);
        if (wasStopped)
            layersWithDelayedTileUpdates.append(WTF::move(layer));
    }

    // We didn't stop. If new pre-rendered tiles are ready, they already
    // scheduled a new task in the compositor.
    if (layersWithDelayedTileUpdates.isEmpty())
        return ChangedPreRenderedTiles::No;

    // Add the layers for which we stopped the updates back to m_layersWithPreRenderedTiles.
    Locker locker { m_layersWithPreRenderedTilesLock };
    bool wasEmpty = m_layersWithPreRenderedTiles.isEmpty();
    for (auto& layer : layersWithDelayedTileUpdates)
        m_layersWithPreRenderedTiles.add(WTF::move(layer));

    // Only re-schedule from the compositor if there weren't new pre-rendered tiles ready,
    // otherwise they already scheduled a new task in the compositor.
    return wasEmpty ? ChangedPreRenderedTiles::Yes : ChangedPreRenderedTiles::No;
}
#endif

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)
