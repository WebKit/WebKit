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

#ifndef CCCanvasLayerImpl_h
#define CCCanvasLayerImpl_h

#include "ProgramBinding.h"
#include "ShaderChromium.h"
#include "cc/CCLayerImpl.h"

namespace WebCore {

class CCCanvasLayerImpl : public CCLayerImpl {
public:
    static PassRefPtr<CCCanvasLayerImpl> create(int id)
    {
        return adoptRef(new CCCanvasLayerImpl(id));
    }
    virtual ~CCCanvasLayerImpl();

    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexFlipAlpha> Program;

    virtual void draw(LayerRendererChromium*);

    virtual void dumpLayerProperties(TextStream&, int indent) const;

    unsigned textureId() const { return m_textureId; }
    void setTextureId(unsigned id) { m_textureId = id; }
    void setHasAlpha(bool hasAlpha) { m_hasAlpha = hasAlpha; }
    void setPremultipliedAlpha(bool premultipliedAlpha) { m_premultipliedAlpha = premultipliedAlpha; }
private:
    explicit CCCanvasLayerImpl(int);

    virtual const char* layerTypeAsString() const { return "CanvasLayer"; }

    unsigned m_textureId;
    bool m_hasAlpha;
    bool m_premultipliedAlpha;
};

}

#endif // CCCanvasLayerImpl_h
