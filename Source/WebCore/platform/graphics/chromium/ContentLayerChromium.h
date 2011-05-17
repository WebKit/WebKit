/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef ContentLayerChromium_h
#define ContentLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"
#include "LayerTilerChromium.h"
#include "TextureManager.h"

namespace WebCore {

class LayerTexture;

// A Layer that requires a GraphicsContext to render its contents.
class ContentLayerChromium : public LayerChromium {
    friend class LayerRendererChromium;
public:
    enum TilingOption { AlwaysTile, NeverTile, AutoTile };

    static PassRefPtr<ContentLayerChromium> create(GraphicsLayerChromium* owner = 0);

    virtual ~ContentLayerChromium();

    virtual void paintContentsIfDirty(const IntRect& targetSurfaceRect);
    virtual void updateCompositorResources();
    virtual void setIsMask(bool);
    virtual void bindContentsTexture();

    virtual void draw(const IntRect& targetSurfaceRect);
    virtual bool drawsContent() const { return m_owner && m_owner->drawsContent() && (!m_tiler || !m_tiler->skipsDraw()); }

protected:
    explicit ContentLayerChromium(GraphicsLayerChromium* owner);

    virtual const char* layerTypeAsString() const { return "ContentLayer"; }
    virtual void dumpLayerProperties(TextStream&, int indent) const;

    virtual void setLayerRenderer(LayerRendererChromium*);

    virtual IntRect layerBounds() const;

    virtual TransformationMatrix tilingTransform();

    // For a given render surface rect that this layer will be transformed and
    // drawn into, return the layer space rect that is visible in that surface.
    IntRect visibleLayerRect(const IntRect&);

    void updateLayerSize(const IntSize&);
    void createTilerIfNeeded();
    void setTilingOption(TilingOption);

    OwnPtr<LayerTilerChromium> m_tiler;
    TilingOption m_tilingOption;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
