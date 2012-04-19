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

#ifndef WebPageCompositor_p_h
#define WebPageCompositor_p_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerCompositingThread.h"
#include "LayerRenderer.h"

#include <BlackBerryPlatformAnimation.h>
#include <BlackBerryPlatformGLES2Context.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class LayerWebKitThread;
};

namespace BlackBerry {
namespace WebKit {

class WebPageCompositorClient;
class WebPagePrivate;

// This class may only be used on the compositing thread. So it does not need to be threadsaferefcounted.
class WebPageCompositorPrivate : public RefCounted<WebPageCompositorPrivate>, public Platform::AnimationFrameRateClient {
public:
    static PassRefPtr<WebPageCompositorPrivate> create(WebPagePrivate* page, WebPageCompositorClient* client)
    {
        return adoptRef(new WebPageCompositorPrivate(page, client));
    }

    ~WebPageCompositorPrivate();

    bool hardwareCompositing() const;

    Platform::Graphics::GLES2Context* context() const { return m_context; }
    void setContext(Platform::Graphics::GLES2Context*);

    WebCore::LayerCompositingThread* rootLayer() const { return m_rootLayer.get(); }
    void setRootLayer(WebCore::LayerCompositingThread*);

    void commit(WebCore::LayerWebKitThread* rootLayerProxy);

    // This is mapped from the public API, thus takes transformed contents
    void render(const WebCore::IntRect& dstRect, const WebCore::IntRect& transformedContents);

    // Returns true if the WebPageCompositor draws the root layer, false if the BackingStore draws the root layer
    bool drawsRootLayer() const;
    void setDrawsRootLayer(bool drawsRootLayer) { m_drawsRootLayer = drawsRootLayer; }

    // Render everything but the root layer, or everything if drawsRootLayer() is true.
    bool drawLayers(const WebCore::IntRect& dstRect, const WebCore::FloatRect& contents);

    WebCore::IntRect layoutRectForCompositing() const { return m_layoutRectForCompositing; }
    void setLayoutRectForCompositing(const WebCore::IntRect& rect) { m_layoutRectForCompositing = rect; }

    WebCore::IntSize contentsSizeForCompositing() const { return m_contentsSizeForCompositing; }
    void setContentsSizeForCompositing(const WebCore::IntSize& size) { m_contentsSizeForCompositing = size; }

    WebCore::LayerRenderingResults lastCompositingResults() const { return m_lastCompositingResults; }
    void setLastCompositingResults(const WebCore::LayerRenderingResults& results) { m_lastCompositingResults = results; }

    void releaseLayerResources();

    WebPagePrivate* page() const { return m_webPage; }
    WebPageCompositorClient* client() const { return m_client; }
    void compositorDestroyed();

protected:
    WebPageCompositorPrivate(WebPagePrivate*, WebPageCompositorClient*);

private:
    void animationFrameChanged();

    WebPageCompositorClient* m_client;
    WebPagePrivate* m_webPage;
    Platform::Graphics::GLES2Context* m_context;
    OwnPtr<WebCore::LayerRenderer> m_layerRenderer;
    RefPtr<WebCore::LayerCompositingThread> m_rootLayer;
    WebCore::IntRect m_layoutRectForCompositing;
    WebCore::IntSize m_contentsSizeForCompositing;
    WebCore::LayerRenderingResults m_lastCompositingResults;
    bool m_drawsRootLayer;
};

} // namespace WebKit
} // namespace BlackBerry

#endif // USE(ACCELERATED_COMPOSITING)

#endif // WebPageCompositor_p_h
