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

#include "GraphicsLayerChromium.h"

#include "CompositorFakeWebGraphicsContext3D.h"
#include "GraphicsLayer.h"
#include "Matrix3DTransformOperation.h"
#include "RotateTransformOperation.h"
#include "ScrollableArea.h"
#include "TranslateTransformOperation.h"
#include <gtest/gtest.h>
#include <public/Platform.h>
#include <public/WebCompositorSupport.h>
#include <public/WebFloatAnimationCurve.h>
#include <public/WebGraphicsContext3D.h>
#include <public/WebLayerTreeView.h>
#include <public/WebTransformationMatrix.h>
#include <public/WebUnitTestSupport.h>
#include <wtf/PassOwnPtr.h>

using namespace WebCore;
using namespace WebKit;

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
  public:
    virtual void notifyAnimationStarted(const GraphicsLayer*, double time) OVERRIDE { }
    virtual void notifyFlushRequired(const GraphicsLayer*) OVERRIDE { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) OVERRIDE { }
    virtual float deviceScaleFactor() const OVERRIDE { return 2; }
};

class GraphicsLayerChromiumTest : public testing::Test {
public:
    GraphicsLayerChromiumTest()
    {
        Platform::current()->compositorSupport()->initialize(0);
        m_graphicsLayer = adoptPtr(new GraphicsLayerChromium(&m_client));
        m_platformLayer = m_graphicsLayer->platformLayer();
        m_layerTreeView = adoptPtr(Platform::current()->unitTestSupport()->createLayerTreeViewForTesting(WebUnitTestSupport::TestViewTypeUnitTest));
        ASSERT(m_layerTreeView);
        m_layerTreeView->setRootLayer(*m_platformLayer);
        m_layerTreeView->setViewportSize(WebSize(1, 1), WebSize(1, 1));
    }

    virtual ~GraphicsLayerChromiumTest()
    {
        m_graphicsLayer.clear();
        m_layerTreeView.clear();
        Platform::current()->compositorSupport()->shutdown();
    }

protected:
    static void expectTranslateX(double translateX, const WebTransformationMatrix& matrix)
    {
        EXPECT_FLOAT_EQ(translateX, matrix.m41());
    }

    WebLayer* m_platformLayer;
    OwnPtr<GraphicsLayerChromium> m_graphicsLayer;

private:
    OwnPtr<WebLayerTreeView> m_layerTreeView;
    MockGraphicsLayerClient m_client;
};

TEST_F(GraphicsLayerChromiumTest, updateLayerPreserves3DWithAnimations)
{
    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

    OwnPtr<WebFloatAnimationCurve> curve = adoptPtr(Platform::current()->compositorSupport()->createFloatAnimationCurve());
    curve->add(WebFloatKeyframe(0.0, 0.0));
    OwnPtr<WebAnimation> floatAnimation(adoptPtr(Platform::current()->compositorSupport()->createAnimation(*curve, WebAnimation::TargetPropertyOpacity)));
    int animationId = floatAnimation->id();
    ASSERT_TRUE(m_platformLayer->addAnimation(floatAnimation.get()));

    ASSERT_TRUE(m_platformLayer->hasActiveAnimation());

    m_graphicsLayer->setPreserves3D(true);

    m_platformLayer = m_graphicsLayer->platformLayer();
    ASSERT_TRUE(m_platformLayer);

    ASSERT_TRUE(m_platformLayer->hasActiveAnimation());
    m_platformLayer->removeAnimation(animationId);
    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

    m_graphicsLayer->setPreserves3D(false);

    m_platformLayer = m_graphicsLayer->platformLayer();
    ASSERT_TRUE(m_platformLayer);

    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());
}

class FakeScrollableArea : public ScrollableArea {
public:
    virtual bool isActive() const OVERRIDE { return false; }
    virtual int scrollSize(ScrollbarOrientation) const OVERRIDE { return 100; }
    virtual int scrollPosition(Scrollbar*) const OVERRIDE { return 0; }
    virtual bool isScrollCornerVisible() const OVERRIDE { return false; }
    virtual IntRect scrollCornerRect() const OVERRIDE { return IntRect(); }
    virtual int visibleWidth() const OVERRIDE { return 10; }
    virtual int visibleHeight() const OVERRIDE { return 10; }
    virtual IntSize contentsSize() const OVERRIDE { return IntSize(100, 100); }
    virtual bool scrollbarsCanBeActive() const OVERRIDE { return false; }
    virtual ScrollableArea* enclosingScrollableArea() const OVERRIDE { return 0; }
    virtual IntRect scrollableAreaBoundingBox() const OVERRIDE { return IntRect(); }
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&) OVERRIDE { }
    virtual void invalidateScrollCornerRect(const IntRect&) OVERRIDE { }

    virtual void setScrollOffset(const IntPoint& scrollOffset) OVERRIDE { m_scrollPosition = scrollOffset; }
    virtual IntPoint scrollPosition() const OVERRIDE { return m_scrollPosition; }

private:
    IntPoint m_scrollPosition;
};

TEST_F(GraphicsLayerChromiumTest, applyScrollToScrollableArea)
{
    FakeScrollableArea scrollableArea;
    m_graphicsLayer->setScrollableArea(&scrollableArea);

    WebPoint scrollPosition(7, 9);
    m_platformLayer->setScrollPosition(scrollPosition);

    EXPECT_EQ(scrollPosition, WebPoint(scrollableArea.scrollPosition()));
}

} // namespace
