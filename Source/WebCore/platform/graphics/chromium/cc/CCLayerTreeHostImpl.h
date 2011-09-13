/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCLayerTreeHostImpl_h
#define CCLayerTreeHostImpl_h

#include "cc/CCLayerTreeHost.h"
#include <wtf/RefPtr.h>

#if USE(SKIA)
class GrContext;
#endif

namespace WebCore {

class CCCompletionEvent;
class CCLayerImpl;
class LayerRendererChromium;
struct LayerRendererCapabilities;

// CCLayerTreeHostImpl owns the CCLayerImpl tree as well as associated rendering state
class CCLayerTreeHostImpl {
    WTF_MAKE_NONCOPYABLE(CCLayerTreeHostImpl);
public:
    static PassOwnPtr<CCLayerTreeHostImpl> create(const CCSettings&);
    virtual ~CCLayerTreeHostImpl();

    virtual void beginCommit();
    virtual void commitComplete();

    GraphicsContext3D* context();

    void drawLayers();

    void finishAllRendering();
    int frameNumber() const { return m_frameNumber; }

    bool initializeLayerRenderer(CCLayerTreeHost* ownerHack, PassRefPtr<GraphicsContext3D>);
    bool isContextLost();
    LayerRendererChromium* layerRenderer() { return m_layerRenderer.get(); }
    const LayerRendererCapabilities& layerRendererCapabilities() const;

    void present();

    void readback(void* pixels, const IntRect&);

    CCLayerImpl* rootLayer() const { return m_rootLayerImpl.get(); }
    void setRootLayer(PassRefPtr<CCLayerImpl>);

    void setVisible(bool);

    int sourceFrameNumber() const { return m_sourceFrameNumber; }
    void setSourceFrameNumber(int frameNumber) { m_sourceFrameNumber = frameNumber; }

    void setViewport(const IntSize& viewportSize);
    const IntSize& viewportSize() const { return m_viewportSize; }
    void setZoomAnimatorScale(double);

protected:
    explicit CCLayerTreeHostImpl(const CCSettings&);
    int m_sourceFrameNumber;
    int m_frameNumber;

private:
    RefPtr<LayerRendererChromium> m_layerRenderer;
    RefPtr<CCLayerImpl> m_rootLayerImpl;
    CCSettings m_settings;
    IntSize m_viewportSize;
};

};

#endif
