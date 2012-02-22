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

    private:
        FakeLayerTextureUpdater* m_layer;
    };

    FakeLayerTextureUpdater()
        : m_prepareCount(0)
        , m_updateCount(0)
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

    void setOpaquePaintRect(const IntRect& opaquePaintRect) { m_opaquePaintRect = opaquePaintRect; }

    // Last rect passed to prepareToUpdate().
    const IntRect& lastUpdateRect()  const { return m_lastUpdateRect; }

    virtual PassOwnPtr<LayerTextureUpdater::Texture> createTexture(TextureManager* manager) { return adoptPtr(new Texture(this, ManagedTexture::create(manager))); }
    virtual SampledTexelFormat sampledTexelFormat(GC3Denum) { return SampledTexelFormatRGBA; }
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize&, int, float, IntRect* resultingOpaqueRect);

private:
    int m_prepareCount;
    int m_updateCount;
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

    void prepareToUpdate(const IntRect& rect)
    {
        TiledLayerChromium::prepareToUpdate(rect);
    }

    void prepareToUpdateIdle(const IntRect& rect)
    {
        TiledLayerChromium::prepareToUpdateIdle(rect);
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

    virtual void paintContentsIfDirty(const Region& /* occludedScreenSpace */)
    {
        prepareToUpdate(visibleLayerRect());
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
    RefPtr<FakeCCTiledLayerImpl> layerImpl = adoptRef(new FakeCCTiledLayerImpl(0));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->prepareToUpdate(IntRect(0, 0, 100, 200));
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->unprotectAllTextures();

    // Invalidates both tiles...
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    // ....but then only update one of them.
    layer->prepareToUpdate(IntRect(0, 0, 100, 100));
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
    RefPtr<FakeCCTiledLayerImpl> layerImpl = adoptRef(new FakeCCTiledLayerImpl(0));

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
    layer->prepareToUpdate(visibleRect);

    // We should need idle-painting for 3x3 tiles in the center.
    EXPECT_TRUE(layer->needsIdlePaint(visibleRect));

    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(2, 2));

    textureManager->unprotectAllTextures();

    // For the next four updates, we should detect we still need idle painting.
    for (int i = 0; i < 4; i++) {
        layer->prepareToUpdate(visibleRect);
        EXPECT_TRUE(layer->needsIdlePaint(visibleRect));
        layer->prepareToUpdateIdle(visibleRect);
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
    RefPtr<FakeCCTiledLayerImpl> layerImpl = adoptRef(new FakeCCTiledLayerImpl(0));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // This invalidates 9 tiles and then paints one visible tile.
    layer->setBounds(contentBounds);
    layer->setVisibleLayerRect(visibleRect);
    layer->invalidateRect(contentRect);
    layer->prepareToUpdate(visibleRect);

    // We should need idle-painting for 3x3 tiles surounding visible tile.
    EXPECT_TRUE(layer->needsIdlePaint(visibleRect));

    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(1, 1));

    textureManager->unprotectAllTextures();
    layer->prepareToUpdate(visibleRect);
    layer->prepareToUpdateIdle(visibleRect);

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
    RefPtr<FakeCCTiledLayerImpl> layerImpl = adoptRef(new FakeCCTiledLayerImpl(0));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->prepareToUpdate(IntRect(0, 0, 100, 200));
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->unprotectAllTextures();

    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    // Invoke prepareToUpdate again. As the layer is valid prepareToUpdate shouldn't be invoked on
    // the LayerTextureUpdater.
    layer->prepareToUpdate(IntRect(0, 0, 100, 200));
    EXPECT_EQ(0, layer->fakeLayerTextureUpdater()->prepareCount());

    layer->invalidateRect(IntRect(0, 0, 50, 50));
    // setRectToInvalidate triggers invalidateRect() being invoked from prepareToUpdate.
    layer->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(25, 25, 50, 50), layer.get());
    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    layer->prepareToUpdate(IntRect(0, 0, 100, 200));
    EXPECT_EQ(1, layer->fakeLayerTextureUpdater()->prepareCount());
    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    // The layer should still be invalid as prepareToUpdate invoked invalidate.
    layer->prepareToUpdate(IntRect(0, 0, 100, 200));
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
    layer->prepareToUpdate(contentBounds);
    layer->updateCompositorResources(0, updater);
    EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 300, 300 * 0.8), layer->updateRect());

    // After the tiles are updated once, another invalidate only needs to update the bounds of the layer.
    layer->invalidateRect(contentBounds);
    layer->prepareToUpdate(contentBounds);
    layer->updateCompositorResources(0, updater);
    EXPECT_FLOAT_RECT_EQ(FloatRect(layerBounds), layer->updateRect());

    // Partial re-paint should also be represented by the updateRect in layer space, not content space.
    IntRect partialDamage(30, 100, 10, 10);
    layer->invalidateRect(partialDamage);
    layer->prepareToUpdate(contentBounds);
    layer->updateCompositorResources(0, updater);
    EXPECT_FLOAT_RECT_EQ(FloatRect(45, 80, 15, 8), layer->updateRect());
}

TEST(TiledLayerChromiumTest, verifyInvalidationWhenContentsScaleChanges)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    RefPtr<FakeCCTiledLayerImpl> layerImpl = adoptRef(new FakeCCTiledLayerImpl(0));

    FakeTextureAllocator textureAllocator;
    CCTextureUpdater updater(&textureAllocator);

    // Create a layer with one tile.
    layer->setBounds(IntSize(100, 100));

    // Invalidate the entire layer.
    layer->setNeedsDisplay();
    EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 100, 100), layer->lastNeedsDisplayRect());

    // Push the tiles to the impl side and check that there is exactly one.
    layer->prepareToUpdate(IntRect(0, 0, 100, 100));
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
    layer->prepareToUpdate(IntRect(0, 0, 200, 200));
    layer->updateCompositorResources(0, updater);
    layer->pushPropertiesTo(layerImpl.get());
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));
    EXPECT_TRUE(layerImpl->hasTileAt(1, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(1, 1));

    // Invalidate the entire layer again, but do not paint. All tiles should be gone now from the
    // impl side.
    layer->setNeedsDisplay();
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

TEST(TiledLayerChromiumTest, layerAddsSelfToOccludedRegion)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));

    // The tile size is 100x100, so this invalidates and then paints two tiles in various ways.

    Region occluded;
    IntRect contentBounds = IntRect(0, 0, 100, 200);
    IntRect visibleBounds = IntRect(0, 0, 100, 150);

    layer->setBounds(contentBounds.size());
    layer->setVisibleLayerRect(visibleBounds);
    layer->setDrawOpacity(1);

    // The screenSpaceTransform is verified in CCLayerTreeHostCommonTests
    TransformationMatrix screenSpaceTransform;
    layer->setScreenSpaceTransform(screenSpaceTransform);

    // If the layer is opaque then the occluded region should be the whole layer's visible region.
    layer->setOpaque(true);
    layer->invalidateRect(contentBounds);
    layer->prepareToUpdate(contentBounds);

    occluded = Region();
    layer->addSelfToOccludedScreenSpace(occluded);
    EXPECT_EQ_RECT(visibleBounds, occluded.bounds());
    EXPECT_EQ(1u, occluded.rects().size());

    // If the layer is not opaque then the occluded region should be empty.
    layer->setOpaque(false);
    layer->invalidateRect(contentBounds);
    layer->prepareToUpdate(contentBounds);

    occluded = Region();
    layer->addSelfToOccludedScreenSpace(occluded);
    EXPECT_EQ_RECT(IntRect(), occluded.bounds());
    EXPECT_EQ(0u, occluded.rects().size());

    // If the layer paints opaque content, then the occluded region should match the visible opaque content.
    IntRect opaquePaintRect = IntRect(10, 10, 90, 190);
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(opaquePaintRect);
    layer->invalidateRect(contentBounds);
    layer->prepareToUpdate(contentBounds);

    occluded = Region();
    layer->addSelfToOccludedScreenSpace(occluded);
    EXPECT_EQ_RECT(intersection(opaquePaintRect, visibleBounds), occluded.bounds());
    EXPECT_EQ(1u, occluded.rects().size());

    // If we paint again without invalidating, the same stuff should be occluded.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->prepareToUpdate(contentBounds);

    occluded = Region();
    layer->addSelfToOccludedScreenSpace(occluded);
    EXPECT_EQ_RECT(intersection(opaquePaintRect, visibleBounds), occluded.bounds());
    EXPECT_EQ(1u, occluded.rects().size());

    // If we repaint a non-opaque part of the tile, then it shouldn't lose its opaque-ness. And other tiles should
    // not be affected.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(IntRect(0, 0, 1, 1));
    layer->prepareToUpdate(contentBounds);

    occluded = Region();
    layer->addSelfToOccludedScreenSpace(occluded);
    EXPECT_EQ_RECT(intersection(opaquePaintRect, visibleBounds), occluded.bounds());
    EXPECT_EQ(1u, occluded.rects().size());

    // If we repaint an opaque part of the tile, then it should lose its opaque-ness. But other tiles should still
    // not be affected.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(IntRect(10, 10, 1, 1));
    layer->prepareToUpdate(contentBounds);

    occluded = Region();
    layer->addSelfToOccludedScreenSpace(occluded);
    EXPECT_EQ_RECT(intersection(IntRect(10, 100, 90, 100), visibleBounds), occluded.bounds());
    EXPECT_EQ(1u, occluded.rects().size());

    // If the layer is transformed then the resulting occluded area needs to be transformed to its target space.
    TransformationMatrix transform;
    transform.translate(contentBounds.width() / 2.0, contentBounds.height() / 2.0);
    transform.rotate(90);
    transform.translate(-contentBounds.width() / 2.0, -contentBounds.height() / 2.0);
    transform.translate(10, 10);
    screenSpaceTransform.translate(contentBounds.width() / 2.0, contentBounds.height() / 2.0);
    screenSpaceTransform *= transform;
    screenSpaceTransform.translate(-contentBounds.width() / 2.0, -contentBounds.height() / 2.0);
    layer->setScreenSpaceTransform(screenSpaceTransform);
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(opaquePaintRect);
    layer->invalidateRect(opaquePaintRect);
    layer->prepareToUpdate(contentBounds);

    occluded = Region();
    layer->addSelfToOccludedScreenSpace(occluded);
    EXPECT_EQ_RECT(screenSpaceTransform.mapRect(intersection(opaquePaintRect, visibleBounds)), occluded.bounds());
    EXPECT_EQ(1u, occluded.rects().size());

    // But a non-axis-aligned transform does not get considered for occlusion.
    transform.translate(contentBounds.width() / 2.0, contentBounds.height() / 2.0);
    transform.rotate(5);
    transform.translate(-contentBounds.width() / 2.0, -contentBounds.height() / 2.0);
    screenSpaceTransform.translate(contentBounds.width() / 2.0, contentBounds.height() / 2.0);
    screenSpaceTransform *= transform;
    screenSpaceTransform.translate(-contentBounds.width() / 2.0, -contentBounds.height() / 2.0);
    layer->setScreenSpaceTransform(screenSpaceTransform);
    layer->prepareToUpdate(contentBounds);

    occluded = Region();
    layer->addSelfToOccludedScreenSpace(occluded);
    // FIXME: If we find an opaque rect contained in the rotated non-axis-aligned rect, then
    // this won't be an empty result.
    EXPECT_EQ_RECT(IntRect(), occluded.bounds());
    EXPECT_EQ(0u, occluded.rects().size());
}

TEST(TiledLayerChromiumTest, resizeToSmaller)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(60*1024*1024, 60*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));

    layer->setBounds(IntSize(700, 700));
    layer->invalidateRect(IntRect(0, 0, 700, 700));
    layer->prepareToUpdate(IntRect(0, 0, 700, 700));

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

} // namespace
