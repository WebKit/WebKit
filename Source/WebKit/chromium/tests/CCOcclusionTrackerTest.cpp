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

#include "cc/CCOcclusionTracker.h"

#include "FilterOperations.h"
#include "LayerChromium.h"
#include "Region.h"
#include "TransformationMatrix.h"
#include "cc/CCLayerTreeHostCommon.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;

#define EXPECT_EQ_RECT(a, b) \
    EXPECT_EQ(a.x(), b.x()); \
    EXPECT_EQ(a.y(), b.y()); \
    EXPECT_EQ(a.width(), b.width()); \
    EXPECT_EQ(a.height(), b.height());

namespace {

void setLayerPropertiesForTesting(LayerChromium* layer, const TransformationMatrix& transform, const TransformationMatrix& sublayerTransform, const FloatPoint& anchor, const FloatPoint& position, const IntSize& bounds, bool opaque)
{
    layer->setTransform(transform);
    layer->setSublayerTransform(sublayerTransform);
    layer->setAnchorPoint(anchor);
    layer->setPosition(position);
    layer->setBounds(bounds);
    layer->setOpaque(opaque);
}

class LayerChromiumWithForcedDrawsContent : public LayerChromium {
public:
    LayerChromiumWithForcedDrawsContent()
        : LayerChromium()
    {
    }

    virtual bool drawsContent() const { return true; }
};

// A subclass to expose the total current occlusion.
class TestCCOcclusionTracker : public CCOcclusionTracker {
public:
    Region occlusionInScreenSpace() const { return CCOcclusionTracker::m_stack.last().occlusionInScreen; }
    Region occlusionInTargetSurface() const { return CCOcclusionTracker::m_stack.last().occlusionInTarget; }

    void setOcclusionInScreenSpace(const Region& region) { CCOcclusionTracker::m_stack.last().occlusionInScreen = region; }
    void setOcclusionInTargetSurface(const Region& region) { CCOcclusionTracker::m_stack.last().occlusionInTarget = region; }
};

TEST(CCOcclusionTrackerTest, layerAddedToOccludedRegion)
{
    // This tests that the right transforms are being used.
    TestCCOcclusionTracker occlusion;
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(layer);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    occlusion.enterTargetRenderSurface(parent->renderSurface());
    occlusion.markOccludedBehindLayer(layer.get());
    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(29, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 29, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(31, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 31, 70, 70)));

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(30, 30, 70, 70)).isEmpty());
    EXPECT_EQ_RECT(IntRect(29, 30, 1, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(29, 30, 70, 70)));
    EXPECT_EQ_RECT(IntRect(29, 29, 70, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(29, 29, 70, 70)));
    EXPECT_EQ_RECT(IntRect(30, 29, 70, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(30, 29, 70, 70)));
    EXPECT_EQ_RECT(IntRect(31, 29, 70, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(31, 29, 70, 70)));
    EXPECT_EQ_RECT(IntRect(100, 30, 1, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(31, 30, 70, 70)));
    EXPECT_EQ_RECT(IntRect(31, 31, 70, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(31, 31, 70, 70)));
    EXPECT_EQ_RECT(IntRect(30, 100, 70, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(30, 31, 70, 70)));
    EXPECT_EQ_RECT(IntRect(29, 31, 70, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(29, 31, 70, 70)));
}

TEST(CCOcclusionTrackerTest, layerAddedToOccludedRegionWithRotation)
{
    // This tests that the right transforms are being used.
    TestCCOcclusionTracker occlusion;
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(layer);

    TransformationMatrix layerTransform;
    layerTransform.translate(250, 250);
    layerTransform.rotate(90);
    layerTransform.translate(-250, -250);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(layer.get(), layerTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    occlusion.enterTargetRenderSurface(parent->renderSurface());
    occlusion.markOccludedBehindLayer(layer.get());
    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(29, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 29, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(31, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 31, 70, 70)));

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(30, 30, 70, 70)).isEmpty());
    EXPECT_EQ_RECT(IntRect(29, 30, 1, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(29, 30, 70, 70)));
    EXPECT_EQ_RECT(IntRect(29, 29, 70, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(29, 29, 70, 70)));
    EXPECT_EQ_RECT(IntRect(30, 29, 70, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(30, 29, 70, 70)));
    EXPECT_EQ_RECT(IntRect(31, 29, 70, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(31, 29, 70, 70)));
    EXPECT_EQ_RECT(IntRect(100, 30, 1, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(31, 30, 70, 70)));
    EXPECT_EQ_RECT(IntRect(31, 31, 70, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(31, 31, 70, 70)));
    EXPECT_EQ_RECT(IntRect(30, 100, 70, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(30, 31, 70, 70)));
    EXPECT_EQ_RECT(IntRect(29, 31, 70, 70), occlusion.unoccludedContentRect(parent.get(), IntRect(29, 31, 70, 70)));
}

TEST(CCOcclusionTrackerTest, layerAddedToOccludedRegionWithTranslation)
{
    // This tests that the right transforms are being used.
    TestCCOcclusionTracker occlusion;
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(layer);

    TransformationMatrix layerTransform;
    layerTransform.translate(20, 20);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(layer.get(), layerTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    occlusion.enterTargetRenderSurface(parent->renderSurface());
    occlusion.markOccludedBehindLayer(layer.get());
    EXPECT_EQ_RECT(IntRect(50, 50, 50, 50), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(50, 50, 50, 50), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(50, 50, 50, 50)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(49, 50, 50, 50)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(50, 49, 50, 50)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(51, 50, 50, 50)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(50, 51, 50, 50)));

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(50, 50, 50, 50)).isEmpty());
    EXPECT_EQ_RECT(IntRect(49, 50, 1, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(49, 50, 50, 50)));
    EXPECT_EQ_RECT(IntRect(49, 49, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(49, 49, 50, 50)));
    EXPECT_EQ_RECT(IntRect(50, 49, 50, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(50, 49, 50, 50)));
    EXPECT_EQ_RECT(IntRect(51, 49, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(51, 49, 50, 50)));
    EXPECT_EQ_RECT(IntRect(100, 50, 1, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(51, 50, 50, 50)));
    EXPECT_EQ_RECT(IntRect(51, 51, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(51, 51, 50, 50)));
    EXPECT_EQ_RECT(IntRect(50, 100, 50, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(50, 51, 50, 50)));
    EXPECT_EQ_RECT(IntRect(49, 51, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(49, 51, 50, 50)));
}

TEST(CCOcclusionTrackerTest, layerAddedToOccludedRegionWithRotatedSurface)
{
    // This tests that the right transforms are being used.
    TestCCOcclusionTracker occlusion;
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(layer);

    TransformationMatrix childTransform;
    childTransform.translate(250, 250);
    childTransform.rotate(90);
    childTransform.translate(-250, -250);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(child.get(), childTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), false);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

    child->setMasksToBounds(true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    occlusion.enterTargetRenderSurface(child->renderSurface());
    occlusion.markOccludedBehindLayer(layer.get());

    EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(10, 430, 60, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(10, 430, 60, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(9, 430, 60, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(10, 429, 60, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(10, 430, 61, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(10, 430, 60, 71)));

    occlusion.markOccludedBehindLayer(child.get());
    occlusion.finishedTargetRenderSurface(child.get(), child->renderSurface());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 40, 70, 60)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(29, 40, 70, 60)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 39, 70, 60)));


    /* Justification for the above occlusion from |layer|:
               100
      +---------------------+                                      +---------------------+
      |                     |                                      |                     |30  Visible region of |layer|: /////
      |    30               |           rotate(90)                 |                     |
      | 30 + ---------------------------------+                    |     +---------------------------------+
  100 |    |  10            |                 |            ==>     |     |               |10               |
      |    |10+---------------------------------+                  |  +---------------------------------+  |
      |    |  |             |                 | |                  |  |  |///////////////|     420      |  |
      |    |  |             |                 | |                  |  |  |///////////////|60            |  |
      |    |  |             |                 | |                  |  |  |///////////////|              |  |
      +----|--|-------------+                 | |                  +--|--|---------------+              |  |
           |  |                               | |                   20|10|     70                       |  |
           |  |                               | |                     |  |                              |  |
           |  |                               | |500                  |  |                              |  |
           |  |                               | |                     |  |                              |  |
           |  |                               | |                     |  |                              |  |
           |  |                               | |                     |  |                              |  |
           |  |                               | |                     |  |                              |10|
           +--|-------------------------------+ |                     |  +------------------------------|--+
              |                                 |                     |                 490             |
              +---------------------------------+                     +---------------------------------+
                             500                                                     500
     */
}

TEST(CCOcclusionTrackerTest, layerAddedToOccludedRegionWithSurfaceAlreadyOnStack)
{
    // This tests that the right transforms are being used.
    TestCCOcclusionTracker occlusion;
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> child2 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(layer);
    parent->addChild(child2);

    TransformationMatrix childTransform;
    childTransform.translate(250, 250);
    childTransform.rotate(90);
    childTransform.translate(-250, -250);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(child.get(), childTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), false);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 500), true);

    // |child2| makes |parent|'s surface get considered by CCOcclusionTracker first, instead of |child|'s. This exercises different code in
    // leaveToTargetRenderSurface, as the target surface has already been seen.
    setLayerPropertiesForTesting(child2.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(60, 20), true);

    child->setMasksToBounds(true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    occlusion.enterTargetRenderSurface(parent->renderSurface());
    occlusion.markOccludedBehindLayer(child2.get());

    EXPECT_EQ_RECT(IntRect(30, 30, 60, 20), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(30, 30, 60, 20), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    occlusion.enterTargetRenderSurface(child->renderSurface());
    occlusion.markOccludedBehindLayer(layer.get());

    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(10, 430, 60, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(10, 430, 60, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(9, 430, 60, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(10, 429, 60, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(11, 430, 60, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(10, 431, 60, 70)));

    EXPECT_TRUE(occlusion.unoccludedContentRect(child.get(), IntRect(10, 430, 60, 70)).isEmpty());
    // This is the little piece not occluded by child2
    EXPECT_EQ_RECT(IntRect(9, 430, 1, 10), occlusion.unoccludedContentRect(child.get(), IntRect(9, 430, 60, 70)));
    // This extends past both sides of child2, so it will be the original rect.
    EXPECT_EQ_RECT(IntRect(9, 430, 60, 80), occlusion.unoccludedContentRect(child.get(), IntRect(9, 430, 60, 80)));
    // This extends past two adjacent sides of child2, and should included the unoccluded parts of each side.
    // This also demonstrates that the rect can be arbitrary and does not get clipped to the layer's visibleLayerRect().
    EXPECT_EQ_RECT(IntRect(-10, 430, 20, 70), occlusion.unoccludedContentRect(child.get(), IntRect(-10, 430, 60, 70)));
    // This extends past three adjacent sides of child2, so it should contain the unoccluded parts of each side. The left
    // and bottom edges are completely unoccluded for some row/column so we get back the original query rect.
    EXPECT_EQ_RECT(IntRect(-10, 430, 60, 80), occlusion.unoccludedContentRect(child.get(), IntRect(-10, 430, 60, 80)));
    EXPECT_EQ_RECT(IntRect(10, 429, 60, 1), occlusion.unoccludedContentRect(child.get(), IntRect(10, 429, 60, 70)));
    EXPECT_EQ_RECT(IntRect(70, 430, 1, 70), occlusion.unoccludedContentRect(child.get(), IntRect(11, 430, 60, 70)));
    EXPECT_EQ_RECT(IntRect(10, 500, 60, 1), occlusion.unoccludedContentRect(child.get(), IntRect(10, 431, 60, 70)));

    // Surface is not occluded by things that draw into itself.
    EXPECT_EQ_RECT(IntRect(10, 430, 60, 70), occlusion.surfaceUnoccludedContentRect(child.get(), IntRect(10, 430, 60, 70)));

    occlusion.markOccludedBehindLayer(child.get());
    // |child2| should get merged with the surface we are leaving now
    occlusion.finishedTargetRenderSurface(child.get(), child->renderSurface());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(2u, occlusion.occlusionInTargetSurface().rects().size());

    Vector<IntRect> screen = occlusion.occlusionInScreenSpace().rects();
    Vector<IntRect> target = occlusion.occlusionInTargetSurface().rects();

    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 30, 70, 70)));
    EXPECT_EQ_RECT(IntRect(90, 30, 10, 10), occlusion.unoccludedContentRect(parent.get(), IntRect(30, 30, 70, 70)));

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 30, 60, 10)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(29, 30, 60, 10)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 29, 60, 10)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(31, 30, 60, 10)));
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 31, 60, 10)));

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 40, 70, 60)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(29, 40, 70, 60)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 39, 70, 60)));

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(30, 30, 60, 10)).isEmpty());
    EXPECT_EQ_RECT(IntRect(29, 30, 1, 10), occlusion.unoccludedContentRect(parent.get(), IntRect(29, 30, 60, 10)));
    EXPECT_EQ_RECT(IntRect(30, 29, 60, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(30, 29, 60, 10)));
    EXPECT_EQ_RECT(IntRect(90, 30, 1, 10), occlusion.unoccludedContentRect(parent.get(), IntRect(31, 30, 60, 10)));
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(30, 31, 60, 10)).isEmpty());

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(30, 40, 70, 60)).isEmpty());
    EXPECT_EQ_RECT(IntRect(29, 40, 1, 60), occlusion.unoccludedContentRect(parent.get(), IntRect(29, 40, 70, 60)));
    // This rect is mostly occluded by |child2|.
    EXPECT_EQ_RECT(IntRect(90, 39, 10, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(30, 39, 70, 60)));
    // This rect extends past top/right ends of |child2|.
    EXPECT_EQ_RECT(IntRect(30, 29, 70, 11), occlusion.unoccludedContentRect(parent.get(), IntRect(30, 29, 70, 70)));
    // This rect extends past left/right ends of |child2|.
    EXPECT_EQ_RECT(IntRect(20, 39, 80, 60), occlusion.unoccludedContentRect(parent.get(), IntRect(20, 39, 80, 60)));
    EXPECT_EQ_RECT(IntRect(100, 40, 1, 60), occlusion.unoccludedContentRect(parent.get(), IntRect(31, 40, 70, 60)));
    EXPECT_EQ_RECT(IntRect(30, 100, 70, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(30, 41, 70, 60)));

    // Surface is not occluded by things that draw into itself.
    EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), occlusion.surfaceUnoccludedContentRect(parent.get(), IntRect(30, 40, 70, 60)));


    /* Justification for the above occlusion from |layer|:
               100
      +---------------------+                                      +---------------------+
      |                     |                                      |                     |30  Visible region of |layer|: /////
      |    30               |           rotate(90)                 |     30   60         |    |child2|: \\\\\
      | 30 + ------------+--------------------+                    |  30 +------------+--------------------+
  100 |    |  10         |  |                 |            ==>     |     |\\\\\\\\\\\\|  |10               |
      |    |10+----------|----------------------+                  |  +--|\\\\\\\\\\\\|-----------------+  |
      |    + ------------+  |                 | |                  |  |  +------------+//|     420      |  |
      |    |  |             |                 | |                  |  |  |///////////////|60            |  |
      |    |  |             |                 | |                  |  |  |///////////////|              |  |
      +----|--|-------------+                 | |                  +--|--|---------------+              |  |
           |  |                               | |                   20|10|     70                       |  |
           |  |                               | |                     |  |                              |  |
           |  |                               | |500                  |  |                              |  |
           |  |                               | |                     |  |                              |  |
           |  |                               | |                     |  |                              |  |
           |  |                               | |                     |  |                              |  |
           |  |                               | |                     |  |                              |10|
           +--|-------------------------------+ |                     |  +------------------------------|--+
              |                                 |                     |                 490             |
              +---------------------------------+                     +---------------------------------+
                             500                                                     500
     */
}

TEST(CCOcclusionTrackerTest, layerAddedToOccludedRegionWithRotatedOffAxisSurface)
{
    TestCCOcclusionTracker occlusion;
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(layer);

    // Now rotate the child a little more so it is not axis-aligned. The parent will no longer be occluded by |layer|, but
    // the child's occlusion should be unchanged.

    TransformationMatrix childTransform;
    childTransform.translate(250, 250);
    childTransform.rotate(95);
    childTransform.translate(-250, -250);

    TransformationMatrix layerTransform;
    layerTransform.translate(10, 10);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(child.get(), childTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), false);
    setLayerPropertiesForTesting(layer.get(), layerTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), true);

    child->setMasksToBounds(true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    IntRect clippedLayerInChild = layerTransform.mapRect(layer->visibleLayerRect());

    occlusion.enterTargetRenderSurface(child->renderSurface());
    occlusion.markOccludedBehindLayer(layer.get());

    EXPECT_EQ_RECT(IntRect(), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(0u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(clippedLayerInChild, occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(child.get(), clippedLayerInChild));
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), clippedLayerInChild).isEmpty());
    clippedLayerInChild.move(-1, 0);
    EXPECT_FALSE(occlusion.occluded(child.get(), clippedLayerInChild));
    EXPECT_FALSE(occlusion.unoccludedContentRect(parent.get(), clippedLayerInChild).isEmpty());
    clippedLayerInChild.move(1, 0);
    clippedLayerInChild.move(1, 0);
    EXPECT_FALSE(occlusion.occluded(child.get(), clippedLayerInChild));
    EXPECT_FALSE(occlusion.unoccludedContentRect(parent.get(), clippedLayerInChild).isEmpty());
    clippedLayerInChild.move(-1, 0);
    clippedLayerInChild.move(0, -1);
    EXPECT_FALSE(occlusion.occluded(child.get(), clippedLayerInChild));
    EXPECT_FALSE(occlusion.unoccludedContentRect(parent.get(), clippedLayerInChild).isEmpty());
    clippedLayerInChild.move(0, 1);
    clippedLayerInChild.move(0, 1);
    EXPECT_FALSE(occlusion.occluded(child.get(), clippedLayerInChild));
    EXPECT_FALSE(occlusion.unoccludedContentRect(parent.get(), clippedLayerInChild).isEmpty());
    clippedLayerInChild.move(0, -1);

    // Surface is not occluded by things that draw into itself.
    EXPECT_EQ_RECT(IntRect(0, 0, 500, 500), occlusion.surfaceUnoccludedContentRect(child.get(), IntRect(0, 0, 500, 500)));

    occlusion.markOccludedBehindLayer(child.get());
    occlusion.finishedTargetRenderSurface(child.get(), child->renderSurface());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(0u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(0u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(75, 55, 1, 1)));
    EXPECT_EQ_RECT(IntRect(75, 55, 1, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(75, 55, 1, 1)));

    // Surface is not occluded by things that draw into itself.
    EXPECT_EQ_RECT(IntRect(0, 0, 100, 100), occlusion.surfaceUnoccludedContentRect(parent.get(), IntRect(0, 0, 100, 100)));
}

TEST(CCOcclusionTrackerTest, layerAddedToOccludedRegionWithMultipleOpaqueLayers)
{
    // This is similar to the previous test but now we make a few opaque layers inside of |child| so that the occluded parts of child are not a simple rect.
    TestCCOcclusionTracker occlusion;
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer1 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer2 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(child);
    child->addChild(layer1);
    child->addChild(layer2);

    TransformationMatrix childTransform;
    childTransform.translate(250, 250);
    childTransform.rotate(90);
    childTransform.translate(-250, -250);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(child.get(), childTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), false);
    setLayerPropertiesForTesting(layer1.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(10, 10), IntSize(500, 440), true);
    setLayerPropertiesForTesting(layer2.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(10, 450), IntSize(500, 60), true);

    child->setMasksToBounds(true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    occlusion.enterTargetRenderSurface(child->renderSurface());
    occlusion.markOccludedBehindLayer(layer2.get());
    occlusion.markOccludedBehindLayer(layer1.get());

    EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(10, 430, 60, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(10, 430, 60, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(9, 430, 60, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(10, 429, 60, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(11, 430, 60, 70)));
    EXPECT_FALSE(occlusion.occluded(child.get(), IntRect(10, 431, 60, 70)));

    EXPECT_TRUE(occlusion.unoccludedContentRect(child.get(), IntRect(10, 430, 60, 70)).isEmpty());
    EXPECT_EQ_RECT(IntRect(9, 430, 1, 70), occlusion.unoccludedContentRect(child.get(), IntRect(9, 430, 60, 70)));
    EXPECT_EQ_RECT(IntRect(10, 429, 60, 1), occlusion.unoccludedContentRect(child.get(), IntRect(10, 429, 60, 70)));
    EXPECT_EQ_RECT(IntRect(70, 430, 1, 70), occlusion.unoccludedContentRect(child.get(), IntRect(11, 430, 60, 70)));
    EXPECT_EQ_RECT(IntRect(10, 500, 60, 1), occlusion.unoccludedContentRect(child.get(), IntRect(10, 431, 60, 70)));

    // Surface is not occluded by things that draw into itself.
    EXPECT_EQ_RECT(IntRect(10, 430, 60, 70), occlusion.surfaceUnoccludedContentRect(child.get(), IntRect(10, 430, 60, 70)));

    occlusion.markOccludedBehindLayer(child.get());
    occlusion.finishedTargetRenderSurface(child.get(), child->renderSurface());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 40, 70, 60)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(29, 40, 70, 60)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 39, 70, 60)));

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(30, 40, 70, 60)).isEmpty());
    EXPECT_EQ_RECT(IntRect(29, 40, 1, 60), occlusion.unoccludedContentRect(parent.get(), IntRect(29, 40, 70, 60)));
    EXPECT_EQ_RECT(IntRect(30, 39, 70, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(30, 39, 70, 60)));
    EXPECT_EQ_RECT(IntRect(100, 40, 1, 60), occlusion.unoccludedContentRect(parent.get(), IntRect(31, 40, 70, 60)));
    EXPECT_EQ_RECT(IntRect(30, 100, 70, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(30, 41, 70, 60)));

    // Surface is not occluded by things that draw into itself.
    EXPECT_EQ_RECT(IntRect(30, 40, 70, 60), occlusion.surfaceUnoccludedContentRect(parent.get(), IntRect(30, 40, 70, 60)));


    /* Justification for the above occlusion from |layer1| and |layer2|:

       +---------------------+
       |                     |30  Visible region of |layer1|: /////
       |                     |    Visible region of |layer2|: \\\\\
       |     +---------------------------------+
       |     |               |10               |
       |  +---------------+-----------------+  |
       |  |  |\\\\\\\\\\\\|//|     420      |  |
       |  |  |\\\\\\\\\\\\|//|60            |  |
       |  |  |\\\\\\\\\\\\|//|              |  |
       +--|--|------------|--+              |  |
        20|10|     70     |                 |  |
          |  |            |                 |  |
          |  |            |                 |  |
          |  |            |                 |  |
          |  |            |                 |  |
          |  |            |                 |  |
          |  |            |                 |10|
          |  +------------|-----------------|--+
          |               | 490             |
          +---------------+-----------------+
                60               440
     */
}

TEST(CCOcclusionTrackerTest, surfaceOcclusionWithOverlappingSiblingSurfaces)
{
    // This tests that the right transforms are being used.
    TestCCOcclusionTracker occlusion;
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child1 = LayerChromium::create();
    RefPtr<LayerChromium> child2 = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer1 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer2 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(child1);
    parent->addChild(child2);
    child1->addChild(layer1);
    child2->addChild(layer2);

    TransformationMatrix childTransform;
    childTransform.translate(250, 250);
    childTransform.rotate(90);
    childTransform.translate(-250, -250);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(child1.get(), childTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), false);
    setLayerPropertiesForTesting(layer1.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), true);
    setLayerPropertiesForTesting(child2.get(), childTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(20, 40), IntSize(500, 500), false);
    setLayerPropertiesForTesting(layer2.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(500, 500), true);

    child1->setMasksToBounds(true);
    child2->setMasksToBounds(true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    occlusion.enterTargetRenderSurface(child2->renderSurface());
    occlusion.markOccludedBehindLayer(layer2.get());

    EXPECT_EQ_RECT(IntRect(20, 40, 80, 60), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(0, 420, 60, 80), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(child2.get(), IntRect(0, 420, 60, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(-1, 420, 60, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(0, 419, 60, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(0, 420, 61, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(0, 420, 60, 81)));

    // Surface is not occluded by things that draw into itself.
    EXPECT_EQ_RECT(IntRect(0, 420, 60, 80), occlusion.surfaceUnoccludedContentRect(child2.get(), IntRect(0, 420, 60, 80)));

    occlusion.markOccludedBehindLayer(child2.get());
    occlusion.finishedTargetRenderSurface(child2.get(), child2->renderSurface());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(child1->renderSurface());
    occlusion.markOccludedBehindLayer(layer1.get());

    EXPECT_EQ_RECT(IntRect(20, 30, 80, 70), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(0, 430, 70, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(child1.get(), IntRect(0, 430, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(-1, 430, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(0, 429, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(0, 430, 71, 70)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(0, 430, 70, 71)));

    // Surface is not occluded by things that draw into itself, but the |child1| surface should be occluded by the |child2| surface.
    EXPECT_EQ_RECT(IntRect(0, 430, 10, 70), occlusion.surfaceUnoccludedContentRect(child1.get(), IntRect(0, 430, 70, 70)));

    occlusion.markOccludedBehindLayer(child1.get());
    occlusion.finishedTargetRenderSurface(child1.get(), child1->renderSurface());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(20, 30, 80, 70), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(20, 30, 80, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(2u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(20, 30, 80, 70)));

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(29, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 29, 70, 70)));

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(20, 40, 80, 60)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(19, 40, 80, 60)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(20, 39, 80, 60)));

    // |child1| and |child2| both draw into parent so they should not occlude it.
    EXPECT_EQ_RECT(IntRect(20, 30, 80, 70), occlusion.surfaceUnoccludedContentRect(parent.get(), IntRect(20, 30, 80, 70)));


    /* Justification for the above occlusion:
               100
      +---------------------+
      |                     |
      |    30               |       child1
      |  30+ ---------------------------------+
  100 |  40|                |     child2      |
      |20+----------------------------------+ |
      |  | |                |               | |
      |  | |                |               | |
      |  | |                |               | |
      +--|-|----------------+               | |
         | |                                | | 500
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | +--------------------------------|-+
         |                                  |
         +----------------------------------+
                         500
     */
}

TEST(CCOcclusionTrackerTest, surfaceOcclusionInScreenSpace)
{
    // This tests that the right transforms are being used.
    TestCCOcclusionTracker occlusion;
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child1 = LayerChromium::create();
    RefPtr<LayerChromium> child2 = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer1 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer2 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(child1);
    parent->addChild(child2);
    child1->addChild(layer1);
    child2->addChild(layer2);

    TransformationMatrix childTransform;
    childTransform.translate(250, 250);
    childTransform.rotate(90);
    childTransform.translate(-250, -250);

    // The owning layers have very different bounds from the surfaces that they own.
    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(child1.get(), childTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(10, 10), false);
    setLayerPropertiesForTesting(layer1.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(-10, -10), IntSize(510, 510), true);
    setLayerPropertiesForTesting(child2.get(), childTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(20, 40), IntSize(10, 10), false);
    setLayerPropertiesForTesting(layer2.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(-10, -10), IntSize(510, 510), true);

    // Make them both render surfaces
    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    child1->setFilters(filters);
    child2->setFilters(filters);

    child1->setMasksToBounds(false);
    child2->setMasksToBounds(false);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    occlusion.enterTargetRenderSurface(child2->renderSurface());
    occlusion.markOccludedBehindLayer(layer2.get());

    EXPECT_EQ_RECT(IntRect(20, 30, 80, 70), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(-10, 420, 70, 80), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(child2.get(), IntRect(-10, 420, 70, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(-11, 420, 70, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(-10, 419, 70, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(-10, 420, 71, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(-10, 420, 70, 81)));

    // Surface is not occluded by things that draw into itself.
    EXPECT_EQ_RECT(IntRect(-10, 420, 70, 80), occlusion.surfaceUnoccludedContentRect(child2.get(), IntRect(-10, 420, 70, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(child2.get(), IntRect(30, 250, 1, 1)));

    occlusion.markOccludedBehindLayer(child2.get());
    occlusion.finishedTargetRenderSurface(child2.get(), child2->renderSurface());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(child1->renderSurface());
    occlusion.markOccludedBehindLayer(layer1.get());

    EXPECT_EQ_RECT(IntRect(20, 20, 80, 80), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(-10, 430, 80, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(child1.get(), IntRect(-10, 430, 80, 70)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(-11, 430, 80, 70)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(-10, 429, 80, 70)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(-10, 430, 81, 70)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(-10, 430, 80, 71)));

    // Surface is not occluded by things that draw into itself, but the |child1| surface should be occluded by the |child2| surface.
    EXPECT_EQ_RECT(IntRect(-10, 430, 10, 80), occlusion.surfaceUnoccludedContentRect(child1.get(), IntRect(-10, 430, 70, 80)));
    EXPECT_TRUE(occlusion.surfaceOccluded(child1.get(), IntRect(0, 430, 70, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(child1.get(), IntRect(-1, 430, 70, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(child1.get(), IntRect(0, 429, 70, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(child1.get(), IntRect(1, 430, 70, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(child1.get(), IntRect(0, 431, 70, 80)));

    occlusion.markOccludedBehindLayer(child1.get());
    occlusion.finishedTargetRenderSurface(child1.get(), child1->renderSurface());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(20, 20, 80, 80), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(2u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(20, 20, 80, 80), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(2u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(20, 20, 80, 80)));

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 20, 70, 80)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(29, 20, 70, 80)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 19, 70, 80)));

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(20, 30, 80, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(19, 30, 80, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(20, 29, 80, 70)));

    // |child1| and |child2| both draw into parent so they should not occlude it.
    EXPECT_EQ_RECT(IntRect(20, 20, 80, 80), occlusion.surfaceUnoccludedContentRect(parent.get(), IntRect(20, 20, 80, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(parent.get(), IntRect(50, 50, 1, 1)));


    /* Justification for the above occlusion:
               100
      +---------------------+
      |    20               |       layer1
      |  30+ ---------------------------------+
  100 |  30|                |     layer2      |
      |20+----------------------------------+ |
      |  | |                |               | |
      |  | |                |               | |
      |  | |                |               | |
      +--|-|----------------+               | |
         | |                                | | 510
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | |                                | |
         | +--------------------------------|-+
         |                                  |
         +----------------------------------+
                         510
     */
}

TEST(CCOcclusionTrackerTest, surfaceOcclusionInScreenSpaceDifferentTransforms)
{
    // This tests that the right transforms are being used.
    TestCCOcclusionTracker occlusion;
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromium> child1 = LayerChromium::create();
    RefPtr<LayerChromium> child2 = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer1 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer2 = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(child1);
    parent->addChild(child2);
    child1->addChild(layer1);
    child2->addChild(layer2);

    TransformationMatrix child1Transform;
    child1Transform.translate(250, 250);
    child1Transform.rotate(-90);
    child1Transform.translate(-250, -250);

    TransformationMatrix child2Transform;
    child2Transform.translate(250, 250);
    child2Transform.rotate(90);
    child2Transform.translate(-250, -250);

    // The owning layers have very different bounds from the surfaces that they own.
    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(child1.get(), child1Transform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 20), IntSize(10, 10), false);
    setLayerPropertiesForTesting(layer1.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(-10, -20), IntSize(510, 520), true);
    setLayerPropertiesForTesting(child2.get(), child2Transform, identityMatrix, FloatPoint(0, 0), FloatPoint(20, 40), IntSize(10, 10), false);
    setLayerPropertiesForTesting(layer2.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(-10, -10), IntSize(510, 510), true);

    // Make them both render surfaces
    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    child1->setFilters(filters);
    child2->setFilters(filters);

    child1->setMasksToBounds(false);
    child2->setMasksToBounds(false);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    occlusion.enterTargetRenderSurface(child2->renderSurface());
    occlusion.markOccludedBehindLayer(layer2.get());

    EXPECT_EQ_RECT(IntRect(20, 30, 80, 70), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(-10, 420, 70, 80), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(child2.get(), IntRect(-10, 420, 70, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(-11, 420, 70, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(-10, 419, 70, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(-10, 420, 71, 80)));
    EXPECT_FALSE(occlusion.occluded(child2.get(), IntRect(-10, 420, 70, 81)));

    // Surface is not occluded by things that draw into itself.
    EXPECT_EQ_RECT(IntRect(-10, 420, 70, 80), occlusion.surfaceUnoccludedContentRect(child2.get(), IntRect(-10, 420, 70, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(child2.get(), IntRect(30, 250, 1, 1)));

    occlusion.markOccludedBehindLayer(child2.get());
    occlusion.finishedTargetRenderSurface(child2.get(), child2->renderSurface());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(child1->renderSurface());
    occlusion.markOccludedBehindLayer(layer1.get());

    EXPECT_EQ_RECT(IntRect(10, 20, 90, 80), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(420, -20, 80, 90), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(child1.get(), IntRect(420, -20, 80, 90)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(419, -20, 80, 90)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(420, -21, 80, 90)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(420, -19, 80, 90)));
    EXPECT_FALSE(occlusion.occluded(child1.get(), IntRect(421, -20, 80, 90)));

    // Surface is not occluded by things that draw into itself, but the |child1| surface should be occluded by the |child2| surface.
    EXPECT_EQ_RECT(IntRect(420, -20, 80, 90), occlusion.surfaceUnoccludedContentRect(child1.get(), IntRect(420, -20, 80, 90)));
    EXPECT_EQ_RECT(IntRect(490, -10, 10, 80), occlusion.surfaceUnoccludedContentRect(child1.get(), IntRect(420, -10, 80, 80)));
    EXPECT_EQ_RECT(IntRect(420, -20, 70, 10), occlusion.surfaceUnoccludedContentRect(child1.get(), IntRect(420, -20, 70, 90)));
    EXPECT_TRUE(occlusion.surfaceOccluded(child1.get(), IntRect(420, -10, 70, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(child1.get(), IntRect(419, -10, 70, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(child1.get(), IntRect(420, -11, 70, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(child1.get(), IntRect(421, -10, 70, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(child1.get(), IntRect(420, -9, 70, 80)));

    occlusion.markOccludedBehindLayer(child1.get());
    occlusion.finishedTargetRenderSurface(child1.get(), child1->renderSurface());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(10, 20, 90, 80), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(10, 20, 90, 80), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(10, 20, 90, 80)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(9, 20, 90, 80)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(10, 19, 90, 80)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(11, 20, 90, 80)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(10, 21, 90, 80)));

    // |child1| and |child2| both draw into parent so they should not occlude it.
    EXPECT_EQ_RECT(IntRect(10, 20, 90, 80), occlusion.surfaceUnoccludedContentRect(parent.get(), IntRect(10, 20, 90, 80)));
    EXPECT_FALSE(occlusion.surfaceOccluded(parent.get(), IntRect(10, 20, 1, 1)));
    EXPECT_FALSE(occlusion.surfaceOccluded(parent.get(), IntRect(99, 20, 1, 1)));
    EXPECT_FALSE(occlusion.surfaceOccluded(parent.get(), IntRect(10, 9, 1, 1)));
    EXPECT_FALSE(occlusion.surfaceOccluded(parent.get(), IntRect(99, 99, 1, 1)));


    /* Justification for the above occlusion:
               100
      +---------------------+
      |20                   |       layer1
     10+----------------------------------+
  100 || 30                 |     layer2  |
      |20+----------------------------------+
      || |                  |             | |
      || |                  |             | |
      || |                  |             | |
      +|-|------------------+             | |
       | |                                | | 510
       | |                            510 | |
       | |                                | |
       | |                                | |
       | |                                | |
       | |                                | |
       | |                520             | |
       +----------------------------------+ |
         |                                  |
         +----------------------------------+
                         510
     */
}

TEST(CCOcclusionTrackerTest, occlusionInteractionWithFilters)
{
    // This tests that the right transforms are being used.
    TestCCOcclusionTracker occlusion;
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> blurLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> opacityLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> opaqueLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(blurLayer);
    parent->addChild(opacityLayer);
    parent->addChild(opaqueLayer);

    TransformationMatrix layerTransform;
    layerTransform.translate(250, 250);
    layerTransform.rotate(90);
    layerTransform.translate(-250, -250);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, identityMatrix, FloatPoint(0, 0), FloatPoint(0, 0), IntSize(100, 100), false);
    setLayerPropertiesForTesting(blurLayer.get(), layerTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
    setLayerPropertiesForTesting(opaqueLayer.get(), layerTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);
    setLayerPropertiesForTesting(opacityLayer.get(), layerTransform, identityMatrix, FloatPoint(0, 0), FloatPoint(30, 30), IntSize(500, 500), true);

    {
        FilterOperations filters;
        filters.operations().append(BlurFilterOperation::create(Length(10, WebCore::Percent), FilterOperation::BLUR));
        blurLayer->setFilters(filters);
    }

    {
        FilterOperations filters;
        filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
        opaqueLayer->setFilters(filters);
    }

    {
        FilterOperations filters;
        filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::OPACITY));
        opacityLayer->setFilters(filters);
    }


    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    // Opacity layer won't contribute to occlusion.
    occlusion.enterTargetRenderSurface(opacityLayer->renderSurface());
    occlusion.markOccludedBehindLayer(opacityLayer.get());
    occlusion.finishedTargetRenderSurface(opacityLayer.get(), opacityLayer->renderSurface());

    EXPECT_TRUE(occlusion.occlusionInScreenSpace().isEmpty());
    EXPECT_TRUE(occlusion.occlusionInTargetSurface().isEmpty());

    // And has nothing to contribute to its parent surface.
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    EXPECT_TRUE(occlusion.occlusionInScreenSpace().isEmpty());
    EXPECT_TRUE(occlusion.occlusionInTargetSurface().isEmpty());

    // Opaque layer will contribute to occlusion.
    occlusion.enterTargetRenderSurface(opaqueLayer->renderSurface());
    occlusion.markOccludedBehindLayer(opaqueLayer.get());
    occlusion.finishedTargetRenderSurface(opaqueLayer.get(), opaqueLayer->renderSurface());

    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(0, 430, 70, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    // And it gets translated to the parent surface.
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());

    // The blur layer needs to throw away any occlusion from outside its subtree.
    occlusion.enterTargetRenderSurface(blurLayer->renderSurface());
    EXPECT_TRUE(occlusion.occlusionInScreenSpace().isEmpty());
    EXPECT_TRUE(occlusion.occlusionInTargetSurface().isEmpty());

    // And it won't contribute to occlusion.
    occlusion.markOccludedBehindLayer(blurLayer.get());
    occlusion.finishedTargetRenderSurface(blurLayer.get(), blurLayer->renderSurface());
    EXPECT_TRUE(occlusion.occlusionInScreenSpace().isEmpty());
    EXPECT_TRUE(occlusion.occlusionInTargetSurface().isEmpty());

    // But the opaque layer's occlusion is preserved on the parent.
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(30, 30, 70, 70), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(1u, occlusion.occlusionInTargetSurface().rects().size());
}

} // namespace
