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
#include "DrawingBuffer.h"

namespace WebCore {

class GraphicsContext3D;
class WebGLLayerChromiumRateLimitTask;

// A Layer containing a WebGL canvas
class WebGLLayerChromium : public CanvasLayerChromium {
public:
    static PassRefPtr<WebGLLayerChromium> create();

    virtual ~WebGLLayerChromium();

    unsigned textureId() const { return m_textureId; }
    void setTextureId(unsigned textureId) { m_textureId = textureId; }

    virtual bool drawsContent() const;
    virtual void updateCompositorResources(GraphicsContext3D*, CCTextureUpdater&);
    virtual void pushPropertiesTo(CCLayerImpl*);
    virtual void contentChanged();
    bool paintRenderedResultsToCanvas(ImageBuffer*);

    GraphicsContext3D* context() const;

    void setDrawingBuffer(DrawingBuffer*);
    DrawingBuffer* drawingBuffer() const { return m_drawingBuffer; }
private:
    WebGLLayerChromium();
    friend class WebGLLayerChromiumRateLimitTask;

    GraphicsContext3D* layerRendererContext();

    bool m_hasAlpha;
    bool m_premultipliedAlpha;
    unsigned m_textureId;
    bool m_textureChanged;
    bool m_textureUpdated;

    // The DrawingBuffer holding the WebGL contents for this layer.
    // A reference is not held here, because the DrawingBuffer already holds
    // a reference to the WebGLLayerChromium.
    DrawingBuffer* m_drawingBuffer;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
