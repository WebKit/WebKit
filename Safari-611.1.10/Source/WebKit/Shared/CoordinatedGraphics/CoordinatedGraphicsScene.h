/*
    Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2013 Company 100, Inc.

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

#pragma once

#if USE(COORDINATED_GRAPHICS)

#include <WebCore/CoordinatedGraphicsState.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSize.h>
#include <WebCore/NicosiaPlatformLayer.h>
#include <WebCore/TextureMapper.h>
#include <WebCore/TextureMapperBackingStore.h>
#include <WebCore/TextureMapperFPSCounter.h>
#include <WebCore/TextureMapperLayer.h>
#include <WebCore/TextureMapperPlatformLayerProxy.h>
#include <WebCore/Timer.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/ThreadingPrimitives.h>
#include <wtf/Vector.h>

namespace Nicosia {
class Buffer;
}

namespace WebCore {
class CoordinatedBackingStore;
class TextureMapperGL;
}

namespace WebKit {

class CoordinatedGraphicsSceneClient {
public:
    virtual ~CoordinatedGraphicsSceneClient() { }
    virtual void updateViewport() = 0;
};

class CoordinatedGraphicsScene : public ThreadSafeRefCounted<CoordinatedGraphicsScene>, public WebCore::TextureMapperPlatformLayerProxy::Compositor {
public:
    explicit CoordinatedGraphicsScene(CoordinatedGraphicsSceneClient*);
    virtual ~CoordinatedGraphicsScene();

    void applyStateChanges(const Vector<WebCore::CoordinatedGraphicsState>&);
    void paintToCurrentGLContext(const WebCore::TransformationMatrix&, const WebCore::FloatRect&, WebCore::TextureMapper::PaintFlags = 0);
    void detach();

    // The painting thread must lock the main thread to use below two methods, because two methods access members that the main thread manages. See m_client.
    // Currently, QQuickWebPage::updatePaintNode() locks the main thread before calling both methods.
    void purgeGLResources();

    bool isActive() const { return m_isActive; }
    void setActive(bool active) { m_isActive = active; }

private:
    void commitSceneState(const WebCore::CoordinatedGraphicsState::NicosiaState&);
    void updateSceneState();

    WebCore::TextureMapperLayer* rootLayer() { return m_rootLayer.get(); }

    void updateViewport();

    void ensureRootLayer();

    void onNewBufferAvailable() override;

    struct {
        RefPtr<Nicosia::Scene> scene;
        Nicosia::Scene::State state;
    } m_nicosia;

    std::unique_ptr<WebCore::TextureMapper> m_textureMapper;

    // Below two members are accessed by only the main thread. The painting thread must lock the main thread to access both members.
    CoordinatedGraphicsSceneClient* m_client;
    bool m_isActive { false };

    std::unique_ptr<WebCore::TextureMapperLayer> m_rootLayer;

    Nicosia::PlatformLayer::LayerID m_rootLayerID { 0 };

    WebCore::TextureMapperFPSCounter m_fpsCounter;
};

} // namespace WebKit

#endif // USE(COORDINATED_GRAPHICS)


