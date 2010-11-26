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


#ifndef PluginLayerChromium_h
#define PluginLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerChromium.h"

namespace WebCore {

// A Layer containing a the rendered output of a plugin instance.
class PluginLayerChromium : public LayerChromium {
public:
    static PassRefPtr<PluginLayerChromium> create(GraphicsLayerChromium* owner = 0);
    virtual bool drawsContent() { return true; }
    virtual void updateContents();
    virtual void draw();
    
    void setTextureId(unsigned textureId);
        
    class SharedValues {
    public:
        SharedValues(GraphicsContext3D* context);
        ~SharedValues();

        unsigned shaderProgram() const { return m_shaderProgram; }
        int shaderSamplerLocation() const { return m_shaderSamplerLocation; }
        int shaderMatrixLocation() const { return m_shaderMatrixLocation; }
        int shaderAlphaLocation() const { return m_shaderAlphaLocation; }
        bool initialized() const { return m_initialized; }

    private:
        GraphicsContext3D* m_context;
        unsigned m_shaderProgram;
        int m_shaderSamplerLocation;
        int m_shaderMatrixLocation;
        int m_shaderAlphaLocation;
        bool m_initialized;
    };

private:
    PluginLayerChromium(GraphicsLayerChromium* owner);
    unsigned m_textureId;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
