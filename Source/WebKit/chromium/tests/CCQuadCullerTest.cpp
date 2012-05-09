/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "cc/CCQuadCuller.h"

#include "cc/CCOcclusionTracker.h"
#include "cc/CCOverdrawMetrics.h"
#include "cc/CCSingleThreadProxy.h"
#include "cc/CCTiledLayerImpl.h"
#include "cc/CCTileDrawQuad.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class TestCCOcclusionTrackerImpl : public CCOcclusionTrackerImpl {
public:
    TestCCOcclusionTrackerImpl(const IntRect& scissorRectInScreen, bool recordMetricsForFrame = true)
        : CCOcclusionTrackerImpl(scissorRectInScreen, recordMetricsForFrame)
        , m_scissorRectInScreen(scissorRectInScreen)
    {
    }

protected:
    virtual IntRect layerScissorRectInTargetSurface(const CCLayerImpl* layer) const { return m_scissorRectInScreen; }

private:
    IntRect m_scissorRectInScreen;
};

typedef CCLayerIterator<CCLayerImpl, Vector<CCLayerImpl*>, CCRenderSurface, CCLayerIteratorActions::FrontToBack> CCLayerIteratorType;

static PassOwnPtr<CCTiledLayerImpl> makeLayer(CCTiledLayerImpl* parent, const TransformationMatrix& drawTransform, const IntRect& layerRect, float opacity, bool opaque, const IntRect& layerOpaqueRect, Vector<CCLayerImpl*>& surfaceLayerList)
{
    OwnPtr<CCTiledLayerImpl> layer = CCTiledLayerImpl::create(0);
    OwnPtr<CCLayerTilingData> tiler = CCLayerTilingData::create(IntSize(100, 100), CCLayerTilingData::NoBorderTexels);
    tiler->setBounds(layerRect.size());
    layer->setTilingData(*tiler);
    layer->setSkipsDraw(false);
    layer->setDrawTransform(drawTransform);
    layer->setScreenSpaceTransform(drawTransform);
    layer->setVisibleLayerRect(layerRect);
    layer->setDrawOpacity(opacity);
    layer->setOpaque(opaque);

    int textureId = 1;
    for (int i = 0; i < tiler->numTilesX(); ++i)
        for (int j = 0; j < tiler->numTilesY(); ++j) {
            IntRect tileOpaqueRect = opaque ? tiler->tileBounds(i, j) : intersection(tiler->tileBounds(i, j), layerOpaqueRect);
            layer->pushTileProperties(i, j, static_cast<Platform3DObject>(textureId++), tileOpaqueRect);
        }

    if (!parent) {
        layer->createRenderSurface();
        layer->setTargetRenderSurface(layer->renderSurface());
        surfaceLayerList.append(layer.get());
        layer->renderSurface()->layerList().append(layer.get());
    } else {
        layer->setTargetRenderSurface(parent->targetRenderSurface());
        parent->renderSurface()->layerList().append(layer.get());
    }

    return layer.release();
}

static void appendQuads(CCQuadList& quadList, Vector<OwnPtr<CCSharedQuadState> >& sharedStateList, CCTiledLayerImpl* layer, CCLayerIteratorType& it, CCOcclusionTrackerImpl& occlusionTracker)
{
    occlusionTracker.enterLayer(it);
    CCQuadCuller quadCuller(quadList, layer, &occlusionTracker, false);
    OwnPtr<CCSharedQuadState> sharedQuadState = layer->createSharedQuadState();
    bool hadMissingTiles = false;
    layer->appendQuads(quadCuller, sharedQuadState.get(), hadMissingTiles);
    sharedStateList.append(sharedQuadState.release());
    occlusionTracker.leaveLayer(it);
    ++it;
}

#define DECLARE_AND_INITIALIZE_TEST_QUADS               \
    DebugScopedSetImplThread impl;                      \
    CCQuadList quadList;                                \
    Vector<OwnPtr<CCSharedQuadState> > sharedStateList; \
    Vector<CCLayerImpl*> renderSurfaceLayerList;        \
    TransformationMatrix childTransform;                \
    IntSize rootSize = IntSize(300, 300);               \
    IntRect rootRect = IntRect(IntPoint(), rootSize);   \
    IntSize childSize = IntSize(200, 200);              \
    IntRect childRect = IntRect(IntPoint(), childSize);

TEST(CCQuadCullerTest, verifyNoCulling)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), TransformationMatrix(), childRect, 1.0, false, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 13u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 40000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

TEST(CCQuadCullerTest, verifyCullChildLinesUpTopLeft)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), TransformationMatrix(), childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 9u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 40000, 1);
}

TEST(CCQuadCullerTest, verifyCullWhenChildOpacityNotOne)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 0.9f, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 13u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 40000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

TEST(CCQuadCullerTest, verifyCullWhenChildOpaqueFlagFalse)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1.0, false, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 13u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 40000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

TEST(CCQuadCullerTest, verifyCullCenterTileOnly)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.translate(50, 50);

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 12u);

    IntRect quadVisibleRect1 = quadList[5].get()->quadVisibleRect();
    EXPECT_EQ(quadVisibleRect1.height(), 50);

    IntRect quadVisibleRect3 = quadList[7].get()->quadVisibleRect();
    EXPECT_EQ(quadVisibleRect3.width(), 50);

    // Next index is 8, not 9, since centre quad culled.
    IntRect quadVisibleRect4 = quadList[8].get()->quadVisibleRect();
    EXPECT_EQ(quadVisibleRect4.width(), 50);
    EXPECT_EQ(quadVisibleRect4.x(), 250);

    IntRect quadVisibleRect6 = quadList[10].get()->quadVisibleRect();
    EXPECT_EQ(quadVisibleRect6.height(), 50);
    EXPECT_EQ(quadVisibleRect6.y(), 250);

    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 100000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 30000, 1);
}

TEST(CCQuadCullerTest, verifyCullCenterTileNonIntegralSize1)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.translate(100, 100);

    // Make the root layer's quad have extent (99.1, 99.1) -> (200.9, 200.9) to make
    // sure it doesn't get culled due to transform rounding.
    TransformationMatrix rootTransform;
    rootTransform.translate(99.1, 99.1);
    rootTransform.scale(1.018);

    rootRect = childRect = IntRect(0, 0, 100, 100);

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, rootTransform, rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 2u);

    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 20363, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

TEST(CCQuadCullerTest, verifyCullCenterTileNonIntegralSize2)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    // Make the child's quad slightly smaller than, and centred over, the root layer tile.
    // Verify the child does not cause the quad below to be culled due to rounding.
    childTransform.translate(100.1, 100.1);
    childTransform.scale(0.982);

    TransformationMatrix rootTransform;
    rootTransform.translate(100, 100);

    rootRect = childRect = IntRect(0, 0, 100, 100);

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, rootTransform, rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 2u);

    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 19643, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

TEST(CCQuadCullerTest, verifyCullChildLinesUpBottomRight)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.translate(100, 100);

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 9u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 40000, 1);
}

TEST(CCQuadCullerTest, verifyCullSubRegion)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.translate(50, 50);

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    IntRect childOpaqueRect(childRect.x() + childRect.width() / 4, childRect.y() + childRect.height() / 4, childRect.width() / 2, childRect.height() / 2);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1.0, false, childOpaqueRect, renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 12u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 30000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 10000, 1);
}

TEST(CCQuadCullerTest, verifyCullSubRegion2)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.translate(50, 10);

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    IntRect childOpaqueRect(childRect.x() + childRect.width() / 4, childRect.y() + childRect.height() / 4, childRect.width() / 2, childRect.height() * 3 / 4);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1.0, false, childOpaqueRect, renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 12u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 25000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 15000, 1);
}

TEST(CCQuadCullerTest, verifyCullSubRegionCheckOvercull)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.translate(50, 49);

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    IntRect childOpaqueRect(childRect.x() + childRect.width() / 4, childRect.y() + childRect.height() / 4, childRect.width() / 2, childRect.height() / 2);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1.0, false, childOpaqueRect, renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 13u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 90000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 30000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 10000, 1);
}

TEST(CCQuadCullerTest, verifyNonAxisAlignedQuadsDontOcclude)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    // Use a small rotation so as to not disturb the geometry significantly.
    childTransform.rotate(1);

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), childTransform, childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 13u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 130000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}

// This test requires some explanation: here we are rotating the quads to be culled.
// The 2x2 tile child layer remains in the top-left corner, unrotated, but the 3x3
// tile parent layer is rotated by 1 degree. Of the four tiles the child would
// normally occlude, three will move (slightly) out from under the child layer, and
// one moves further under the child. Only this last tile should be culled.
TEST(CCQuadCullerTest, verifyNonAxisAlignedQuadsSafelyCulled)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    // Use a small rotation so as to not disturb the geometry significantly.
    TransformationMatrix parentTransform;
    parentTransform.rotate(1);

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, parentTransform, rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), TransformationMatrix(), childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(-100, -100, 1000, 1000));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 12u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 100600, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 29400, 1);
}

TEST(CCQuadCullerTest, verifyCullOutsideScissorOverTile)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), TransformationMatrix(), childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(200, 100, 100, 100));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 1u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 10000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 120000, 1);
}

TEST(CCQuadCullerTest, verifyCullOutsideScissorOverCulledTile)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), TransformationMatrix(), childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(100, 100, 100, 100));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 1u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 10000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 120000, 1);
}

TEST(CCQuadCullerTest, verifyCullOutsideScissorOverPartialTiles)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), TransformationMatrix(), childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(50, 50, 200, 200));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 9u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 40000, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 90000, 1);
}

TEST(CCQuadCullerTest, verifyCullOutsideScissorOverNoTiles)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), TransformationMatrix(), childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(500, 500, 100, 100));
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 0u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 130000, 1);
}

TEST(CCQuadCullerTest, verifyWithoutMetrics)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCTiledLayerImpl> rootLayer = makeLayer(0, TransformationMatrix(), rootRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    OwnPtr<CCTiledLayerImpl> childLayer = makeLayer(rootLayer.get(), TransformationMatrix(), childRect, 1.0, true, IntRect(), renderSurfaceLayerList);
    TestCCOcclusionTrackerImpl occlusionTracker(IntRect(50, 50, 200, 200), false);
    CCLayerIteratorType it = CCLayerIteratorType::begin(&renderSurfaceLayerList);

    appendQuads(quadList, sharedStateList, childLayer.get(), it, occlusionTracker);
    appendQuads(quadList, sharedStateList, rootLayer.get(), it, occlusionTracker);
    EXPECT_EQ(quadList.size(), 9u);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnOpaque(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsDrawnTranslucent(), 0, 1);
    EXPECT_NEAR(occlusionTracker.overdrawMetrics().pixelsCulledForDrawing(), 0, 1);
}


} // namespace
