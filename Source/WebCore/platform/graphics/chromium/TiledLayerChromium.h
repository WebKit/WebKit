/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef TiledLayerChromium_h
#define TiledLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"
#include "cc/CCLayerTilingData.h"
#include "cc/CCTiledLayerImpl.h"

namespace WebCore {

class LayerTextureUpdater;
class Region;
class UpdatableTile;

class TiledLayerChromium : public LayerChromium {
public:
    enum TilingOption { AlwaysTile, NeverTile, AutoTile };

    virtual ~TiledLayerChromium();

    virtual void updateCompositorResources(GraphicsContext3D*, CCTextureUpdater&);
    virtual void setIsMask(bool);

    virtual void pushPropertiesTo(CCLayerImpl*);

    virtual bool drawsContent() const;
    virtual bool needsContentsScale() const;

    virtual IntSize contentBounds() const;

    virtual void setNeedsDisplayRect(const FloatRect&);

    virtual void setIsNonCompositedContent(bool);

    // Reserves all existing and valid tile textures to protect them from being
    // recycled by the texture manager.
    void protectTileTextures(const IntRect& layerRect);

    virtual void reserveTextures();

    virtual void addSelfToOccludedScreenSpace(Region& occludedScreenSpace);

protected:
    TiledLayerChromium();

    virtual void cleanupResources();
    void updateTileSizeAndTilingOption();
    void updateBounds();

    // Exposed to subclasses for testing.
    void setTileSize(const IntSize&);
    void setTextureFormat(GC3Denum textureFormat) { m_textureFormat = textureFormat; }
    void setBorderTexelOption(CCLayerTilingData::BorderTexelOption);
    void setSampledTexelFormat(LayerTextureUpdater::SampledTexelFormat sampledTexelFormat) { m_sampledTexelFormat = sampledTexelFormat; }

    virtual LayerTextureUpdater* textureUpdater() const = 0;
    virtual void createTextureUpdaterIfNeeded() = 0;

    // Set invalidations to be potentially repainted during update().
    void invalidateRect(const IntRect& layerRect);

    // Prepare data needed to update textures that intersect with layerRect.
    void prepareToUpdate(const IntRect& layerRect);

    // Same as above, but this will try to paint additional surrounding content if idle.
    void prepareToUpdateIdle(const IntRect& layerRect);

    // After preparing an update, returns true if more pre-painting is needed.
    bool needsIdlePaint(const IntRect& layerRect);

    bool skipsDraw() const { return m_skipsDraw; }

    virtual void protectVisibleTileTextures();

    virtual TextureManager* textureManager() const;

private:
    virtual PassRefPtr<CCLayerImpl> createCCLayerImpl();

    void createTilerIfNeeded();
    void setTilingOption(TilingOption);

    void prepareToUpdateTiles(bool idle, int left, int top, int right, int bottom);
    IntRect idlePaintRect(const IntRect& visibleLayerRect);

    UpdatableTile* tileAt(int, int) const;
    UpdatableTile* createTile(int, int);

    // Temporary state held between prepareToUpdate() and updateCompositorResources().
    IntRect m_requestedUpdateTilesRect;

    // State held between prepareToUpdate() and pushPropertiesTo(). This represents the area
    // of the layer that is actually re-painted by WebKit.
    IntRect m_paintRect;

    GC3Denum m_textureFormat;
    bool m_skipsDraw;
    bool m_skipsIdlePaint;
    LayerTextureUpdater::SampledTexelFormat m_sampledTexelFormat;

    TilingOption m_tilingOption;
    OwnPtr<CCLayerTilingData> m_tiler;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
