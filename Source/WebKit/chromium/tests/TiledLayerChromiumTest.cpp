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

#include "CCAnimationTestCommon.h"
#include "CCLayerTreeTestCommon.h"
#include "CCTiledLayerTestCommon.h"
#include "FakeCCLayerTreeHostClient.h"
#include "WebCompositor.h"
#include "cc/CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include <gtest/gtest.h>

using namespace WebCore;
using namespace WebKitTests;
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

TEST(TiledLayerChromiumTest, pushDirtyTiles)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    CCTextureUpdater updater;

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->unprotectAllTextures();

    // Invalidates both tiles...
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    // ....but then only update one of them.
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 100), 0);
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

    CCTextureUpdater updater;

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), &occluded);
    layer->pushPropertiesTo(layerImpl.get());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->unprotectAllTextures();

    // Invalidates part of the top tile...
    layer->invalidateRect(IntRect(0, 0, 50, 50));
    // ....but the area is occluded.
    occluded.setOcclusion(IntRect(0, 0, 50, 50));
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 100), &occluded);
    layer->pushPropertiesTo(layerImpl.get());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 2500, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

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

    CCTextureUpdater updater;
    FakeTextureAllocator allocator;

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->evictAndDeleteAllTextures(&allocator);
    textureManager->setMaxMemoryLimitBytes(4*1024*1024);
    textureManager->setPreferredMemoryLimitBytes(4*1024*1024);

    // This should drop the tiles on the impl thread.
    layer->pushPropertiesTo(layerImpl.get());

    // We should now have no textures on the impl thread.
    EXPECT_FALSE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));

    // This should recreate and update the deleted textures.
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 100), 0);
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

    CCTextureUpdater updater;

    // The tile size is 100x100. Setup 5x5 tiles with one visible tile in the center.
    IntSize contentBounds(500, 500);
    IntRect contentRect(IntPoint::zero(), contentBounds);
    IntRect visibleRect(200, 200, 100, 100);

    // This invalidates 25 tiles and then paints one visible tile.
    layer->setBounds(contentBounds);
    layer->setVisibleLayerRect(visibleRect);
    layer->invalidateRect(contentRect);
    layer->updateLayerRect(updater, visibleRect, 0);

    // We should need idle-painting for 3x3 tiles in the center.
    EXPECT_TRUE(layer->needsIdlePaint(visibleRect));

    layer->pushPropertiesTo(layerImpl.get());

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(2, 2));

    textureManager->unprotectAllTextures();

    // For the next four updates, we should detect we still need idle painting.
    for (int i = 0; i < 4; i++) {
        layer->updateLayerRect(updater, visibleRect, 0);
        EXPECT_TRUE(layer->needsIdlePaint(visibleRect));
        layer->idleUpdateLayerRect(updater, visibleRect, 0);
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

TEST(TiledLayerChromiumTest, pushTilesAfterIdlePaintFailed)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(1024*1024, 1024*1024, 1024);
    DebugScopedSetImplThread implThread;
    RefPtr<FakeTiledLayerChromium> layer1 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    OwnPtr<FakeCCTiledLayerImpl> layerImpl1(adoptPtr(new FakeCCTiledLayerImpl(0)));
    RefPtr<FakeTiledLayerChromium> layer2 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    OwnPtr<FakeCCTiledLayerImpl> layerImpl2(adoptPtr(new FakeCCTiledLayerImpl(0)));

    CCTextureUpdater updater;
    FakeTextureAllocator allocator;
    FakeTextureCopier copier;
    FakeTextureUploader uploader;

    // For this test we have two layers. layer1 exhausts most texture memory, leaving room for 2 more tiles from
    // layer2, but not all three tiles. First we paint layer1, and one tile from layer2. Then when we idle paint
    // layer2, we will fail on the third tile of layer2, and this should not leave the second tile in a bad state.

    // This requires 4*30000 bytes of memory.
    IntRect layer2Rect(0, 0, 100, 300);
    layer2->setBounds(layer2Rect.size());
    layer2->setVisibleLayerRect(layer2Rect);
    layer2->invalidateRect(layer2Rect);

    // This uses 960000 bytes, leaving 88576 bytes of memory left, which is enough for 2 tiles only in the other layer.
    IntRect layerRect(IntPoint::zero(), IntSize(100, 2400));
    layer1->setBounds(layerRect.size());
    layer1->setVisibleLayerRect(layerRect);
    layer1->invalidateRect(layerRect);
    layer1->updateLayerRect(updater, layerRect, 0);

    // Paint a single tile in layer2 so that it will idle paint.
    layer2->updateLayerRect(updater, IntRect(0, 0, 100, 100), 0);

    // We should need idle-painting for both remaining tiles in layer2.
    EXPECT_TRUE(layer2->needsIdlePaint(layer2Rect));

    // Commit the frame over to impl.
    updater.update(0, &allocator, &copier, &uploader, 5000);
    layer1->pushPropertiesTo(layerImpl1.get());
    layer2->pushPropertiesTo(layerImpl2.get());

    // Now idle paint layer2. We are going to run out of memory though!
    layer2->updateLayerRect(updater, IntRect(0, 0, 100, 100), 0);
    layer2->idleUpdateLayerRect(updater, layer2Rect, 0);

    // Oh well, commit the frame and push.
    updater.update(0, &allocator, &copier, &uploader, 5000);
    layer1->pushPropertiesTo(layerImpl1.get());
    layer2->pushPropertiesTo(layerImpl2.get());

    // Sanity check, we should have textures for the big layer.
    EXPECT_TRUE(layerImpl1->hasTextureIdForTileAt(0, 0));

    // We should only have the first tile from layer2 since it failed to idle update.
    EXPECT_TRUE(layerImpl2->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl2->hasTextureIdForTileAt(0, 0));
    EXPECT_FALSE(layerImpl2->hasTileAt(0, 1));
    EXPECT_FALSE(layerImpl2->hasTileAt(0, 2));

    // Now if layer2 becomes fully visible, we should be able to paint it and push valid textures.
    textureManager->unprotectAllTextures();

    layer2->updateLayerRect(updater, layer2Rect, 0);
    layer1->updateLayerRect(updater, IntRect(), 0);

    updater.update(0, &allocator, &copier, &uploader, 5000);
    layer1->pushPropertiesTo(layerImpl1.get());
    layer2->pushPropertiesTo(layerImpl2.get());

    EXPECT_TRUE(layerImpl2->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl2->hasTileAt(0, 1));
    EXPECT_TRUE(layerImpl2->hasTileAt(0, 2));
    EXPECT_TRUE(layerImpl2->hasTextureIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl2->hasTextureIdForTileAt(0, 1));
    EXPECT_TRUE(layerImpl2->hasTextureIdForTileAt(0, 2));
}

TEST(TiledLayerChromiumTest, pushIdlePaintedOccludedTiles)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));
    TestCCOcclusionTracker occluded;

    CCTextureUpdater updater;

    // The tile size is 100x100, so this invalidates one occluded tile, culls it during paint, but prepaints it.
    occluded.setOcclusion(IntRect(0, 0, 100, 100));

    layer->setBounds(IntSize(100, 100));
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 100));
    layer->invalidateRect(IntRect(0, 0, 100, 100));
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 100), &occluded);
    layer->idleUpdateLayerRect(updater, IntRect(0, 0, 100, 100), &occluded);
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

    CCTextureUpdater updater;

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    // However, during the paint, we invalidate one of the tiles. This should
    // not prevent the tile from being pushed.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(0, 50, 100, 50), layer.get());
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
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

    CCTextureUpdater updater;

    layer1->setBounds(IntSize(100, 200));
    layer1->invalidateRect(IntRect(0, 0, 100, 200));
    layer2->setBounds(IntSize(100, 200));
    layer2->invalidateRect(IntRect(0, 0, 100, 200));

    layer1->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);

    // Invalidate a tile on layer1
    layer2->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(0, 50, 100, 50), layer1.get());
    layer2->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);

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

    CCTextureUpdater updater;

    layer1->setBounds(IntSize(100, 200));
    layer1->invalidateRect(IntRect(0, 0, 100, 200));
    layer2->setBounds(IntSize(100, 200));
    layer2->invalidateRect(IntRect(0, 0, 100, 200));

    // Invalidate a tile on layer2
    layer1->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(0, 50, 100, 50), layer2.get());
    layer1->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);

    layer2->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);

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

    CCTextureUpdater updater;

    // This invalidates 9 tiles and then paints one visible tile.
    layer->setBounds(contentBounds);
    layer->setVisibleLayerRect(visibleRect);
    layer->invalidateRect(contentRect);
    layer->updateLayerRect(updater, visibleRect, 0);

    // We should need idle-painting for 3x3 tiles surounding visible tile.
    EXPECT_TRUE(layer->needsIdlePaint(visibleRect));

    layer->pushPropertiesTo(layerImpl.get());

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(1, 1));

    textureManager->unprotectAllTextures();
    layer->updateLayerRect(updater, visibleRect, 0);
    layer->idleUpdateLayerRect(updater, visibleRect, 0);

    // We shouldn't signal we need another idle paint after we run out of memory.
    EXPECT_FALSE(layer->needsIdlePaint(visibleRect));

    layer->pushPropertiesTo(layerImpl.get());
}

TEST(TiledLayerChromiumTest, idlePaintZeroSizedLayer)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(20000, 10000, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    CCTextureUpdater updater;

    // The layer's bounds are empty.
    IntRect contentRect;

    layer->setBounds(contentRect.size());
    layer->setVisibleLayerRect(contentRect);
    layer->invalidateRect(contentRect);
    layer->updateLayerRect(updater, contentRect, 0);

    // Empty layers don't have tiles.
    EXPECT_EQ(0u, layer->numPaintedTiles());

    // Empty layers don't need prepaint.
    EXPECT_FALSE(layer->needsIdlePaint(contentRect));

    layer->pushPropertiesTo(layerImpl.get());

    // Empty layers don't have tiles.
    EXPECT_FALSE(layerImpl->hasTileAt(0, 0));

    // Non-visible layers don't idle paint.
    layer->idleUpdateLayerRect(updater, contentRect, 0);

    // Empty layers don't have tiles.
    EXPECT_EQ(0u, layer->numPaintedTiles());

    layer->pushPropertiesTo(layerImpl.get());

    // Empty layers don't have tiles.
    EXPECT_FALSE(layerImpl->hasTileAt(0, 0));
}

TEST(TiledLayerChromiumTest, idlePaintZeroSizedAnimatingLayer)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(20000, 10000, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    CCTextureUpdater updater;

    // Pretend the layer is animating.
    layer->setDrawTransformIsAnimating(true);

    // The layer's bounds are empty.
    IntRect contentRect;

    layer->setBounds(contentRect.size());
    layer->setVisibleLayerRect(contentRect);
    layer->invalidateRect(contentRect);
    layer->updateLayerRect(updater, contentRect, 0);

    // Empty layers don't have tiles.
    EXPECT_EQ(0u, layer->numPaintedTiles());

    // Empty layers don't need prepaint.
    EXPECT_FALSE(layer->needsIdlePaint(contentRect));

    layer->pushPropertiesTo(layerImpl.get());

    // Empty layers don't have tiles.
    EXPECT_FALSE(layerImpl->hasTileAt(0, 0));

    // Non-visible layers don't idle paint.
    layer->idleUpdateLayerRect(updater, contentRect, 0);

    // Empty layers don't have tiles.
    EXPECT_EQ(0u, layer->numPaintedTiles());

    layer->pushPropertiesTo(layerImpl.get());

    // Empty layers don't have tiles.
    EXPECT_FALSE(layerImpl->hasTileAt(0, 0));
}

TEST(TiledLayerChromiumTest, idlePaintNonVisibleLayers)
{
    IntSize contentBounds(100, 100);
    IntRect contentRect(IntPoint::zero(), contentBounds);

    OwnPtr<TextureManager> textureManager = TextureManager::create(20000, 10000, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    CCTextureUpdater updater;

    // Invalidate the layer but make none of it visible, so nothing paints.
    IntRect visibleRect;

    layer->setBounds(contentBounds);
    layer->setVisibleLayerRect(visibleRect);
    layer->invalidateRect(contentRect);
    layer->updateLayerRect(updater, visibleRect, 0);

    // Non-visible layers don't need idle paint.
    EXPECT_FALSE(layer->needsIdlePaint(visibleRect));

    layer->pushPropertiesTo(layerImpl.get());

    // We should not have any tiles pushed since the layer is not visible.
    EXPECT_FALSE(layerImpl->hasTileAt(0, 0));

    // Non-visible layers don't idle paint.
    layer->idleUpdateLayerRect(updater, visibleRect, 0);

    layer->pushPropertiesTo(layerImpl.get());

    // We should not have any tiles pushed since the layer is not visible.
    EXPECT_FALSE(layerImpl->hasTileAt(0, 0));
}

static void idlePaintRepeat(int repeatTimes, FakeTiledLayerChromium* layer, FakeCCTiledLayerImpl* layerImpl, CCTextureUpdater& updater, const IntRect& visibleRect)
{
    for (int i = 0; i < repeatTimes; ++i) {
        layer->updateLayerRect(updater, visibleRect, 0);
        layer->idleUpdateLayerRect(updater, visibleRect, 0);
        layer->pushPropertiesTo(layerImpl);
    }
}

static void testHaveOuterTiles(FakeCCTiledLayerImpl* layerImpl, int width, int height, int have)
{
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            bool hasTile = i < have || j < have || i >= width - have || j >= height - have;
            EXPECT_EQ(hasTile, layerImpl->hasTileAt(i, j));
        }
    }
}

TEST(TiledLayerChromiumTest, idlePaintNonVisibleAnimatingLayers)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(8000*8000*8, 8000*8000*4, 1024);
    DebugScopedSetImplThread implThread;

    CCTextureUpdater updater;

    int tileWidth = FakeTiledLayerChromium::tileSize().width();
    int tileHeight = FakeTiledLayerChromium::tileSize().height();
    int width[] = { 1, 2, 3, 4, 9, 10, 0 };
    int height[] = { 1, 2, 3, 4, 9, 10, 0 };

    for (int j = 0; height[j]; ++j) {
        for (int i = 0; width[i]; ++i) {
            RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
            OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

            // Pretend the layer is animating.
            layer->setDrawTransformIsAnimating(true);

            IntSize contentBounds(width[i] * tileWidth, height[j] * tileHeight);
            IntRect contentRect(IntPoint::zero(), contentBounds);
            IntRect visibleRect;

            layer->setBounds(contentBounds);
            layer->setVisibleLayerRect(visibleRect);
            layer->invalidateRect(contentRect);

            // If idlePaintRect gives back a non-empty result then we should paint it. Otherwise,
            // we shoud paint nothing.
            bool shouldPrepaint = !layer->idlePaintRect(visibleRect).isEmpty();

            // This paints the layer but there's nothing visible so it's a no-op.
            layer->updateLayerRect(updater, visibleRect, 0);
            layer->pushPropertiesTo(layerImpl.get());

            // We should not have any tiles pushed yet since the layer is not visible and we've not prepainted.
            testHaveOuterTiles(layerImpl.get(), width[i], height[j], 0);

            // Normally we don't allow non-visible layers to pre-paint, but if they are animating then we should.
            EXPECT_EQ(shouldPrepaint, layer->needsIdlePaint(visibleRect));

            // If the layer is to be prepainted at all, then after four updates we should have the outer row/columns painted.
            idlePaintRepeat(4, layer.get(), layerImpl.get(), updater, visibleRect);
            testHaveOuterTiles(layerImpl.get(), width[i], height[j], shouldPrepaint ? 1 : 0);

            // We don't currently idle paint past the outermost tiles.
            EXPECT_FALSE(layer->needsIdlePaint(visibleRect));
            idlePaintRepeat(4, layer.get(), layerImpl.get(), updater, visibleRect);
            testHaveOuterTiles(layerImpl.get(), width[i], height[j], shouldPrepaint ? 1 : 0);
        }
    }
}

TEST(TiledLayerChromiumTest, invalidateFromPrepare)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    CCTextureUpdater updater;
    FakeTextureAllocator fakeAllocator;
    FakeTextureCopier fakeCopier;
    FakeTextureUploader fakeUploader;
    RefPtr<GraphicsContext3D> context = createCompositorMockGraphicsContext3D(GraphicsContext3D::Attributes());

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updater.update(context.get(), &fakeAllocator, &fakeCopier, &fakeUploader, 1000);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->unprotectAllTextures();

    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    // Invoke updateLayerRect again. As the layer is valid updateLayerRect shouldn't be invoked on
    // the LayerTextureUpdater.
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updater.update(context.get(), &fakeAllocator, &fakeCopier, &fakeUploader, 1000);
    EXPECT_EQ(0, layer->fakeLayerTextureUpdater()->prepareCount());

    layer->invalidateRect(IntRect(0, 0, 50, 50));
    // setRectToInvalidate triggers invalidateRect() being invoked from updateLayerRect.
    layer->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(25, 25, 50, 50), layer.get());
    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updater.update(context.get(), &fakeAllocator, &fakeCopier, &fakeUploader, 1000);
    EXPECT_EQ(1, layer->fakeLayerTextureUpdater()->prepareCount());
    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    // The layer should still be invalid as updateLayerRect invoked invalidate.
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updater.update(context.get(), &fakeAllocator, &fakeCopier, &fakeUploader, 1000);
    EXPECT_EQ(1, layer->fakeLayerTextureUpdater()->prepareCount());
}

TEST(TiledLayerChromiumTest, verifyUpdateRectWhenContentBoundsAreScaled)
{
    // The updateRect (that indicates what was actually painted) should be in
    // layer space, not the content space.

    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerWithScaledBounds> layer = adoptRef(new FakeTiledLayerWithScaledBounds(textureManager.get()));

    CCTextureUpdater updater;

    IntRect layerBounds(0, 0, 300, 200);
    IntRect contentBounds(0, 0, 200, 250);

    layer->setBounds(layerBounds.size());
    layer->setContentBounds(contentBounds.size());
    layer->setVisibleLayerRect(contentBounds);

    // On first update, the updateRect includes all tiles, even beyond the boundaries of the layer.
    // However, it should still be in layer space, not content space.
    layer->invalidateRect(contentBounds);
    layer->updateLayerRect(updater, contentBounds, 0);
    EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 300, 300 * 0.8), layer->updateRect());

    // After the tiles are updated once, another invalidate only needs to update the bounds of the layer.
    layer->invalidateRect(contentBounds);
    layer->updateLayerRect(updater, contentBounds, 0);
    EXPECT_FLOAT_RECT_EQ(FloatRect(layerBounds), layer->updateRect());

    // Partial re-paint should also be represented by the updateRect in layer space, not content space.
    IntRect partialDamage(30, 100, 10, 10);
    layer->invalidateRect(partialDamage);
    layer->updateLayerRect(updater, contentBounds, 0);
    EXPECT_FLOAT_RECT_EQ(FloatRect(45, 80, 15, 8), layer->updateRect());
}

TEST(TiledLayerChromiumTest, verifyInvalidationWhenContentsScaleChanges)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));

    CCTextureUpdater updater;

    // Create a layer with one tile.
    layer->setBounds(IntSize(100, 100));

    // Invalidate the entire layer.
    layer->setNeedsDisplay();
    EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 100, 100), layer->lastNeedsDisplayRect());

    // Push the tiles to the impl side and check that there is exactly one.
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 100), 0);
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
    layer->updateLayerRect(updater, IntRect(0, 0, 200, 200), 0);
    layer->pushPropertiesTo(layerImpl.get());
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));
    EXPECT_TRUE(layerImpl->hasTileAt(1, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(1, 1));

    // Invalidate the entire layer again, but do not paint. All tiles should be gone now from the
    // impl side.
    layer->setNeedsDisplay();
    layer->updateLayerRect(updater, IntRect(1, 0, 0, 1), 0);
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
    OwnPtr<CCLayerTreeHost> ccLayerTreeHost = CCLayerTreeHost::create(&fakeCCLayerTreeHostClient, CCSettings());

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

    CCTextureUpdater updater;

    ccLayerTreeHost->setRootLayer(rootLayer);
    ccLayerTreeHost->setViewportSize(IntSize(300, 300));
    textureManager->setMaxMemoryLimitBytes(memoryLimit);
    ccLayerTreeHost->updateLayers(updater);

    // We'll skip the root layer.
    EXPECT_TRUE(rootLayer->skipsDraw());
    EXPECT_FALSE(childLayer->skipsDraw());

    ccLayerTreeHost->commitComplete();
    textureManager->unprotectAllTextures(); // CCLayerTreeHost::commitComplete() normally does this, but since we're mocking out the manager we have to do it.

    // Remove the child layer.
    rootLayer->removeAllChildren();

    ccLayerTreeHost->updateLayers(updater);
    EXPECT_FALSE(rootLayer->skipsDraw());

    ccLayerTreeHost->setRootLayer(0);
    ccLayerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

TEST(TiledLayerChromiumTest, resizeToSmaller)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(60*1024*1024, 60*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    CCTextureUpdater updater;

    layer->setBounds(IntSize(700, 700));
    layer->invalidateRect(IntRect(0, 0, 700, 700));
    layer->updateLayerRect(updater, IntRect(0, 0, 700, 700), 0);

    layer->setBounds(IntSize(200, 200));
    layer->invalidateRect(IntRect(0, 0, 200, 200));
}

TEST(TiledLayerChromiumTest, hugeLayerUpdateCrash)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(60*1024*1024, 60*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    CCTextureUpdater updater;

    int size = 1 << 30;
    layer->setBounds(IntSize(size, size));
    layer->invalidateRect(IntRect(0, 0, size, size));

    // Ensure no crash for bounds where size * size would overflow an int.
    layer->updateLayerRect(updater, IntRect(0, 0, 700, 700), 0);
}

TEST(TiledLayerChromiumTest, partialUpdates)
{
    CCSettings settings;
    settings.maxPartialTextureUpdates = 4;
    // Initialize without threading support.
    WebKit::WebCompositor::initialize(0);
    FakeCCLayerTreeHostClient fakeCCLayerTreeHostClient;
    OwnPtr<CCLayerTreeHost> ccLayerTreeHost = CCLayerTreeHost::create(&fakeCCLayerTreeHostClient, settings);

    // Create one 500 x 300 tiled layer.
    IntSize contentBounds(300, 200);
    IntRect contentRect(IntPoint::zero(), contentBounds);

    OwnPtr<TextureManager> textureManager = TextureManager::create(60*1024*1024, 60*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    layer->setBounds(contentBounds);
    layer->setPosition(FloatPoint(150, 150));
    layer->invalidateRect(contentRect);

    CCTextureUpdater updater;
    FakeTextureAllocator allocator;
    FakeTextureCopier copier;
    FakeTextureUploader uploader;

    ccLayerTreeHost->setRootLayer(layer);
    ccLayerTreeHost->setViewportSize(IntSize(300, 200));

    // Full update of all 6 tiles.
    ccLayerTreeHost->updateLayers(updater);
    {
        DebugScopedSetImplThread implThread;
        OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));
        updater.update(0, &allocator, &copier, &uploader, 4);
        EXPECT_EQ(4, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_TRUE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        updater.update(0, &allocator, &copier, &uploader, 4);
        EXPECT_EQ(2, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_FALSE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        layer->pushPropertiesTo(layerImpl.get());
    }
    ccLayerTreeHost->commitComplete();

    // Full update of 3 tiles and partial update of 3 tiles.
    layer->invalidateRect(IntRect(0, 0, 300, 150));
    ccLayerTreeHost->updateLayers(updater);
    {
        DebugScopedSetImplThread implThread;
        OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));
        updater.update(0, &allocator, &copier, &uploader, 4);
        EXPECT_EQ(3, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_TRUE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        updater.update(0, &allocator, &copier, &uploader, 4);
        EXPECT_EQ(3, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_FALSE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        layer->pushPropertiesTo(layerImpl.get());
    }
    ccLayerTreeHost->commitComplete();

    // Partial update of 6 tiles.
    layer->invalidateRect(IntRect(50, 50, 200, 100));
    {
        DebugScopedSetImplThread implThread;
        OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));
        ccLayerTreeHost->updateLayers(updater);
        updater.update(0, &allocator, &copier, &uploader, 4);
        EXPECT_EQ(2, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_TRUE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        updater.update(0, &allocator, &copier, &uploader, 4);
        EXPECT_EQ(4, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_FALSE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        layer->pushPropertiesTo(layerImpl.get());
    }
    ccLayerTreeHost->commitComplete();

    // Checkerboard all tiles.
    layer->invalidateRect(IntRect(0, 0, 300, 200));
    {
        DebugScopedSetImplThread implThread;
        OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));
        layer->pushPropertiesTo(layerImpl.get());
    }
    ccLayerTreeHost->commitComplete();

    // Partail update of 6 checkerboard tiles.
    layer->invalidateRect(IntRect(50, 50, 200, 100));
    {
        DebugScopedSetImplThread implThread;
        OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(0)));
        ccLayerTreeHost->updateLayers(updater);
        updater.update(0, &allocator, &copier, &uploader, 4);
        EXPECT_EQ(4, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_TRUE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        updater.update(0, &allocator, &copier, &uploader, 4);
        EXPECT_EQ(2, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_FALSE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        layer->pushPropertiesTo(layerImpl.get());
    }
    ccLayerTreeHost->commitComplete();

    ccLayerTreeHost->setRootLayer(0);
    ccLayerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

TEST(TiledLayerChromiumTest, tilesPaintedWithoutOcclusion)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    CCTextureUpdater updater;

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));

    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    EXPECT_EQ(2, layer->fakeLayerTextureUpdater()->prepareRectCount());
}

TEST(TiledLayerChromiumTest, tilesPaintedWithOcclusion)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;
    CCTextureUpdater updater;

    // The tile size is 100x100.

    layer->setBounds(IntSize(600, 600));
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(200, 200, 300, 100));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36-3, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000, 1);
    EXPECT_EQ(3, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    occluded.setOcclusion(IntRect(250, 200, 300, 100));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36-2, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000 + 340000, 1);
    EXPECT_EQ(3 + 2, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    occluded.setOcclusion(IntRect(250, 250, 300, 100));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000 + 340000 + 360000, 1);
    EXPECT_EQ(3 + 2, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST(TiledLayerChromiumTest, tilesPaintedWithOcclusionAndVisiblityConstraints)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;
    CCTextureUpdater updater;

    // The tile size is 100x100.

    layer->setBounds(IntSize(600, 600));
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    // The partially occluded tiles (by the 150 occlusion height) are visible beyond the occlusion, so not culled.
    occluded.setOcclusion(IntRect(200, 200, 300, 150));
    layer->setVisibleLayerRect(IntRect(0, 0, 600, 360));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 360), &occluded);
    EXPECT_EQ(24-3, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 210000, 1);
    EXPECT_EQ(3, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    // Now the visible region stops at the edge of the occlusion so the partly visible tiles become fully occluded.
    occluded.setOcclusion(IntRect(200, 200, 300, 150));
    layer->setVisibleLayerRect(IntRect(0, 0, 600, 350));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 350), &occluded);
    EXPECT_EQ(24-6, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 210000 + 180000, 1);
    EXPECT_EQ(3 + 6, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    // Now the visible region is even smaller than the occlusion, it should have the same result.
    occluded.setOcclusion(IntRect(200, 200, 300, 150));
    layer->setVisibleLayerRect(IntRect(0, 0, 600, 340));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 340), &occluded);
    EXPECT_EQ(24-6, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 210000 + 180000 + 180000, 1);
    EXPECT_EQ(3 + 6 + 6, occluded.overdrawMetrics().tilesCulledForUpload());

}

TEST(TiledLayerChromiumTest, tilesNotPaintedWithoutInvalidation)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;
    CCTextureUpdater updater;

    // The tile size is 100x100.

    layer->setBounds(IntSize(600, 600));
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(200, 200, 300, 100));
    layer->setVisibleLayerRect(IntRect(0, 0, 600, 600));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36-3, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000, 1);
    EXPECT_EQ(3, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    // Repaint without marking it dirty. The culled tiles remained dirty.
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(0, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000, 1);
    EXPECT_EQ(6, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST(TiledLayerChromiumTest, tilesPaintedWithOcclusionAndTransforms)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;
    CCTextureUpdater updater;

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
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36-3, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000, 1);
    EXPECT_EQ(3, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST(TiledLayerChromiumTest, tilesPaintedWithOcclusionAndScaling)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;
    CCTextureUpdater updater;

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
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    // The content is half the size of the layer (so the number of tiles is fewer).
    // In this case, the content is 300x300, and since the tile size is 100, the
    // number of tiles 3x3.
    EXPECT_EQ(9, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 90000, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    // This makes sure the painting works when the content space is scaled to
    // a different layer space. In this case the occluded region catches the
    // blown up tiles.
    occluded.setOcclusion(IntRect(200, 200, 300, 200));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(9-1, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 90000 + 80000, 1);
    EXPECT_EQ(1, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    // This makes sure content scaling and transforms work together.
    TransformationMatrix screenTransform;
    screenTransform.scale(0.5);
    layer->setScreenSpaceTransform(screenTransform);
    layer->setDrawTransform(screenTransform * TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(100, 100, 150, 100));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(9-1, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 90000 + 80000 + 80000, 1);
    EXPECT_EQ(1 + 1, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST(TiledLayerChromiumTest, visibleContentOpaqueRegion)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;
    CCTextureUpdater updater;

    // The tile size is 100x100, so this invalidates and then paints two tiles in various ways.

    IntRect opaquePaintRect;
    Region opaqueContents;

    IntRect contentBounds = IntRect(0, 0, 100, 200);
    IntRect visibleBounds = IntRect(0, 0, 100, 150);

    layer->setBounds(contentBounds.size());
    layer->setDrawTransform(TransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));
    layer->setVisibleLayerRect(visibleBounds);
    layer->setDrawOpacity(1);

    // If the layer doesn't paint opaque content, then the visibleContentOpaqueRegion should be empty.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(contentBounds);
    layer->updateLayerRect(updater, contentBounds, &occluded);
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_TRUE(opaqueContents.isEmpty());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // visibleContentOpaqueRegion should match the visible part of what is painted opaque.
    opaquePaintRect = IntRect(10, 10, 90, 190);
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(opaquePaintRect);
    layer->invalidateRect(contentBounds);
    layer->updateLayerRect(updater, contentBounds, &occluded);
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_EQ_RECT(intersection(opaquePaintRect, visibleBounds), opaqueContents.bounds());
    EXPECT_EQ(1u, opaqueContents.rects().size());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 20000 - 17100, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // If we paint again without invalidating, the same stuff should be opaque.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->updateLayerRect(updater, contentBounds, &occluded);
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_EQ_RECT(intersection(opaquePaintRect, visibleBounds), opaqueContents.bounds());
    EXPECT_EQ(1u, opaqueContents.rects().size());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 20000 - 17100, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // If we repaint a non-opaque part of the tile, then it shouldn't lose its opaque-ness. And other tiles should
    // not be affected.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(IntRect(0, 0, 1, 1));
    layer->updateLayerRect(updater, contentBounds, &occluded);
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_EQ_RECT(intersection(opaquePaintRect, visibleBounds), opaqueContents.bounds());
    EXPECT_EQ(1u, opaqueContents.rects().size());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2 + 1, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 20000 - 17100 + 1, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // If we repaint an opaque part of the tile, then it should lose its opaque-ness. But other tiles should still
    // not be affected.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(IntRect(10, 10, 1, 1));
    layer->updateLayerRect(updater, contentBounds, &occluded);
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_EQ_RECT(intersection(IntRect(10, 100, 90, 100), visibleBounds), opaqueContents.bounds());
    EXPECT_EQ(1u, opaqueContents.rects().size());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2 + 1  + 1, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 20000 - 17100 + 1 + 1, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // No metrics are recorded in prepaint, so the values should not change from above.
    layer->idleUpdateLayerRect(updater, contentBounds, &occluded);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2 + 1  + 1, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 20000 - 17100 + 1 + 1, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST(TiledLayerChromiumTest, pixelsPaintedMetrics)
{
    OwnPtr<TextureManager> textureManager = TextureManager::create(4*1024*1024, 2*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;
    CCTextureUpdater updater;

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
    layer->updateLayerRect(updater, contentBounds, &occluded);
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_TRUE(opaqueContents.isEmpty());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 30000, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 30000, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // Invalidates an area on the top and bottom tile, which will cause us to paint the tile in the middle,
    // even though it is not dirty and will not be uploaded.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(IntRect(0, 0, 1, 1));
    layer->invalidateRect(IntRect(50, 200, 10, 10));
    layer->updateLayerRect(updater, contentBounds, &occluded);
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_TRUE(opaqueContents.isEmpty());

    // The middle tile was painted even though not invalidated.
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 30000 + 60 * 210, 1);
    // The pixels uploaded will not include the non-invalidated tile in the middle.
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 30000 + 1 + 100, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());
}

} // namespace
