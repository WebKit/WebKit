/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef LayerTilerChromium_h
#define LayerTilerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"
#include "LayerTexture.h"
#include "PlatformCanvas.h"
#include "TilingData.h"
#include <wtf/HashTraits.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class GraphicsContext3D;
class LayerRendererChromium;

class TilePaintInterface {
public:
    virtual void paint(GraphicsContext& context, const IntRect& contentRect) = 0;
};

class LayerTilerChromium {
    WTF_MAKE_NONCOPYABLE(LayerTilerChromium);
public:
    enum BorderTexelOption { HasBorderTexels, NoBorderTexels };

    static PassOwnPtr<LayerTilerChromium> create(LayerRendererChromium*, const IntSize& tileSize, BorderTexelOption);

    ~LayerTilerChromium();

    // Set invalidations to be potentially repainted during update().
    void invalidateRect(const IntRect& contentRect);
    void invalidateEntireLayer();

    // Paint all invalidations and missing tiles needed to draw the contentRect
    // into the internal canvas.
    void update(TilePaintInterface& painter, const IntRect& contentRect);

    // Reserve and upload tile textures from the internal canvas.
    void uploadCanvas();

    // Reserve and upload tile textures from an externally painted buffer.
    void updateFromPixels(const IntRect& contentRect, const IntRect& paintRect, const uint8_t* pixels);

    // Draw all tiles that intersect with the content rect.
    void draw(const IntRect& contentRect, const TransformationMatrix&, float opacity);

    // Set position of this tiled layer in content space.
    void setLayerPosition(const IntPoint& position);
    // Change the tile size.  This may invalidate all the existing tiles.
    void setTileSize(const IntSize& size);
    void setLayerRenderer(LayerRendererChromium*);

    bool skipsDraw() const { return m_skipsDraw; }

    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderTexAlpha> Program;

    // If this tiler has exactly one tile, return its texture. Otherwise, null.
    LayerTexture* getSingleTexture();

private:
    LayerTilerChromium(LayerRendererChromium*, const IntSize& tileSize, BorderTexelOption);

    class Tile : public RefCounted<Tile> {
        WTF_MAKE_NONCOPYABLE(Tile);
    public:
        explicit Tile(PassOwnPtr<LayerTexture> tex) : m_tex(tex), m_i(-1), m_j(-1) {}

        LayerTexture* texture() { return m_tex.get(); }

        bool dirty() const { return !m_dirtyLayerRect.isEmpty(); }
        void clearDirty() { m_dirtyLayerRect = IntRect(); }

        int i() const { return m_i; }
        int j() const { return m_j; }
        void moveTo(int i, int j) { m_i = i; m_j = j; }

        // Layer-space dirty rectangle that needs to be repainted.
        IntRect m_dirtyLayerRect;
    private:
        OwnPtr<LayerTexture> m_tex;
        int m_i;
        int m_j;
    };

    void drawTexturedQuad(GraphicsContext3D*, const TransformationMatrix& projectionMatrix, const TransformationMatrix& drawMatrix,
                          float width, float height, float opacity,
                          float texTranslateX, float texTranslateY,
                          float texScaleX, float texScaleY,
                          const LayerTilerChromium::Program*);

    // Grow layer size to contain this rectangle.
    void growLayerToContain(const IntRect& contentRect);

    LayerRendererChromium* layerRenderer() const { return m_layerRenderer; }
    GraphicsContext3D* layerRendererContext() const;
    Tile* createTile(int i, int j);
    // Invalidate any tiles which do not intersect with the contentRect
    void invalidateTiles(const IntRect& contentRect);
    void reset();
    void contentRectToTileIndices(const IntRect& contentRect, int &left, int &top, int &right, int &bottom) const;
    IntRect contentRectToLayerRect(const IntRect& contentRect) const;
    IntRect layerRectToContentRect(const IntRect& layerRect) const;

    Tile* tileAt(int, int) const;
    IntRect tileContentRect(const Tile*) const;
    IntRect tileLayerRect(const Tile*) const;

    IntSize m_tileSize;
    IntPoint m_layerPosition;

    bool m_skipsDraw;

    // Default hash key traits for integers disallow 0 and -1 as a key, so
    // use a custom hash trait which disallows -1 and -2 instead.
    typedef std::pair<int, int> TileMapKey;
    struct TileMapKeyTraits : HashTraits<TileMapKey> {
        static const bool emptyValueIsZero = false;
        static const bool needsDestruction = false;
        static TileMapKey emptyValue() { return std::make_pair(-1, -1); }
        static void constructDeletedValue(TileMapKey& slot) { slot = std::make_pair(-2, -2); }
        static bool isDeletedValue(TileMapKey value) { return value.first == -2 && value.second == -2; }
    };
    // FIXME: The mapped value in TileMap should really be an OwnPtr, as the
    // refcount of a Tile should never be more than 1. However, HashMap
    // doesn't easily support OwnPtr as a value.
    typedef HashMap<TileMapKey, RefPtr<Tile>, DefaultHash<TileMapKey>::Hash, TileMapKeyTraits> TileMap;
    TileMap m_tiles;
    // Tightly packed set of unused tiles.
    Vector<RefPtr<Tile> > m_unusedTiles;

    // State held between update and uploadCanvas.
    IntRect m_paintRect;
    IntRect m_updateRect;
    PlatformCanvas m_canvas;

    // Cache a tile-sized pixel buffer to draw into.
    OwnArrayPtr<uint8_t> m_tilePixels;

    TilingData m_tilingData;

    LayerRendererChromium* m_layerRenderer;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
