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

#include "cc/CCLayerIterator.h"

#include "LayerChromium.h"
#include "cc/CCLayerTreeHostCommon.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <public/WebTransformationMatrix.h>

using namespace WebCore;
using WebKit::WebTransformationMatrix;
using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace {

class TestLayerChromium : public LayerChromium {
public:
    static PassRefPtr<TestLayerChromium> create() { return adoptRef(new TestLayerChromium()); }

    int m_countRepresentingTargetSurface;
    int m_countRepresentingContributingSurface;
    int m_countRepresentingItself;

    virtual bool drawsContent() const OVERRIDE { return m_drawsContent; }
    void setDrawsContent(bool drawsContent) { m_drawsContent = drawsContent; }

private:
    TestLayerChromium()
        : LayerChromium()
        , m_drawsContent(true)
    {
        setBounds(IntSize(100, 100));
        setPosition(IntPoint::zero());
        setAnchorPoint(IntPoint::zero());
    }

    bool m_drawsContent;
};

#define EXPECT_COUNT(layer, target, contrib, itself) \
    EXPECT_EQ(target, layer->m_countRepresentingTargetSurface);          \
    EXPECT_EQ(contrib, layer->m_countRepresentingContributingSurface);   \
    EXPECT_EQ(itself, layer->m_countRepresentingItself);

typedef CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, CCLayerIteratorActions::FrontToBack> FrontToBack;
typedef CCLayerIterator<LayerChromium, Vector<RefPtr<LayerChromium> >, RenderSurfaceChromium, CCLayerIteratorActions::BackToFront> BackToFront;

void resetCounts(Vector<RefPtr<LayerChromium> >& renderSurfaceLayerList)
{
    for (unsigned surfaceIndex = 0; surfaceIndex < renderSurfaceLayerList.size(); ++surfaceIndex) {
        TestLayerChromium* renderSurfaceLayer = static_cast<TestLayerChromium*>(renderSurfaceLayerList[surfaceIndex].get());
        RenderSurfaceChromium* renderSurface = renderSurfaceLayer->renderSurface();

        renderSurfaceLayer->m_countRepresentingTargetSurface = -1;
        renderSurfaceLayer->m_countRepresentingContributingSurface = -1;
        renderSurfaceLayer->m_countRepresentingItself = -1;

        for (unsigned layerIndex = 0; layerIndex < renderSurface->layerList().size(); ++layerIndex) {
            TestLayerChromium* layer = static_cast<TestLayerChromium*>(renderSurface->layerList()[layerIndex].get());

            layer->m_countRepresentingTargetSurface = -1;
            layer->m_countRepresentingContributingSurface = -1;
            layer->m_countRepresentingItself = -1;
        }
    }
}

void iterateFrontToBack(Vector<RefPtr<LayerChromium> >* renderSurfaceLayerList)
{
    resetCounts(*renderSurfaceLayerList);
    int count = 0;
    for (FrontToBack it = FrontToBack::begin(renderSurfaceLayerList); it != FrontToBack::end(renderSurfaceLayerList); ++it, ++count) {
        TestLayerChromium* layer = static_cast<TestLayerChromium*>(*it);
        if (it.representsTargetRenderSurface())
            layer->m_countRepresentingTargetSurface = count;
        if (it.representsContributingRenderSurface())
            layer->m_countRepresentingContributingSurface = count;
        if (it.representsItself())
            layer->m_countRepresentingItself = count;
    }
}

void iterateBackToFront(Vector<RefPtr<LayerChromium> >* renderSurfaceLayerList)
{
    resetCounts(*renderSurfaceLayerList);
    int count = 0;
    for (BackToFront it = BackToFront::begin(renderSurfaceLayerList); it != BackToFront::end(renderSurfaceLayerList); ++it, ++count) {
        TestLayerChromium* layer = static_cast<TestLayerChromium*>(*it);
        if (it.representsTargetRenderSurface())
            layer->m_countRepresentingTargetSurface = count;
        if (it.representsContributingRenderSurface())
            layer->m_countRepresentingContributingSurface = count;
        if (it.representsItself())
            layer->m_countRepresentingItself = count;
    }
}

TEST(CCLayerIteratorTest, emptyTree)
{
    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;

    iterateBackToFront(&renderSurfaceLayerList);
    iterateFrontToBack(&renderSurfaceLayerList);
}

TEST(CCLayerIteratorTest, simpleTree)
{
    RefPtr<TestLayerChromium> rootLayer = TestLayerChromium::create();
    RefPtr<TestLayerChromium> first = TestLayerChromium::create();
    RefPtr<TestLayerChromium> second = TestLayerChromium::create();
    RefPtr<TestLayerChromium> third = TestLayerChromium::create();
    RefPtr<TestLayerChromium> fourth = TestLayerChromium::create();

    rootLayer->createRenderSurface();

    rootLayer->addChild(first);
    rootLayer->addChild(second);
    rootLayer->addChild(third);
    rootLayer->addChild(fourth);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    CCLayerTreeHostCommon::calculateDrawTransforms(rootLayer.get(), rootLayer->bounds(), 1, 256, renderSurfaceLayerList);
    CCLayerTreeHostCommon::calculateVisibleRects(renderSurfaceLayerList);

    iterateBackToFront(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 0, -1, 1);
    EXPECT_COUNT(first, -1, -1, 2);
    EXPECT_COUNT(second, -1, -1, 3);
    EXPECT_COUNT(third, -1, -1, 4);
    EXPECT_COUNT(fourth, -1, -1, 5);

    iterateFrontToBack(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 5, -1, 4);
    EXPECT_COUNT(first, -1, -1, 3);
    EXPECT_COUNT(second, -1, -1, 2);
    EXPECT_COUNT(third, -1, -1, 1);
    EXPECT_COUNT(fourth, -1, -1, 0);

}

TEST(CCLayerIteratorTest, complexTree)
{
    RefPtr<TestLayerChromium> rootLayer = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root1 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root2 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root3 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root21 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root22 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root23 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root221 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root231 = TestLayerChromium::create();

    rootLayer->createRenderSurface();

    rootLayer->addChild(root1);
    rootLayer->addChild(root2);
    rootLayer->addChild(root3);
    root2->addChild(root21);
    root2->addChild(root22);
    root2->addChild(root23);
    root22->addChild(root221);
    root23->addChild(root231);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    CCLayerTreeHostCommon::calculateDrawTransforms(rootLayer.get(), rootLayer->bounds(), 1, 256, renderSurfaceLayerList);
    CCLayerTreeHostCommon::calculateVisibleRects(renderSurfaceLayerList);

    iterateBackToFront(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 0, -1, 1);
    EXPECT_COUNT(root1, -1, -1, 2);
    EXPECT_COUNT(root2, -1, -1, 3);
    EXPECT_COUNT(root21, -1, -1, 4);
    EXPECT_COUNT(root22, -1, -1, 5);
    EXPECT_COUNT(root221, -1, -1, 6);
    EXPECT_COUNT(root23, -1, -1, 7);
    EXPECT_COUNT(root231, -1, -1, 8);
    EXPECT_COUNT(root3, -1, -1, 9);

    iterateFrontToBack(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 9, -1, 8);
    EXPECT_COUNT(root1, -1, -1, 7);
    EXPECT_COUNT(root2, -1, -1, 6);
    EXPECT_COUNT(root21, -1, -1, 5);
    EXPECT_COUNT(root22, -1, -1, 4);
    EXPECT_COUNT(root221, -1, -1, 3);
    EXPECT_COUNT(root23, -1, -1, 2);
    EXPECT_COUNT(root231, -1, -1, 1);
    EXPECT_COUNT(root3, -1, -1, 0);

}

TEST(CCLayerIteratorTest, complexTreeMultiSurface)
{
    RefPtr<TestLayerChromium> rootLayer = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root1 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root2 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root3 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root21 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root22 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root23 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root221 = TestLayerChromium::create();
    RefPtr<TestLayerChromium> root231 = TestLayerChromium::create();

    rootLayer->createRenderSurface();
    rootLayer->renderSurface()->setContentRect(IntRect(IntPoint(), rootLayer->bounds()));

    rootLayer->addChild(root1);
    rootLayer->addChild(root2);
    rootLayer->addChild(root3);
    root2->setDrawsContent(false);
    root2->setOpacity(0.5); // Force the layer to own a new surface.
    root2->addChild(root21);
    root2->addChild(root22);
    root2->addChild(root23);
    root22->setOpacity(0.5);
    root22->addChild(root221);
    root23->setOpacity(0.5);
    root23->addChild(root231);

    Vector<RefPtr<LayerChromium> > renderSurfaceLayerList;
    CCLayerTreeHostCommon::calculateDrawTransforms(rootLayer.get(), rootLayer->bounds(), 1, 256, renderSurfaceLayerList);
    CCLayerTreeHostCommon::calculateVisibleRects(renderSurfaceLayerList);

    iterateBackToFront(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 0, -1, 1);
    EXPECT_COUNT(root1, -1, -1, 2);
    EXPECT_COUNT(root2, 4, 3, -1);
    EXPECT_COUNT(root21, -1, -1, 5);
    EXPECT_COUNT(root22, 7, 6, 8);
    EXPECT_COUNT(root221, -1, -1, 9);
    EXPECT_COUNT(root23, 11, 10, 12);
    EXPECT_COUNT(root231, -1, -1, 13);
    EXPECT_COUNT(root3, -1, -1, 14);

    iterateFrontToBack(&renderSurfaceLayerList);
    EXPECT_COUNT(rootLayer, 14, -1, 13);
    EXPECT_COUNT(root1, -1, -1, 12);
    EXPECT_COUNT(root2, 10, 11, -1);
    EXPECT_COUNT(root21, -1, -1, 9);
    EXPECT_COUNT(root22, 7, 8, 6);
    EXPECT_COUNT(root221, -1, -1, 5);
    EXPECT_COUNT(root23, 3, 4, 2);
    EXPECT_COUNT(root231, -1, -1, 1);
    EXPECT_COUNT(root3, -1, -1, 0);
}

} // namespace
