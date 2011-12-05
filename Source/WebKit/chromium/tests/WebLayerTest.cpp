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
#include "platform/WebLayer.h"

#include "WebFloatPoint.h"
#include "WebFloatRect.h"
#include "WebRect.h"
#include "WebSize.h"
#include "platform/WebContentLayer.h"
#include "platform/WebContentLayerClient.h"
#include "platform/WebExternalTextureLayer.h"
#include "platform/WebLayerClient.h"

#include <gmock/gmock.h>

using namespace WebKit;
using namespace testing;

namespace {

class MockWebLayerClient : public WebLayerClient {
public:
    MOCK_METHOD0(notifyNeedsComposite, void());
};

class MockWebContentLayerClient : public WebContentLayerClient {
public:
    MOCK_METHOD2(paintContents, void(WebCanvas*, const WebRect& clip));
};

class WebLayerTest : public Test {
public:
    WebLayerTest() { }
};

// Tests that the client gets called to ask for a composite if we change the
// fields.
TEST_F(WebLayerTest, Client)
{
    // Base layer.
    MockWebLayerClient client;
    EXPECT_CALL(client, notifyNeedsComposite()).Times(AnyNumber());
    WebLayer layer = WebLayer::create(&client);
    Mock::VerifyAndClearExpectations(&client);

    WebFloatPoint point(3, 4);
    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    layer.setAnchorPoint(point);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_EQ(point, layer.anchorPoint());

    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    float anchorZ = 5;
    layer.setAnchorPointZ(anchorZ);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_EQ(anchorZ, layer.anchorPointZ());

    WebSize size(7, 8);
    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    layer.setBounds(size);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_EQ(size, layer.bounds());

    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    layer.setMasksToBounds(true);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_TRUE(layer.masksToBounds());

    MockWebLayerClient otherClient;
    EXPECT_CALL(otherClient, notifyNeedsComposite()).Times(AnyNumber());
    WebLayer otherLayer = WebLayer::create(&otherClient);
    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    layer.setMaskLayer(otherLayer);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_EQ(otherLayer, layer.maskLayer());

    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    float opacity = 0.123;
    layer.setOpacity(opacity);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_EQ(opacity, layer.opacity());

    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    layer.setOpaque(true);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_TRUE(layer.opaque());

    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    layer.setPosition(point);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_EQ(point, layer.position());

    // Texture layer.
    EXPECT_CALL(client, notifyNeedsComposite()).Times(AnyNumber());
    WebExternalTextureLayer textureLayer = WebExternalTextureLayer::create(&client);
    Mock::VerifyAndClearExpectations(&client);

    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    textureLayer.setTextureId(3);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_EQ(3u, textureLayer.textureId());

    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    textureLayer.setFlipped(true);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_TRUE(textureLayer.flipped());

    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    WebFloatRect uvRect(0.1, 0.1, 0.9, 0.9);
    textureLayer.setUVRect(uvRect);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_TRUE(textureLayer.flipped());


    // Content layer.
    MockWebContentLayerClient contentClient;
    EXPECT_CALL(contentClient, paintContents(_, _)).Times(AnyNumber());
    EXPECT_CALL(client, notifyNeedsComposite()).Times(AnyNumber());
    WebContentLayer contentLayer = WebContentLayer::create(&client, &contentClient);
    Mock::VerifyAndClearExpectations(&client);

    EXPECT_CALL(client, notifyNeedsComposite()).Times(AtLeast(1));
    contentLayer.setDrawsContent(false);
    Mock::VerifyAndClearExpectations(&client);
    EXPECT_FALSE(contentLayer.drawsContent());
}

TEST_F(WebLayerTest, Hierarchy)
{
    MockWebLayerClient client;
    EXPECT_CALL(client, notifyNeedsComposite()).Times(AnyNumber());
    WebLayer layer1 = WebLayer::create(&client);
    WebLayer layer2 = WebLayer::create(&client);

    EXPECT_TRUE(layer1.parent().isNull());
    EXPECT_TRUE(layer2.parent().isNull());

    layer1.addChild(layer2);
    EXPECT_TRUE(layer1.parent().isNull());
    EXPECT_EQ(layer1, layer2.parent());

    layer2.removeFromParent();
    EXPECT_TRUE(layer2.parent().isNull());

    layer1.addChild(layer2);
    EXPECT_EQ(layer1, layer2.parent());
    layer1.removeAllChildren();
    EXPECT_TRUE(layer2.parent().isNull());

    MockWebContentLayerClient contentClient;
    EXPECT_CALL(contentClient, paintContents(_, _)).Times(AnyNumber());
    WebContentLayer contentLayer = WebContentLayer::create(&client, &contentClient);
    WebExternalTextureLayer textureLayer = WebExternalTextureLayer::create(&client);

    textureLayer.addChild(contentLayer);
    contentLayer.addChild(layer1);
    layer1.addChild(layer2);

    // Release reference on all layers, checking that destruction (which may
    // generate calls to the client) doesn't crash.
    layer2.reset();
    layer1.reset();
    contentLayer.reset();
    textureLayer.reset();
}

}
