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

#include "config.h"

#if USE(COORDINATED_GRAPHICS)
#include "LayerTreeCoordinatorProxy.h"

#include "LayerTreeCoordinatorMessages.h"
#include "LayerTreeRenderer.h"
#include "UpdateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebLayerTreeInfo.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {

using namespace WebCore;

LayerTreeCoordinatorProxy::LayerTreeCoordinatorProxy(DrawingAreaProxy* drawingAreaProxy)
    : m_drawingAreaProxy(drawingAreaProxy)
    , m_renderer(adoptRef(new LayerTreeRenderer(this)))
    , m_lastSentScale(0)
{
}

LayerTreeCoordinatorProxy::~LayerTreeCoordinatorProxy()
{
    m_renderer->detach();
}

void LayerTreeCoordinatorProxy::updateViewport()
{
    m_drawingAreaProxy->updateViewport();
}

void LayerTreeCoordinatorProxy::dispatchUpdate(const Function<void()>& function)
{
    m_renderer->appendUpdate(function);
}

void LayerTreeCoordinatorProxy::createTileForLayer(int layerID, int tileID, const IntRect& targetRect, const WebKit::SurfaceUpdateInfo& updateInfo)
{
    dispatchUpdate(bind(&LayerTreeRenderer::createTile, m_renderer.get(), layerID, tileID, updateInfo.scaleFactor));
    updateTileForLayer(layerID, tileID, targetRect, updateInfo);
}

void LayerTreeCoordinatorProxy::updateTileForLayer(int layerID, int tileID, const IntRect& targetRect, const WebKit::SurfaceUpdateInfo& updateInfo)
{
    RefPtr<ShareableSurface> surface;
#if USE(GRAPHICS_SURFACE)
    int token = updateInfo.surfaceHandle.graphicsSurfaceToken();
    if (token) {
        HashMap<uint32_t, RefPtr<ShareableSurface> >::iterator it = m_surfaces.find(token);
        if (it == m_surfaces.end()) {
            surface = ShareableSurface::create(updateInfo.surfaceHandle);
            m_surfaces.add(token, surface);
        } else
            surface = it->second;
    } else
        surface = ShareableSurface::create(updateInfo.surfaceHandle);
#else
    surface = ShareableSurface::create(updateInfo.surfaceHandle);
#endif
    dispatchUpdate(bind(&LayerTreeRenderer::updateTile, m_renderer.get(), layerID, tileID, LayerTreeRenderer::TileUpdate(updateInfo.updateRect, targetRect, surface, updateInfo.surfaceOffset)));
}

void LayerTreeCoordinatorProxy::removeTileForLayer(int layerID, int tileID)
{
    dispatchUpdate(bind(&LayerTreeRenderer::removeTile, m_renderer.get(), layerID, tileID));
}

void LayerTreeCoordinatorProxy::deleteCompositingLayer(WebLayerID id)
{
    dispatchUpdate(bind(&LayerTreeRenderer::deleteLayer, m_renderer.get(), id));
    updateViewport();
}

void LayerTreeCoordinatorProxy::setRootCompositingLayer(WebLayerID id)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setRootLayerID, m_renderer.get(), id));
    updateViewport();
}

void LayerTreeCoordinatorProxy::setCompositingLayerState(WebLayerID id, const WebLayerInfo& info)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setLayerState, m_renderer.get(), id, info));
}

void LayerTreeCoordinatorProxy::setCompositingLayerChildren(WebLayerID id, const Vector<WebLayerID>& children)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setLayerChildren, m_renderer.get(), id, children));
}

#if ENABLE(CSS_FILTERS)
void LayerTreeCoordinatorProxy::setCompositingLayerFilters(WebLayerID id, const FilterOperations& filters)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setLayerFilters, m_renderer.get(), id, filters));
}
#endif

void LayerTreeCoordinatorProxy::didRenderFrame()
{
    dispatchUpdate(bind(&LayerTreeRenderer::flushLayerChanges, m_renderer.get()));
    updateViewport();
}

void LayerTreeCoordinatorProxy::createDirectlyCompositedImage(int64_t key, const WebKit::ShareableBitmap::Handle& handle)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(handle);
    dispatchUpdate(bind(&LayerTreeRenderer::createImage, m_renderer.get(), key, bitmap));
}

void LayerTreeCoordinatorProxy::destroyDirectlyCompositedImage(int64_t key)
{
    dispatchUpdate(bind(&LayerTreeRenderer::destroyImage, m_renderer.get(), key));
}

void LayerTreeCoordinatorProxy::setContentsSize(const FloatSize& contentsSize)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setContentsSize, m_renderer.get(), contentsSize));
}

void LayerTreeCoordinatorProxy::setLayerAnimatedOpacity(uint32_t id, float opacity)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setAnimatedOpacity, m_renderer.get(), id, opacity));
}

void LayerTreeCoordinatorProxy::setLayerAnimatedTransform(uint32_t id, const WebCore::TransformationMatrix& transform)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setAnimatedTransform, m_renderer.get(), id, transform));
}

void LayerTreeCoordinatorProxy::setVisibleContentsRect(const FloatRect& rect, float scale, const FloatPoint& trajectoryVector)
{
    // Inform the renderer to adjust viewport-fixed layers.
    dispatchUpdate(bind(&LayerTreeRenderer::setVisibleContentsRect, m_renderer.get(), rect));

    // Round the rect instead of enclosing it to make sure that its size stays the same while panning. This can have nasty effects on layout.
    IntRect roundedRect = roundedIntRect(rect);
    if (roundedRect == m_lastSentVisibleRect && scale == m_lastSentScale && trajectoryVector == m_lastSentTrajectoryVector)
        return;

    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeCoordinator::SetVisibleContentsRect(roundedRect, scale, trajectoryVector), m_drawingAreaProxy->page()->pageID());
    m_lastSentVisibleRect = roundedRect;
    m_lastSentScale = scale;
    m_lastSentTrajectoryVector = trajectoryVector;
}

void LayerTreeCoordinatorProxy::renderNextFrame()
{
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeCoordinator::RenderNextFrame(), m_drawingAreaProxy->page()->pageID());
}

void LayerTreeCoordinatorProxy::didChangeScrollPosition(const IntPoint& position)
{
    dispatchUpdate(bind(&LayerTreeRenderer::didChangeScrollPosition, m_renderer.get(), position));
}

void LayerTreeCoordinatorProxy::syncCanvas(uint32_t id, const IntSize& canvasSize, uint64_t graphicsSurfaceToken, uint32_t frontBuffer)
{
    dispatchUpdate(bind(&LayerTreeRenderer::syncCanvas, m_renderer.get(), id, canvasSize, graphicsSurfaceToken, frontBuffer));
}

void LayerTreeCoordinatorProxy::purgeBackingStores()
{
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeCoordinator::PurgeBackingStores(), m_drawingAreaProxy->page()->pageID());
}

}
#endif // USE(COORDINATED_GRAPHICS)
