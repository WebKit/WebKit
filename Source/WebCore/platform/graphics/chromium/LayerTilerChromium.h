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
#include <wtf/OwnArrayPtr.h>

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

    void invalidateRect(const IntRect& contentRect);
    void invalidateEntireLayer();
    void update(TilePaintInterface& painter, const IntRect& contentRect);
    void updateFromPixels(const IntRect& paintRect, const uint8_t* pixels);
    void draw(const IntRect& contentRect);

    // Set position of this tiled layer in content space.
    void setLayerPosition(const IntPoint& position);
    // Change the tile size.  This may invalidate all the existing tiles.
    void setTileSize(const IntSize& size);

    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderTexAlpha> Program;

private:
    LayerTilerChromium(LayerRendererChromium*, const IntSize& tileSize, BorderTexelOption);

    class Tile {
        WTF_MAKE_NONCOPYABLE(Tile);
    public:
        explicit Tile(PassOwnPtr<LayerTexture> tex) : m_tex(tex) {}

        LayerTexture* texture() { return m_tex.get(); }

        bool dirty() const { return !m_dirtyLayerRect.isEmpty(); }
        void clearDirty() { m_dirtyLayerRect = IntRect(); }

        // Layer-space dirty rectangle that needs to be repainted.
        IntRect m_dirtyLayerRect;
    private:
        OwnPtr<LayerTexture> m_tex;
    };

    void drawTexturedQuad(GraphicsContext3D*, const TransformationMatrix& projectionMatrix, const TransformationMatrix& drawMatrix,
                          float width, float height, float opacity,
                          float texTranslateX, float texTranslateY,
                          float texScaleX, float texScaleY,
                          const LayerTilerChromium::Program*);

    void resizeLayer(const IntSize& size);
    // Grow layer size to contain this rectangle.
    void growLayerToContain(const IntRect& contentRect);

    LayerRendererChromium* layerRenderer() const { return m_layerRenderer; }
    GraphicsContext3D* layerRendererContext() const;
    Tile* createTile(int i, int j);
    // Invalidate any tiles which do not intersect with the newLayerRect.
    void invalidateTiles(const IntRect& oldLayerRect, const IntRect& newLayerRect);
    void reset();
    void contentRectToTileIndices(const IntRect& contentRect, int &left, int &top, int &right, int &bottom) const;
    IntRect contentRectToLayerRect(const IntRect& contentRect) const;
    IntRect layerRectToContentRect(const IntRect& layerRect) const;

    // Returns the index into m_tiles for a given tile location.
    int tileIndex(int i, int j) const;
    // Returns the bounds in content space for a given tile location.
    IntRect tileContentRect(int i, int j) const;
    // Returns the bounds in layer space for a given tile location.
    IntRect tileLayerRect(int i, int j) const;

    IntSize layerSize() const;
    IntSize layerTileSize() const;

    IntSize m_tileSize;
    IntRect m_lastUpdateLayerRect;
    IntPoint m_layerPosition;

    bool m_skipsDraw;

    // Logical 2D array of tiles (dimensions of m_layerTileSize)
    Vector<OwnPtr<Tile> > m_tiles;
    // Linear array of unused tiles.
    Vector<OwnPtr<Tile> > m_unusedTiles;

    PlatformCanvas m_canvas;

    // Cache a tile-sized pixel buffer to draw into.
    OwnArrayPtr<uint8_t> m_tilePixels;

    TilingData m_tilingData;

    LayerRendererChromium* m_layerRenderer;
};

}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
