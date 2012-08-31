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

#ifndef WebLayerTreeViewImpl_h
#define WebLayerTreeViewImpl_h

#include "CCLayerTreeHostClient.h"
#include <public/WebLayerTreeView.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {
class CCLayerTreeHost;
}

namespace WebKit {
class WebLayer;
class WebLayerTreeViewClient;
class WebLayerTreeViewClientAdapter;

class WebLayerTreeViewImpl : public WebLayerTreeView, public WebCore::CCLayerTreeHostClient {
public:
    explicit WebLayerTreeViewImpl(WebLayerTreeViewClient*);
    virtual ~WebLayerTreeViewImpl();

    bool initialize(const Settings&);

    // WebLayerTreeView implementation.
    virtual void setSurfaceReady() OVERRIDE;
    virtual void setRootLayer(const WebLayer&) OVERRIDE;
    virtual void clearRootLayer() OVERRIDE;
    virtual int compositorIdentifier() OVERRIDE;
    virtual void setViewportSize(const WebSize& layoutViewportSize, const WebSize& deviceViewportSize = WebSize()) OVERRIDE;
    virtual WebSize layoutViewportSize() const OVERRIDE;
    virtual WebSize deviceViewportSize() const OVERRIDE;
    virtual void setDeviceScaleFactor(float) OVERRIDE;
    virtual float deviceScaleFactor() const OVERRIDE;
    virtual void setBackgroundColor(WebColor) OVERRIDE;
    virtual void setHasTransparentBackground(bool) OVERRIDE;
    virtual void setVisible(bool) OVERRIDE;
    virtual void setPageScaleFactorAndLimits(float pageScaleFactor, float minimum, float maximum) OVERRIDE;
    virtual void startPageScaleAnimation(const WebPoint& destination, bool useAnchor, float newPageScale, double durationSec) OVERRIDE;
    virtual void setNeedsAnimate() OVERRIDE;
    virtual void setNeedsRedraw() OVERRIDE;
    virtual bool commitRequested() const OVERRIDE;
    virtual void composite() OVERRIDE;
    virtual void updateAnimations(double frameBeginTime) OVERRIDE;
    virtual bool compositeAndReadback(void *pixels, const WebRect&) OVERRIDE;
    virtual void finishAllRendering() OVERRIDE;
    virtual void renderingStats(WebRenderingStats&) const OVERRIDE;
    virtual void setFontAtlas(SkBitmap, WebRect asciiToRectTable[128], int fontHeight) OVERRIDE;
    virtual void loseCompositorContext(int numTimes) OVERRIDE;

    // WebCore::CCLayerTreeHostClient implementation.
    virtual void willBeginFrame() OVERRIDE;
    virtual void didBeginFrame() OVERRIDE;
    virtual void animate(double monotonicFrameBeginTime) OVERRIDE;
    virtual void layout() OVERRIDE;
    virtual void applyScrollAndScale(const WebCore::IntSize& scrollDelta, float pageScale) OVERRIDE;
    virtual PassOwnPtr<WebCompositorOutputSurface> createOutputSurface() OVERRIDE;
    virtual void didRecreateOutputSurface(bool success) OVERRIDE;
    virtual void willCommit() OVERRIDE;
    virtual void didCommit() OVERRIDE;
    virtual void didCommitAndDrawFrame() OVERRIDE;
    virtual void didCompleteSwapBuffers() OVERRIDE;
    virtual void scheduleComposite() OVERRIDE;

private:
    WebLayerTreeViewClient* m_client;
    OwnPtr<WebCore::CCLayerTreeHost> m_layerTreeHost;
};

} // namespace WebKit

#endif // WebLayerTreeViewImpl_h
