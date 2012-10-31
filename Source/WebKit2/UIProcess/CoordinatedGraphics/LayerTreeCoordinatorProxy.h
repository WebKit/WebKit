/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef LayerTreeCoordinatorProxy_h
#define LayerTreeCoordinatorProxy_h

#if USE(COORDINATED_GRAPHICS)

#include "BackingStore.h"
#include "CoordinatedGraphicsArgumentCoders.h"
#include "DrawingAreaProxy.h"
#include "Region.h"
#include "SurfaceUpdateInfo.h"
#include "WebLayerTreeInfo.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/GraphicsLayerAnimation.h>
#include <WebCore/GraphicsSurfaceToken.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/RunLoop.h>
#include <WebCore/Timer.h>
#include <wtf/Functional.h>
#include <wtf/HashSet.h>

namespace WebKit {

class WebLayerInfo;
class LayerTreeRenderer;
class WebLayerUpdateInfo;

class LayerTreeCoordinatorProxy {
public:
    LayerTreeCoordinatorProxy(DrawingAreaProxy*);
    virtual ~LayerTreeCoordinatorProxy();
    void setCompositingLayerState(WebLayerID, const WebLayerInfo&);
    void setCompositingLayerChildren(WebLayerID, const Vector<WebLayerID>&);
#if ENABLE(CSS_FILTERS)
    void setCompositingLayerFilters(WebLayerID, const WebCore::FilterOperations&);
#endif
    void deleteCompositingLayer(WebLayerID);
    void setRootCompositingLayer(WebLayerID);
    void setContentsSize(const WebCore::FloatSize&);
    void setVisibleContentsRect(const WebCore::FloatRect&, float scale, const WebCore::FloatPoint& trajectoryVector);
    void didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect);
    void createTileForLayer(int layerID, int tileID, const WebCore::IntRect&, const SurfaceUpdateInfo&);
    void updateTileForLayer(int layerID, int tileID, const WebCore::IntRect&, const SurfaceUpdateInfo&);
    void removeTileForLayer(int layerID, int tileID);
    void createDirectlyCompositedImage(int64_t, const WebKit::ShareableBitmap::Handle&);
    void destroyDirectlyCompositedImage(int64_t);
    void didReceiveLayerTreeCoordinatorProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    void updateViewport();
    void renderNextFrame();
    void didChangeScrollPosition(const WebCore::IntPoint& position);
#if USE(GRAPHICS_SURFACE)
    void syncCanvas(uint32_t id, const WebCore::IntSize& canvasSize, const WebCore::GraphicsSurfaceToken&, uint32_t frontBuffer);
#endif
    void purgeBackingStores();
    LayerTreeRenderer* layerTreeRenderer() const { return m_renderer.get(); }
    void setLayerAnimations(WebLayerID, const WebCore::GraphicsLayerAnimations&);
    void setAnimationsLocked(bool);
#if ENABLE(REQUEST_ANIMATION_FRAME)
    void requestAnimationFrame();
    void animationFrameReady();
#endif

protected:
    void dispatchUpdate(const Function<void()>&);

    DrawingAreaProxy* m_drawingAreaProxy;
    RefPtr<LayerTreeRenderer> m_renderer;
    WebCore::IntRect m_lastSentVisibleRect;
    float m_lastSentScale;
    WebCore::FloatPoint m_lastSentTrajectoryVector;
#if USE(GRAPHICS_SURFACE)
    HashMap<WebCore::GraphicsSurfaceToken::BufferHandle, RefPtr<ShareableSurface> > m_surfaces;
#endif
};

}

#endif

#endif // LayerTreeCoordinatorProxy_h
