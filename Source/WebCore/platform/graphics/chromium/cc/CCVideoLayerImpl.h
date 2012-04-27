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

#ifndef CCVideoLayerImpl_h
#define CCVideoLayerImpl_h

#include "ManagedTexture.h"
#include "cc/CCLayerImpl.h"
#include <public/WebVideoFrameProvider.h>

namespace WebKit {
class WebVideoFrame;
}

namespace WebCore {

class CCLayerTreeHostImpl;
class CCVideoLayerImpl;

class CCVideoLayerImpl : public CCLayerImpl
                       , public WebKit::WebVideoFrameProvider::Client {
public:
    static PassOwnPtr<CCVideoLayerImpl> create(int id, WebKit::WebVideoFrameProvider* provider)
    {
        return adoptPtr(new CCVideoLayerImpl(id, provider));
    }
    virtual ~CCVideoLayerImpl();

    virtual void willDraw(LayerRendererChromium*) OVERRIDE;
    virtual void appendQuads(CCQuadCuller&, const CCSharedQuadState*, bool& hadMissingTiles) OVERRIDE;
    virtual void didDraw() OVERRIDE;

    virtual void dumpLayerProperties(TextStream&, int indent) const OVERRIDE;

    Mutex& providerMutex() { return m_providerMutex; }

    // WebKit::WebVideoFrameProvider::Client implementation.
    virtual void stopUsingProvider(); // Callable on any thread.
    virtual void didReceiveFrame(); // Callable on impl thread.
    virtual void didUpdateMatrix(const float*); // Callable on impl thread.

    virtual void didLoseContext() OVERRIDE;

    void setNeedsRedraw();

    static const float yuv2RGB[9];
    static const float yuvAdjust[3];
    static const float flipTransform[16];

    struct Texture {
        OwnPtr<ManagedTexture> m_texture;
        IntSize m_visibleSize;
    };
    enum { MaxPlanes = 3 };

private:
    CCVideoLayerImpl(int, WebKit::WebVideoFrameProvider*);

    static IntSize computeVisibleSize(const WebKit::WebVideoFrame&, unsigned plane);
    virtual const char* layerTypeAsString() const OVERRIDE { return "VideoLayer"; }

    void willDrawInternal(LayerRendererChromium*);
    bool reserveTextures(const WebKit::WebVideoFrame&, GC3Denum format, LayerRendererChromium*);

    // Guards the destruction of m_provider and the frame that it provides
    Mutex m_providerMutex;
    WebKit::WebVideoFrameProvider* m_provider;

    Texture m_textures[MaxPlanes];

    float m_streamTextureMatrix[16];
    CCLayerTreeHostImpl* m_layerTreeHostImpl;

    WebKit::WebVideoFrame* m_frame;
    GC3Denum m_format;
};

}

#endif // CCVideoLayerImpl_h
