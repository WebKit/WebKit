/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "TiledLayerChromium.h"

#include "CCLayerTreeTestCommon.h"
#include "FakeCCLayerTreeHostClient.h"
#include "LayerTextureUpdater.h"
#include "Region.h"
#include "TextureManager.h"
#include "WebCompositor.h"
#include "cc/CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include "cc/CCTextureUpdater.h"
#include "cc/CCTiledLayerImpl.h"
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WTF;

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.width(), b.width()); \
    EXPECT_EQ(a.height(), b.height());

namespace {

class TestCCOcclusionTracker : public CCOcclusionTracker {
public:
    TestCCOcclusionTracker()
        : CCOcclusionTracker(IntRect(0, 0, 1000, 1000), true)
        , m_scissorRectInScreen(IntRect(0, 0, 1000, 1000))
    {
        // Pretend we have visited a render surface.
        m_stack.append(StackObject());
    }

    void setOcclusion(const Region& occlusion) { m_stack.last().occlusionInScreen = occlusion; }

protected:
    virtual IntRect layerScissorRectInTargetSurface(const LayerChromium* layer) const { return m_scissorRectInScreen; }

private:
    IntRect m_scissorRectInScreen;
};

class FakeTextureAllocator : public TextureAllocator {
public:
    virtual unsigned createTexture(const IntSize&, GC3Denum) { return 0; }
    virtual void deleteTexture(unsigned, const IntSize&, GC3Denum) { }
};

class FakeTiledLayerChromium;

class FakeLayerTextureUpdater : public LayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(FakeLayerTextureUpdater* layer, PassOwnPtr<ManagedTexture> texture)
            : LayerTextureUpdater::Texture(texture)
            , m_layer(layer)
        {
        }
        virtual ~Texture() { }

        virtual void updateRect(GraphicsContext3D*, TextureAllocator*, const IntRect&, const IntRect&) { m_layer->updateRect(); }
        virtual void prepareRect(const IntRect&) { m_layer->prepareRect(); }

    private:
        FakeLayerTextureUpdater* m_layer;
    };

    FakeLayerTextureUpdater()
        : m_prepareCount(0)
        , m_updateCount(0)
        , m_prepareRectCount(0)
    {
    }
    virtual ~FakeLayerTextureUpdater() { }

    // Sets the rect to invalidate during the next call to prepareToUpdate(). After the next
    // call to prepareToUpdate() the rect is reset.
    void setRectToInvalidate(const IntRect&, FakeTiledLayerChromium*);

    // Number of times prepareToUpdate has been invoked.
    int prepareCount() const { return m_prepareCount; }
    void clearPrepareCount() { m_prepareCount = 0; }

    // Number of times updateRect has been invoked.
    int updateCount() const { return m_updateCount; }
    void clearUpdateCount() { m_updateCount = 0; }
    void updateRect() { m_updateCount++; }

    // Number of times prepareRect() has been invoked on a texture.
    int prepareRectCount() const { return m_prepareRectCount; }
    void clearPrepareRectCount() { m_prepareRectCount = 0; }
    void prepareRect() { m_prepareRectCount++; }

    void setOpaquePaintRect(const IntRect& opaquePaintRect) { m_opaquePaintRect = opaquePaintRect; }

    // Last rect passed to prepareToUpdate().
    const IntRect& lastUpdateRect()  const { return m_lastUpdateRect; }

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(TextureManager* manager) { return adoptPtr(new Texture(this, ManagedTexture::create(manager))); }
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum) { return SampledTexelFormatRGBA; }
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize&, int, float, IntRect* resultingOpaqueRect);

private:
    int m_prepareCount;
    int m_updateCount;
    int m_prepareRectCount;
    IntRect m_rectToInvalidate;
    IntRect m_lastUpdateRect;
    IntRect m_opaquePaintRect;
    RefPtr<FakeTiledLayerChromium> m_layer;
};

class FakeCCTiledLayerImpl : public CCTiledLayerImpl {
public:
    explicit FakeCCTiledLayerImpl(int id)
        : CCTiledLayerImpl(id) { }
    virtual ~FakeCCTiledLayerImpl() { }

    bool hasTileAt(int i, int j)
    {
        return CCTiledLayerImpl::hasTileAt(i, j);
    }
};

class FakeTiledLayerChromium : public TiledLayerChromium {
public:
    explicit FakeTiledLayerChromium(TextureManager* textureManager)
        : TiledLayerChromium()
        , m_fakeTextureUpdater(adoptRef(new FakeLayerTextureUpdater))
        , m_textureManager(textureManager)
    {
        setTileSize(IntSize(100, 100));
        setTextureFormat(GraphicsContext3D::RGBA);
        setBorderTexelOption(CCLayerTilingData::NoBorderTexels);
        setIsDrawable(true); // So that we don't get false positives if any of these tests expect to return false from drawsContent() for other reasons.
    }
    virtual ~FakeTiledLayerChromium() { }

    void invalidateRect(const IntRect& rect)
    {
        TiledLayerChromium::invalidateRect(rect);
    }

    void prepareToUpdate(const IntRect& rect, const CCOcclusionTracker* occlusion)
    {
        TiledLayerChromium::prepareToUpdate(rect, occlusion);
    }

    void prepareToUpdateIdle(const IntRect& rect, const CCOcclusionTracker* occlusion)
    {
        TiledLayerChromium::prepareToUpdateIdle(rect, occlusion);
    }

    bool needsIdlePaint(const IntRect& rect)
    {
        return TiledLayerChromium::needsIdlePaint(rect);
    }

    bool skipsDraw() const
    {
        return TiledLayerChromium::skipsDraw();
    }

    virtual void setNeedsDisplayRect(const FloatRect& rect)
    {
        m_lastNeedsDisplayRect = rect;
        TiledLayerChromium::setNeedsDisplayRect(rect);
    }

    const FloatRect& lastNeedsDisplayRect() const { return m_lastNeedsDisplayRect; }

    FakeLayerTextureUpdater* fakeLayerTextureUpdater() { return m_fakeTextureUpdater.get(); }

    virtual TextureManager* textureManager() const { return m_textureManager; }

    virtual void paintContentsIfDirty(const CCOcclusionTracker* occlusion)
    {
        prepareToUpdate(visibleLayerRect(), occlusion);
    }

private:
    virtual LayerTextureUpdater* textureUpdater() const
    {
        return m_fakeTextureUpdater.get();
    }

    virtual void createTextureUpdaterIfNeeded() { }

    RefPtr<FakeLayerTextureUpdater> m_fakeTextureUpdater;
    TextureManager* m_textureManager;
    FloatRect m_lastNeedsDisplayRect;
};

class FakeTiledLayerWithScaledBounds : public FakeTiledLayerChromium {
public:
    explicit FakeTiledLayerWithScaledBounds(TextureManager* textureManager)
        : FakeTiledLayerChromium(textureManager)
    {
    }

    void setContentBounds(const IntSize& contentBounds) { m_forcedContentBounds = contentBounds; }
    virtual IntSize contentBounds() const { return m_forcedContentBounds; }

    FloatRect updateRect() { return m_updateRect; }

protected:
    IntSize m_forcedContentBounds;
};

void FakeLayerTextureUpdater::setRectToInvalidate(const IntRect& rect, FakeTiledLayerChromium* layer)
{
    m_rectToInvalidate = rect;
    m_layer = layer;
}

void FakeLayerTextureUpdater::prepareToUpdate(const IntRect& contentRect, const IntSize&, int, float, IntRect* resultingOpaqueRect)
{
    m_prepareCount++;
    m_lastUpdateRect = contentRect;
    if (!m_rectToInvalidate.isEmpty()) {
        m_layer->invalidateRect(m_rectToInvalidate);
        m_rectToInvalidate = IntRect();
        m_layer = 0;
    }
    *resultingOpaqueRect = m_opaquePaintRect;
}

TEST(TiledLayerChromiumTest, pushDirtyTiles)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->prepareToUpdate(IntRect(0, 0, 100, 200), 0);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->unprotectAllTextures();

    // Invalidates both tiles...
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    // ....but then only update one of them.
    layer->prepareToUpdate(IntRect(0, 0, 100, 100), 0);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should only have the first tile since the other tile was invalidated but not painted.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));
}

TEST(TiledLayerChromiumTest, pushOccludedDirtyTiles)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));
    TestCCOcclusionTracker occluded;

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->prepareToUpdate(IntRect(0, 0, 100, 200), &occluded);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 20000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 0, 1);

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->unprotectAllTextures();

    // Invalidates part of the top tile...
    layer->invalidateRect(IntRect(0, 0, 50, 50));
    // ....but the area is occluded.
    occluded.setOcclusion(IntRect(0, 0, 50, 50));
    layer->prepareToUpdate(IntRect(0, 0, 100, 100), &occluded);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 20000 + 2500, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 0, 1);

    // We should still have both tiles, as part of the top tile is still unoccluded.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));
}

TEST(TiledLayerChromiumTest, pushDeletedTiles)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->prepareToUpdate(IntRect(0, 0, 100, 200), 0);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->evictAndDeleteAllTextures(&textureAllocator);
    textureManager->setMaxMemoryLimitBytes(4*1024*1024);
    textureManager->setPreferredMemoryLimitBytes(4*1024*1024);

    // This should drop the tiles on the impl thread.
    layer->pushPropertiesTo(layerImpl.get());

    // We should now have no textures on the impl thread.
    EXPECT_FALSE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));

    // This should recreate and update the deleted textures.
    layer->prepareToUpdate(IntRect(0, 0, 100, 100), 0);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should only have the first tile since the other tile was invalidated but not painted.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));
}

TEST(TiledLayerChromiumTest, pushIdlePaintTiles)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // The tile size is 100x100. Setup 5x5 tiles with one visible tile in the center.
    IntSize contentBounds(500, 500);
    IntRect contentRect(IntPoint::zero(), contentBounds);
    IntRect visibleRect(200, 200, 100, 100);

    // This invalidates 25 tiles and then paints one visible tile.
    layer->setBounds(contentBounds);
    layer->setVisibleLayerRect(visibleRect);
    layer->invalidateRect(contentRect);
    layer->prepareToUpdate(visibleRect, 0);

    // We should need idle-painting for 3x3 tiles in the center.
    EXPECT_TRUE(layer->needsIdlePaint(visibleRect));

    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(2, 2));

    textureManager->unprotectAllTextures();

    // For the next four updates, we should detect we still need idle painting.
    for (int i = 0; i < 4; i++) {
        layer->prepareToUpdate(visibleRect, 0);
        EXPECT_TRUE(layer->needsIdlePaint(visibleRect));
        layer->prepareToUpdateIdle(visibleRect, 0);
        layer->updateCompositorResources(0, updater);
        layer->pushPropertiesTo(layerImpl.get());
        textureManager->unprotectAllTextures();
    }

    // After four passes of idle painting, we should be finished painting
    EXPECT_FALSE(layer->needsIdlePaint(visibleRect));

    // We should have one tile surrounding the visible tile on all sides, but no other tiles.
    IntRect idlePaintTiles(1, 1, 3, 3);
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (idlePaintTiles.contains(i, j))
                EXPECT_TRUE(layerImpl->hasTileAt(i, j));
            else
                EXPECT_FALSE(layerImpl->hasTileAt(i, j));
        }
    }
}

TEST(TiledLayerChromiumTest, pushIdlePaintedOccludedTiles)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));
    TestCCOcclusionTracker occluded;

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // The tile size is 100x100, so this invalidates one occluded tile, culls it during paint, but prepaints it.
    occluded.setOcclusion(IntRect(0, 0, 100, 100));

    layer->setBounds(IntSize(100, 100));
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 100));
    layer->invalidateRect(IntRect(0, 0, 100, 100));
    layer->prepareToUpdate(IntRect(0, 0, 100, 100), &occluded);
    layer->prepareToUpdateIdle(IntRect(0, 0, 100, 100), &occluded);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have the prepainted tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
}

TEST(TiledLayerChromiumTest, pushTilesMarkedDirtyDuringPaint)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    // However, during the paint, we invalidate one of the tiles. This should
    // not prevent the tile from being pushed.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(0, 50, 100, 50), layer.get());
    layer->prepareToUpdate(IntRect(0, 0, 100, 200), 0);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));
}

TEST(TiledLayerChromiumTest, pushTilesLayerMarkedDirtyDuringPaintOnNextLayer)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer1 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    RefPtr<FakeTiledLayerChromium> layer2 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layer1Impl(adoptPtr(new FakeCCTiledLayerImpl(0)));
    OwnPtr<FakeCCTiledLayerImpl> layer2Impl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    layer1->setBounds(IntSize(100, 200));
    layer1->invalidateRect(IntRect(0, 0, 100, 200));
    layer2->setBounds(IntSize(100, 200));
    layer2->invalidateRect(IntRect(0, 0, 100, 200));

    layer1->prepareToUpdate(IntRect(0, 0, 100, 200), 0);

    // Invalidate a tile on layer1
    layer2->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(0, 50, 100, 50), layer1.get());
    layer2->prepareToUpdate(IntRect(0, 0, 100, 200), 0);

    layer1->updateCompositorResources(0, updater);
    layer2->updateCompositorResources(0, updater);

    layer1->pushPropertiesTo(layer1Impl.get());
    layer2->pushPropertiesTo(layer2Impl.get());

    // We should have both tiles on the impl side for all layers.
    EXPECT_TRUE(layer1Impl->hasTileAt(0, 0));
    EXPECT_TRUE(layer1Impl->hasTileAt(0, 1));
    EXPECT_TRUE(layer2Impl->hasTileAt(0, 0));
    EXPECT_TRUE(layer2Impl->hasTileAt(0, 1));
}

TEST(TiledLayerChromiumTest, pushTilesLayerMarkedDirtyDuringPaintOnPreviousLayer)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer1 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    RefPtr<FakeTiledLayerChromium> layer2 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layer1Impl(adoptPtr(new FakeCCTiledLayerImpl(0)));
    OwnPtr<FakeCCTiledLayerImpl> layer2Impl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    layer1->setBounds(IntSize(100, 200));
    layer1->invalidateRect(IntRect(0, 0, 100, 200));
    layer2->setBounds(IntSize(100, 200));
    layer2->invalidateRect(IntRect(0, 0, 100, 200));

    // Invalidate a tile on layer2
    layer1->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(0, 50, 100, 50), layer2.get());
    layer1->prepareToUpdate(IntRect(0, 0, 100, 200), 0);

    layer2->prepareToUpdate(IntRect(0, 0, 100, 200), 0);

    layer1->updateCompositorResources(0, updater);
    layer2->updateCompositorResources(0, updater);

    layer1->pushPropertiesTo(layer1Impl.get());
    layer2->pushPropertiesTo(layer2Impl.get());

    // We should have both tiles on the impl side for all layers.
    EXPECT_TRUE(layer1Impl->hasTileAt(0, 0));
    EXPECT_TRUE(layer1Impl->hasTileAt(0, 1));
    EXPECT_TRUE(layer2Impl->hasTileAt(0, 0));
    EXPECT_TRUE(layer2Impl->hasTileAt(0, 1));
}

TEST(TiledLayerChromiumTest, idlePaintOutOfMemory)
{
    // The tile size is 100x100. Setup 5x5 tiles with one 1x1 visible tile in the center.
    IntSize contentBounds(300, 300);
    IntRect contentRect(IntPoint::zero(), contentBounds);
    IntRect visibleRect(100, 100, 100, 100);

    // We have enough memory for only the visible rect, so we will run out of memory in first idle paint.
    int memoryLimit = 4 * 100 * 100; // 2 tiles, 4 bytes per pixel.

    OwnPtr<TextureManager> textureManager = TextureManager::create(memoryLimit, memoryLimit / 2, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // This invalidates 9 tiles and then paints one visible tile.
    layer->setBounds(contentBounds);
    layer->setVisibleLayerRect(visibleRect);
    layer->invalidateRect(contentRect);
    layer->prepareToUpdate(visibleRect, 0);

    // We should need idle-painting for 3x3 tiles surounding visible tile.
    EXPECT_TRUE(layer->needsIdlePaint(visibleRect));

    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(1, 1));

    textureManager->unprotectAllTextures();
    layer->prepareToUpdate(visibleRect, 0);
    layer->prepareToUpdateIdle(visibleRect, 0);

    // We shouldn't signal we need another idle paint after we run out of memory.
    EXPECT_FALSE(layer->needsIdlePaint(visibleRect));

    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());
}

TEST(TiledLayerChromiumTest, invalidateFromPrepare)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->prepareToUpdate(IntRect(0, 0, 100, 200), 0);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->unprotectAllTextures();

    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    // Invoke prepareToUpdate again. As the layer is valid prepareToUpdate shouldn't be invoked on
    // the LayerTextureUpdater.
    layer->prepareToUpdate(IntRect(0, 0, 100, 200), 0);
    EXPECT_EQ(0, layer->fakeLayerTextureUpdater()->prepareCount());

    layer->invalidateRect(IntRect(0, 0, 50, 50));
    // setRectToInvalidate triggers invalidateRect() being invoked from prepareToUpdate.
    layer->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(25, 25, 50, 50), layer.get());
    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    layer->prepareToUpdate(IntRect(0, 0, 100, 200), 0);
    EXPECT_EQ(1, layer->fakeLayerTextureUpdater()->prepareCount());
    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    // The layer should still be invalid as prepareToUpdate invoked invalidate.
    layer->prepareToUpdate(IntRect(0, 0, 100, 200), 0);
    EXPECT_EQ(1, layer->fakeLayerTextureUpdater()->prepareCount());
}

TEST(TiledLayerChromiumTest, verifyUpdateRectWhenContentBoundsAreScaled)
{
    // The updateRect (that indicates what was actually painted) should be in
    // layer space, not the content space.

    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerWithScaledBounds> layer = adoptRef(new FakeTiledLayerWithScaledBounds(textureManager.get()));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    IntRect layerBounds(0, 0, 300, 200);
    IntRect contentBounds(0, 0, 200, 250);

    layer->setBounds(layerBounds.size());
    layer->setContentBounds(contentBounds.size());
    layer->setVisibleLayerRect(contentBounds);

    // On first update, the updateRect includes all tiles, even beyond the boundaries of the layer.
    // However, it should still be in layer space, not content space.
    layer->invalidateRect(contentBounds);
    layer->prepareToUpdate(contentBounds, 0);
    layer->updateCompositorResources(0, updater);
    EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 300, 300 * 0.8), layer->updateRect());

    // After the tiles are updated once, another invalidate only needs to update the bounds of the layer.
    layer->invalidateRect(contentBounds);
    layer->prepareToUpdate(contentBounds, 0);
    layer->updateCompositorResources(0, updater);
    EXPECT_FLOAT_RECT_EQ(FloatRect(layerBounds), layer->updateRect());

    // Partial re-paint should also be represented by the updateRect in layer space, not content space.
    IntRect partialDamage(30, 100, 10, 10);
    layer->invalidateRect(partialDamage);
    layer->prepareToUpdate(contentBounds, 0);
    layer->updateCompositorResources(0, updater);
    EXPECT_FLOAT_RECT_EQ(FloatRect(45, 80, 15, 8), layer->updateRect());
}

TEST(TiledLayerChromiumTest, verifyInvalidationWhenContentsScaleChanges)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // Create a layer with one tile.
    layer->setBounds(IntSize(100, 100));

    // Invalidate the entire layer.
    layer->setNeedsDisplay();
    EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 100, 100), layer->lastNeedsDisplayRect());

    // Push the tiles to the impl side and check that there is exactly one.
    layer->prepareToUpdate(IntRect(0, 0, 100, 100), 0);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));
    EXPECT_FALSE(layerImpl->hasTileAt(1, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(1, 1));

    // Change the contents scale and verify that the content rectangle requiring painting
    // is not scaled.
    layer->setContentsScale(2);
    EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 100, 100), layer->lastNeedsDisplayRect());

    // The impl side should get 2x2 tiles now.
    layer->prepareToUpdate(IntRect(0, 0, 200, 200), 0);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));
    EXPECT_TRUE(layerImpl->hasTileAt(1, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(1, 1));

    // Invalidate the entire layer again, but do not paint. All tiles should be gone now from the
    // impl side.
    layer->setNeedsDisplay();
    layer->prepareToUpdate(IntRect(1, 0, 0, 1), 0);
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());
    EXPECT_FALSE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));
    EXPECT_FALSE(layerImpl->hasTileAt(1, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(1, 1));
}

TEST(TiledLayerChromiumTest, skipsDrawGetsReset)
{
    // Initialize without threading support.
    WebKit::WebCompositor::initialize(0);
    FakeCCLayerTreeHostClient fakeCCLayerTreeHostClient;
    RefPtr<CCLayerTreeHost> ccLayerTreeHost = CCLayerTreeHost::create(&fakeCCLayerTreeHostClient, CCSettings());

    // Create two 300 x 300 tiled layers.
    IntSize contentBounds(300, 300);
    IntRect contentRect(IntPoint::zero(), contentBounds);

    // We have enough memory for only one of the two layers.
    int memoryLimit = 4 * 300 * 300; // 4 bytes per pixel.
    OwnPtr<TextureManager> textureManager = TextureManager::create(memoryLimit, memoryLimit, memoryLimit);

    RefPtr<FakeTiledLayerChromium> rootLayer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    RefPtr<FakeTiledLayerChromium> childLayer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    rootLayer->addChild(childLayer);

    rootLayer->setBounds(contentBounds);
    rootLayer->setPosition(FloatPoint(150, 150));
    childLayer->setBounds(contentBounds);
    childLayer->setPosition(FloatPoint(150, 150));
    rootLayer->invalidateRect(contentRect);
    childLayer->invalidateRect(contentRect);

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    ccLayerTreeHost->setRootLayer(rootLayer);
    ccLayerTreeHost->setViewportSize(IntSize(300, 300));
    textureManager->setMaxMemoryLimitBytes(memoryLimit);
    ccLayerTreeHost->updateLayers();
    ccLayerTreeHost->updateCompositorResources(ccLayerTreeHost->context(), updater);

    // We'll skip the root layer.
    EXPECT_TRUE(rootLayer->skipsDraw());
    EXPECT_FALSE(childLayer->skipsDraw());

    ccLayerTreeHost->commitComplete();
    textureManager->unprotectAllTextures(); // CCLayerTreeHost::commitComplete() normally does this, but since we're mocking out the manager we have to do it.

    // Remove the child layer.
    rootLayer->removeAllChildren();

    ccLayerTreeHost->updateLayers();
    EXPECT_FALSE(rootLayer->skipsDraw());

    ccLayerTreeHost->setRootLayer(0);
    ccLayerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

TEST(TiledLayerChromiumTest, resizeToSmaller)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(60*1024*1024, 60*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));

    layer->setBounds(IntSize(700, 700));
    layer->invalidateRect(IntRect(0, 0, 700, 700));
    layer->prepareToUpdate(IntRect(0, 0, 700, 700), 0);

    layer->setBounds(IntSize(200, 200));
    layer->invalidateRect(IntRect(0, 0, 200, 200));
}

TEST(TiledLayerChromiumTest, partialUpdates)
{
    CCSettings settings;
    settings.maxPartialTextureUpdates = 4;
    // Initialize without threading support.
    WebKit::WebCompositor::initialize(0);
    FakeCCLayerTreeHostClient fakeCCLayerTreeHostClient;
    RefPtr<CCLayerTreeHost> ccLayerTreeHost = CCLayerTreeHost::create(&fakeCCLayerTreeHostClient, settings);

    // Create one 500 x 300 tiled layer.
    IntSize contentBounds(300, 200);
    IntRect contentRect(IntPoint::zero(), contentBounds);

    OwnPtr<TextureManager> textureManager = TextureManager::create(60*1024*1024, 60*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    layer->setBounds(contentBounds);
    layer->setPosition(FloatPoint(150, 150));
    layer->invalidateRect(contentRect);

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    ccLayerTreeHost->setRootLayer(layer);
    ccLayerTreeHost->setViewportSize(IntSize(300, 200));

    // Full update of all 6 tiles.
    ccLayerTreeHost->updateLayers();
    ccLayerTreeHost->updateCompositorResources(ccLayerTreeHost->context(), updater);
    updater.update(0, 4);
    EXPECT_EQ(4, layer->fakeLayerTextureUpdater()->updateCount());
    EXPECT_TRUE(updater.hasMoreUpdates());
    layer->fakeLayerTextureUpdater()->clearUpdateCount();
    updater.update(0, 4);
    EXPECT_EQ(2, layer->fakeLayerTextureUpdater()->updateCount());
    EXPECT_FALSE(updater.hasMoreUpdates());
    layer->fakeLayerTextureUpdater()->clearUpdateCount();
    ccLayerTreeHost->commitComplete();

    // Full update of 3 tiles and partial update of 3 tiles.
    layer->invalidateRect(IntRect(0, 0, 300, 150));
    ccLayerTreeHost->updateLayers();
    ccLayerTreeHost->updateCompositorResources(ccLayerTreeHost->context(), updater);
    updater.update(0, 4);
    EXPECT_EQ(3, layer->fakeLayerTextureUpdater()->updateCount());
    EXPECT_TRUE(updater.hasMoreUpdates());
    layer->fakeLayerTextureUpdater()->clearUpdateCount();
    updater.update(0, 4);
    EXPECT_EQ(3, layer->fakeLayerTextureUpdater()->updateCount());
    EXPECT_FALSE(updater.hasMoreUpdates());
    layer->fakeLayerTextureUpdater()->clearUpdateCount();
    ccLayerTreeHost->commitComplete();

    // Partial update of 6 tiles.
    layer->invalidateRect(IntRect(50, 50, 200, 100));
    ccLayerTreeHost->updateLayers();
    ccLayerTreeHost->updateCompositorResources(ccLayerTreeHost->context(), updater);
    updater.update(0, 4);
    EXPECT_EQ(2, layer->fakeLayerTextureUpdater()->updateCount());
    EXPECT_TRUE(updater.hasMoreUpdates());
    layer->fakeLayerTextureUpdater()->clearUpdateCount();
    updater.update(0, 4);
    EXPECT_EQ(4, layer->fakeLayerTextureUpdater()->updateCount());
    EXPECT_FALSE(updater.hasMoreUpdates());
    layer->fakeLayerTextureUpdater()->clearUpdateCount();
    ccLayerTreeHost->commitComplete();

    ccLayerTreeHost->setRootLayer(0);
    ccLayerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

TEST(TiledLayerChromiumTest, tilesPaintedWithoutOcclusion)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));

    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->prepareToUpdate(IntRect(0, 0, 100, 200), 0);
    EXPECT_EQ(2, layer->fakeLayerTextureUpdater()->prepareRectCount());
}

TEST(TiledLayerChromiumTest, tilesPaintedWithOcclusion)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100.

    layer->setBounds(IntSize(600, 600));
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(200, 200, 300, 100));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36-3, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 330000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 30000, 1);

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    occluded.setOcclusion(IntRect(250, 200, 300, 100));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36-2, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 330000 + 340000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 30000 + 20000, 1);

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    occluded.setOcclusion(IntRect(250, 250, 300, 100));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 330000 + 340000 + 360000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 30000 + 20000, 1);
}

TEST(TiledLayerChromiumTest, tilesPaintedWithOcclusionAndVisiblityConstraints)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100.

    layer->setBounds(IntSize(600, 600));
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    // The partially occluded tiles (by the 150 occlusion height) are visible beyond the occlusion, so not culled.
    occluded.setOcclusion(IntRect(200, 200, 300, 150));
    layer->setVisibleLayerRect(IntRect(0, 0, 600, 360));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(24-3, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 210000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 30000, 1);

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    // Now the visible region stops at the edge of the occlusion so the partly visible tiles become fully occluded.
    occluded.setOcclusion(IntRect(200, 200, 300, 150));
    layer->setVisibleLayerRect(IntRect(0, 0, 600, 350));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(24-6, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 210000 + 180000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 30000 + 60000, 1);

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    // Now the visible region is even smaller than the occlusion, it should have the same result.
    occluded.setOcclusion(IntRect(200, 200, 300, 150));
    layer->setVisibleLayerRect(IntRect(0, 0, 600, 340));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(24-6, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 210000 + 180000 + 180000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 30000 + 60000 + 60000, 1);

}

TEST(TiledLayerChromiumTest, tilesNotPaintedWithoutInvalidation)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100.

    layer->setBounds(IntSize(600, 600));
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(200, 200, 300, 100));
    layer->setVisibleLayerRect(IntRect(0, 0, 600, 600));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36-3, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 330000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 30000, 1);

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    // Repaint without marking it dirty.
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(0, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 330000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 30000, 1);
}

TEST(TiledLayerChromiumTest, tilesPaintedWithOcclusionAndTransforms)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100.

    // This makes sure the painting works when the occluded region (in screen space)
    // is transformed differently than the layer.
    layer->setBounds(IntSize(600, 600));
    TransformationMatrix screenTransform;
    screenTransform.scale(0.5);
    layer->setScreenSpaceTransform(screenTransform);
    layer->setDrawTransform(screenTransform * TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(100, 100, 150, 50));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36-3, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 330000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 30000, 1);
}

TEST(TiledLayerChromiumTest, tilesPaintedWithOcclusionAndScaling)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100.

    // This makes sure the painting works when the content space is scaled to
    // a different layer space. In this case tiles are scaled to be 200x200
    // pixels, which means none should be occluded.
    layer->setContentsScale(0.5);
    layer->setBounds(IntSize(600, 600));
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(200, 200, 300, 100));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    // The content is half the size of the layer (so the number of tiles is fewer).
    // In this case, the content is 300x300, and since the tile size is 100, the
    // number of tiles 3x3.
    EXPECT_EQ(9, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 90000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 0, 1);

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    // This makes sure the painting works when the content space is scaled to
    // a different layer space. In this case the occluded region catches the
    // blown up tiles.
    occluded.setOcclusion(IntRect(200, 200, 300, 200));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(9-1, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 90000 + 80000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 10000, 1);

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    // This makes sure content scaling and transforms work together.
    TransformationMatrix screenTransform;
    screenTransform.scale(0.5);
    layer->setScreenSpaceTransform(screenTransform);
    layer->setDrawTransform(screenTransform * TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(100, 100, 150, 100));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->prepareToUpdate(IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(9-1, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 90000 + 80000 + 80000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 10000 + 10000, 1);
}

TEST(TiledLayerChromiumTest, opaqueContentsRegion)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100, so this invalidates and then paints two tiles in various ways.

    IntRect opaquePaintRect;
    Region opaqueContents;

    IntRect contentBounds = IntRect(0, 0, 100, 200);
    IntRect visibleBounds = IntRect(0, 0, 100, 150);

    layer->setBounds(contentBounds.size());
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));
    layer->setVisibleLayerRect(visibleBounds);
    layer->setDrawOpacity(1);

    // If the layer doesn't paint opaque content, then the opaqueContentsRegion should be empty.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(contentBounds);
    layer->prepareToUpdate(contentBounds, &occluded);
    opaqueContents = layer->opaqueContentsRegion();
    EXPECT_TRUE(opaqueContents.isEmpty());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 20000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 0, 1);

    // opaqueContentsRegion should match the visible part of what is painted opaque.
    opaquePaintRect = IntRect(10, 10, 90, 190);
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(opaquePaintRect);
    layer->invalidateRect(contentBounds);
    layer->prepareToUpdate(contentBounds, &occluded);
    opaqueContents = layer->opaqueContentsRegion();
    EXPECT_EQ_RECT(intersection(opaquePaintRect, visibleBounds), opaqueContents.bounds());
    EXPECT_EQ(1u, opaqueContents.rects().size());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 20000 + 20000 - 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 0, 1);

    // If we paint again without invalidating, the same stuff should be opaque.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->prepareToUpdate(contentBounds, &occluded);
    opaqueContents = layer->opaqueContentsRegion();
    EXPECT_EQ_RECT(intersection(opaquePaintRect, visibleBounds), opaqueContents.bounds());
    EXPECT_EQ(1u, opaqueContents.rects().size());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 20000 + 20000 - 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 0, 1);

    // If we repaint a non-opaque part of the tile, then it shouldn't lose its opaque-ness. And other tiles should
    // not be affected.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(IntRect(0, 0, 1, 1));
    layer->prepareToUpdate(contentBounds, &occluded);
    opaqueContents = layer->opaqueContentsRegion();
    EXPECT_EQ_RECT(intersection(opaquePaintRect, visibleBounds), opaqueContents.bounds());
    EXPECT_EQ(1u, opaqueContents.rects().size());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2 + 1, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 20000 + 20000 - 17100 + 1, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 0, 1);

    // If we repaint an opaque part of the tile, then it should lose its opaque-ness. But other tiles should still
    // not be affected.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(IntRect(10, 10, 1, 1));
    layer->prepareToUpdate(contentBounds, &occluded);
    opaqueContents = layer->opaqueContentsRegion();
    EXPECT_EQ_RECT(intersection(IntRect(10, 100, 90, 100), visibleBounds), opaqueContents.bounds());
    EXPECT_EQ(1u, opaqueContents.rects().size());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2 + 1  + 1, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 20000 + 20000 - 17100 + 1 + 1, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 0, 1);
}

TEST(TiledLayerChromiumTest, pixelsPaintedMetrics)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100, so this invalidates and then paints two tiles in various ways.

    IntRect opaquePaintRect;
    Region opaqueContents;

    IntRect contentBounds = IntRect(0, 0, 100, 300);
    IntRect visibleBounds = IntRect(0, 0, 100, 300);

    layer->setBounds(contentBounds.size());
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));
    layer->setVisibleLayerRect(visibleBounds);
    layer->setDrawOpacity(1);

    // Invalidates and paints the whole layer.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(contentBounds);
    layer->prepareToUpdate(contentBounds, &occluded);
    opaqueContents = layer->opaqueContentsRegion();
    EXPECT_TRUE(opaqueContents.isEmpty());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 30000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 30000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 0, 1);

    // Invalidates an area on the top and bottom tile, which will cause us to paint the tile in the middle,
    // even though it is not dirty and will not be uploaded.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(IntRect(0, 0, 1, 1));
    layer->invalidateRect(IntRect(50, 200, 10, 10));
    layer->prepareToUpdate(contentBounds, &occluded);
    opaqueContents = layer->opaqueContentsRegion();
    EXPECT_TRUE(opaqueContents.isEmpty());

    // The middle tile was painted even though not invalidated.
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 30000 + 60 * 210, 1);
    // The pixels uploaded will not include the non-invalidated tile in the middle.
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsDrawnTranslucent(), 30000 + 1 + 100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsCulled(), 0, 1);
}

} // namespace
