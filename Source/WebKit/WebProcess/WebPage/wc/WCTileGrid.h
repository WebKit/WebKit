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

#pragma once

#if USE(GRAPHICS_LAYER_WC)

#include <WebCore/IntRect.h>
#include <WebCore/TextureMapperSparseBackingStore.h>

namespace WebKit {

class WCTileGrid {
public:
    using TileIndex = WebCore::TextureMapperSparseBackingStore::TileIndex;

    class Tile {
        WTF_MAKE_FAST_ALLOCATED;
        WTF_MAKE_NONCOPYABLE(Tile);
    public:
        Tile(WebCore::IntRect);
        bool willRemove() const { return m_willRemove; }
        void setWillRemove(bool v) { m_willRemove = v; }
        void addDirtyRect(const WebCore::IntRect&);
        void clearDirtyRect();
        bool hasDirtyRect() const;
        WebCore::IntRect& dirtyRect() { return m_dirtyRect; }

    private:
        bool m_willRemove { false };
        WebCore::IntRect m_tileRect;
        WebCore::IntRect m_dirtyRect;
    };

    void setSize(const WebCore::IntSize&);
    void addDirtyRect(const WebCore::IntRect&);
    void clearDirtyRects();
    bool setCoverageRect(const WebCore::IntRect&);
    auto& tiles() { return m_tiles; }
    WebCore::IntSize tilePixelSize() const;

private:
    bool ensureTile(TileIndex);
    WebCore::IntRect tileRectFromPixelRect(const WebCore::IntRect&);
    WebCore::IntSize tileSizeFromPixelSize(const WebCore::IntSize&);
    
    WebCore::IntSize m_size;
    HashMap<TileIndex, std::unique_ptr<Tile>> m_tiles;
};

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
