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

#include "CoordinatedLayerTreeHostMessages.h"
#include "CoordinatedLayerTreeHostProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/CoordinatedLayerInfo.h>
#include <WebCore/GraphicsSurface.h>
#include <WebCore/SurfaceUpdateInfo.h>

#if ENABLE(CSS_SHADERS)
#include "CustomFilterProgramInfo.h"
#endif

namespace WebKit {

using namespace WebCore;

CoordinatedLayerTreeHostProxy::CoordinatedLayerTreeHostProxy(DrawingAreaProxy* drawingAreaProxy)
    : m_drawingAreaProxy(drawingAreaProxy)
    , m_scene(adoptRef(new CoordinatedGraphicsScene(this)))
{
    m_drawingAreaProxy->page()->process()->addMessageReceiver(Messages::CoordinatedLayerTreeHostProxy::messageReceiverName(), m_drawingAreaProxy->page()->pageID(), this);
}

CoordinatedLayerTreeHostProxy::~CoordinatedLayerTreeHostProxy()
{
    m_drawingAreaProxy->page()->process()->removeMessageReceiver(Messages::CoordinatedLayerTreeHostProxy::messageReceiverName(), m_drawingAreaProxy->page()->pageID());
    m_scene->detach();
}

void CoordinatedLayerTreeHostProxy::updateViewport()
{
    m_drawingAreaProxy->updateViewport();
}

void CoordinatedLayerTreeHostProxy::dispatchUpdate(const Function<void()>& function)
{
    m_scene->appendUpdate(function);
}

void CoordinatedLayerTreeHostProxy::createTileForLayer(CoordinatedLayerID layerID, uint32_t tileID, const IntRect& tileRect, const SurfaceUpdateInfo& updateInfo)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::createTile, m_scene.get(), layerID, tileID, updateInfo.scaleFactor));
    updateTileForLayer(layerID, tileID, tileRect, updateInfo);
}

void CoordinatedLayerTreeHostProxy::updateTileForLayer(CoordinatedLayerID layerID, uint32_t tileID, const IntRect& tileRect, const SurfaceUpdateInfo& updateInfo)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::updateTile, m_scene.get(), layerID, tileID, CoordinatedGraphicsScene::TileUpdate(updateInfo.updateRect, tileRect, updateInfo.atlasID, updateInfo.surfaceOffset)));
}

void CoordinatedLayerTreeHostProxy::removeTileForLayer(CoordinatedLayerID layerID, uint32_t tileID)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::removeTile, m_scene.get(), layerID, tileID));
}

void CoordinatedLayerTreeHostProxy::createUpdateAtlas(uint32_t atlasID, const WebCoordinatedSurface::Handle& handle)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::createUpdateAtlas, m_scene.get(), atlasID, WebCoordinatedSurface::create(handle)));
}

void CoordinatedLayerTreeHostProxy::removeUpdateAtlas(uint32_t atlasID)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::removeUpdateAtlas, m_scene.get(), atlasID));
}

void CoordinatedLayerTreeHostProxy::createCompositingLayers(const Vector<CoordinatedLayerID>& ids)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::createLayers, m_scene.get(), ids));
}

void CoordinatedLayerTreeHostProxy::deleteCompositingLayers(const Vector<CoordinatedLayerID>& ids)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::deleteLayers, m_scene.get(), ids));
}

void CoordinatedLayerTreeHostProxy::setRootCompositingLayer(CoordinatedLayerID id)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setRootLayerID, m_scene.get(), id));
}

void CoordinatedLayerTreeHostProxy::setCompositingLayerState(CoordinatedLayerID id, const CoordinatedLayerInfo& info)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setLayerState, m_scene.get(), id, info));
}

void CoordinatedLayerTreeHostProxy::setCompositingLayerChildren(CoordinatedLayerID id, const Vector<CoordinatedLayerID>& children)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setLayerChildren, m_scene.get(), id, children));
}

#if ENABLE(CSS_FILTERS)
void CoordinatedLayerTreeHostProxy::setCompositingLayerFilters(CoordinatedLayerID id, const FilterOperations& filters)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setLayerFilters, m_scene.get(), id, filters));
}
#endif

#if ENABLE(CSS_SHADERS)
void CoordinatedLayerTreeHostProxy::removeCustomFilterProgram(int id)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::removeCustomFilterProgram, m_scene.get(), id));
}
void CoordinatedLayerTreeHostProxy::createCustomFilterProgram(int id, const CustomFilterProgramInfo& programInfo)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::createCustomFilterProgram, m_scene.get(), id, programInfo));
}
#endif

void CoordinatedLayerTreeHostProxy::didRenderFrame(const IntSize& contentsSize, const IntRect& coveredRect)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::flushLayerChanges, m_scene.get()));
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
    dispatchUpdate(bind(&CoordinatedGraphicsScene::createImageBacking, m_scene.get(), imageID));
}

void CoordinatedLayerTreeHostProxy::updateImageBacking(CoordinatedImageBackingID imageID, const WebCoordinatedSurface::Handle& handle)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::updateImageBacking, m_scene.get(), imageID, WebCoordinatedSurface::create(handle)));
}

void CoordinatedLayerTreeHostProxy::clearImageBackingContents(CoordinatedImageBackingID imageID)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::clearImageBackingContents, m_scene.get(), imageID));
}

void CoordinatedLayerTreeHostProxy::removeImageBacking(CoordinatedImageBackingID imageID)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::removeImageBacking, m_scene.get(), imageID));
}

void CoordinatedLayerTreeHostProxy::setContentsSize(const FloatSize& contentsSize)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setContentsSize, m_scene.get(), contentsSize));
}

void CoordinatedLayerTreeHostProxy::setLayerAnimations(CoordinatedLayerID id, const GraphicsLayerAnimations& animations)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setLayerAnimations, m_scene.get(), id, animations));
}

void CoordinatedLayerTreeHostProxy::setAnimationsLocked(bool locked)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setAnimationsLocked, m_scene.get(), locked));
}

void CoordinatedLayerTreeHostProxy::setVisibleContentsRect(const FloatRect& rect, const FloatPoint& trajectoryVector)
{
    // Inform the renderer to adjust viewport-fixed layers.
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setVisibleContentsRect, m_scene.get(), rect));

    if (rect == m_lastSentVisibleRect && trajectoryVector == m_lastSentTrajectoryVector)
        return;

    m_drawingAreaProxy->page()->process()->send(Messages::CoordinatedLayerTreeHost::SetVisibleContentsRect(rect, trajectoryVector), m_drawingAreaProxy->page()->pageID());
    m_lastSentVisibleRect = rect;
    m_lastSentTrajectoryVector = trajectoryVector;
}

void CoordinatedLayerTreeHostProxy::renderNextFrame()
{
    m_drawingAreaProxy->page()->process()->send(Messages::CoordinatedLayerTreeHost::RenderNextFrame(), m_drawingAreaProxy->page()->pageID());
}

#if ENABLE(REQUEST_ANIMATION_FRAME)
void CoordinatedLayerTreeHostProxy::requestAnimationFrame()
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::requestAnimationFrame, m_scene.get()));
    updateViewport();
}

void CoordinatedLayerTreeHostProxy::animationFrameReady()
{
    m_drawingAreaProxy->page()->process()->send(Messages::CoordinatedLayerTreeHost::AnimationFrameReady(), m_drawingAreaProxy->page()->pageID());
}
#endif

void CoordinatedLayerTreeHostProxy::didChangeScrollPosition(const FloatPoint& position)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::didChangeScrollPosition, m_scene.get(), position));
}

#if USE(GRAPHICS_SURFACE)
void CoordinatedLayerTreeHostProxy::createCanvas(CoordinatedLayerID id, const IntSize& canvasSize, const GraphicsSurfaceToken& token)
{
    GraphicsSurface::Flags surfaceFlags = GraphicsSurface::SupportsTextureTarget | GraphicsSurface::SupportsSharing;
    dispatchUpdate(bind(&CoordinatedGraphicsScene::createCanvas, m_scene.get(), id, canvasSize, GraphicsSurface::create(canvasSize, surfaceFlags, token)));
}

void CoordinatedLayerTreeHostProxy::syncCanvas(CoordinatedLayerID id, uint32_t frontBuffer)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::syncCanvas, m_scene.get(), id, frontBuffer));
}

void CoordinatedLayerTreeHostProxy::destroyCanvas(CoordinatedLayerID id)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::destroyCanvas, m_scene.get(), id));
}
#endif

void CoordinatedLayerTreeHostProxy::setLayerRepaintCount(CoordinatedLayerID id, int value)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setLayerRepaintCount, m_scene.get(), id, value));
}

void CoordinatedLayerTreeHostProxy::purgeBackingStores()
{
    m_drawingAreaProxy->page()->process()->send(Messages::CoordinatedLayerTreeHost::PurgeBackingStores(), m_drawingAreaProxy->page()->pageID());
}

void CoordinatedLayerTreeHostProxy::setBackgroundColor(const Color& color)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setBackgroundColor, m_scene.get(), color));
}

}
#endif // USE(COORDINATED_GRAPHICS)
