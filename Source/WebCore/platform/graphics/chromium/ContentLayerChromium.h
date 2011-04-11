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
#include "PlatformCanvas.h"
#include "TextureManager.h"

namespace WebCore {

class LayerTexture;

// A Layer that requires a GraphicsContext to render its contents.
class ContentLayerChromium : public LayerChromium {
    friend class LayerRendererChromium;
public:
    static PassRefPtr<ContentLayerChromium> create(GraphicsLayerChromium* owner = 0);

    virtual ~ContentLayerChromium();

    virtual void paintContentsIfDirty();
    virtual void updateCompositorResources();
    virtual void unreserveContentsTexture();
    virtual void bindContentsTexture();

    virtual void draw();
    virtual bool drawsContent() const { return m_owner && m_owner->drawsContent(); }

    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderTexAlpha> Program;

protected:
    explicit ContentLayerChromium(GraphicsLayerChromium* owner);

    virtual void cleanupResources();
    bool requiresClippedUpdateRect();
    void resizeUploadBuffer(const IntSize&);

    virtual const char* layerTypeAsString() const { return "ContentLayer"; }
    virtual void dumpLayerProperties(TextStream&, int indent) const;

    OwnPtr<LayerTexture> m_contentsTexture;
    bool m_skipsDraw;

    // The portion of the upload buffer that has a pending update, in the coordinates of the texture.
    IntRect m_uploadUpdateRect;

    virtual void updateTextureIfNeeded();
    void updateTexture(const uint8_t* pixels, const IntSize&);

private:
    PlatformCanvas m_canvas;

    IntRect m_visibleRectInLayerCoords;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
