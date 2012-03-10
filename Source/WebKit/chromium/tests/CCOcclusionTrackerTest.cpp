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

class LayerChromiumWithForcedDrawsContent : public LayerChromium {
public:
    LayerChromiumWithForcedDrawsContent()
        : LayerChromium()
    {
    }

    virtual bool drawsContent() const { return true; }
    virtual Region opaqueContentsRegion() const
    {
        return intersection(m_opaquePaintRect, visibleLayerRect());
    }

    void setOpaquePaintRect(const IntRect& opaquePaintRect) { m_opaquePaintRect = opaquePaintRect; }

private:
    IntRect m_opaquePaintRect;
};

void setLayerPropertiesForTesting(LayerChromium* layer, const TransformationMatrix& transform, const FloatPoint& position, const IntSize& bounds)
{
    layer->setTransform(transform);
    layer->setSublayerTransform(TransformationMatrix());
    layer->setAnchorPoint(FloatPoint(0, 0));
    layer->setPosition(position);
    layer->setBounds(bounds);
}

void setLayerPropertiesForTesting(LayerChromiumWithForcedDrawsContent* layer, const TransformationMatrix& transform, const FloatPoint& position, const IntSize& bounds, bool opaque, bool opaqueLayers)
{
    setLayerPropertiesForTesting(layer, transform, position, bounds);
    if (opaqueLayers)
        layer->setOpaque(opaque);
    else {
        layer->setOpaque(false);
        if (opaque)
            layer->setOpaquePaintRect(IntRect(IntPoint(), bounds));
        else
            layer->setOpaquePaintRect(IntRect());
    }
}

// A subclass to expose the total current occlusion.
class TestCCOcclusionTracker : public CCOcclusionTracker {
public:
    TestCCOcclusionTracker(IntRect screenScissorRect)
        : CCOcclusionTracker(screenScissorRect)
        , m_overrideLayerScissorRect(false)
    {
    }

    TestCCOcclusionTracker(IntRect screenScissorRect, const CCOcclusionTrackerDamageClient* damageClient)
        : CCOcclusionTracker(screenScissorRect, damageClient)
        , m_overrideLayerScissorRect(false)
    {
    }

    Region occlusionInScreenSpace() const { return CCOcclusionTracker::m_stack.last().occlusionInScreen; }
    Region occlusionInTargetSurface() const { return CCOcclusionTracker::m_stack.last().occlusionInTarget; }

    void setOcclusionInScreenSpace(const Region& region) { CCOcclusionTracker::m_stack.last().occlusionInScreen = region; }
    void setOcclusionInTargetSurface(const Region& region) { CCOcclusionTracker::m_stack.last().occlusionInTarget = region; }

    void setLayerScissorRect(const IntRect& rect) { m_overrideLayerScissorRect = true; m_layerScissorRect = rect;}
    void useDefaultLayerScissorRect() { m_overrideLayerScissorRect = false; }

protected:
    virtual IntRect layerScissorRectInTargetSurface(const LayerChromium* layer) const { return m_overrideLayerScissorRect ? m_layerScissorRect : CCOcclusionTracker::layerScissorRectInTargetSurface(layer); }

private:
    bool m_overrideLayerScissorRect;
    IntRect m_layerScissorRect;
};

class TestDamageClient : public CCOcclusionTrackerDamageClient {
public:
    // The interface
    virtual FloatRect damageRect(const RenderSurfaceChromium*) const { return m_damageRect; }

    // Testing stuff
    TestDamageClient(const FloatRect& damageRect) : m_damageRect(damageRect) { }
    void setDamageRect(const FloatRect& damageRect) { m_damageRect = damageRect; }

private:
    FloatRect m_damageRect;
};

#define TEST_OPAQUE_AND_PAINTED_OPAQUE(FULLTESTNAME, SHORTTESTNAME) \
    TEST(FULLTESTNAME, opaqueLayer)                                 \
    {                                                               \
        SHORTTESTNAME(true);                                        \
    }                                                               \
    TEST(FULLTESTNAME, opaquePaintRect)                             \
    {                                                               \
        SHORTTESTNAME(false);                                       \
    }

void layerAddedToOccludedRegion(bool opaqueLayers)
{
    // This tests that the right transforms are being used.
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(layer);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(30, 30), IntSize(500, 500), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

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

    occlusion.useDefaultLayerScissorRect();
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(29, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 29, 70, 70)));
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(31, 30, 70, 70)));
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 31, 70, 70)));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

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

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerAddedToOccludedRegion, layerAddedToOccludedRegion);

void layerAddedToOccludedRegionWithRotation(bool opaqueLayers)
{
    // This tests that the right transforms are being used.
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(layer);

    TransformationMatrix layerTransform;
    layerTransform.translate(250, 250);
    layerTransform.rotate(90);
    layerTransform.translate(-250, -250);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
    setLayerPropertiesForTesting(layer.get(), layerTransform, FloatPoint(30, 30), IntSize(500, 500), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

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

    occlusion.useDefaultLayerScissorRect();
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(29, 30, 70, 70)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 29, 70, 70)));
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(31, 30, 70, 70)));
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 31, 70, 70)));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

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

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerAddedToOccludedRegionWithRotation, layerAddedToOccludedRegionWithRotation);

void layerAddedToOccludedRegionWithTranslation(bool opaqueLayers)
{
    // This tests that the right transforms are being used.
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(layer);

    TransformationMatrix layerTransform;
    layerTransform.translate(20, 20);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
    setLayerPropertiesForTesting(layer.get(), layerTransform, FloatPoint(30, 30), IntSize(500, 500), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

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

    occlusion.useDefaultLayerScissorRect();
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(50, 50, 50, 50)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(49, 50, 50, 50)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(50, 49, 50, 50)));
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(51, 50, 50, 50)));
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(50, 51, 50, 50)));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(50, 50, 50, 50)).isEmpty());
    EXPECT_EQ_RECT(IntRect(49, 50, 1, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(49, 50, 50, 50)));
    EXPECT_EQ_RECT(IntRect(49, 49, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(49, 49, 50, 50)));
    EXPECT_EQ_RECT(IntRect(50, 49, 50, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(50, 49, 50, 50)));
    EXPECT_EQ_RECT(IntRect(51, 49, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(51, 49, 50, 50)));
    EXPECT_EQ_RECT(IntRect(100, 50, 1, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(51, 50, 50, 50)));
    EXPECT_EQ_RECT(IntRect(51, 51, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(51, 51, 50, 50)));
    EXPECT_EQ_RECT(IntRect(50, 100, 50, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(50, 51, 50, 50)));
    EXPECT_EQ_RECT(IntRect(49, 51, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(49, 51, 50, 50)));

    occlusion.useDefaultLayerScissorRect();
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(50, 50, 50, 50)).isEmpty());
    EXPECT_EQ_RECT(IntRect(49, 50, 1, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(49, 50, 50, 50)));
    EXPECT_EQ_RECT(IntRect(49, 49, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(49, 49, 50, 50)));
    EXPECT_EQ_RECT(IntRect(50, 49, 50, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(50, 49, 50, 50)));
    EXPECT_EQ_RECT(IntRect(51, 49, 49, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(51, 49, 50, 50)));
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(51, 50, 50, 50)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(51, 51, 50, 50)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(50, 51, 50, 50)).isEmpty());
    EXPECT_EQ_RECT(IntRect(49, 51, 1, 49), occlusion.unoccludedContentRect(parent.get(), IntRect(49, 51, 50, 50)));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerAddedToOccludedRegionWithTranslation, layerAddedToOccludedRegionWithTranslation);

void layerAddedToOccludedRegionWithRotatedSurface(bool opaqueLayers)
{
    // This tests that the right transforms are being used.
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

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
    setLayerPropertiesForTesting(child.get(), childTransform, FloatPoint(30, 30), IntSize(500, 500));
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(10, 10), IntSize(500, 500), true, opaqueLayers);

    child->setMasksToBounds(true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

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

    occlusion.useDefaultLayerScissorRect();
    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(10, 430, 60, 70)));
    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(9, 430, 60, 70)));
    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(10, 429, 60, 70)));
    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(10, 430, 61, 70)));
    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(10, 430, 60, 71)));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

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
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(31, 40, 70, 60)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 41, 70, 60)));

    occlusion.useDefaultLayerScissorRect();
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 40, 70, 60)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(29, 40, 70, 60)));
    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(30, 39, 70, 60)));
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(31, 40, 70, 60)));
    EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(30, 41, 70, 60)));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));


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

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerAddedToOccludedRegionWithRotatedSurface, layerAddedToOccludedRegionWithRotatedSurface);

void layerAddedToOccludedRegionWithSurfaceAlreadyOnStack(bool opaqueLayers)
{
    // This tests that the right transforms are being used.
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

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
    setLayerPropertiesForTesting(child.get(), childTransform, FloatPoint(30, 30), IntSize(500, 500));
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(10, 10), IntSize(500, 500), true, opaqueLayers);

    // |child2| makes |parent|'s surface get considered by CCOcclusionTracker first, instead of |child|'s. This exercises different code in
    // leaveToTargetRenderSurface, as the target surface has already been seen.
    setLayerPropertiesForTesting(child2.get(), identityMatrix, FloatPoint(30, 30), IntSize(60, 20), true, opaqueLayers);

    child->setMasksToBounds(true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(-10, -10, 1000, 1000));

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

    occlusion.useDefaultLayerScissorRect();
    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(10, 430, 60, 70)));
    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(9, 430, 60, 70)));
    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(10, 429, 60, 70)));
    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(11, 430, 60, 70)));
    EXPECT_TRUE(occlusion.occluded(child.get(), IntRect(10, 431, 60, 70)));
    occlusion.setLayerScissorRect(IntRect(-10, -10, 1000, 1000));

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

    occlusion.useDefaultLayerScissorRect();
    EXPECT_TRUE(occlusion.unoccludedContentRect(child.get(), IntRect(10, 430, 60, 70)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(child.get(), IntRect(9, 430, 60, 70)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(child.get(), IntRect(9, 430, 60, 80)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(child.get(), IntRect(-10, 430, 60, 70)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(child.get(), IntRect(-10, 430, 60, 80)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(child.get(), IntRect(10, 429, 60, 70)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(child.get(), IntRect(11, 430, 60, 70)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(child.get(), IntRect(10, 431, 60, 70)).isEmpty());
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

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

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerAddedToOccludedRegionWithSurfaceAlreadyOnStack, layerAddedToOccludedRegionWithSurfaceAlreadyOnStack);

void layerAddedToOccludedRegionWithRotatedOffAxisSurface(bool opaqueLayers)
{
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

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
    setLayerPropertiesForTesting(child.get(), childTransform, FloatPoint(30, 30), IntSize(500, 500));
    setLayerPropertiesForTesting(layer.get(), layerTransform, FloatPoint(0, 0), IntSize(500, 500), true, opaqueLayers);

    child->setMasksToBounds(true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

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

    occlusion.markOccludedBehindLayer(child.get());
    occlusion.finishedTargetRenderSurface(child.get(), child->renderSurface());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(), occlusion.occlusionInScreenSpace().bounds());
    EXPECT_EQ(0u, occlusion.occlusionInScreenSpace().rects().size());
    EXPECT_EQ_RECT(IntRect(), occlusion.occlusionInTargetSurface().bounds());
    EXPECT_EQ(0u, occlusion.occlusionInTargetSurface().rects().size());

    EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(75, 55, 1, 1)));
    EXPECT_EQ_RECT(IntRect(75, 55, 1, 1), occlusion.unoccludedContentRect(parent.get(), IntRect(75, 55, 1, 1)));
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerAddedToOccludedRegionWithRotatedOffAxisSurface, layerAddedToOccludedRegionWithRotatedOffAxisSurface);

void layerAddedToOccludedRegionWithMultipleOpaqueLayers(bool opaqueLayers)
{
    // This is similar to the previous test but now we make a few opaque layers inside of |child| so that the occluded parts of child are not a simple rect.
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

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
    setLayerPropertiesForTesting(child.get(), childTransform, FloatPoint(30, 30), IntSize(500, 500));
    setLayerPropertiesForTesting(layer1.get(), identityMatrix, FloatPoint(10, 10), IntSize(500, 440), true, opaqueLayers);
    setLayerPropertiesForTesting(layer2.get(), identityMatrix, FloatPoint(10, 450), IntSize(500, 60), true, opaqueLayers);

    child->setMasksToBounds(true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

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

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerAddedToOccludedRegionWithMultipleOpaqueLayers, layerAddedToOccludedRegionWithMultipleOpaqueLayers);

void surfaceOcclusionWithOverlappingSiblingSurfaces(bool opaqueLayers)
{
    // This tests that the right transforms are being used.
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

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
    setLayerPropertiesForTesting(child1.get(), childTransform, FloatPoint(30, 30), IntSize(500, 500));
    setLayerPropertiesForTesting(layer1.get(), identityMatrix, FloatPoint(0, 0), IntSize(500, 500), true, opaqueLayers);
    setLayerPropertiesForTesting(child2.get(), childTransform, FloatPoint(20, 40), IntSize(500, 500));
    setLayerPropertiesForTesting(layer2.get(), identityMatrix, FloatPoint(0, 0), IntSize(500, 500), true, opaqueLayers);

    child1->setMasksToBounds(true);
    child2->setMasksToBounds(true);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(-10, -10, 1000, 1000));

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

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_surfaceOcclusionWithOverlappingSiblingSurfaces, surfaceOcclusionWithOverlappingSiblingSurfaces);

void surfaceOcclusionInScreenSpace(bool opaqueLayers)
{
    // This tests that the right transforms are being used.
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
    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
    setLayerPropertiesForTesting(child1.get(), childTransform, FloatPoint(30, 30), IntSize(10, 10));
    setLayerPropertiesForTesting(layer1.get(), identityMatrix, FloatPoint(-10, -10), IntSize(510, 510), true, opaqueLayers);
    setLayerPropertiesForTesting(child2.get(), childTransform, FloatPoint(20, 40), IntSize(10, 10));
    setLayerPropertiesForTesting(layer2.get(), identityMatrix, FloatPoint(-10, -10), IntSize(510, 510), true, opaqueLayers);

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

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(-20, -20, 1000, 1000));

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

    occlusion.useDefaultLayerScissorRect();
    EXPECT_TRUE(occlusion.occluded(child2.get(), IntRect(-10, 420, 70, 80)));
    EXPECT_TRUE(occlusion.occluded(child2.get(), IntRect(-11, 420, 70, 80)));
    EXPECT_TRUE(occlusion.occluded(child2.get(), IntRect(-10, 419, 70, 80)));
    EXPECT_TRUE(occlusion.occluded(child2.get(), IntRect(-10, 420, 71, 80)));
    EXPECT_TRUE(occlusion.occluded(child2.get(), IntRect(-10, 420, 70, 81)));
    occlusion.setLayerScissorRect(IntRect(-20, -20, 1000, 1000));

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

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_surfaceOcclusionInScreenSpace, surfaceOcclusionInScreenSpace);

void surfaceOcclusionInScreenSpaceDifferentTransforms(bool opaqueLayers)
{
    // This tests that the right transforms are being used.
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
    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
    setLayerPropertiesForTesting(child1.get(), child1Transform, FloatPoint(30, 20), IntSize(10, 10));
    setLayerPropertiesForTesting(layer1.get(), identityMatrix, FloatPoint(-10, -20), IntSize(510, 520), true, opaqueLayers);
    setLayerPropertiesForTesting(child2.get(), child2Transform, FloatPoint(20, 40), IntSize(10, 10));
    setLayerPropertiesForTesting(layer2.get(), identityMatrix, FloatPoint(-10, -10), IntSize(510, 510), true, opaqueLayers);

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

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(-30, -30, 1000, 1000));

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

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_surfaceOcclusionInScreenSpaceDifferentTransforms, surfaceOcclusionInScreenSpaceDifferentTransforms);

void occlusionInteractionWithFilters(bool opaqueLayers)
{
    // This tests that the right transforms are being used.
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

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(100, 100));
    setLayerPropertiesForTesting(blurLayer.get(), layerTransform, FloatPoint(30, 30), IntSize(500, 500), true, opaqueLayers);
    setLayerPropertiesForTesting(opaqueLayer.get(), layerTransform, FloatPoint(30, 30), IntSize(500, 500), true, opaqueLayers);
    setLayerPropertiesForTesting(opacityLayer.get(), layerTransform, FloatPoint(30, 30), IntSize(500, 500), true, opaqueLayers);

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

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));

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

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_occlusionInteractionWithFilters, occlusionInteractionWithFilters);

void layerScissorRectOverTile(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(200, 100, 100, 100));

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(200, 100, 100, 100)));

    occlusion.useDefaultLayerScissorRect();
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(200, 100, 100, 100)));
    occlusion.setLayerScissorRect(IntRect(200, 100, 100, 100));

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(200, 100, 100, 100), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)));
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerScissorRectOverTile, layerScissorRectOverTile);

void screenScissorRectOverTile(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(200, 100, 100, 100));

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    // Occluded since its outside the surface bounds.
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(200, 100, 100, 100)));

    // Test without any scissors.
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(200, 100, 100, 100)));
    occlusion.useDefaultLayerScissorRect();

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(200, 100, 100, 100), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)));
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_screenScissorRectOverTile, screenScissorRectOverTile);

void layerScissorRectOverCulledTile(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(100, 100, 100, 100));

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)).isEmpty());
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerScissorRectOverCulledTile, layerScissorRectOverCulledTile);

void screenScissorRectOverCulledTile(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(100, 100, 100, 100));

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)).isEmpty());
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_screenScissorRectOverCulledTile, screenScissorRectOverCulledTile);

void layerScissorRectOverPartialTiles(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(50, 50, 200, 200));

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(50, 50, 200, 200), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)));
    EXPECT_EQ_RECT(IntRect(200, 50, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 100)));
    EXPECT_EQ_RECT(IntRect(200, 100, 50, 100), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 100, 300, 100)));
    EXPECT_EQ_RECT(IntRect(200, 100, 50, 100), occlusion.unoccludedContentRect(parent.get(), IntRect(200, 100, 100, 100)));
    EXPECT_EQ_RECT(IntRect(100, 200, 100, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(100, 200, 100, 100)));
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerScissorRectOverPartialTiles, layerScissorRectOverPartialTiles);

void screenScissorRectOverPartialTiles(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(50, 50, 200, 200));

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(50, 50, 200, 200), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)));
    EXPECT_EQ_RECT(IntRect(200, 50, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 100)));
    EXPECT_EQ_RECT(IntRect(200, 100, 50, 100), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 100, 300, 100)));
    EXPECT_EQ_RECT(IntRect(200, 100, 50, 100), occlusion.unoccludedContentRect(parent.get(), IntRect(200, 100, 100, 100)));
    EXPECT_EQ_RECT(IntRect(100, 200, 100, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(100, 200, 100, 100)));
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_screenScissorRectOverPartialTiles, screenScissorRectOverPartialTiles);

void layerScissorRectOverNoTiles(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
    occlusion.setLayerScissorRect(IntRect(500, 500, 100, 100));

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 100)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 100, 300, 100)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(200, 100, 100, 100)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(100, 200, 100, 100)).isEmpty());
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerScissorRectOverNoTiles, layerScissorRectOverNoTiles);

void screenScissorRectOverNoTiles(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(500, 500, 100, 100));

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 100)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 100, 300, 100)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(200, 100, 100, 100)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(100, 200, 100, 100)).isEmpty());
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_screenScissorRectOverNoTiles, screenScissorRectOverNoTiles);

void layerScissorRectForLayerOffOrigin(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(100, 100), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    // This layer is translated when drawn into its target. So if the scissor rect given from the target surface
    // is not in that target space, then after translating these query rects into the target, they will fall outside
    // the scissor and be considered occluded.
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_layerScissorRectForLayerOffOrigin, layerScissorRectForLayerOffOrigin);

void damageRectOverTile(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestDamageClient damage(FloatRect(200, 100, 100, 100));
    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000), &damage);

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    // Outside the layer's clip rect.
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(200, 100, 100, 100)));

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(200, 100, 100, 100), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)));
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_damageRectOverTile, damageRectOverTile);

void damageRectOverCulledTile(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestDamageClient damage(FloatRect(100, 100, 100, 100));
    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000), &damage);

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)).isEmpty());
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_damageRectOverCulledTile, damageRectOverCulledTile);

void damageRectOverPartialTiles(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestDamageClient damage(FloatRect(50, 50, 200, 200));
    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000), &damage);

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_EQ_RECT(IntRect(50, 50, 200, 200), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)));
    EXPECT_EQ_RECT(IntRect(200, 50, 50, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 100)));
    EXPECT_EQ_RECT(IntRect(200, 100, 50, 100), occlusion.unoccludedContentRect(parent.get(), IntRect(0, 100, 300, 100)));
    EXPECT_EQ_RECT(IntRect(200, 100, 50, 100), occlusion.unoccludedContentRect(parent.get(), IntRect(200, 100, 100, 100)));
    EXPECT_EQ_RECT(IntRect(100, 200, 100, 50), occlusion.unoccludedContentRect(parent.get(), IntRect(100, 200, 100, 100)));
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_damageRectOverPartialTiles, damageRectOverPartialTiles);

void damageRectOverNoTiles(bool opaqueLayers)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> parentLayer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(parentLayer);
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    parentLayer->setFilters(filters);
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(parentLayer.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300), false, opaqueLayers);
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), true, opaqueLayers);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestDamageClient damage(FloatRect(500, 500, 100, 100));
    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000), &damage);

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());
    occlusion.enterTargetRenderSurface(parentLayer->renderSurface());

    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 100, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 0, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(0, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(100, 200, 100, 100)));
    EXPECT_TRUE(occlusion.occluded(parentLayer.get(), IntRect(200, 200, 100, 100)));

    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 300)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 0, 300, 100)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(0, 100, 300, 100)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(200, 100, 100, 100)).isEmpty());
    EXPECT_TRUE(occlusion.unoccludedContentRect(parent.get(), IntRect(100, 200, 100, 100)).isEmpty());
}

TEST_OPAQUE_AND_PAINTED_OPAQUE(CCOcclusionTrackerTest_damageRectOverNoTiles, damageRectOverNoTiles);

TEST(CCOcclusionTrackerTest, opaqueContentsRegionEmpty)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(layer);

    FilterOperations filters;
    filters.operations().append(BasicComponentTransferFilterOperation::create(0.5, FilterOperation::GRAYSCALE));
    layer->setFilters(filters);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(0, 0), IntSize(200, 200), false, false);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));

    occlusion.enterTargetRenderSurface(layer->renderSurface());

    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(0, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 0, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(0, 100, 100, 100)));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(100, 100, 100, 100)));

    // Occluded since its outside the surface bounds.
    EXPECT_TRUE(occlusion.occluded(layer.get(), IntRect(200, 100, 100, 100)));

    // Test without any scissors.
    occlusion.setLayerScissorRect(IntRect(0, 0, 1000, 1000));
    EXPECT_FALSE(occlusion.occluded(layer.get(), IntRect(200, 100, 100, 100)));
    occlusion.useDefaultLayerScissorRect();

    occlusion.markOccludedBehindLayer(layer.get());
    occlusion.leaveToTargetRenderSurface(parent->renderSurface());

    EXPECT_TRUE(occlusion.occlusionInScreenSpace().bounds().isEmpty());
    EXPECT_EQ(0u, occlusion.occlusionInScreenSpace().rects().size());
}

TEST(CCOcclusionTrackerTest, opaqueContentsRegionNonEmpty)
{
    const TransformationMatrix identityMatrix;
    RefPtr<LayerChromium> parent = LayerChromium::create();
    RefPtr<LayerChromiumWithForcedDrawsContent> layer = adoptRef(new LayerChromiumWithForcedDrawsContent());
    parent->createRenderSurface();
    parent->addChild(layer);

    setLayerPropertiesForTesting(parent.get(), identityMatrix, FloatPoint(0, 0), IntSize(300, 300));
    setLayerPropertiesForTesting(layer.get(), identityMatrix, FloatPoint(100, 100), IntSize(200, 200), false, false);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    Vector<RefPtr<LayerChromium> > dummyLayerList;
    int dummyMaxTextureSize = 512;

    parent->renderSurface()->setContentRect(IntRect(IntPoint::zero(), parent->bounds()));
    parent->setClipRect(IntRect(IntPoint::zero(), parent->bounds()));
    renderSurfaceLayerList.append(parent);

    CCLayerTreeHostCommon::calculateDrawTransformsAndVisibility(parent.get(), parent.get(), identityMatrix, identityMatrix, renderSurfaceLayerList, dummyLayerList, dummyMaxTextureSize);

    {
        TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
        layer->setOpaquePaintRect(IntRect(0, 0, 100, 100));

        occlusion.enterTargetRenderSurface(parent->renderSurface());
        occlusion.markOccludedBehindLayer(layer.get());

        EXPECT_EQ_RECT(IntRect(100, 100, 100, 100), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());

        EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(0, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(100, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(200, 200, 100, 100)));
    }

    {
        TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
        layer->setOpaquePaintRect(IntRect(20, 20, 180, 180));

        occlusion.enterTargetRenderSurface(parent->renderSurface());
        occlusion.markOccludedBehindLayer(layer.get());

        EXPECT_EQ_RECT(IntRect(120, 120, 180, 180), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());

        EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(100, 100, 100, 100)));
        EXPECT_TRUE(occlusion.occluded(parent.get(), IntRect(200, 200, 100, 100)));
    }

    {
        TestCCOcclusionTracker occlusion(IntRect(0, 0, 1000, 1000));
        layer->setOpaquePaintRect(IntRect(150, 150, 100, 100));

        occlusion.enterTargetRenderSurface(parent->renderSurface());
        occlusion.markOccludedBehindLayer(layer.get());

        EXPECT_EQ_RECT(IntRect(250, 250, 50, 50), occlusion.occlusionInScreenSpace().bounds());
        EXPECT_EQ(1u, occlusion.occlusionInScreenSpace().rects().size());

        EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(0, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(100, 100, 100, 100)));
        EXPECT_FALSE(occlusion.occluded(parent.get(), IntRect(200, 200, 100, 100)));
    }
}

} // namespace
