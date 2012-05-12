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

#ifndef WebLayerTreeView_h
#define WebLayerTreeView_h

#include "WebColor.h"
#include "WebCommon.h"
#include "WebNonCopyable.h"
#include "WebPrivateOwnPtr.h"

namespace WebCore {
class CCLayerTreeHost;
struct CCSettings;
}

namespace WebKit {
class WebGraphicsContext3D;
class WebLayer;
class WebLayerTreeViewClient;
class WebLayerTreeViewImpl;
struct WebPoint;
struct WebRect;
struct WebSize;

class WebLayerTreeView : public WebNonCopyable {
public:
    struct Settings {
        Settings()
            : acceleratePainting(false)
            , showFPSCounter(false)
            , showPlatformLayerTree(false)
            , showPaintRects(false)
            , refreshRate(0)
            , perTilePainting(false)
            , partialSwapEnabled(false)
            , threadedAnimationEnabled(false)
        {
        }

        bool acceleratePainting;
        bool showFPSCounter;
        bool showPlatformLayerTree;
        bool showPaintRects;
        double refreshRate;
        bool perTilePainting;
        bool partialSwapEnabled;
        bool threadedAnimationEnabled;
#if WEBKIT_IMPLEMENTATION
        operator WebCore::CCSettings() const;
#endif
    };

    WebLayerTreeView() { }
    ~WebLayerTreeView() { reset(); }

    WEBKIT_EXPORT void reset();

    bool isNull() const;

    // Initialization and lifecycle --------------------------------------

    // Attempts to initialize this WebLayerTreeView with the given client, root layer, and settings.
    // If initialization fails, this will return false and .isNull() will return true.
    // Must be called before any methods below.
    WEBKIT_EXPORT bool initialize(WebLayerTreeViewClient*, const WebLayer& root, const Settings&);

    // Indicates that the compositing surface used by this WebLayerTreeView is ready to use.
    // A WebLayerTreeView may request a context from its client before the surface is ready,
    // but it won't attempt to use it.
    WEBKIT_EXPORT void setSurfaceReady();

    // Sets the root of the tree. The root is set by way of the constructor.
    // This is typically used to explicitly set the root to null to break
    // cycles.
    WEBKIT_EXPORT void setRootLayer(WebLayer*);

    // Returns a unique identifier that can be used on the compositor thread to request a
    // WebCompositorInputHandler instance.
    WEBKIT_EXPORT int compositorIdentifier();


    // View properties ---------------------------------------------------

    WEBKIT_EXPORT void setViewportSize(const WebSize&);
    WEBKIT_EXPORT WebSize viewportSize() const;

    // Sets the background color for the viewport.
    WEBKIT_EXPORT void setBackgroundColor(WebColor);

    // Sets whether this view is visible. In threaded mode, a view that is not visible will not
    // composite or trigger updateAnimations() or layout() calls until it becomes visible.
    WEBKIT_EXPORT void setVisible(bool);

    // Sets the current page scale factor and minimum / maximum limits. Both limits are initially 1 (no page scale allowed).
    WEBKIT_EXPORT void setPageScaleFactorAndLimits(float pageScaleFactor, float minimum, float maximum);

    // Starts an animation of the page scale to a target scale factor and scroll offset.
    // If useAnchor is true, destination is a point on the screen that will remain fixed for the duration of the animation.
    // If useAnchor is false, destination is the final top-left scroll position.
    WEBKIT_EXPORT void startPageScaleAnimation(const WebPoint& destination, bool useAnchor, float newPageScale, double durationSec);


    // Flow control and scheduling ---------------------------------------

    // Requests an updateAnimations() call.
    WEBKIT_EXPORT void setNeedsAnimate();

    // Indicates that the view needs to be redrawn. This is typically used when the frontbuffer is damaged.
    WEBKIT_EXPORT void setNeedsRedraw();

    // Indicates whether a commit is pending.
    WEBKIT_EXPORT bool commitRequested() const;

    // Triggers a compositing pass. If the compositor thread was not
    // enabled via WebCompositor::initialize, the compositing pass happens
    // immediately. If it is enabled, the compositing pass will happen at a
    // later time. Before the compositing pass happens (i.e. before composite()
    // returns when the compositor thread is disabled), WebContentLayers will be
    // asked to paint their dirty region, through
    // WebContentLayerClient::paintContents.
    WEBKIT_EXPORT void composite();

    // Immediately update animations. This should only be used when frame scheduling is handled by
    // the WebLayerTreeView user and not internally by the compositor, meaning only in single-threaded
    // mode.
    WEBKIT_EXPORT void updateAnimations(double frameBeginTime);

    // Composites and attempts to read back the result into the provided
    // buffer. If it wasn't possible, e.g. due to context lost, will return
    // false. Pixel format is 32bit (RGBA), and the provided buffer must be
    // large enough contain viewportSize().width() * viewportSize().height()
    // pixels. The WebLayerTreeView does not assume ownership of the buffer.
    // The buffer is not modified if the false is returned.
    WEBKIT_EXPORT bool compositeAndReadback(void *pixels, const WebRect&);

    // Blocks until the most recently composited frame has finished rendering on the GPU.
    // This can have a significant performance impact and should be used with care.
    WEBKIT_EXPORT void finishAllRendering();

    // Returns the context being used for rendering this view. In threaded compositing mode, it is
    // not safe to use this context for anything on the main thread, other than passing the pointer to
    // the compositor thread.
    WEBKIT_EXPORT WebGraphicsContext3D* context();

    // Debugging / dangerous ---------------------------------------------

    // Simulates a lost context. For testing only.
    WEBKIT_EXPORT void loseCompositorContext(int numTimes);

protected:
    WebPrivateOwnPtr<WebLayerTreeViewImpl> m_private;
};

} // namespace WebKit

#endif // WebLayerTreeView_h
