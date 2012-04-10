/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "TiledBackingStoreRemoteTile.h"

#if USE(TILED_BACKING_STORE)

#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "SurfaceUpdateInfo.h"
#include "TiledBackingStoreClient.h"

using namespace WebCore;

namespace WebKit {

TiledBackingStoreRemoteTile::TiledBackingStoreRemoteTile(TiledBackingStoreRemoteTileClient* client, TiledBackingStore* tiledBackingStore, const Coordinate& tileCoordinate)
    : m_client(client)
    , m_tiledBackingStore(tiledBackingStore)
    , m_coordinate(tileCoordinate)
    , m_rect(tiledBackingStore->tileRectForCoordinate(tileCoordinate))
    , m_ID(0)
    , m_dirtyRect(m_rect)
{
}

TiledBackingStoreRemoteTile::~TiledBackingStoreRemoteTile()
{
    if (m_ID)
        m_client->removeTile(m_ID);
}

bool TiledBackingStoreRemoteTile::isDirty() const
{
    return !m_dirtyRect.isEmpty();
}

void TiledBackingStoreRemoteTile::invalidate(const IntRect& dirtyRect)
{
    IntRect tileDirtyRect = intersection(dirtyRect, m_rect);
    if (tileDirtyRect.isEmpty())
        return;

    m_dirtyRect.unite(tileDirtyRect);
}

Vector<IntRect> TiledBackingStoreRemoteTile::updateBackBuffer()
{
    if (!isDirty())
        return Vector<IntRect>();

    SurfaceUpdateInfo updateInfo;
    OwnPtr<GraphicsContext> graphicsContext = m_client->beginContentUpdate(m_dirtyRect.size(), updateInfo.surfaceHandle, updateInfo.surfaceOffset);
    if (!graphicsContext)
        return Vector<IntRect>();
    graphicsContext->translate(-m_dirtyRect.x(), -m_dirtyRect.y());
    graphicsContext->scale(FloatSize(m_tiledBackingStore->contentsScale(), m_tiledBackingStore->contentsScale()));
    m_tiledBackingStore->client()->tiledBackingStorePaint(graphicsContext.get(), m_tiledBackingStore->mapToContents(m_dirtyRect));

    updateInfo.updateRect = m_dirtyRect;
    updateInfo.updateRect.move(-m_rect.x(), -m_rect.y());
    updateInfo.scaleFactor = m_tiledBackingStore->contentsScale();
    graphicsContext.release();

    static int id = 0;
    if (!m_ID) {
        m_ID = ++id;
        m_client->createTile(m_ID, updateInfo, m_rect);
    } else
        m_client->updateTile(m_ID, updateInfo, m_rect);

    m_dirtyRect = IntRect();
    return Vector<IntRect>();
}

void TiledBackingStoreRemoteTile::swapBackBufferToFront()
{
    // Handled by tiledBackingStorePaintEnd.
}

bool TiledBackingStoreRemoteTile::isReadyToPaint() const
{
    return !!m_ID;
}

void TiledBackingStoreRemoteTile::paint(GraphicsContext* context, const IntRect& rect)
{
    ASSERT_NOT_REACHED();
}

void TiledBackingStoreRemoteTile::resize(const IntSize& newSize)
{
    m_rect = IntRect(m_rect.location(), newSize);
    m_dirtyRect = m_rect;
}

TiledBackingStoreRemoteTileBackend::TiledBackingStoreRemoteTileBackend(TiledBackingStoreRemoteTileClient* client)
    : m_client(client)
{
}

PassRefPtr<WebCore::Tile> TiledBackingStoreRemoteTileBackend::createTile(WebCore::TiledBackingStore* tiledBackingStore, const WebCore::Tile::Coordinate& tileCoordinate)
{
    return TiledBackingStoreRemoteTile::create(m_client, tiledBackingStore, tileCoordinate);
}

void TiledBackingStoreRemoteTileBackend::paintCheckerPattern(WebCore::GraphicsContext*, const WebCore::FloatRect&)
{
}

} // namespace WebKit

#endif // USE(TILED_BACKING_STORE)
