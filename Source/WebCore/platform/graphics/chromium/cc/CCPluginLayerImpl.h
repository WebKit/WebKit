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

#ifndef CCPluginLayerImpl_h
#define CCPluginLayerImpl_h

#include "FloatRect.h"
#include "ProgramBinding.h"
#include "ShaderChromium.h"
#include "cc/CCLayerImpl.h"

namespace WebCore {

class CCPluginLayerImpl : public CCLayerImpl {
public:
    static PassRefPtr<CCPluginLayerImpl> create(int id)
    {
        return adoptRef(new CCPluginLayerImpl(id));
    }
    virtual ~CCPluginLayerImpl();

    virtual void appendQuads(CCQuadList&, const CCSharedQuadState*);

    typedef ProgramBinding<VertexShaderPosTexStretch, FragmentShaderRGBATexAlpha> Program;
    typedef ProgramBinding<VertexShaderPosTexStretch, FragmentShaderRGBATexFlipAlpha> ProgramFlip;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexRectAlpha> TexRectProgram;
    typedef ProgramBinding<VertexShaderPosTexTransform, FragmentShaderRGBATexRectFlipAlpha> TexRectProgramFlip;

    virtual void draw(LayerRendererChromium*);

    virtual void dumpLayerProperties(TextStream&, int indent) const;

    void setTextureId(unsigned id) { m_textureId = id; }
    void setFlipped(bool flipped) { m_flipped = flipped; }
    void setUVRect(const FloatRect& rect) { m_uvRect = rect; }
    void setIOSurfaceProperties(int width, int height, uint32_t ioSurfaceId);

private:
    explicit CCPluginLayerImpl(int);

    virtual const char* layerTypeAsString() const { return "PluginLayer"; }

    void cleanupResources();

    unsigned m_textureId;
    bool m_flipped;
    FloatRect m_uvRect;
    uint32_t m_ioSurfaceId;
    int m_ioSurfaceWidth;
    int m_ioSurfaceHeight;

    // Internals for the IOSurface rendering path.
    bool m_ioSurfaceChanged;
    unsigned m_ioSurfaceTextureId;
};

}

#endif // CCPluginLayerImpl_h
