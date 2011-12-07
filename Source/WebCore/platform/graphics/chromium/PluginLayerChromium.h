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
    static PassRefPtr<PluginLayerChromium> create(CCLayerDelegate* = 0);
    virtual bool drawsContent() const { return true; }
    virtual void updateCompositorResources(GraphicsContext3D*, CCTextureUpdater&);

    virtual PassRefPtr<CCLayerImpl> createCCLayerImpl();

    // Code path for plugins which supply their own texture ID.
    void setTextureId(unsigned textureId);
    unsigned textureId() const { return m_textureId; }
    void setFlipped(bool);
    bool flipped() const { return m_flipped; }
    void setUVRect(const FloatRect&);
    const FloatRect& uvRect() const { return m_uvRect; }

    // Code path for plugins which render via an IOSurface.
    void setIOSurfaceProperties(int width, int height, uint32_t ioSurfaceId);
    uint32_t getIOSurfaceId() const;

    virtual void pushPropertiesTo(CCLayerImpl*);

    void invalidateRect(const FloatRect& dirtyRect);

protected:
    explicit PluginLayerChromium(CCLayerDelegate*);

private:
    unsigned m_textureId;
    bool m_flipped;
    FloatRect m_uvRect;
    int m_ioSurfaceWidth;
    int m_ioSurfaceHeight;
    uint32_t m_ioSurfaceId;
    FloatRect m_dirtyRect;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
