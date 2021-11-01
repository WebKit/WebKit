/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WCScene.h"

#if USE(GRAPHICS_LAYER_WC)

#include "RemoteGraphicsContextGL.h"
#include "WCSceneContext.h"
#include "WCUpateInfo.h"
#include <WebCore/GraphicsContextGLOpenGL.h>
#include <WebCore/TextureMapperLayer.h>
#include <WebCore/TextureMapperTiledBackingStore.h>

namespace WebKit {

struct WCScene::Layer : public WebCore::TextureMapperPlatformLayer::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Layer() = default;

    // TextureMapperPlatformLayer::Client
    void platformLayerWillBeDestroyed() override
    {
        texmapLayer.setContentsLayer(nullptr);
    }
    void setPlatformLayerNeedsDisplay() override { }

    WebCore::TextureMapperLayer texmapLayer;
    RefPtr<WebCore::TextureMapperTiledBackingStore> backingStore;
    std::unique_ptr<WebCore::TextureMapperLayer> backdropLayer;
};

void WCScene::initialize(WCSceneContext& context)
{
    // The creation of the TextureMapper needs an active OpenGL context.
    m_context = &context;
    m_context->makeContextCurrent();
    m_textureMapper = m_context->createTextureMapper();
}

WCScene::WCScene() = default;

WCScene::~WCScene()
{
    m_context->makeContextCurrent();
    m_textureMapper = nullptr;
}

void WCScene::update(WCUpateInfo&& update, Vector<RefPtr<RemoteGraphicsContextGL>>&& remoteGCGL)
{
    if (!m_context->makeContextCurrent())
        return;

    for (auto id : update.addedLayers) {
        auto layer = makeUnique<Layer>();
        m_layers.add(id, WTFMove(layer));
    }

    auto remoteGCGLIterator = remoteGCGL.begin();

    for (auto& layerUpdate : update.changedLayers) {
        auto layer = m_layers.get(layerUpdate.id);
        if (layerUpdate.changes & WCLayerChange::Children) {
            layer->texmapLayer.setChildren(WTF::map(layerUpdate.children, [&](auto id) {
                return &m_layers.get(id)->texmapLayer;
            }));
        }
        if (layerUpdate.changes & WCLayerChange::MaskLayer) {
            WebCore::TextureMapperLayer* maskLayer = nullptr;
            if (layerUpdate.maskLayer)
                maskLayer = &m_layers.get(*layerUpdate.maskLayer)->texmapLayer;
            layer->texmapLayer.setMaskLayer(maskLayer);
        }
        if (layerUpdate.changes & WCLayerChange::ReplicaLayer) {
            WebCore::TextureMapperLayer* replicaLayer = nullptr;
            if (layerUpdate.replicaLayer)
                replicaLayer = &m_layers.get(*layerUpdate.replicaLayer)->texmapLayer;
            layer->texmapLayer.setReplicaLayer(replicaLayer);
        }
        if (layerUpdate.changes & WCLayerChange::Geometry) {
            layer->texmapLayer.setPosition(layerUpdate.position);
            layer->texmapLayer.setAnchorPoint(layerUpdate.anchorPoint);
            layer->texmapLayer.setSize(layerUpdate.size);
            layer->texmapLayer.setBoundsOrigin(layerUpdate.boundsOrigin);
        }
        if (layerUpdate.changes & WCLayerChange::Preserves3D)
            layer->texmapLayer.setPreserves3D(layerUpdate.preserves3D);
        if (layerUpdate.changes & WCLayerChange::ContentsRect)
            layer->texmapLayer.setContentsRect(layerUpdate.contentsRect);
        if (layerUpdate.changes & WCLayerChange::ContentsClippingRect)
            layer->texmapLayer.setContentsClippingRect(layerUpdate.contentsClippingRect);
        if (layerUpdate.changes & WCLayerChange::ContentsVisible)
            layer->texmapLayer.setContentsVisible(layerUpdate.contentsVisible);
        if (layerUpdate.changes & WCLayerChange::BackfaceVisibility)
            layer->texmapLayer.setBackfaceVisibility(layerUpdate.backfaceVisibility);
        if (layerUpdate.changes & WCLayerChange::MasksToBounds)
            layer->texmapLayer.setMasksToBounds(layerUpdate.masksToBounds);
        if (layerUpdate.changes & WCLayerChange::BackingStore) {
            auto bitmap = layerUpdate.backingStore.bitmap();
            if (bitmap) {
                layer->backingStore = WebCore::TextureMapperTiledBackingStore::create();
                auto image = bitmap->createImage();
                layer->backingStore->updateContents(*m_textureMapper, image.get(), bitmap->size(), { { }, bitmap->size() });
                layer->texmapLayer.setBackingStore(layer->backingStore.get());
            } else {
                layer->texmapLayer.setBackingStore(nullptr);
                layer->backingStore = nullptr;
            }
        }
        if (layerUpdate.changes & WCLayerChange::SolidColor)
            layer->texmapLayer.setSolidColor(layerUpdate.solidColor);
        if (layerUpdate.changes & WCLayerChange::DebugVisuals)
            layer->texmapLayer.setDebugVisuals(layerUpdate.showDebugBorder, layerUpdate.debugBorderColor, layerUpdate.debugBorderWidth);
        if (layerUpdate.changes & WCLayerChange::RepaintCount)
            layer->texmapLayer.setRepaintCounter(layerUpdate.showRepaintCounter, layerUpdate.repaintCount);
        if (layerUpdate.changes & WCLayerChange::Opacity)
            layer->texmapLayer.setOpacity(layerUpdate.opacity);
        if (layerUpdate.changes & WCLayerChange::Transform)
            layer->texmapLayer.setTransform(layerUpdate.transform);
        if (layerUpdate.changes & WCLayerChange::ChildrenTransform)
            layer->texmapLayer.setChildrenTransform(layerUpdate.childrenTransform);
        if (layerUpdate.changes & WCLayerChange::Filters)
            layer->texmapLayer.setFilters(layerUpdate.filters);
        if (layerUpdate.changes & WCLayerChange::BackdropFilters) {
            if (layerUpdate.backdropFilters.isEmpty())
                layer->backdropLayer.reset();
            else {
                if (!layer->backdropLayer) {
                    layer->backdropLayer = makeUnique<WebCore::TextureMapperLayer>();
                    layer->backdropLayer->setAnchorPoint({ });
                    layer->backdropLayer->setContentsVisible(true);
                    layer->backdropLayer->setMasksToBounds(true);
                }
                layer->backdropLayer->setFilters(layerUpdate.backdropFilters);
                layer->backdropLayer->setSize(layerUpdate.backdropFiltersRect.rect().size());
                layer->backdropLayer->setPosition(layerUpdate.backdropFiltersRect.rect().location());
            }
            layer->texmapLayer.setBackdropLayer(layer->backdropLayer.get());
            layer->texmapLayer.setBackdropFiltersRect(layerUpdate.backdropFiltersRect);
        }
        if (layerUpdate.changes & WCLayerChange::PlatformLayer) {
            if (*remoteGCGLIterator) {
                auto platformLayer = (*remoteGCGLIterator)->platformLayer();
                platformLayer->setClient(layer);
                layer->texmapLayer.setContentsLayer(platformLayer);
            } else
                layer->texmapLayer.setContentsLayer(nullptr);
        }
        remoteGCGLIterator++;
    }
    ASSERT(remoteGCGLIterator == remoteGCGL.end());

    for (auto id : update.removedLayers)
        m_layers.remove(id);

    auto rootLayer = &m_layers.get(update.rootLayer)->texmapLayer;
    rootLayer->applyAnimationsRecursively(MonotonicTime::now());

    WebCore::IntSize windowSize = expandedIntSize(rootLayer->size());
    glViewport(0, 0, windowSize.width(), windowSize.height());

    m_textureMapper->beginPainting();
    rootLayer->paint(*m_textureMapper);
    m_fpsCounter.updateFPSAndDisplay(*m_textureMapper);
    m_textureMapper->endPainting();

    m_context->swapBuffers();
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
