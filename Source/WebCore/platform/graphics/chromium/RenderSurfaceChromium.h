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


#ifndef RenderSurfaceChromium_h
#define RenderSurfaceChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatRect.h"
#include "IntRect.h"
#include "TextureManager.h"
#include "TransformationMatrix.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class LayerChromium;
class LayerRendererChromium;
class LayerTexture;

class RenderSurfaceChromium {
    WTF_MAKE_NONCOPYABLE(RenderSurfaceChromium);
    friend class LayerRendererChromium;
public:
    explicit RenderSurfaceChromium(LayerChromium*);
    ~RenderSurfaceChromium();

    bool prepareContentsTexture();
    void cleanupResources();
    void draw();

    FloatPoint contentRectCenter() const { return FloatRect(m_contentRect).center(); }
    IntRect contentRect() const { return m_contentRect; }

    // Returns the rect that encloses the RenderSurface including any reflection.
    FloatRect drawableContentRect() const;

    TransformationMatrix drawTransform() const { return m_drawTransform; }

    // Stores values that are shared between instances of this class that are
    // associated with the same LayerRendererChromium (and hence the same GL
    // context).
    class SharedValues {
    public:
        explicit SharedValues(GraphicsContext3D*);
        ~SharedValues();

        unsigned shaderProgram() const { return m_shaderProgram; }
        unsigned maskShaderProgram() const { return m_maskShaderProgram; }
        int shaderSamplerLocation() const { return m_shaderSamplerLocation; }
        int shaderMatrixLocation() const { return m_shaderMatrixLocation; }
        int shaderAlphaLocation() const { return m_shaderAlphaLocation; }
        int maskShaderSamplerLocation() const { return m_maskShaderSamplerLocation; }
        int maskShaderMaskSamplerLocation() const { return m_maskShaderMaskSamplerLocation; }
        int maskShaderMatrixLocation() const { return m_maskShaderMatrixLocation; }
        int maskShaderAlphaLocation() const { return m_maskShaderAlphaLocation; }
        bool initialized() const { return m_initialized; }

    private:
        GraphicsContext3D* m_context;

        unsigned m_shaderProgram;
        unsigned m_maskShaderProgram;
        int m_shaderSamplerLocation;
        int m_shaderMatrixLocation;
        int m_shaderAlphaLocation;
        int m_maskShaderSamplerLocation;
        int m_maskShaderMaskSamplerLocation;
        int m_maskShaderMatrixLocation;
        int m_maskShaderAlphaLocation;
        bool m_initialized;
    };

private:
    LayerRendererChromium* layerRenderer();
    void drawSurface(LayerChromium* maskLayer, const TransformationMatrix& drawTransform);

    LayerChromium* m_owningLayer;
    LayerChromium* m_maskLayer;

    IntRect m_contentRect;
    bool m_skipsDraw;
    OwnPtr<LayerTexture> m_contentsTexture;
    float m_drawOpacity;
    TransformationMatrix m_drawTransform;
    TransformationMatrix m_replicaDrawTransform;
    TransformationMatrix m_originTransform;
    IntRect m_scissorRect;
    Vector<LayerChromium*> m_layerList;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
