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
#include "CoordinatedLayerTreeHostProxy.h"

#include "CoordinatedLayerInfo.h"
#include "CoordinatedLayerTreeHostMessages.h"
#include "LayerTreeRenderer.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/GraphicsSurface.h>

#if ENABLE(CSS_SHADERS)
#include "CustomFilterProgramInfo.h"
#endif

namespace WebKit {

using namespace WebCore;

CoordinatedLayerTreeHostProxy::CoordinatedLayerTreeHostProxy(DrawingAreaProxy* drawingAreaProxy)
    : m_drawingAreaProxy(drawingAreaProxy)
    , m_renderer(adoptRef(new LayerTreeRenderer(this)))
    , m_lastSentScale(0)
{
}

CoordinatedLayerTreeHostProxy::~CoordinatedLayerTreeHostProxy()
{
    m_renderer->detach();
}

void CoordinatedLayerTreeHostProxy::updateViewport()
{
    m_drawingAreaProxy->updateViewport();
}

void CoordinatedLayerTreeHostProxy::dispatchUpdate(const Function<void()>& function)
{
    m_renderer->appendUpdate(function);
}

void CoordinatedLayerTreeHostProxy::createTileForLayer(CoordinatedLayerID layerID, uint32_t tileID, const WebCore::IntRect& tileRect, const WebKit::SurfaceUpdateInfo& updateInfo)
{
    dispatchUpdate(bind(&LayerTreeRenderer::createTile, m_renderer.get(), layerID, tileID, updateInfo.scaleFactor));
    updateTileForLayer(layerID, tileID, tileRect, updateInfo);
}

void CoordinatedLayerTreeHostProxy::updateTileForLayer(CoordinatedLayerID layerID, uint32_t tileID, const IntRect& tileRect, const WebKit::SurfaceUpdateInfo& updateInfo)
{
    SurfaceMap::iterator it = m_surfaces.find(updateInfo.atlasID);
    ASSERT(it != m_surfaces.end());
    dispatchUpdate(bind(&LayerTreeRenderer::updateTile, m_renderer.get(), layerID, tileID, LayerTreeRenderer::TileUpdate(updateInfo.updateRect, tileRect, it->value, updateInfo.surfaceOffset)));
}

void CoordinatedLayerTreeHostProxy::removeTileForLayer(CoordinatedLayerID layerID, uint32_t tileID)
{
    dispatchUpdate(bind(&LayerTreeRenderer::removeTile, m_renderer.get(), layerID, tileID));
}

void CoordinatedLayerTreeHostProxy::createUpdateAtlas(uint32_t atlasID, const WebCoordinatedSurface::Handle& handle)
{
    ASSERT(!m_surfaces.contains(atlasID));
    m_surfaces.add(atlasID, WebCoordinatedSurface::create(handle));
}

void CoordinatedLayerTreeHostProxy::removeUpdateAtlas(uint32_t atlasID)
{
    ASSERT(m_surfaces.contains(atlasID));
    m_surfaces.remove(atlasID);
}

void CoordinatedLayerTreeHostProxy::deleteCompositingLayer(CoordinatedLayerID id)
{
    dispatchUpdate(bind(&LayerTreeRenderer::deleteLayer, m_renderer.get(), id));
}

void CoordinatedLayerTreeHostProxy::setRootCompositingLayer(CoordinatedLayerID id)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setRootLayerID, m_renderer.get(), id));
}

void CoordinatedLayerTreeHostProxy::setCompositingLayerState(CoordinatedLayerID id, const CoordinatedLayerInfo& info)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setLayerState, m_renderer.get(), id, info));
}

void CoordinatedLayerTreeHostProxy::setCompositingLayerChildren(CoordinatedLayerID id, const Vector<CoordinatedLayerID>& children)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setLayerChildren, m_renderer.get(), id, children));
}

#if ENABLE(CSS_FILTERS)
void CoordinatedLayerTreeHostProxy::setCompositingLayerFilters(CoordinatedLayerID id, const FilterOperations& filters)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setLayerFilters, m_renderer.get(), id, filters));
}
#endif

#if ENABLE(CSS_SHADERS)
void CoordinatedLayerTreeHostProxy::removeCustomFilterProgram(int id)
{
    dispatchUpdate(bind(&LayerTreeRenderer::removeCustomFilterProgram, m_renderer.get(), id));
}
void CoordinatedLayerTreeHostProxy::createCustomFilterProgram(int id, const WebCore::CustomFilterProgramInfo& programInfo)
{
    dispatchUpdate(bind(&LayerTreeRenderer::createCustomFilterProgram, m_renderer.get(), id, programInfo));
}
#endif

void CoordinatedLayerTreeHostProxy::didRenderFrame(const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect)
{
    dispatchUpdate(bind(&LayerTreeRenderer::flushLayerChanges, m_renderer.get()));
    updateViewport();
#if USE(TILED_BACKING_STORE)
    m_drawingAreaProxy->page()->didRenderFrame(contentsSize, coveredRect);
#else
    UNUSED_PARAM(contentsSize);
    UNUSED_PARAM(coveredRect);
#endif
}

void CoordinatedLayerTreeHostProxy::createImageBacking(CoordinatedImageBackingID imageID)
{
    dispatchUpdate(bind(&LayerTreeRenderer::createImageBacking, m_renderer.get(), imageID));
}

void CoordinatedLayerTreeHostProxy::updateImageBacking(CoordinatedImageBackingID imageID, const WebCoordinatedSurface::Handle& handle)
{
    dispatchUpdate(bind(&LayerTreeRenderer::updateImageBacking, m_renderer.get(), imageID, WebCoordinatedSurface::create(handle)));
}

void CoordinatedLayerTreeHostProxy::clearImageBackingContents(CoordinatedImageBackingID imageID)
{
    dispatchUpdate(bind(&LayerTreeRenderer::clearImageBackingContents, m_renderer.get(), imageID));
}

void CoordinatedLayerTreeHostProxy::removeImageBacking(CoordinatedImageBackingID imageID)
{
    dispatchUpdate(bind(&LayerTreeRenderer::removeImageBacking, m_renderer.get(), imageID));
}

void CoordinatedLayerTreeHostProxy::setContentsSize(const FloatSize& contentsSize)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setContentsSize, m_renderer.get(), contentsSize));
}

void CoordinatedLayerTreeHostProxy::setLayerAnimations(CoordinatedLayerID id, const GraphicsLayerAnimations& animations)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setLayerAnimations, m_renderer.get(), id, animations));
}

void CoordinatedLayerTreeHostProxy::setAnimationsLocked(bool locked)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setAnimationsLocked, m_renderer.get(), locked));
}

void CoordinatedLayerTreeHostProxy::setVisibleContentsRect(const FloatRect& rect, float scale, const FloatPoint& trajectoryVector)
{
    // Inform the renderer to adjust viewport-fixed layers.
    dispatchUpdate(bind(&LayerTreeRenderer::setVisibleContentsRect, m_renderer.get(), rect));

    if (rect == m_lastSentVisibleRect && scale == m_lastSentScale && trajectoryVector == m_lastSentTrajectoryVector)
        return;

    m_drawingAreaProxy->page()->process()->send(Messages::CoordinatedLayerTreeHost::SetVisibleContentsRect(rect, scale, trajectoryVector), m_drawingAreaProxy->page()->pageID());
    m_lastSentVisibleRect = rect;
    m_lastSentScale = scale;
    m_lastSentTrajectoryVector = trajectoryVector;
}

void CoordinatedLayerTreeHostProxy::renderNextFrame()
{
    m_drawingAreaProxy->page()->process()->send(Messages::CoordinatedLayerTreeHost::RenderNextFrame(), m_drawingAreaProxy->page()->pageID());
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void CoordinatedLayerTreeHostProxy::requestAnimationFrame()
{
    dispatchUpdate(bind(&LayerTreeRenderer::requestAnimationFrame, m_renderer.get()));
    updateViewport();
}

void CoordinatedLayerTreeHostProxy::animationFrameReady()
{
    m_drawingAreaProxy->page()->process()->send(Messages::CoordinatedLayerTreeHost::AnimationFrameReady(), m_drawingAreaProxy->page()->pageID());
}
#endif

void CoordinatedLayerTreeHostProxy::didChangeScrollPosition(const FloatPoint& position)
{
    dispatchUpdate(bind(&LayerTreeRenderer::didChangeScrollPosition, m_renderer.get(), position));
}

#if USE(GRAPHICS_SURFACE)
void CoordinatedLayerTreeHostProxy::createCanvas(CoordinatedLayerID id, const IntSize& canvasSize, const GraphicsSurfaceToken& token)
{
    GraphicsSurface::Flags surfaceFlags = GraphicsSurface::SupportsTextureTarget | GraphicsSurface::SupportsSharing;
    dispatchUpdate(bind(&LayerTreeRenderer::createCanvas, m_renderer.get(), id, canvasSize, GraphicsSurface::create(canvasSize, surfaceFlags, token)));
}

void CoordinatedLayerTreeHostProxy::syncCanvas(CoordinatedLayerID id, uint32_t frontBuffer)
{
    dispatchUpdate(bind(&LayerTreeRenderer::syncCanvas, m_renderer.get(), id, frontBuffer));
}

void CoordinatedLayerTreeHostProxy::destroyCanvas(CoordinatedLayerID id)
{
    dispatchUpdate(bind(&LayerTreeRenderer::destroyCanvas, m_renderer.get(), id));
}
#endif

void CoordinatedLayerTreeHostProxy::purgeBackingStores()
{
    m_surfaces.clear();
    m_drawingAreaProxy->page()->process()->send(Messages::CoordinatedLayerTreeHost::PurgeBackingStores(), m_drawingAreaProxy->page()->pageID());
}

void CoordinatedLayerTreeHostProxy::setBackgroundColor(const WebCore::Color& color)
{
    dispatchUpdate(bind(&LayerTreeRenderer::setBackgroundColor, m_renderer.get(), color));
}

}
#endif // USE(COORDINATED_GRAPHICS)
