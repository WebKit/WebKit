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
#include "TransformationMatrix.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class LayerRendererChromium;
class LayerChromium;

class RenderSurfaceChromium : public Noncopyable {
    friend class LayerRendererChromium;
public:
    explicit RenderSurfaceChromium(LayerChromium*);
    ~RenderSurfaceChromium();

    void prepareContentsTexture();
    void cleanupResources();

    FloatPoint contentRectCenter() const { return FloatRect(m_contentRect).center(); }
    IntRect contentRect() const { return m_contentRect; }

private:
    LayerRendererChromium* layerRenderer();

    LayerChromium* m_owningLayer;
    IntRect m_contentRect;
    unsigned m_contentsTextureId;
    float m_drawOpacity;
    IntSize m_allocatedTextureSize;
    TransformationMatrix m_drawTransform;
    TransformationMatrix m_originTransform;
    IntRect m_scissorRect;
    Vector<LayerChromium*> m_layerList;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
