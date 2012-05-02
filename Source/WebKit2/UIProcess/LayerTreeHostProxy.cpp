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

#if USE(UI_SIDE_COMPOSITING)
#include "LayerTreeHostProxy.h"

#include "LayerTreeHostMessages.h"
#include "UpdateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebLayerTreeInfo.h"
#include "WebLayerTreeRenderer.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {

using namespace WebCore;

LayerTreeHostProxy::LayerTreeHostProxy(DrawingAreaProxy* drawingAreaProxy)
    : m_drawingAreaProxy(drawingAreaProxy)
    , m_renderer(adoptRef(new WebLayerTreeRenderer(this)))
{
}

LayerTreeHostProxy::~LayerTreeHostProxy()
{
    m_renderer->detach();
}

void LayerTreeHostProxy::updateViewport()
{
    m_drawingAreaProxy->updateViewport();
}

void LayerTreeHostProxy::dispatchUpdate(const Function<void()>& function)
{
    m_renderer->appendUpdate(function);
}

void LayerTreeHostProxy::createTileForLayer(int layerID, int tileID, const IntRect& targetRect, const WebKit::SurfaceUpdateInfo& updateInfo)
{
    dispatchUpdate(bind(&WebLayerTreeRenderer::createTile, m_renderer.get(), layerID, tileID, updateInfo.scaleFactor));
    updateTileForLayer(layerID, tileID, targetRect, updateInfo);
}

void LayerTreeHostProxy::updateTileForLayer(int layerID, int tileID, const IntRect& targetRect, const WebKit::SurfaceUpdateInfo& updateInfo)
{
    RefPtr<ShareableSurface> surface;
#if USE(GRAPHICS_SURFACE)
    uint32_t token = updateInfo.surfaceHandle.graphicsSurfaceToken();
    HashMap<uint32_t, RefPtr<ShareableSurface> >::iterator it = m_surfaces.find(token);
    if (it == m_surfaces.end()) {
        surface = ShareableSurface::create(updateInfo.surfaceHandle);
        m_surfaces.add(token, surface);
    } else
        surface = it->second;
#else
    surface = ShareableSurface::create(updateInfo.surfaceHandle);
#endif
    dispatchUpdate(bind(&WebLayerTreeRenderer::updateTile, m_renderer.get(), layerID, tileID, WebLayerTreeRenderer::TileUpdate(updateInfo.updateRect, targetRect, surface, updateInfo.surfaceOffset)));
}

void LayerTreeHostProxy::removeTileForLayer(int layerID, int tileID)
{
    dispatchUpdate(bind(&WebLayerTreeRenderer::removeTile, m_renderer.get(), layerID, tileID));
}

void LayerTreeHostProxy::deleteCompositingLayer(WebLayerID id)
{
    dispatchUpdate(bind(&WebLayerTreeRenderer::deleteLayer, m_renderer.get(), id));
    updateViewport();
}

void LayerTreeHostProxy::setRootCompositingLayer(WebLayerID id)
{
    dispatchUpdate(bind(&WebLayerTreeRenderer::setRootLayerID, m_renderer.get(), id));
    updateViewport();
}

void LayerTreeHostProxy::setCompositingLayerState(WebLayerID id, const WebLayerInfo& info)
{
    dispatchUpdate(bind(&WebLayerTreeRenderer::setLayerState, m_renderer.get(), id, info));
}

void LayerTreeHostProxy::setCompositingLayerChildren(WebLayerID id, const Vector<WebLayerID>& children)
{
    dispatchUpdate(bind(&WebLayerTreeRenderer::setLayerChildren, m_renderer.get(), id, children));
}

#if ENABLE(CSS_FILTERS)
void LayerTreeHostProxy::setCompositingLayerFilters(WebLayerID id, const FilterOperations& filters)
{
    dispatchUpdate(bind(&WebLayerTreeRenderer::setLayerFilters, m_renderer.get(), id, filters));
}
#endif

void LayerTreeHostProxy::didRenderFrame()
{
    dispatchUpdate(bind(&WebLayerTreeRenderer::flushLayerChanges, m_renderer.get()));
    updateViewport();
}

void LayerTreeHostProxy::createDirectlyCompositedImage(int64_t key, const WebKit::ShareableBitmap::Handle& handle)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(handle);
    dispatchUpdate(bind(&WebLayerTreeRenderer::createImage, m_renderer.get(), key, bitmap));
}

void LayerTreeHostProxy::destroyDirectlyCompositedImage(int64_t key)
{
    dispatchUpdate(bind(&WebLayerTreeRenderer::destroyImage, m_renderer.get(), key));
}

void LayerTreeHostProxy::setContentsSize(const FloatSize& contentsSize)
{
    m_renderer->setContentsSize(contentsSize);
}

void LayerTreeHostProxy::setVisibleContentsRect(const IntRect& rect, float scale, const FloatPoint& trajectoryVector, const WebCore::FloatPoint& accurateVisibleContentsPosition)
{
    dispatchUpdate(bind(&WebLayerTreeRenderer::setVisibleContentsRect, m_renderer.get(), rect, scale, accurateVisibleContentsPosition));
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeHost::SetVisibleContentsRect(rect, scale, trajectoryVector), m_drawingAreaProxy->page()->pageID());
}

void LayerTreeHostProxy::renderNextFrame()
{
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeHost::RenderNextFrame(), m_drawingAreaProxy->page()->pageID());
}

void LayerTreeHostProxy::didChangeScrollPosition(const IntPoint& position)
{
    dispatchUpdate(bind(&WebLayerTreeRenderer::didChangeScrollPosition, m_renderer.get(), position));
}

void LayerTreeHostProxy::purgeBackingStores()
{
    m_renderer->setActive(false);
    m_drawingAreaProxy->page()->process()->send(Messages::LayerTreeHost::PurgeBackingStores(), m_drawingAreaProxy->page()->pageID());
}

}
#endif // USE(UI_SIDE_COMPOSITING)
