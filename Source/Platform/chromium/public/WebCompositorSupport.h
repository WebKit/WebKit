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
#include "WebContentLayer.h"
#include "WebExternalTextureLayer.h"
#include "WebFloatAnimationCurve.h"
#include "WebIOSurfaceLayer.h"
#include "WebImageLayer.h"
#include "WebLayer.h"
#include "WebLayerTreeView.h"
#include "WebPassOwnPtr.h"
#include "WebScrollbar.h"
#include "WebScrollbarLayer.h"
#include "WebScrollbarThemeGeometry.h"
#include "WebScrollbarThemePainter.h"
#include "WebSolidColorLayer.h"
#include "WebTransformAnimationCurve.h"
#include "WebVideoLayer.h"

namespace WebKit {

class WebContentLayerClient;
class WebExternalTextureLayerClient;
class WebVideoFrameProvider;

class WebCompositorSupport {
public:
    // May return 0 if initialization fails.
    virtual WebPassOwnPtr<WebLayerTreeView> createLayerTreeView(WebLayerTreeViewClient*, const WebLayer& root, const WebLayerTreeView::Settings&) { return WebPassOwnPtr<WebLayerTreeView>(); }


    // Layers -------------------------------------------------------

    virtual WebPassOwnPtr<WebLayer> createLayer() { return WebPassOwnPtr<WebLayer>(); }

    virtual WebPassOwnPtr<WebContentLayer> createContentLayer(WebContentLayerClient*) { return WebPassOwnPtr<WebContentLayer>(); }

    virtual WebPassOwnPtr<WebExternalTextureLayer> createExternalTextureLayer(WebExternalTextureLayerClient* = 0) { return WebPassOwnPtr<WebExternalTextureLayer>(); }

    virtual WebPassOwnPtr<WebIOSurfaceLayer> createIOSurfaceLayer() { return WebPassOwnPtr<WebIOSurfaceLayer>(); }

    virtual WebPassOwnPtr<WebImageLayer> createImageLayer() { return WebPassOwnPtr<WebImageLayer>(); }

    virtual WebPassOwnPtr<WebSolidColorLayer> createSolidColorLayer() { return WebPassOwnPtr<WebSolidColorLayer>(); }

    virtual WebPassOwnPtr<WebVideoLayer> createVideoLayer(WebVideoFrameProvider*) { return WebPassOwnPtr<WebVideoLayer>(); }

    virtual WebPassOwnPtr<WebScrollbarLayer> createScrollbarLayer(WebPassOwnPtr<WebScrollbar>, WebScrollbarThemePainter, WebPassOwnPtr<WebScrollbarThemeGeometry>) { return WebPassOwnPtr<WebScrollbarLayer>(); }


    // Animation ----------------------------------------------------

    virtual WebPassOwnPtr<WebAnimation> createAnimation(const WebAnimationCurve&, WebAnimation::TargetProperty, int animationId = 0) { return WebPassOwnPtr<WebAnimation>(); }

    virtual WebPassOwnPtr<WebFloatAnimationCurve> createFloatAnimationCurve() { return WebPassOwnPtr<WebFloatAnimationCurve>(); }

    virtual WebPassOwnPtr<WebTransformAnimationCurve> createTransformAnimationCurve() { return WebPassOwnPtr<WebTransformAnimationCurve>(); }

private:
    virtual ~WebCompositorSupport() { }
};

}

#endif // WebCompositorSupport_h
