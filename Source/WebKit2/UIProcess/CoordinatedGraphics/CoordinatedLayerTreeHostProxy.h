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

#ifndef CoordinatedLayerTreeHostProxy_h
#define CoordinatedLayerTreeHostProxy_h

#if USE(COORDINATED_GRAPHICS)

#include "BackingStore.h"
#include "CoordinatedGraphicsArgumentCoders.h"
#include "DrawingAreaProxy.h"
#include "Region.h"
#include "WebCoordinatedSurface.h"
#include <WebCore/CoordinatedGraphicsScene.h>
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
#include <wtf/Vector.h>

namespace WebCore {
class CoordinatedLayerInfo;
class SurfaceUpdateInfo;
}

namespace WebKit {

class CoordinatedLayerTreeHostProxy : public WebCore::CoordinatedGraphicsSceneClient, public CoreIPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(CoordinatedLayerTreeHostProxy);
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit CoordinatedLayerTreeHostProxy(DrawingAreaProxy*);
    virtual ~CoordinatedLayerTreeHostProxy();
    void setCompositingLayerState(WebCore::CoordinatedLayerID, const WebCore::CoordinatedLayerInfo&);
    void setCompositingLayerChildren(WebCore::CoordinatedLayerID, const Vector<WebCore::CoordinatedLayerID>&);
#if ENABLE(CSS_FILTERS)
    void setCompositingLayerFilters(WebCore::CoordinatedLayerID, const WebCore::FilterOperations&);
#endif
#if ENABLE(CSS_SHADERS)
    void createCustomFilterProgram(int id, const WebCore::CustomFilterProgramInfo&);
    void removeCustomFilterProgram(int id);
#endif
    void createCompositingLayers(const Vector<WebCore::CoordinatedLayerID>&);
    void deleteCompositingLayers(const Vector<WebCore::CoordinatedLayerID>&);
    void setRootCompositingLayer(WebCore::CoordinatedLayerID);
    void setContentsSize(const WebCore::FloatSize&);
    void setVisibleContentsRect(const WebCore::FloatRect&, const WebCore::FloatPoint& trajectoryVector);
    void didRenderFrame(const WebCore::FloatPoint& scrollPosition, const WebCore::IntSize& contentsSize, const WebCore::IntRect& coveredRect);
    void createTileForLayer(WebCore::CoordinatedLayerID, uint32_t tileID, const WebCore::IntRect&, const WebCore::SurfaceUpdateInfo&);
    void updateTileForLayer(WebCore::CoordinatedLayerID, uint32_t tileID, const WebCore::IntRect&, const WebCore::SurfaceUpdateInfo&);
    void removeTileForLayer(WebCore::CoordinatedLayerID, uint32_t tileID);
    void createUpdateAtlas(uint32_t atlasID, const WebCoordinatedSurface::Handle&);
    void removeUpdateAtlas(uint32_t atlasID);
    void createImageBacking(WebCore::CoordinatedImageBackingID);
    void updateImageBacking(WebCore::CoordinatedImageBackingID, const WebCoordinatedSurface::Handle&);
    void clearImageBackingContents(WebCore::CoordinatedImageBackingID);
    void removeImageBacking(WebCore::CoordinatedImageBackingID);
#if USE(GRAPHICS_SURFACE)
    void createCanvas(WebCore::CoordinatedLayerID, const WebCore::IntSize&, const WebCore::GraphicsSurfaceToken&);
    void syncCanvas(WebCore::CoordinatedLayerID, uint32_t frontBuffer);
    void destroyCanvas(WebCore::CoordinatedLayerID);
#endif
    void setLayerRepaintCount(WebCore::CoordinatedLayerID, int value);
    WebCore::CoordinatedGraphicsScene* coordinatedGraphicsScene() const { return m_scene.get(); }
    void setLayerAnimations(WebCore::CoordinatedLayerID, const WebCore::GraphicsLayerAnimations&);
    void setAnimationsLocked(bool);
#if ENABLE(REQUEST_ANIMATION_FRAME)
    void requestAnimationFrame();
#endif
    void setBackgroundColor(const WebCore::Color&);

    // CoordinatedGraphicsSceneClient Methods.
#if ENABLE(REQUEST_ANIMATION_FRAME)
    virtual void animationFrameReady() OVERRIDE;
#endif
    virtual void updateViewport() OVERRIDE;
    virtual void renderNextFrame() OVERRIDE;
    virtual void purgeBackingStores() OVERRIDE;

protected:
    void dispatchUpdate(const Function<void()>&);

    // CoreIPC::MessageReceiver
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageDecoder&) OVERRIDE;

    DrawingAreaProxy* m_drawingAreaProxy;
    RefPtr<WebCore::CoordinatedGraphicsScene> m_scene;
    WebCore::FloatRect m_lastSentVisibleRect;
    WebCore::FloatPoint m_lastSentTrajectoryVector;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)

#endif // CoordinatedLayerTreeHostProxy_h
