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
#include "Tile.h"

#if USE(COORDINATED_GRAPHICS)

#include "TiledBackingStore.h"
#include "TiledBackingStoreClient.h"

namespace WebCore {

static const uint32_t InvalidTileID = 0;

Tile::Tile(TiledBackingStore& tiledBackingStore, const Coordinate& tileCoordinate)
    : m_tiledBackingStore(tiledBackingStore)
    , m_ID(InvalidTileID)
    , m_coordinate(tileCoordinate)
    , m_rect(tiledBackingStore.tileRectForCoordinate(tileCoordinate))
    , m_dirtyRect(m_rect)
{
}

Tile::~Tile()
{
    if (m_ID != InvalidTileID)
        m_tiledBackingStore.client().removeTile(m_ID);
}

void Tile::ensureTileID()
{
    static uint32_t id = 1;
    if (m_ID == InvalidTileID) {
        m_ID = id++;
        // We may get an invalid ID due to wrap-around on overflow.
        if (m_ID == InvalidTileID)
            m_ID = id++;
        m_tiledBackingStore.client().createTile(m_ID, m_tiledBackingStore.contentsScale());
    }
}

bool Tile::isDirty() const
{
    return !m_dirtyRect.isEmpty();
}

bool Tile::isReadyToPaint() const
{
    return m_ID != InvalidTileID;
}

void Tile::invalidate(const IntRect& dirtyRect)
{
    IntRect tileDirtyRect = intersection(dirtyRect, m_rect);
    if (!tileDirtyRect.isEmpty())
        m_dirtyRect.unite(tileDirtyRect);
}

void Tile::markClean()
{
    m_dirtyRect = { };
}

void Tile::resize(const IntSize& newSize)
{
    m_rect = IntRect(m_rect.location(), newSize);
    m_dirtyRect = m_rect;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
