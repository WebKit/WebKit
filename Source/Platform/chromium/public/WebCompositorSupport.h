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

#ifndef WebCompositorSupport_h
#define WebCompositorSupport_h

#include "WebAnimation.h"
#include "WebCommon.h"
#include "WebLayerTreeView.h"
#include "WebScrollbarThemePainter.h"

namespace WebKit {

class WebAnimationCurve;
class WebCompositorOutputSurface;
class WebContentLayer;
class WebContentLayerClient;
class WebExternalTextureLayer;
class WebExternalTextureLayerClient;
class WebFloatAnimationCurve;
class WebGraphicsContext3D;
class WebIOSurfaceLayer;
class WebImageLayer;
class WebLayer;
class WebScrollbar;
class WebScrollbarLayer;
class WebScrollbarThemeGeometry;
class WebSolidColorLayer;
class WebThread;
class WebTransformAnimationCurve;
class WebVideoFrameProvider;
class WebVideoLayer;

class WebCompositorSupport {
public:
    // Initializes the compositor. Threaded compositing is enabled by passing in
    // a non-null WebThread. No compositor classes or methods should be used
    // prior to calling initialize.
    virtual void initialize(WebThread*) { }

    // Returns whether the compositor was initialized with threading enabled.
    virtual bool isThreadingEnabled() { return false; }

    // Shuts down the compositor. This must be called when all compositor data
    // types have been deleted. No compositor classes or methods should be used
    // after shutdown.
    virtual void shutdown() { }

    // May return 0 if initialization fails.
    virtual WebLayerTreeView* createLayerTreeView(WebLayerTreeViewClient*, const WebLayer& root, const WebLayerTreeView::Settings&) { return 0; }

    // Creates an output surface for the compositor backed by a 3d context.
    virtual WebCompositorOutputSurface* createOutputSurfaceFor3D(WebKit::WebGraphicsContext3D*) { return 0; }

    // Creates an output surface for the compositor backed by a software device.
    virtual WebCompositorOutputSurface* createOutputSurfaceForSoftware() { return 0; }

    // Layers -------------------------------------------------------

    virtual WebLayer* createLayer() { return 0; }

    virtual WebContentLayer* createContentLayer(WebContentLayerClient*) { return 0; }

    virtual WebExternalTextureLayer* createExternalTextureLayer(WebExternalTextureLayerClient* = 0) { return 0; }

    virtual WebIOSurfaceLayer* createIOSurfaceLayer() { return 0; }

    virtual WebImageLayer* createImageLayer() { return 0; }

    virtual WebSolidColorLayer* createSolidColorLayer() { return 0; }

    virtual WebVideoLayer* createVideoLayer(WebVideoFrameProvider*) { return 0; }

    virtual WebScrollbarLayer* createScrollbarLayer(WebScrollbar*, WebScrollbarThemePainter, WebScrollbarThemeGeometry*) { return 0; }


    // Animation ----------------------------------------------------

    virtual WebAnimation* createAnimation(const WebAnimationCurve&, WebAnimation::TargetProperty, int animationId = 0) { return 0; }

    virtual WebFloatAnimationCurve* createFloatAnimationCurve() { return 0; }

    virtual WebTransformAnimationCurve* createTransformAnimationCurve() { return 0; }

protected:
    virtual ~WebCompositorSupport() { }
};

}

#endif // WebCompositorSupport_h
