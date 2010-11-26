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

namespace WebCore {

// A Layer that requires a GraphicsContext to render its contents.
class ContentLayerChromium : public LayerChromium {
    friend class LayerRendererChromium;
public:
    static PassRefPtr<ContentLayerChromium> create(GraphicsLayerChromium* owner = 0);

    virtual ~ContentLayerChromium();

    virtual void updateContents();
    virtual void draw();
    virtual bool drawsContent() { return m_owner && m_owner->drawsContent(); }

    // Stores values that are shared between instances of this class that are
    // associated with the same LayerRendererChromium (and hence the same GL
    // context).
    class SharedValues {
    public:
        explicit SharedValues(GraphicsContext3D*);
        ~SharedValues();

        unsigned contentShaderProgram() const { return m_contentShaderProgram; }
        int shaderSamplerLocation() const { return m_shaderSamplerLocation; }
        int shaderMatrixLocation() const { return m_shaderMatrixLocation; }
        int shaderAlphaLocation() const { return m_shaderAlphaLocation; }
        int initialized() const { return m_initialized; }

    private:
        GraphicsContext3D* m_context;
        unsigned m_contentShaderProgram;
        int m_shaderSamplerLocation;
        int m_shaderMatrixLocation;
        int m_shaderAlphaLocation;
        int m_initialized;
    };

protected:
    ContentLayerChromium(GraphicsLayerChromium* owner);

    void updateTextureRect(void* pixels, const IntSize& bitmapSize, const IntSize& requiredTextureSize,
                           const IntRect& updateRect, unsigned textureId);

    virtual void cleanupResources();
    bool requiresClippedUpdateRect() const;

    unsigned m_contentsTexture;
    IntSize m_allocatedTextureSize;
    bool m_skipsDraw;

private:
    void calculateClippedUpdateRect(IntRect& dirtyRect, IntRect& drawRect) const;
    IntRect m_largeLayerDrawRect;
    IntRect m_largeLayerDirtyRect;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
