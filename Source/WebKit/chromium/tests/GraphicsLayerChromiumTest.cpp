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

#include "CCLayerTreeHost.h"
#include "CCLayerTreeHostImpl.h"
#include "CCSingleThreadProxy.h"
#include "CompositorFakeWebGraphicsContext3D.h"
#include "GraphicsLayer.h"
#include "Matrix3DTransformOperation.h"
#include "RotateTransformOperation.h"
#include "TranslateTransformOperation.h"
#include "WebLayerTreeViewTestCommon.h"
#include <gtest/gtest.h>
#include <public/WebCompositor.h>
#include <public/WebFloatAnimationCurve.h>
#include <public/WebGraphicsContext3D.h>
#include <public/WebLayerTreeView.h>
#include <wtf/PassOwnPtr.h>

using namespace WebCore;
using namespace WebKit;

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
  public:
    virtual void notifyAnimationStarted(const GraphicsLayer*, double time) OVERRIDE { }
    virtual void notifySyncRequired(const GraphicsLayer*) OVERRIDE { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) OVERRIDE { }
    virtual bool showDebugBorders(const GraphicsLayer*) const OVERRIDE { return false; }
    virtual bool showRepaintCounter(const GraphicsLayer*) const OVERRIDE { return false; }
    virtual float deviceScaleFactor() const OVERRIDE { return 2; }
};

class GraphicsLayerChromiumTest : public testing::Test {
public:
    GraphicsLayerChromiumTest()
    {
        // For these tests, we will enable threaded animations.
        WebCompositor::setAcceleratedAnimationEnabled(true);
        WebCompositor::initialize(0);
        m_graphicsLayer = static_pointer_cast<GraphicsLayerChromium>(GraphicsLayer::create(&m_client));
        m_platformLayer = m_graphicsLayer->platformLayer();
        m_layerTreeView.initialize(&m_layerTreeViewClient, *m_platformLayer, WebLayerTreeView::Settings());
        m_layerTreeView.setViewportSize(WebSize(1, 1), WebSize(1, 1));
    }

    virtual ~GraphicsLayerChromiumTest()
    {
        m_graphicsLayer.clear();
        m_layerTreeView.reset();
        WebCompositor::shutdown();
    }

protected:
    static void expectTranslateX(double translateX, const WebTransformationMatrix& matrix)
    {
        EXPECT_FLOAT_EQ(translateX, matrix.m41());
    }

    WebLayer* m_platformLayer;
    OwnPtr<GraphicsLayerChromium> m_graphicsLayer;

private:
    MockWebLayerTreeViewClient m_layerTreeViewClient;
    WebLayerTreeView m_layerTreeView;
    MockGraphicsLayerClient m_client;
};

TEST_F(GraphicsLayerChromiumTest, updateLayerPreserves3DWithAnimations)
{
    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

    WebFloatAnimationCurve curve;
    curve.add(WebFloatKeyframe(0.0, 0.0));
    OwnPtr<WebAnimation> floatAnimation(adoptPtr(WebAnimation::create(curve, 1, 1, WebAnimation::TargetPropertyOpacity)));
    ASSERT_TRUE(m_platformLayer->addAnimation(floatAnimation.get()));

    ASSERT_TRUE(m_platformLayer->hasActiveAnimation());

    m_graphicsLayer->setPreserves3D(true);

    m_platformLayer = m_graphicsLayer->platformLayer();
    ASSERT_TRUE(m_platformLayer);

    ASSERT_TRUE(m_platformLayer->hasActiveAnimation());
    m_platformLayer->removeAnimation(1);
    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());

    m_graphicsLayer->setPreserves3D(false);

    m_platformLayer = m_graphicsLayer->platformLayer();
    ASSERT_TRUE(m_platformLayer);

    ASSERT_FALSE(m_platformLayer->hasActiveAnimation());
}

TEST_F(GraphicsLayerChromiumTest, shouldStartWithCorrectContentsScale)
{
    EXPECT_EQ(2, m_graphicsLayer->contentsScale());
}

} // namespace
