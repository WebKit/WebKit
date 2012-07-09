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

#include "BitmapCanvasLayerTextureUpdater.h"
#include "CCAnimationTestCommon.h"
#include "CCLayerTreeTestCommon.h"
#include "CCTiledLayerTestCommon.h"
#include "FakeCCLayerTreeHostClient.h"
#include "LayerPainterChromium.h"
#include "WebCompositor.h"
#include "cc/CCOverdrawMetrics.h"
#include "cc/CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include <gtest/gtest.h>
#include <public/WebTransformationMatrix.h>

using namespace WebCore;
using namespace WebKitTests;
using namespace WTF;
using WebKit::WebTransformationMatrix;

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

class TiledLayerChromiumTest : public testing::Test {
public:
    TiledLayerChromiumTest() : ccContext(CCGraphicsContext::create3D(WebKit::CompositorFakeWebGraphicsContext3D::create(WebKit::WebGraphicsContext3D::Attributes()))) { }
    virtual ~TiledLayerChromiumTest() { }

    void updateTextures(int count = 500)
    {
        updater.update(ccContext.get(), &allocator, &copier, &uploader, count);
    }
public:
    // FIXME: Rename these to add "m_".
    OwnPtr<CCGraphicsContext> ccContext;
    CCTextureUpdater updater;
    FakeTextureAllocator allocator;
    FakeTextureCopier copier;
    FakeTextureUploader uploader;
    CCPriorityCalculator priorityCalculator;
};



TEST_F(TiledLayerChromiumTest, pushDirtyTiles)
{
    CCPriorityCalculator priorityCalculator;
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);

    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updateTextures();
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    // Invalidates both tiles...
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    // ....but then only update one of them.
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 100), 0);
    layer->pushPropertiesTo(layerImpl.get());

    // We should only have the first tile since the other tile was invalidated but not painted.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));
}

TEST_F(TiledLayerChromiumTest, pushOccludedDirtyTiles)
{
    CCPriorityCalculator priorityCalculator;
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->setDrawTransform(WebTransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);

    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), &occluded);
    updateTextures();
    layer->pushPropertiesTo(layerImpl.get());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    // Invalidates part of the top tile...
    layer->invalidateRect(IntRect(0, 0, 50, 50));
    // ....but the area is occluded.
    occluded.setOcclusion(IntRect(0, 0, 50, 50));
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 100), &occluded);
    updateTextures();
    layer->pushPropertiesTo(layerImpl.get());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 2500, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // We should still have both tiles, as part of the top tile is still unoccluded.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));
}

TEST_F(TiledLayerChromiumTest, pushDeletedTiles)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);

    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updateTextures();
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    textureManager->clearPriorities();
    textureManager->clearAllMemory(&allocator);
    textureManager->setMaxMemoryLimitBytes(4*1024*1024);

    // This should drop the tiles on the impl thread.
    layer->pushPropertiesTo(layerImpl.get());

    // We should now have no textures on the impl thread.
    EXPECT_FALSE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));

    // This should recreate and update the deleted textures.
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 100), 0);
    updateTextures();
    layer->pushPropertiesTo(layerImpl.get());

    // We should only have the first tile since the other tile was invalidated but not painted.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));
}

TEST_F(TiledLayerChromiumTest, pushIdlePaintTiles)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));

    // The tile size is 100x100. Setup 5x5 tiles with one visible tile in the center.
    IntSize contentBounds(500, 500);
    IntRect contentRect(IntPoint::zero(), contentBounds);
    IntRect visibleRect(200, 200, 100, 100);

    // This invalidates 25 tiles and then paints one visible tile.
    layer->setBounds(contentBounds);
    layer->setVisibleLayerRect(visibleRect);
    layer->invalidateRect(contentRect);

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);

    layer->updateLayerRect(updater, visibleRect, 0);
    updateTextures();

    // We should need idle-painting for 3x3 tiles in the center.
    EXPECT_TRUE(layer->needsIdlePaint(visibleRect));

    layer->pushPropertiesTo(layerImpl.get());

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(2, 2));

    // For the next four updates, we should detect we still need idle painting.
    for (int i = 0; i < 4; i++) {
        layer->setTexturePriorities(priorityCalculator);
        textureManager->prioritizeTextures(0);

        layer->updateLayerRect(updater, visibleRect, 0);
        EXPECT_TRUE(layer->needsIdlePaint(visibleRect));
        layer->idleUpdateLayerRect(updater, visibleRect, 0);
        updateTextures();
        layer->pushPropertiesTo(layerImpl.get());
    }

    // After four passes of idle painting, we should be finished painting
    // EXPECT_FALSE(layer->needsIdlePaint(visibleRect));

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

TEST_F(TiledLayerChromiumTest, pushTilesAfterIdlePaintFailed)
{
    // Start with 2mb of memory, but the test is going to try to use just more than 1mb, so we reduce to 1mb later.
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(2*1024*1024, 1024);
    DebugScopedSetImplThread implThread;
    RefPtr<FakeTiledLayerChromium> layer1 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    OwnPtr<FakeCCTiledLayerImpl> layerImpl1(adoptPtr(new FakeCCTiledLayerImpl(1)));
    RefPtr<FakeTiledLayerChromium> layer2 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    OwnPtr<FakeCCTiledLayerImpl> layerImpl2(adoptPtr(new FakeCCTiledLayerImpl(2)));

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

    // Paint a single tile in layer2 so that it will idle paint.
    layer2->setTexturePriorities(priorityCalculator);
    layer1->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer1->updateLayerRect(updater, layerRect, 0);
    layer2->updateLayerRect(updater, IntRect(0, 0, 100, 100), 0);

    // We should need idle-painting for both remaining tiles in layer2.
    EXPECT_TRUE(layer2->needsIdlePaint(layer2Rect));

    // Commit the frame over to impl.
    updateTextures();
    layer1->pushPropertiesTo(layerImpl1.get());
    layer2->pushPropertiesTo(layerImpl2.get());

    // Reduce our memory limits to 1mb.
    textureManager->setMaxMemoryLimitBytes(1024 * 1024);

    // Now idle paint layer2. We are going to run out of memory though!
    layer2->setTexturePriorities(priorityCalculator);
    layer1->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer2->updateLayerRect(updater, IntRect(0, 0, 100, 100), 0);
    layer2->idleUpdateLayerRect(updater, layer2Rect, 0);

    // Oh well, commit the frame and push.
    updateTextures();
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
    layer2->setTexturePriorities(priorityCalculator);
    layer1->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer2->updateLayerRect(updater, layer2Rect, 0);
    layer1->updateLayerRect(updater, layerRect, 0);

    updateTextures();
    layer1->pushPropertiesTo(layerImpl1.get());
    layer2->pushPropertiesTo(layerImpl2.get());

    EXPECT_TRUE(layerImpl2->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl2->hasTileAt(0, 1));
    EXPECT_TRUE(layerImpl2->hasTileAt(0, 2));
    EXPECT_TRUE(layerImpl2->hasTextureIdForTileAt(0, 0));
    EXPECT_TRUE(layerImpl2->hasTextureIdForTileAt(0, 1));
    EXPECT_TRUE(layerImpl2->hasTextureIdForTileAt(0, 2));
}

TEST_F(TiledLayerChromiumTest, pushIdlePaintedOccludedTiles)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100, so this invalidates one occluded tile, culls it during paint, but prepaints it.
    occluded.setOcclusion(IntRect(0, 0, 100, 100));

    layer->setBounds(IntSize(100, 100));
    layer->setDrawTransform(WebTransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 100));
    layer->invalidateRect(IntRect(0, 0, 100, 100));

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 100), &occluded);
    layer->idleUpdateLayerRect(updater, IntRect(0, 0, 100, 100), &occluded);
    updateTextures();
    layer->pushPropertiesTo(layerImpl.get());

    // We should have the prepainted tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
}

TEST_F(TiledLayerChromiumTest, pushTilesMarkedDirtyDuringPaint)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    // However, during the paint, we invalidate one of the tiles. This should
    // not prevent the tile from being pushed.
    layer->setBounds(IntSize(100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(0, 50, 100, 50), layer.get());

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updateTextures();
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));
}

TEST_F(TiledLayerChromiumTest, pushTilesLayerMarkedDirtyDuringPaintOnNextLayer)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer1 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    RefPtr<FakeTiledLayerChromium> layer2 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layer1Impl(adoptPtr(new FakeCCTiledLayerImpl(1)));
    OwnPtr<FakeCCTiledLayerImpl> layer2Impl(adoptPtr(new FakeCCTiledLayerImpl(2)));

    layer1->setBounds(IntSize(100, 200));
    layer1->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer1->invalidateRect(IntRect(0, 0, 100, 200));

    layer2->setBounds(IntSize(100, 200));
    layer2->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer2->invalidateRect(IntRect(0, 0, 100, 200));

    layer1->setTexturePriorities(priorityCalculator);
    layer2->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);

    layer1->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);

    // Invalidate a tile on layer1
    layer2->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(0, 50, 100, 50), layer1.get());
    layer2->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);

    updateTextures();
    layer1->pushPropertiesTo(layer1Impl.get());
    layer2->pushPropertiesTo(layer2Impl.get());

    // We should have both tiles on the impl side for all layers.
    EXPECT_TRUE(layer1Impl->hasTileAt(0, 0));
    EXPECT_TRUE(layer1Impl->hasTileAt(0, 1));
    EXPECT_TRUE(layer2Impl->hasTileAt(0, 0));
    EXPECT_TRUE(layer2Impl->hasTileAt(0, 1));
}

TEST_F(TiledLayerChromiumTest, pushTilesLayerMarkedDirtyDuringPaintOnPreviousLayer)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer1 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    RefPtr<FakeTiledLayerChromium> layer2 = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layer1Impl(adoptPtr(new FakeCCTiledLayerImpl(1)));
    OwnPtr<FakeCCTiledLayerImpl> layer2Impl(adoptPtr(new FakeCCTiledLayerImpl(2)));

    layer1->setBounds(IntSize(100, 200));
    layer1->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer1->invalidateRect(IntRect(0, 0, 100, 200));

    layer2->setBounds(IntSize(100, 200));
    layer2->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer2->invalidateRect(IntRect(0, 0, 100, 200));

    layer1->setTexturePriorities(priorityCalculator);
    layer2->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);

    // Invalidate a tile on layer2
    layer1->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(0, 50, 100, 50), layer2.get());
    layer1->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    layer2->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updateTextures();
    layer1->pushPropertiesTo(layer1Impl.get());
    layer2->pushPropertiesTo(layer2Impl.get());

    // We should have both tiles on the impl side for all layers.
    EXPECT_TRUE(layer1Impl->hasTileAt(0, 0));
    EXPECT_TRUE(layer1Impl->hasTileAt(0, 1));
    EXPECT_TRUE(layer2Impl->hasTileAt(0, 0));
    EXPECT_TRUE(layer2Impl->hasTileAt(0, 1));
}

TEST_F(TiledLayerChromiumTest, idlePaintOutOfMemory)
{
    // The tile size is 100x100. Setup 3x3 tiles with one 1x1 visible tile in the center.
    IntSize contentBounds(300, 300);
    IntRect contentRect(IntPoint::zero(), contentBounds);
    IntRect visibleRect(100, 100, 100, 100);

    // We have enough memory for only the visible rect, so we will run out of memory in first idle paint.
    int memoryLimit = 4 * 100 * 100; // 1 tiles, 4 bytes per pixel.

    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(memoryLimit, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));

    // Invalidates 9 tiles and then paints one visible tile.
    layer->setBounds(contentBounds);
    layer->setVisibleLayerRect(visibleRect);
    layer->invalidateRect(contentRect);

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, visibleRect, 0);

    // Idle-painting should see no more priority tiles for painting.
    EXPECT_FALSE(layer->needsIdlePaint(visibleRect));

    updateTextures();
    layer->pushPropertiesTo(layerImpl.get());

    // We should have one tile on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(1, 1));

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, visibleRect, 0);
    layer->idleUpdateLayerRect(updater, visibleRect, 0);

    // We shouldn't signal we need another idle paint.
    EXPECT_FALSE(layer->needsIdlePaint(visibleRect));
}

TEST_F(TiledLayerChromiumTest, idlePaintZeroSizedLayer)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(20000, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));

    // The layer's bounds are empty.
    IntRect contentRect;

    layer->setBounds(contentRect.size());
    layer->setVisibleLayerRect(contentRect);
    layer->invalidateRect(contentRect);

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
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

TEST_F(TiledLayerChromiumTest, idlePaintZeroSizedAnimatingLayer)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(20000, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));

    // Pretend the layer is animating.
    layer->setDrawTransformIsAnimating(true);

    // The layer's bounds are empty.
    IntRect contentRect;

    layer->setBounds(contentRect.size());
    layer->setVisibleLayerRect(contentRect);
    layer->invalidateRect(contentRect);

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
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

TEST_F(TiledLayerChromiumTest, idlePaintNonVisibleLayers)
{
    IntSize contentBounds(100, 100);
    IntRect contentRect(IntPoint::zero(), contentBounds);

    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(20000, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));

    // Invalidate the layer but make none of it visible, so nothing paints.
    IntRect visibleRect;

    layer->setBounds(contentBounds);
    layer->setVisibleLayerRect(visibleRect);
    layer->invalidateRect(contentRect);

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
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

static void testHaveOuterTiles(FakeCCTiledLayerImpl* layerImpl, int width, int height, int have)
{
    for (int i = 0; i < width; ++i) {
        for (int j = 0; j < height; ++j) {
            bool hasTile = i < have || j < have || i >= width - have || j >= height - have;
            EXPECT_EQ(hasTile, layerImpl->hasTileAt(i, j));
        }
    }
}

TEST_F(TiledLayerChromiumTest, idlePaintNonVisibleAnimatingLayers)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(8000*8000*8, 1024);
    DebugScopedSetImplThread implThread;

    int tileWidth = FakeTiledLayerChromium::tileSize().width();
    int tileHeight = FakeTiledLayerChromium::tileSize().height();
    int width[] = { 1, 2, 3, 4, 9, 10, 0 };
    int height[] = { 1, 2, 3, 4, 9, 10, 0 };

    for (int j = 0; height[j]; ++j) {
        for (int i = 0; width[i]; ++i) {
            RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
            OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));

            // Pretend the layer is animating.
            layer->setDrawTransformIsAnimating(true);

            IntSize contentBounds(width[i] * tileWidth, height[j] * tileHeight);
            IntRect contentRect(IntPoint::zero(), contentBounds);
            IntRect visibleRect;

            layer->setBounds(contentBounds);
            layer->setVisibleLayerRect(visibleRect);
            layer->invalidateRect(contentRect);

            layer->setTexturePriorities(priorityCalculator);
            textureManager->prioritizeTextures(0);

            // If idlePaintRect gives back a non-empty result then we should paint it. Otherwise,
            // we shoud paint nothing.
            bool shouldPrepaint = !layer->idlePaintRect(visibleRect).isEmpty();

            // This paints the layer but there's nothing visible so it's a no-op.
            layer->updateLayerRect(updater, visibleRect, 0);
            // This drops all the tile objects since they were not painted.
            updateTextures();
            layer->pushPropertiesTo(layerImpl.get());

            // We should not have any tiles pushed yet since the layer is not visible and we've not prepainted.
            testHaveOuterTiles(layerImpl.get(), width[i], height[j], 0);

            // Set priorities and recreate tiles.
            layer->setTexturePriorities(priorityCalculator);
            textureManager->prioritizeTextures(0);

            // Normally we don't allow non-visible layers to pre-paint, but if they are animating then we should.
            EXPECT_EQ(shouldPrepaint, layer->needsIdlePaint(visibleRect));

            // If the layer is to be prepainted at all, then after four updates we should have the outer row/columns painted.
            for (int k = 0; k < 4; ++k) {
                layer->setTexturePriorities(priorityCalculator);
                textureManager->prioritizeTextures(0);

                layer->updateLayerRect(updater, visibleRect, 0);
                layer->idleUpdateLayerRect(updater, visibleRect, 0);
                updateTextures();
                layer->pushPropertiesTo(layerImpl.get());
            }
            testHaveOuterTiles(layerImpl.get(), width[i], height[j], shouldPrepaint ? 1 : 0);

            // We don't currently idle paint past the outermost tiles.
            EXPECT_FALSE(layer->needsIdlePaint(visibleRect));
            for (int k = 0; k < 4; ++k) {
                layer->setTexturePriorities(priorityCalculator);
                textureManager->prioritizeTextures(0);

                layer->updateLayerRect(updater, visibleRect, 0);
                layer->idleUpdateLayerRect(updater, visibleRect, 0);
                updateTextures();
                layer->pushPropertiesTo(layerImpl.get());
            }
            testHaveOuterTiles(layerImpl.get(), width[i], height[j], shouldPrepaint ? 1 : 0);
        }
    }
}

TEST_F(TiledLayerChromiumTest, invalidateFromPrepare)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));

    FakeTextureAllocator fakeAllocator;
    FakeTextureCopier fakeCopier;
    FakeTextureUploader fakeUploader;
    OwnPtr<CCGraphicsContext> ccContext = CCGraphicsContext::create3D(WebKit::CompositorFakeWebGraphicsContext3D::create(WebKit::WebGraphicsContext3D::Attributes()));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updater.update(ccContext.get(), &fakeAllocator, &fakeCopier, &fakeUploader, 1000);
    layer->pushPropertiesTo(layerImpl.get());

    // We should have both tiles on the impl side.
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));

    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    // Invoke updateLayerRect again. As the layer is valid updateLayerRect shouldn't be invoked on
    // the LayerTextureUpdater.
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updater.update(ccContext.get(), &fakeAllocator, &fakeCopier, &fakeUploader, 1000);
    EXPECT_EQ(0, layer->fakeLayerTextureUpdater()->prepareCount());

    layer->invalidateRect(IntRect(0, 0, 50, 50));
    // setRectToInvalidate triggers invalidateRect() being invoked from updateLayerRect.
    layer->fakeLayerTextureUpdater()->setRectToInvalidate(IntRect(25, 25, 50, 50), layer.get());
    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updater.update(ccContext.get(), &fakeAllocator, &fakeCopier, &fakeUploader, 1000);
    EXPECT_EQ(1, layer->fakeLayerTextureUpdater()->prepareCount());
    layer->fakeLayerTextureUpdater()->clearPrepareCount();
    // The layer should still be invalid as updateLayerRect invoked invalidate.
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    updater.update(ccContext.get(), &fakeAllocator, &fakeCopier, &fakeUploader, 1000);
    EXPECT_EQ(1, layer->fakeLayerTextureUpdater()->prepareCount());
}

TEST_F(TiledLayerChromiumTest, verifyUpdateRectWhenContentBoundsAreScaled)
{
    // The updateRect (that indicates what was actually painted) should be in
    // layer space, not the content space.

    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerWithScaledBounds> layer = adoptRef(new FakeTiledLayerWithScaledBounds(textureManager.get()));

    IntRect layerBounds(0, 0, 300, 200);
    IntRect contentBounds(0, 0, 200, 250);

    layer->setBounds(layerBounds.size());
    layer->setContentBounds(contentBounds.size());
    layer->setVisibleLayerRect(contentBounds);

    // On first update, the updateRect includes all tiles, even beyond the boundaries of the layer.
    // However, it should still be in layer space, not content space.
    layer->invalidateRect(contentBounds);

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, contentBounds, 0);
    EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 300, 300 * 0.8), layer->updateRect());
    updateTextures();

    // After the tiles are updated once, another invalidate only needs to update the bounds of the layer.
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->invalidateRect(contentBounds);
    layer->updateLayerRect(updater, contentBounds, 0);
    EXPECT_FLOAT_RECT_EQ(FloatRect(layerBounds), layer->updateRect());
    updateTextures();

    // Partial re-paint should also be represented by the updateRect in layer space, not content space.
    IntRect partialDamage(30, 100, 10, 10);
    layer->invalidateRect(partialDamage);
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, contentBounds, 0);
    EXPECT_FLOAT_RECT_EQ(FloatRect(45, 80, 15, 8), layer->updateRect());
}

TEST_F(TiledLayerChromiumTest, verifyInvalidationWhenContentsScaleChanges)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    DebugScopedSetImplThread implThread;
    OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));

    // Create a layer with one tile.
    layer->setBounds(IntSize(100, 100));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 100));

    // Invalidate the entire layer.
    layer->setNeedsDisplay();
    EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 100, 100), layer->lastNeedsDisplayRect());

    // Push the tiles to the impl side and check that there is exactly one.
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 100), 0);
    updateTextures();
    layer->pushPropertiesTo(layerImpl.get());
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));
    EXPECT_FALSE(layerImpl->hasTileAt(1, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(1, 1));

    // Change the contents scale and verify that the content rectangle requiring painting
    // is not scaled.
    layer->setContentsScale(2);
    layer->setVisibleLayerRect(IntRect(0, 0, 200, 200));
    EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 100, 100), layer->lastNeedsDisplayRect());

    // The impl side should get 2x2 tiles now.
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 200, 200), 0);
    updateTextures();
    layer->pushPropertiesTo(layerImpl.get());
    EXPECT_TRUE(layerImpl->hasTileAt(0, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(0, 1));
    EXPECT_TRUE(layerImpl->hasTileAt(1, 0));
    EXPECT_TRUE(layerImpl->hasTileAt(1, 1));

    // Invalidate the entire layer again, but do not paint. All tiles should be gone now from the
    // impl side.
    layer->setNeedsDisplay();
    layer->setTexturePriorities(priorityCalculator);
    layer->updateLayerRect(updater, IntRect(1, 0, 0, 1), 0);
    textureManager->prioritizeTextures(0);

    layer->pushPropertiesTo(layerImpl.get());
    EXPECT_FALSE(layerImpl->hasTileAt(0, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(0, 1));
    EXPECT_FALSE(layerImpl->hasTileAt(1, 0));
    EXPECT_FALSE(layerImpl->hasTileAt(1, 1));
}

TEST_F(TiledLayerChromiumTest, skipsDrawGetsReset)
{
    // Initialize without threading support.
    WebKit::WebCompositor::initialize(0);
    FakeCCLayerTreeHostClient fakeCCLayerTreeHostClient;
    OwnPtr<CCLayerTreeHost> ccLayerTreeHost = CCLayerTreeHost::create(&fakeCCLayerTreeHostClient, CCLayerTreeSettings());
    ASSERT_TRUE(ccLayerTreeHost->initializeLayerRendererIfNeeded());

    // Create two 300 x 300 tiled layers.
    IntSize contentBounds(300, 300);
    IntRect contentRect(IntPoint::zero(), contentBounds);

    // We have enough memory for only one of the two layers.
    int memoryLimit = 4 * 300 * 300; // 4 bytes per pixel.

    RefPtr<FakeTiledLayerChromium> rootLayer = adoptRef(new FakeTiledLayerChromium(ccLayerTreeHost->contentsTextureManager()));
    RefPtr<FakeTiledLayerChromium> childLayer = adoptRef(new FakeTiledLayerChromium(ccLayerTreeHost->contentsTextureManager()));
    rootLayer->addChild(childLayer);

    rootLayer->setBounds(contentBounds);
    rootLayer->setVisibleLayerRect(contentRect);
    rootLayer->setPosition(FloatPoint(150, 150));
    childLayer->setBounds(contentBounds);
    childLayer->setVisibleLayerRect(contentRect);
    childLayer->setPosition(FloatPoint(150, 150));
    rootLayer->invalidateRect(contentRect);
    childLayer->invalidateRect(contentRect);

    ccLayerTreeHost->setRootLayer(rootLayer);
    ccLayerTreeHost->setViewportSize(IntSize(300, 300));

    ccLayerTreeHost->updateLayers(updater, memoryLimit);

    // We'll skip the root layer.
    EXPECT_TRUE(rootLayer->skipsDraw());
    EXPECT_FALSE(childLayer->skipsDraw());

    ccLayerTreeHost->commitComplete();

    // Remove the child layer.
    rootLayer->removeAllChildren();

    ccLayerTreeHost->updateLayers(updater, memoryLimit);
    EXPECT_FALSE(rootLayer->skipsDraw());

    ccLayerTreeHost->contentsTextureManager()->clearAllMemory(&allocator);
    ccLayerTreeHost->setRootLayer(0);
    ccLayerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

TEST_F(TiledLayerChromiumTest, resizeToSmaller)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(60*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));

    layer->setBounds(IntSize(700, 700));
    layer->setVisibleLayerRect(IntRect(0, 0, 700, 700));
    layer->invalidateRect(IntRect(0, 0, 700, 700));

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 700, 700), 0);

    layer->setBounds(IntSize(200, 200));
    layer->invalidateRect(IntRect(0, 0, 200, 200));
}

TEST_F(TiledLayerChromiumTest, hugeLayerUpdateCrash)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(60*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));

    int size = 1 << 30;
    layer->setBounds(IntSize(size, size));
    layer->setVisibleLayerRect(IntRect(0, 0, 700, 700));
    layer->invalidateRect(IntRect(0, 0, size, size));

    // Ensure no crash for bounds where size * size would overflow an int.
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 700, 700), 0);
}

TEST_F(TiledLayerChromiumTest, partialUpdates)
{
    // Initialize without threading support.
    WebKit::WebCompositor::initialize(0);

    CCLayerTreeSettings settings;
    settings.maxPartialTextureUpdates = 4;

    FakeCCLayerTreeHostClient fakeCCLayerTreeHostClient;
    OwnPtr<CCLayerTreeHost> ccLayerTreeHost = CCLayerTreeHost::create(&fakeCCLayerTreeHostClient, settings);
    ASSERT_TRUE(ccLayerTreeHost->initializeLayerRendererIfNeeded());

    // Create one 300 x 200 tiled layer with 3 x 2 tiles.
    IntSize contentBounds(300, 200);
    IntRect contentRect(IntPoint::zero(), contentBounds);

    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(ccLayerTreeHost->contentsTextureManager()));
    layer->setBounds(contentBounds);
    layer->setPosition(FloatPoint(150, 150));
    layer->setVisibleLayerRect(contentRect);
    layer->invalidateRect(contentRect);

    ccLayerTreeHost->setRootLayer(layer);
    ccLayerTreeHost->setViewportSize(IntSize(300, 200));

    // Full update of all 6 tiles.
    ccLayerTreeHost->updateLayers(updater, std::numeric_limits<size_t>::max());
    {
        DebugScopedSetImplThread implThread;
        OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));
        updateTextures(4);
        EXPECT_EQ(4, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_TRUE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        updateTextures(4);
        EXPECT_EQ(2, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_FALSE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        layer->pushPropertiesTo(layerImpl.get());
    }
    ccLayerTreeHost->commitComplete();

    // Full update of 3 tiles and partial update of 3 tiles.
    layer->invalidateRect(IntRect(0, 0, 300, 150));
    ccLayerTreeHost->updateLayers(updater, std::numeric_limits<size_t>::max());
    {
        DebugScopedSetImplThread implThread;
        OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));
        updateTextures(4);
        EXPECT_EQ(3, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_TRUE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        updateTextures(4);
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
        OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));
        ccLayerTreeHost->updateLayers(updater, std::numeric_limits<size_t>::max());
        updateTextures(4);
        EXPECT_EQ(2, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_TRUE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        updateTextures(4);
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
        OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));
        layer->pushPropertiesTo(layerImpl.get());
    }
    ccLayerTreeHost->commitComplete();

    // Partial update of 6 checkerboard tiles.
    layer->invalidateRect(IntRect(50, 50, 200, 100));
    {
        DebugScopedSetImplThread implThread;
        OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));
        ccLayerTreeHost->updateLayers(updater, std::numeric_limits<size_t>::max());
        updateTextures(4);
        EXPECT_EQ(4, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_TRUE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        updateTextures(4);
        EXPECT_EQ(2, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_FALSE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        layer->pushPropertiesTo(layerImpl.get());
    }
    ccLayerTreeHost->commitComplete();

    // Partial update of 4 tiles.
    layer->invalidateRect(IntRect(50, 50, 100, 100));
    {
        DebugScopedSetImplThread implThread;
        OwnPtr<FakeCCTiledLayerImpl> layerImpl(adoptPtr(new FakeCCTiledLayerImpl(1)));
        ccLayerTreeHost->updateLayers(updater, std::numeric_limits<size_t>::max());
        updateTextures(4);
        EXPECT_EQ(4, layer->fakeLayerTextureUpdater()->updateCount());
        EXPECT_FALSE(updater.hasMoreUpdates());
        layer->fakeLayerTextureUpdater()->clearUpdateCount();
        layer->pushPropertiesTo(layerImpl.get());
    }
    ccLayerTreeHost->commitComplete();

    ccLayerTreeHost->contentsTextureManager()->clearAllMemory(&allocator);
    ccLayerTreeHost->setRootLayer(0);
    ccLayerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

TEST_F(TiledLayerChromiumTest, tilesPaintedWithoutOcclusion)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));

    // The tile size is 100x100, so this invalidates and then paints two tiles.
    layer->setBounds(IntSize(100, 200));
    layer->setVisibleLayerRect(IntRect(0, 0, 100, 200));
    layer->invalidateRect(IntRect(0, 0, 100, 200));

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 100, 200), 0);
    EXPECT_EQ(2, layer->fakeLayerTextureUpdater()->prepareRectCount());
}

TEST_F(TiledLayerChromiumTest, tilesPaintedWithOcclusion)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100.

    layer->setBounds(IntSize(600, 600));
    layer->setDrawTransform(WebTransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(200, 200, 300, 100));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
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

TEST_F(TiledLayerChromiumTest, tilesPaintedWithOcclusionAndVisiblityConstraints)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100.

    layer->setBounds(IntSize(600, 600));
    layer->setDrawTransform(WebTransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    // The partially occluded tiles (by the 150 occlusion height) are visible beyond the occlusion, so not culled.
    occluded.setOcclusion(IntRect(200, 200, 300, 150));
    layer->setVisibleLayerRect(IntRect(0, 0, 600, 360));
    layer->invalidateRect(IntRect(0, 0, 600, 600));

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
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
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
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
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 340), &occluded);
    EXPECT_EQ(24-6, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 210000 + 180000 + 180000, 1);
    EXPECT_EQ(3 + 6 + 6, occluded.overdrawMetrics().tilesCulledForUpload());

}

TEST_F(TiledLayerChromiumTest, tilesNotPaintedWithoutInvalidation)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100.

    layer->setBounds(IntSize(600, 600));
    layer->setDrawTransform(WebTransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(200, 200, 300, 100));
    layer->setVisibleLayerRect(IntRect(0, 0, 600, 600));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36-3, layer->fakeLayerTextureUpdater()->prepareRectCount());
    updateTextures();

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

TEST_F(TiledLayerChromiumTest, tilesPaintedWithOcclusionAndTransforms)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100.

    // This makes sure the painting works when the occluded region (in screen space)
    // is transformed differently than the layer.
    layer->setBounds(IntSize(600, 600));
    WebTransformationMatrix screenTransform;
    screenTransform.scale(0.5);
    layer->setScreenSpaceTransform(screenTransform);
    layer->setDrawTransform(screenTransform * WebTransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(100, 100, 150, 50));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(36-3, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 330000, 1);
    EXPECT_EQ(3, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST_F(TiledLayerChromiumTest, tilesPaintedWithOcclusionAndScaling)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100.

    // This makes sure the painting works when the content space is scaled to
    // a different layer space. In this case tiles are scaled to be 200x200
    // pixels, which means none should be occluded.
    layer->setContentsScale(0.5);
    layer->setBounds(IntSize(600, 600));
    layer->setDrawTransform(WebTransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(200, 200, 300, 100));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
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
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(9-1, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 90000 + 80000, 1);
    EXPECT_EQ(1, occluded.overdrawMetrics().tilesCulledForUpload());

    layer->fakeLayerTextureUpdater()->clearPrepareRectCount();

    // This makes sure content scaling and transforms work together.
    WebTransformationMatrix screenTransform;
    screenTransform.scale(0.5);
    layer->setScreenSpaceTransform(screenTransform);
    layer->setDrawTransform(screenTransform * WebTransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));

    occluded.setOcclusion(IntRect(100, 100, 150, 100));
    layer->setVisibleLayerRect(IntRect(IntPoint(), layer->bounds()));
    layer->invalidateRect(IntRect(0, 0, 600, 600));
    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);
    layer->updateLayerRect(updater, IntRect(0, 0, 600, 600), &occluded);
    EXPECT_EQ(9-1, layer->fakeLayerTextureUpdater()->prepareRectCount());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 90000 + 80000 + 80000, 1);
    EXPECT_EQ(1 + 1, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST_F(TiledLayerChromiumTest, visibleContentOpaqueRegion)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100, so this invalidates and then paints two tiles in various ways.

    IntRect opaquePaintRect;
    Region opaqueContents;

    IntRect contentBounds = IntRect(0, 0, 100, 200);
    IntRect visibleBounds = IntRect(0, 0, 100, 150);

    layer->setBounds(contentBounds.size());
    layer->setDrawTransform(WebTransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));
    layer->setVisibleLayerRect(visibleBounds);
    layer->setDrawOpacity(1);

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);

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
    updateTextures();
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
    updateTextures();
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
    updateTextures();
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
    updateTextures();
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_EQ_RECT(intersection(IntRect(10, 100, 90, 100), visibleBounds), opaqueContents.bounds());
    EXPECT_EQ(1u, opaqueContents.rects().size());

    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2 + 1  + 1, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 20000 - 17100 + 1 + 1, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());

    // No metrics are recorded in prepaint, so the values should not change from above.
    layer->idleUpdateLayerRect(updater, contentBounds, &occluded);
    updateTextures();
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 20000 * 2 + 1  + 1, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 17100, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 20000 + 20000 - 17100 + 1 + 1, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST_F(TiledLayerChromiumTest, pixelsPaintedMetrics)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager = CCPrioritizedTextureManager::create(4*1024*1024, 1024);
    RefPtr<FakeTiledLayerChromium> layer = adoptRef(new FakeTiledLayerChromium(textureManager.get()));
    TestCCOcclusionTracker occluded;

    // The tile size is 100x100, so this invalidates and then paints two tiles in various ways.

    IntRect opaquePaintRect;
    Region opaqueContents;

    IntRect contentBounds = IntRect(0, 0, 100, 300);
    IntRect visibleBounds = IntRect(0, 0, 100, 300);

    layer->setBounds(contentBounds.size());
    layer->setDrawTransform(WebTransformationMatrix(1, 0, 0, 1, layer->bounds().width() / 2.0, layer->bounds().height() / 2.0));
    layer->setVisibleLayerRect(visibleBounds);
    layer->setDrawOpacity(1);

    layer->setTexturePriorities(priorityCalculator);
    textureManager->prioritizeTextures(0);

    // Invalidates and paints the whole layer.
    layer->fakeLayerTextureUpdater()->setOpaquePaintRect(IntRect());
    layer->invalidateRect(contentBounds);
    layer->updateLayerRect(updater, contentBounds, &occluded);
    updateTextures();
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
    updateTextures();
    opaqueContents = layer->visibleContentOpaqueRegion();
    EXPECT_TRUE(opaqueContents.isEmpty());

    // The middle tile was painted even though not invalidated.
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsPainted(), 30000 + 60 * 210, 1);
    // The pixels uploaded will not include the non-invalidated tile in the middle.
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedOpaque(), 0, 1);
    EXPECT_NEAR(occluded.overdrawMetrics().pixelsUploadedTranslucent(), 30000 + 1 + 100, 1);
    EXPECT_EQ(0, occluded.overdrawMetrics().tilesCulledForUpload());
}

TEST_F(TiledLayerChromiumTest, dontAllocateContentsWhenTargetSurfaceCantBeAllocated)
{
    // Initialize without threading support.
    WebKit::WebCompositor::initialize(0);

    // Tile size is 100x100.
    IntRect rootRect(0, 0, 300, 200);
    IntRect childRect(0, 0, 300, 100);
    IntRect child2Rect(0, 100, 300, 100);

    CCLayerTreeSettings settings;
    FakeCCLayerTreeHostClient fakeCCLayerTreeHostClient;
    OwnPtr<CCLayerTreeHost> ccLayerTreeHost = CCLayerTreeHost::create(&fakeCCLayerTreeHostClient, settings);
    ASSERT_TRUE(ccLayerTreeHost->initializeLayerRendererIfNeeded());

    RefPtr<FakeTiledLayerChromium> root = adoptRef(new FakeTiledLayerChromium(ccLayerTreeHost->contentsTextureManager()));
    RefPtr<LayerChromium> surface = LayerChromium::create();
    RefPtr<FakeTiledLayerChromium> child = adoptRef(new FakeTiledLayerChromium(ccLayerTreeHost->contentsTextureManager()));
    RefPtr<FakeTiledLayerChromium> child2 = adoptRef(new FakeTiledLayerChromium(ccLayerTreeHost->contentsTextureManager()));

    root->setBounds(rootRect.size());
    root->setAnchorPoint(FloatPoint());
    root->setVisibleLayerRect(rootRect);
    root->addChild(surface);

    surface->setForceRenderSurface(true);
    surface->setAnchorPoint(FloatPoint());
    surface->setOpacity(0.5);
    surface->addChild(child);
    surface->addChild(child2);

    child->setBounds(childRect.size());
    child->setAnchorPoint(FloatPoint());
    child->setPosition(childRect.location());
    child->setVisibleLayerRect(childRect);

    child2->setBounds(child2Rect.size());
    child2->setAnchorPoint(FloatPoint());
    child2->setPosition(child2Rect.location());
    child2->setVisibleLayerRect(child2Rect);

    ccLayerTreeHost->setRootLayer(root);
    ccLayerTreeHost->setViewportSize(rootRect.size());

    // With a huge memory limit, all layers should update and push their textures.
    root->invalidateRect(rootRect);
    child->invalidateRect(childRect);
    child2->invalidateRect(child2Rect);
    ccLayerTreeHost->updateLayers(updater, std::numeric_limits<size_t>::max());
    {
        DebugScopedSetImplThread implThread;
        updateTextures(1000);
        EXPECT_EQ(6, root->fakeLayerTextureUpdater()->updateCount());
        EXPECT_EQ(3, child->fakeLayerTextureUpdater()->updateCount());
        EXPECT_EQ(3, child2->fakeLayerTextureUpdater()->updateCount());
        EXPECT_FALSE(updater.hasMoreUpdates());

        root->fakeLayerTextureUpdater()->clearUpdateCount();
        child->fakeLayerTextureUpdater()->clearUpdateCount();
        child2->fakeLayerTextureUpdater()->clearUpdateCount();

        OwnPtr<FakeCCTiledLayerImpl> rootImpl(adoptPtr(new FakeCCTiledLayerImpl(root->id())));
        OwnPtr<FakeCCTiledLayerImpl> childImpl(adoptPtr(new FakeCCTiledLayerImpl(child->id())));
        OwnPtr<FakeCCTiledLayerImpl> child2Impl(adoptPtr(new FakeCCTiledLayerImpl(child2->id())));
        root->pushPropertiesTo(rootImpl.get());
        child->pushPropertiesTo(childImpl.get());
        child2->pushPropertiesTo(child2Impl.get());

        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned j = 0; j < 2; ++j)
                EXPECT_TRUE(rootImpl->hasTextureIdForTileAt(i, j));
            EXPECT_TRUE(childImpl->hasTextureIdForTileAt(i, 0));
            EXPECT_TRUE(child2Impl->hasTextureIdForTileAt(i, 0));
        }
    }
    ccLayerTreeHost->commitComplete();

    // With a memory limit that includes only the root layer (3x2 tiles) and half the surface that
    // the child layers draw into, the child layers will not be allocated. If the surface isn't
    // accounted for, then one of the children would fit within the memory limit.
    root->invalidateRect(rootRect);
    child->invalidateRect(childRect);
    child2->invalidateRect(child2Rect);
    ccLayerTreeHost->updateLayers(updater, (3 * 2 + 3 * 1) * (100 * 100) * 4);
    {
        DebugScopedSetImplThread implThread;
        updateTextures(1000);
        EXPECT_EQ(6, root->fakeLayerTextureUpdater()->updateCount());
        EXPECT_EQ(0, child->fakeLayerTextureUpdater()->updateCount());
        EXPECT_EQ(0, child2->fakeLayerTextureUpdater()->updateCount());
        EXPECT_FALSE(updater.hasMoreUpdates());

        root->fakeLayerTextureUpdater()->clearUpdateCount();
        child->fakeLayerTextureUpdater()->clearUpdateCount();
        child2->fakeLayerTextureUpdater()->clearUpdateCount();

        OwnPtr<FakeCCTiledLayerImpl> rootImpl(adoptPtr(new FakeCCTiledLayerImpl(root->id())));
        OwnPtr<FakeCCTiledLayerImpl> childImpl(adoptPtr(new FakeCCTiledLayerImpl(child->id())));
        OwnPtr<FakeCCTiledLayerImpl> child2Impl(adoptPtr(new FakeCCTiledLayerImpl(child2->id())));
        root->pushPropertiesTo(rootImpl.get());
        child->pushPropertiesTo(childImpl.get());
        child2->pushPropertiesTo(child2Impl.get());

        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned j = 0; j < 2; ++j)
                EXPECT_TRUE(rootImpl->hasTextureIdForTileAt(i, j));
            EXPECT_FALSE(childImpl->hasTextureIdForTileAt(i, 0));
            EXPECT_FALSE(child2Impl->hasTextureIdForTileAt(i, 0));
        }
    }
    ccLayerTreeHost->commitComplete();

    // With a memory limit that includes only half the root layer, no contents will be
    // allocated. If render surface memory wasn't accounted for, there is enough space
    // for one of the children layers, but they draw into a surface that can't be
    // allocated.
    root->invalidateRect(rootRect);
    child->invalidateRect(childRect);
    child2->invalidateRect(child2Rect);
    ccLayerTreeHost->updateLayers(updater, (3 * 1) * (100 * 100) * 4);
    {
        DebugScopedSetImplThread implThread;
        updateTextures(1000);
        EXPECT_EQ(0, root->fakeLayerTextureUpdater()->updateCount());
        EXPECT_EQ(0, child->fakeLayerTextureUpdater()->updateCount());
        EXPECT_EQ(0, child2->fakeLayerTextureUpdater()->updateCount());
        EXPECT_FALSE(updater.hasMoreUpdates());

        root->fakeLayerTextureUpdater()->clearUpdateCount();
        child->fakeLayerTextureUpdater()->clearUpdateCount();
        child2->fakeLayerTextureUpdater()->clearUpdateCount();

        OwnPtr<FakeCCTiledLayerImpl> rootImpl(adoptPtr(new FakeCCTiledLayerImpl(root->id())));
        OwnPtr<FakeCCTiledLayerImpl> childImpl(adoptPtr(new FakeCCTiledLayerImpl(child->id())));
        OwnPtr<FakeCCTiledLayerImpl> child2Impl(adoptPtr(new FakeCCTiledLayerImpl(child2->id())));
        root->pushPropertiesTo(rootImpl.get());
        child->pushPropertiesTo(childImpl.get());
        child2->pushPropertiesTo(child2Impl.get());

        for (unsigned i = 0; i < 3; ++i) {
            for (unsigned j = 0; j < 2; ++j)
                EXPECT_FALSE(rootImpl->hasTextureIdForTileAt(i, j));
            EXPECT_FALSE(childImpl->hasTextureIdForTileAt(i, 0));
            EXPECT_FALSE(child2Impl->hasTextureIdForTileAt(i, 0));
        }
    }
    ccLayerTreeHost->commitComplete();

    ccLayerTreeHost->contentsTextureManager()->clearAllMemory(&allocator);
    ccLayerTreeHost->setRootLayer(0);
    ccLayerTreeHost.clear();
    WebKit::WebCompositor::shutdown();
}

class TrackingLayerPainter : public LayerPainterChromium {
public:
    static PassOwnPtr<TrackingLayerPainter> create() { return adoptPtr(new TrackingLayerPainter()); }

    virtual void paint(SkCanvas*, const IntRect& contentRect, FloatRect&) OVERRIDE
    {
        m_paintedRect = contentRect;
    }

    const IntRect& paintedRect() const { return m_paintedRect; }
    void resetPaintedRect() { m_paintedRect = IntRect(); }

private:
    TrackingLayerPainter() { }

    IntRect m_paintedRect;
};

class UpdateTrackingTiledLayerChromium : public FakeTiledLayerChromium {
public:
    explicit UpdateTrackingTiledLayerChromium(WebCore::CCPrioritizedTextureManager* manager)
        : FakeTiledLayerChromium(manager)
    {
        OwnPtr<TrackingLayerPainter> trackingLayerPainter(TrackingLayerPainter::create());
        m_trackingLayerPainter = trackingLayerPainter.get();
        m_layerTextureUpdater = BitmapCanvasLayerTextureUpdater::create(trackingLayerPainter.release(), false);
    }
    virtual ~UpdateTrackingTiledLayerChromium() { }

    TrackingLayerPainter* trackingLayerPainter() const { return m_trackingLayerPainter; }

protected:
    virtual WebCore::LayerTextureUpdater* textureUpdater() const OVERRIDE { return m_layerTextureUpdater.get(); }

private:
    TrackingLayerPainter* m_trackingLayerPainter;
    RefPtr<BitmapCanvasLayerTextureUpdater> m_layerTextureUpdater;
};

TEST_F(TiledLayerChromiumTest, nonIntegerContentsScaleIsNotDistortedDuringPaint)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager(CCPrioritizedTextureManager::create(4000000, 4000000));
    CCPriorityCalculator calculator;

    RefPtr<UpdateTrackingTiledLayerChromium> layer = adoptRef(new UpdateTrackingTiledLayerChromium(textureManager.get()));

    IntRect layerRect(0, 0, 30, 31);
    layer->setPosition(layerRect.location());
    layer->setBounds(layerRect.size());
    layer->setContentsScale(1.5);

    IntRect contentRect(0, 0, 45, 47);
    EXPECT_EQ(contentRect.size(), layer->contentBounds());
    layer->setVisibleLayerRect(contentRect);

    layer->setTexturePriorities(calculator);
    textureManager->prioritizeTextures(0);

    // Update the whole tile.
    layer->updateLayerRect(updater, contentRect, 0);
    layer->trackingLayerPainter()->resetPaintedRect();

    EXPECT_INT_RECT_EQ(IntRect(), layer->trackingLayerPainter()->paintedRect());

    updateTextures();

    // Invalidate the entire layer in content space. When painting, the rect given to webkit should match the layer's bounds.
    layer->invalidateRect(contentRect);
    layer->updateLayerRect(updater, contentRect, 0);

    EXPECT_INT_RECT_EQ(layerRect, layer->trackingLayerPainter()->paintedRect());
}

TEST_F(TiledLayerChromiumTest, nonIntegerContentsScaleIsNotDistortedDuringInvalidation)
{
    OwnPtr<CCPrioritizedTextureManager> textureManager(CCPrioritizedTextureManager::create(4000000, 4000000));
    CCPriorityCalculator calculator;

    RefPtr<UpdateTrackingTiledLayerChromium> layer = adoptRef(new UpdateTrackingTiledLayerChromium(textureManager.get()));

    IntRect layerRect(0, 0, 30, 31);
    layer->setPosition(layerRect.location());
    layer->setBounds(layerRect.size());
    layer->setContentsScale(1.3);

    IntRect contentRect(IntPoint(), layer->contentBounds());
    layer->setVisibleLayerRect(contentRect);

    layer->setTexturePriorities(calculator);
    textureManager->prioritizeTextures(0);

    // Update the whole tile.
    layer->updateLayerRect(updater, contentRect, 0);
    layer->trackingLayerPainter()->resetPaintedRect();

    EXPECT_INT_RECT_EQ(IntRect(), layer->trackingLayerPainter()->paintedRect());

    updateTextures();

    // Invalidate the entire layer in layer space. When painting, the rect given to webkit should match the layer's bounds.
    layer->setNeedsDisplayRect(layerRect);
    layer->updateLayerRect(updater, contentRect, 0);

    EXPECT_INT_RECT_EQ(layerRect, layer->trackingLayerPainter()->paintedRect());
}

} // namespace
