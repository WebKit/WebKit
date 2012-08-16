/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef Canvas2DLayerBridge_h
#define Canvas2DLayerBridge_h

#include "GraphicsContext3D.h"
#include "ImageBuffer.h" // For DeferralMode enum.
#include "IntSize.h"
#include "SkDeferredCanvas.h"
#include <public/WebExternalTextureLayer.h>
#include <public/WebExternalTextureLayerClient.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {

class LayerChromium;

class Canvas2DLayerBridge : public WebKit::WebExternalTextureLayerClient, public SkDeferredCanvas::NotificationClient {
    WTF_MAKE_NONCOPYABLE(Canvas2DLayerBridge);
public:
    static PassOwnPtr<Canvas2DLayerBridge> create(PassRefPtr<GraphicsContext3D> context, const IntSize& size, DeferralMode deferralMode, unsigned textureId)
    {
        return adoptPtr(new Canvas2DLayerBridge(context, size, deferralMode, textureId));
    }

    virtual ~Canvas2DLayerBridge();

    // WebKit::WebExternalTextureLayerClient implementation.
    virtual unsigned prepareTexture(WebKit::WebTextureUpdater&) OVERRIDE;
    virtual WebKit::WebGraphicsContext3D* context() OVERRIDE;

    // SkDeferredCanvas::NotificationClient implementation
    virtual void prepareForDraw();

    SkCanvas* skCanvas(SkDevice*);
    WebKit::WebLayer* layer();
    void contextAcquired();

    unsigned backBufferTexture();

private:
    Canvas2DLayerBridge(PassRefPtr<GraphicsContext3D>, const IntSize&, DeferralMode, unsigned textureId);
    SkDeferredCanvas* deferredCanvas();

    DeferralMode m_deferralMode;
    bool m_useDoubleBuffering;
    unsigned m_frontBufferTexture;
    unsigned m_backBufferTexture;
    IntSize m_size;
    SkCanvas* m_canvas;
    WebKit::WebExternalTextureLayer m_layer;
    RefPtr<GraphicsContext3D> m_context;
};

}

#endif
