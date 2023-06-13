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
#include "WCContentBuffer.h"
#include "WCContentBufferManager.h"
#include "WCRemoteFrameHostLayerManager.h"
#include "WCSceneContext.h"
#include "WCUpdateInfo.h"
#include <WebCore/TextureMapperGLHeaders.h>
#include <WebCore/TextureMapperLayer.h>
#include <WebCore/TextureMapperPlatformLayer.h>
#include <WebCore/TextureMapperSparseBackingStore.h>

namespace WebKit {

struct WCScene::Layer final : public WCContentBuffer::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Layer() = default;
    ~Layer()
    {
        if (contentBuffer)
            contentBuffer->setClient(nullptr);
        if (hostIdentifier)
            WCRemoteFrameHostLayerManager::singleton().releaseRemoteFrameHostLayer(*hostIdentifier);
    }

    // WCContentBuffer::Client
    void platformLayerWillBeDestroyed() override
    {
        contentBuffer = nullptr;
        texmapLayer.setContentsLayer(nullptr);
    }

    WebCore::TextureMapperLayer texmapLayer;
    std::unique_ptr<WebCore::TextureMapperSparseBackingStore> backingStore;
    std::unique_ptr<WebCore::TextureMapperLayer> backdropLayer;
    WCContentBuffer* contentBuffer { nullptr };
    Markable<WebCore::LayerHostingContextIdentifier> hostIdentifier;
};

void WCScene::initialize(WCSceneContext& context)
{
    // The creation of the TextureMapper needs an active OpenGL context.
    m_context = &context;
    m_context->makeContextCurrent();
    m_textureMapper = m_context->createTextureMapper();
}

WCScene::WCScene(WebCore::ProcessIdentifier webProcessIdentifier, bool usesOffscreenRendering)
    : m_webProcessIdentifier(webProcessIdentifier)
    , m_usesOffscreenRendering(usesOffscreenRendering)
{
}

WCScene::~WCScene()
{
    m_context->makeContextCurrent();
    m_textureMapper = nullptr;
}

std::optional<UpdateInfo> WCScene::update(WCUpdateInfo&& update)
{
    if (!m_context->makeContextCurrent())
        return std::nullopt;
    m_textureMapper->releaseUnusedTexturesNow();

    for (auto id : update.addedLayers) {
        auto layer = makeUnique<Layer>();
        m_layers.add(id, WTFMove(layer));
    }

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
        if (layerUpdate.changes & WCLayerChange::ContentsClippingRect) {
            layer->texmapLayer.setContentsClippingRect(layerUpdate.contentsClippingRect);
            layer->texmapLayer.setContentsRectClipsDescendants(layerUpdate.contentsRectClipsDescendants);
        }
        if (layerUpdate.changes & WCLayerChange::ContentsVisible)
            layer->texmapLayer.setContentsVisible(layerUpdate.contentsVisible);
        if (layerUpdate.changes & WCLayerChange::BackfaceVisibility)
            layer->texmapLayer.setBackfaceVisibility(layerUpdate.backfaceVisibility);
        if (layerUpdate.changes & WCLayerChange::MasksToBounds)
            layer->texmapLayer.setMasksToBounds(layerUpdate.masksToBounds);
        if (layerUpdate.changes & WCLayerChange::Background) {
            if (layerUpdate.hasBackingStore) {
                if (!layer->backingStore) {
                    layer->backingStore = makeUnique<WebCore::TextureMapperSparseBackingStore>();
                    auto& backingStore = *layer->backingStore;
                    layer->texmapLayer.setBackgroundColor({ });
                    layer->texmapLayer.setBackingStore(&backingStore);
                }
                auto& backingStore = *layer->backingStore;
                backingStore.setSize(WebCore::IntSize(layer->texmapLayer.size()));
                for (auto& tileUpdate : layerUpdate.tileUpdate) {
                    if (tileUpdate.willRemove)
                        backingStore.removeTile(tileUpdate.index);
                    else {
                        auto bitmap = tileUpdate.backingStore.bitmap();
                        if (bitmap) {
                            auto image = bitmap->createImage();
                            backingStore.updateContents(*m_textureMapper, tileUpdate.index, *image, tileUpdate.dirtyRect);
                        }
                    }
                }
            } else {
                layer->texmapLayer.setBackgroundColor(layerUpdate.backgroundColor);
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
            if (!layerUpdate.hasPlatformLayer) {
                if (layer->contentBuffer) {
                    layer->contentBuffer->setClient(nullptr);
                    layer->contentBuffer = nullptr;
                }
                layer->texmapLayer.setContentsLayer(nullptr);
            } else {
                WCContentBuffer* contentBuffer = nullptr;
                for (auto identifier : layerUpdate.contentBufferIdentifiers)
                    contentBuffer = WCContentBufferManager::singleton().releaseContentBufferIdentifier(m_webProcessIdentifier, identifier);
                if (contentBuffer) {
                    if (layer->contentBuffer)
                        layer->contentBuffer->setClient(nullptr);
                    contentBuffer->setClient(layer);
                    layer->contentBuffer = contentBuffer;
                    layer->texmapLayer.setContentsLayer(contentBuffer->platformLayer());
                }
            }
        }
        if (layerUpdate.changes & WCLayerChange::RemoteFrame) {
            if (layerUpdate.hostIdentifier) {
                auto platformLayer = WCRemoteFrameHostLayerManager::singleton().acquireRemoteFrameHostLayer(*layerUpdate.hostIdentifier, m_webProcessIdentifier);
                layer->texmapLayer.setContentsLayer(platformLayer);
            } else {
                ASSERT(layer->hostIdentifier);
                WCRemoteFrameHostLayerManager::singleton().releaseRemoteFrameHostLayer(*layer->hostIdentifier);
            }
            layer->hostIdentifier = layerUpdate.hostIdentifier;
        }
    }

    for (auto id : update.removedLayers)
        m_layers.remove(id);

    auto rootLayer = &m_layers.get(update.rootLayer)->texmapLayer;
    rootLayer->applyAnimationsRecursively(MonotonicTime::now());

    WebCore::IntSize windowSize = expandedIntSize(rootLayer->size());
    if (windowSize.isEmpty())
        return std::nullopt;

    WebCore::BitmapTexture* surface = nullptr;
    RefPtr<WebCore::BitmapTexture> texture;
    bool showFPS = true;
    bool readPixel = false;
    RefPtr<ShareableBitmap> bitmap;

    if (update.remoteContextHostedIdentifier) {
        showFPS = false;
        texture = m_textureMapper->acquireTextureFromPool(windowSize);
        surface = texture.get();
    } else if (m_usesOffscreenRendering) {
        readPixel = true;
        texture = m_textureMapper->acquireTextureFromPool(windowSize);
        surface = texture.get();
    } else
        glViewport(0, 0, windowSize.width(), windowSize.height());

    m_textureMapper->beginPainting(0, surface);
    rootLayer->paint(*m_textureMapper);
    if (showFPS)
        m_fpsCounter.updateFPSAndDisplay(*m_textureMapper);
    if (readPixel) {
        bitmap = ShareableBitmap::create({ windowSize });
        glReadPixels(0, 0, windowSize.width(), windowSize.height(), GL_BGRA, GL_UNSIGNED_BYTE, bitmap->data());
    }
    m_textureMapper->endPainting();

    std::optional<UpdateInfo> result;
    if (update.remoteContextHostedIdentifier)
        WCRemoteFrameHostLayerManager::singleton().updateTexture(*update.remoteContextHostedIdentifier, m_webProcessIdentifier, WTFMove(texture));
    else if (m_usesOffscreenRendering) {
        if (auto handle = bitmap->createHandle()) {
            result.emplace();
            result->viewSize = windowSize;
            result->deviceScaleFactor = 1;
            result->updateScaleFactor = 1;
            WebCore::IntRect viewport = { { }, windowSize };
            result->updateRectBounds = viewport;
            result->updateRects.append(viewport);
            result->bitmapHandle = WTFMove(*handle);
        }
    } else
        m_context->swapBuffers();
    return result;
}

} // namespace WebKit

#endif // USE(GRAPHICS_LAYER_WC)
