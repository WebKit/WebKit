/*
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WCTileGrid.h"

#if USE(GRAPHICS_LAYER_WC)

#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(WCTileGrid, Tile);

WCTileGrid::Tile::Tile(WebCore::IntRect rect)
    : m_tileRect(rect)
    , m_dirtyRect(rect)
{
}

void WCTileGrid::Tile::addDirtyRect(const WebCore::IntRect& dirtyRect)
{
    WebCore::IntRect rect { dirtyRect };
    rect.intersect(m_tileRect);
    m_dirtyRect.unite(rect);
}

void WCTileGrid::Tile::clearDirtyRect()
{
    m_dirtyRect = { };
}

bool WCTileGrid::Tile::hasDirtyRect() const
{
    return !m_dirtyRect.isEmpty();
}

void WCTileGrid::setSize(const WebCore::IntSize& size)
{
    m_size = size;
    m_tiles.clear();
}

WebCore::IntRect WCTileGrid::tileRectFromPixelRect(const WebCore::IntRect& pixelRect)
{
    auto rect = WebCore::intersection(pixelRect, { { }, m_size });
    if (rect.isEmpty())
        return { };
    int x = rect.x();
    int y = rect.y();
    int maxX = rect.maxX();
    int maxY = rect.maxY();
    x /= tilePixelSize().width();
    y /= tilePixelSize().height();
    maxX = (maxX + tilePixelSize().width() - 1) / tilePixelSize().width();
    maxY = (maxY + tilePixelSize().height() - 1) / tilePixelSize().height();
    return { x, y, maxX - x, maxY - y };
}

WebCore::IntSize WCTileGrid::tileSizeFromPixelSize(const WebCore::IntSize& size)
{
    int width = (size.width() + tilePixelSize().width() - 1) / tilePixelSize().width();
    int height = (size.height() + tilePixelSize().height() - 1) / tilePixelSize().height();
    return { width, height };
}

WebCore::IntSize WCTileGrid::tilePixelSize() const
{
    return { 512, 512 };
}

void WCTileGrid::addDirtyRect(const WebCore::IntRect& dirtyRect)
{
    for (auto& iterator : m_tiles)
        iterator.value->addDirtyRect(dirtyRect);
}

void WCTileGrid::clearDirtyRects()
{
    m_tiles.removeIf([](auto& tile) {
        return tile.value->willRemove();
    });
    for (auto& iterator : m_tiles)
        iterator.value->clearDirtyRect();
}

bool WCTileGrid::ensureTile(TileIndex index)
{
    auto addResult = m_tiles.ensure(index, [&]() {
        int x1 = index.x() * tilePixelSize().width();
        int y1 = index.y() * tilePixelSize().height();
        int x2 = std::min(x1 + tilePixelSize().width(), m_size.width());
        int y2 = std::min(y1 + tilePixelSize().height(), m_size.height());
        WebCore::IntRect rect { x1, y1, x2 - x1, y2 - y1 };
        ASSERT(WebCore::IntRect({ }, m_size).contains(rect));
        return makeUnique<Tile>(rect);
    });
    if (!addResult.isNewEntry)
        addResult.iterator->value->setWillRemove(false);
    return addResult.isNewEntry;
}

bool WCTileGrid::setCoverageRect(const WebCore::IntRect& coverage)
{
    bool needsToPaint = false;
    auto tileRect = tileRectFromPixelRect(coverage);

    for (auto& iterator : m_tiles) {
        if (!tileRect.contains(iterator.key)) {
            iterator.value->setWillRemove(true);
            needsToPaint = true;
        }
    }
    for (int y = tileRect.y(); y < tileRect.maxY(); y++) {
        for (int x = tileRect.x(); x < tileRect.maxX(); x++) {
            if (ensureTile({ x, y }))
                needsToPaint = true;
        }
    }
    return needsToPaint;
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
