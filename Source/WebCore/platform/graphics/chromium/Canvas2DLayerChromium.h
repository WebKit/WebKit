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


#ifndef Canvas2DLayerChromium_h
#define Canvas2DLayerChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "CanvasLayerChromium.h"
#include "ImageBuffer.h"
#include "ManagedTexture.h"

class SkCanvas;

namespace WebCore {

class GraphicsContext3D;
class Region;

// A layer containing an accelerated 2d canvas
class Canvas2DLayerChromium : public CanvasLayerChromium {
public:
    enum WillDrawCondition {
        WillDrawUnconditionally,
        WillDrawIfLayerNotDeferred,
    };

    static PassRefPtr<Canvas2DLayerChromium> create(PassRefPtr<GraphicsContext3D>, const IntSize&, DeferralMode);
    virtual ~Canvas2DLayerChromium();

    void setTextureId(unsigned);

    virtual void setNeedsDisplayRect(const FloatRect&) OVERRIDE;

    virtual bool drawsContent() const OVERRIDE;
    virtual void update(CCTextureUpdater&, const CCOcclusionTracker*) OVERRIDE;
    virtual void pushPropertiesTo(CCLayerImpl*) OVERRIDE;

    void setCanvas(SkCanvas*);
    void layerWillDraw(WillDrawCondition) const;

private:
    Canvas2DLayerChromium(PassRefPtr<GraphicsContext3D>, const IntSize&, DeferralMode);
    bool drawingIntoImplThreadTexture() const;

    RefPtr<GraphicsContext3D> m_context;
    bool m_contextLost;
    IntSize m_size;
    unsigned m_backTextureId;
    // When m_useDoubleBuffering is true, the compositor will draw using a copy of the
    // canvas' backing texture. This option should be used with the compositor doesn't
    // synchronize its draws with the canvas updates.
    bool m_useDoubleBuffering;
    OwnPtr<ManagedTexture> m_frontTexture;
    SkCanvas* m_canvas;
    bool m_useRateLimiter;
    DeferralMode m_deferralMode;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
