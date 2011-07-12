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
#include "cc/CCTiledLayerImpl.h"

namespace WebCore {

class LayerTexture;
class LayerTilerChromium;
class LayerTextureUpdater;

class TiledLayerChromium : public LayerChromium {
public:
    enum TilingOption { AlwaysTile, NeverTile, AutoTile };

    virtual ~TiledLayerChromium();

    virtual void updateCompositorResources();
    virtual void setIsMask(bool);

    virtual void pushPropertiesTo(CCLayerImpl*);

    virtual bool drawsContent() const;

protected:
    explicit TiledLayerChromium(GraphicsLayerChromium*);

    virtual void cleanupResources();
    void updateTileSizeAndTilingOption();

    virtual void createTextureUpdaterIfNeeded() = 0;
    virtual LayerTextureUpdater* textureUpdater() const = 0;

    OwnPtr<LayerTilerChromium> m_tiler;

private:
    virtual PassRefPtr<CCLayerImpl> createCCLayerImpl();

    virtual void dumpLayerProperties(TextStream&, int indent) const;

    virtual void setLayerRenderer(LayerRendererChromium*);

    void createTilerIfNeeded();
    void setTilingOption(TilingOption);
    TransformationMatrix tilingTransform() const;

    TilingOption m_tilingOption;
    bool m_borderTexels;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
