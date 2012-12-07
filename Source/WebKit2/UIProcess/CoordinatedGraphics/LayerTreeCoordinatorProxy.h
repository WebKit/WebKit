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
#include "CoordinatedLayerInfo.h"
#include "DrawingAreaProxy.h"
#include "Region.h"
#include "ShareableSurface.h"
#include "SurfaceUpdateInfo.h"
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

class CoordinatedLayerInfo;
class LayerTreeRenderer;
class CoordinatedLayerUpdateInfo;

class LayerTreeCoordinatorProxy {
    WTF_MAKE_NONCOPYABLE(LayerTreeCoordinatorProxy);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit LayerTreeCoordinatorProxy(DrawingAreaProxy*);
    ~LayerTreeCoordinatorProxy();
    void setCompositingLayerState(CoordinatedLayerID, const CoordinatedLayerInfo&);
    void setCompositingLayerChildren(CoordinatedLayerID, const Vector<CoordinatedLayerID>&);
#if ENABLE(CSS_FILTERS)
    void setCompositingLayerFilters(CoordinatedLayerID, const WebCore::FilterOperations&);
#endif
#if ENABLE(CSS_SHADERS)
    void createCustomFilterProgram(int id, const WebCore::CustomFilterProgramInfo&);
    void removeCustomFilterProgram(int id);
#endif
    void deleteCompositingLayer(CoordinatedLayerID);
    void setRootCompositingLayer(CoordinatedLayerID);
    void setContentsSize(const WebCore::FloatSize&);
    void setVisibleContentsRect(const WebCore::FloatRect&, float scale, const WebCore::FloatPoint& trajectoryVector);
    void didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect);
    void createTileForLayer(CoordinatedLayerID, uint32_t tileID, const WebCore::IntRect&, const SurfaceUpdateInfo&);
    void updateTileForLayer(CoordinatedLayerID, uint32_t tileID, const WebCore::IntRect&, const SurfaceUpdateInfo&);
    void removeTileForLayer(CoordinatedLayerID, uint32_t tileID);
    void createUpdateAtlas(int atlasID, const ShareableSurface::Handle&);
    void removeUpdateAtlas(int atlasID);
    void createImageBacking(CoordinatedImageBackingID);
    void updateImageBacking(CoordinatedImageBackingID, const ShareableSurface::Handle&);
    void clearImageBackingContents(CoordinatedImageBackingID);
    void removeImageBacking(CoordinatedImageBackingID);
    void didReceiveLayerTreeCoordinatorProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    void updateViewport();
    void renderNextFrame();
    void didChangeScrollPosition(const WebCore::IntPoint& position);
#if USE(GRAPHICS_SURFACE)
    void createCanvas(CoordinatedLayerID, const WebCore::IntSize&, const WebCore::GraphicsSurfaceToken&);
    void syncCanvas(CoordinatedLayerID, uint32_t frontBuffer);
    void destroyCanvas(CoordinatedLayerID);
#endif
    void purgeBackingStores();
    LayerTreeRenderer* layerTreeRenderer() const { return m_renderer.get(); }
    void setLayerAnimations(CoordinatedLayerID, const WebCore::GraphicsLayerAnimations&);
    void setAnimationsLocked(bool);
#if ENABLE(REQUEST_ANIMATION_FRAME)
    void requestAnimationFrame();
    void animationFrameReady();
#endif
    void setBackgroundColor(const WebCore::Color&);

protected:
    void dispatchUpdate(const Function<void()>&);

    DrawingAreaProxy* m_drawingAreaProxy;
    RefPtr<LayerTreeRenderer> m_renderer;
    WebCore::IntRect m_lastSentVisibleRect;
    float m_lastSentScale;
    WebCore::FloatPoint m_lastSentTrajectoryVector;
    typedef HashMap<int /* atlasID */, RefPtr<ShareableSurface> > SurfaceMap;
    SurfaceMap m_surfaces;
};

}

#endif

#endif // LayerTreeCoordinatorProxy_h
