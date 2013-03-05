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
#include <WebCore/CoordinatedGraphicsState.h>
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

void CoordinatedLayerTreeHostProxy::commitCoordinatedGraphicsState(const CoordinatedGraphicsState& graphicsState)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::commitSceneState, m_scene.get(), graphicsState));
    updateViewport();
#if USE(TILED_BACKING_STORE)
    m_drawingAreaProxy->page()->didRenderFrame(graphicsState.contentsSize, graphicsState.coveredRect);
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

void CoordinatedLayerTreeHostProxy::setAnimationsLocked(bool locked)
{
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setAnimationsLocked, m_scene.get(), locked));
}

void CoordinatedLayerTreeHostProxy::setVisibleContentsRect(const FloatRect& rect, const FloatPoint& trajectoryVector)
{
    // Inform the renderer to adjust viewport-fixed layers.
    dispatchUpdate(bind(&CoordinatedGraphicsScene::setScrollPosition, m_scene.get(), rect.location()));

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
