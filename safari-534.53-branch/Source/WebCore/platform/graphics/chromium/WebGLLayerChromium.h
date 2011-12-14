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


#ifndef WebGLLayerChromium_h
#define WebGLLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "CanvasLayerChromium.h"
#include "Timer.h"

namespace WebCore {

class GraphicsContext3D;
class WebGLLayerChromiumRateLimitTask;

// A Layer containing a WebGL canvas
class WebGLLayerChromium : public CanvasLayerChromium {
public:
    static PassRefPtr<WebGLLayerChromium> create(GraphicsLayerChromium* owner = 0);

    virtual ~WebGLLayerChromium();

    virtual bool drawsContent() const { return m_context; }
    virtual void updateCompositorResources();
    void setTextureUpdated();

    void setContext(const GraphicsContext3D* context);
    GraphicsContext3D* context() { return m_context; }

    virtual void setLayerRenderer(LayerRendererChromium*);

protected:
    virtual const char* layerTypeAsString() const { return "WebGLLayer"; }

private:
    explicit WebGLLayerChromium(GraphicsLayerChromium* owner);
    friend class WebGLLayerChromiumRateLimitTask;

    void rateLimitContext(Timer<WebGLLayerChromium>*);

    // GraphicsContext3D::platformLayer has a side-effect of assigning itself
    // to the layer. Because of that GraphicsContext3D's destructor will reset
    // layer's context to 0.
    GraphicsContext3D* m_context;
    bool m_contextSupportsRateLimitingExtension;
    Timer<WebGLLayerChromium> m_rateLimitingTimer;
    bool m_textureUpdated;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
