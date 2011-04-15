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
#include "ProgramBinding.h"
#include "ShaderChromium.h"
#include "TextureManager.h"
#include "TransformationMatrix.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class CCLayerImpl;
class LayerRendererChromium;
class LayerTexture;

class RenderSurfaceChromium {
    WTF_MAKE_NONCOPYABLE(RenderSurfaceChromium);
    friend class LayerRendererChromium;
public:
    explicit RenderSurfaceChromium(CCLayerImpl*);
    ~RenderSurfaceChromium();

    bool prepareContentsTexture();
    void cleanupResources();
    void draw(const IntRect& targetSurfaceRect);

    String name() const;
    void dumpSurface(TextStream&, int indent) const;

    FloatPoint contentRectCenter() const { return FloatRect(m_contentRect).center(); }
    IntRect contentRect() const { return m_contentRect; }

    // Returns the rect that encloses the RenderSurface including any reflection.
    FloatRect drawableContentRect() const;

    TransformationMatrix drawTransform() const { return m_drawTransform; }

    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexAlpha> Program;
    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexAlphaMask> MaskProgram;

private:
    LayerRendererChromium* layerRenderer();
    void drawSurface(CCLayerImpl* maskLayer, const TransformationMatrix& drawTransform);

    CCLayerImpl* m_owningLayer;
    CCLayerImpl* m_maskLayer;

    IntRect m_contentRect;
    bool m_skipsDraw;

    OwnPtr<LayerTexture> m_contentsTexture;
    float m_drawOpacity;
    TransformationMatrix m_drawTransform;
    TransformationMatrix m_replicaDrawTransform;
    TransformationMatrix m_originTransform;
    IntRect m_scissorRect;
    Vector<RefPtr<CCLayerImpl> > m_layerList;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
