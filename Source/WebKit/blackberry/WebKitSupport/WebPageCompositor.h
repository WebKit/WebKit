/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef WebPageCompositor_h
#define WebPageCompositor_h

#if USE(ACCELERATED_COMPOSITING)

#include "GLES2Context.h"
#include "LayerCompositingThread.h"
#include "LayerRenderer.h"

#include <BlackBerryPlatformTimer.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class LayerWebKitThread;
};

namespace BlackBerry {
namespace WebKit {

class WebPagePrivate;

// This class may only be used on the compositing thread.
class WebPageCompositor {
public:
    WebPageCompositor(WebPagePrivate*);
    ~WebPageCompositor();

    bool hardwareCompositing() const;

    void setRootLayer(WebCore::LayerCompositingThread*);

    void setBackingStoreUsesOpenGL(bool);

    void commit(WebCore::LayerWebKitThread* rootLayerProxy);

    bool drawLayers(const WebCore::IntRect& dstRect, const WebCore::FloatRect& contents);

    WebCore::IntRect layoutRectForCompositing() const { return m_layoutRectForCompositing; }
    void setLayoutRectForCompositing(const WebCore::IntRect& rect) { m_layoutRectForCompositing = rect; }

    WebCore::IntSize contentsSizeForCompositing() const { return m_contentsSizeForCompositing; }
    void setContentsSizeForCompositing(const WebCore::IntSize& size) { m_contentsSizeForCompositing = size; }

    WebCore::LayerRenderingResults lastCompositingResults() const { return m_lastCompositingResults; }
    void setLastCompositingResults(const WebCore::LayerRenderingResults& results) { m_lastCompositingResults = results; }

    void releaseLayerResources();

private:
    void animationTimerFired();

    WebPagePrivate* m_webPage;
    // Please maintain this order since m_layerRenderer depends on m_context in initialization list.
    OwnPtr<GLES2Context> m_context;
    OwnPtr<WebCore::LayerRenderer> m_layerRenderer;
    RefPtr<WebCore::LayerCompositingThread> m_rootLayer;
    WebCore::IntRect m_layoutRectForCompositing;
    WebCore::IntSize m_contentsSizeForCompositing;
    WebCore::LayerRenderingResults m_lastCompositingResults;
    int m_generation;
    int m_compositedGeneration;
    WebCore::IntRect m_compositedDstRect;
    WebCore::FloatRect m_compositedContentsRect;
    bool m_backingStoreUsesOpenGL;
    BlackBerry::Platform::Timer<WebPageCompositor> m_animationTimer;
    BlackBerry::Platform::TimerClient* m_timerClient;
};

} // namespace WebKit
} // namespace BlackBerry

#endif // USE(ACCELERATED_COMPOSITING)

#endif // WebPageCompositor_h
