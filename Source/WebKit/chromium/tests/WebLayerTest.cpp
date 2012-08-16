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
#include <public/WebLayer.h>

#include "CompositorFakeWebGraphicsContext3D.h"
#include <public/WebCompositor.h>
#include <public/WebContentLayer.h>
#include <public/WebContentLayerClient.h>
#include <public/WebExternalTextureLayer.h>
#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebLayerTreeView.h>
#include <public/WebLayerTreeViewClient.h>
#include <public/WebRect.h>
#include <public/WebSize.h>

#include <gmock/gmock.h>

using namespace WebKit;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Mock;
using testing::Test;
using testing::_;

namespace {

class MockWebLayerTreeViewClient : public WebLayerTreeViewClient {
public:
    MOCK_METHOD0(scheduleComposite, void());

    virtual void updateAnimations(double frameBeginTime) { }
    virtual void didBeginFrame() { }
    virtual void layout() { }
    virtual void applyScrollAndScale(const WebSize& scrollDelta, float scaleFactor) { }
    virtual WebGraphicsContext3D* createContext3D() { return CompositorFakeWebGraphicsContext3D::create(WebGraphicsContext3D::Attributes()).leakPtr(); }
    virtual void didRebindGraphicsContext(bool success) { }
    virtual WebCompositorOutputSurface* createOutputSurface() { return 0; }
    virtual void didRecreateOutputSurface(bool success) { }
    virtual void willCommit() { }
    virtual void didCommitAndDrawFrame() { }
    virtual void didCompleteSwapBuffers() { }
};

class MockWebContentLayerClient : public WebContentLayerClient {
public:
    MOCK_METHOD3(paintContents, void(WebCanvas*, const WebRect& clip, WebFloatRect& opaque));
};

class WebLayerTest : public Test {
public:
    virtual void SetUp()
    {
        // Initialize without threading support.
        WebKit::WebCompositor::initialize(0);
        m_rootLayer = WebLayer::create();
        EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
        EXPECT_TRUE(m_view.initialize(&m_client, m_rootLayer, WebLayerTreeView::Settings()));
        Mock::VerifyAndClearExpectations(&m_client);
    }

    virtual void TearDown()
    {
        // We may get any number of scheduleComposite calls during shutdown.
        EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
        m_view.setRootLayer(0);
        m_rootLayer.reset();
        m_view.reset();
        WebKit::WebCompositor::shutdown();
    }

protected:
    MockWebLayerTreeViewClient m_client;
    WebLayer m_rootLayer;
    WebLayerTreeView m_view;
};

// Tests that the client gets called to ask for a composite if we change the
// fields.
TEST_F(WebLayerTest, Client)
{
    // Base layer.
    EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
    WebLayer layer = WebLayer::create();
    m_rootLayer.addChild(layer);
    Mock::VerifyAndClearExpectations(&m_client);

    WebFloatPoint point(3, 4);
    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer.setAnchorPoint(point);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_EQ(point, layer.anchorPoint());

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    float anchorZ = 5;
    layer.setAnchorPointZ(anchorZ);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_EQ(anchorZ, layer.anchorPointZ());

    WebSize size(7, 8);
    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer.setBounds(size);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_EQ(size, layer.bounds());

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer.setMasksToBounds(true);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_TRUE(layer.masksToBounds());

    EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
    WebLayer otherLayer = WebLayer::create();
    m_rootLayer.addChild(otherLayer);
    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer.setMaskLayer(otherLayer);
    Mock::VerifyAndClearExpectations(&m_client);

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    float opacity = 0.123f;
    layer.setOpacity(opacity);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_EQ(opacity, layer.opacity());

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer.setOpaque(true);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_TRUE(layer.opaque());

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    layer.setPosition(point);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_EQ(point, layer.position());

    // Texture layer.
    EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
    WebExternalTextureLayer textureLayer = WebExternalTextureLayer::create();
    m_rootLayer.addChild(textureLayer);
    Mock::VerifyAndClearExpectations(&m_client);

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    textureLayer.setTextureId(3);
    Mock::VerifyAndClearExpectations(&m_client);

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    textureLayer.setFlipped(true);
    Mock::VerifyAndClearExpectations(&m_client);

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    WebFloatRect uvRect(0.1f, 0.1f, 0.9f, 0.9f);
    textureLayer.setUVRect(uvRect);
    Mock::VerifyAndClearExpectations(&m_client);


    // Content layer.
    MockWebContentLayerClient contentClient;
    EXPECT_CALL(contentClient, paintContents(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(m_client, scheduleComposite()).Times(AnyNumber());
    WebContentLayer contentLayer = WebContentLayer::create(&contentClient);
    m_rootLayer.addChild(contentLayer);
    Mock::VerifyAndClearExpectations(&m_client);

    EXPECT_CALL(m_client, scheduleComposite()).Times(AtLeast(1));
    contentLayer.setDrawsContent(false);
    Mock::VerifyAndClearExpectations(&m_client);
    EXPECT_FALSE(contentLayer.drawsContent());
}

}
