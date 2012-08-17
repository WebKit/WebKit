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

#include "TextureLayerChromium.h"

#include "CCLayerTreeHost.h"
#include "FakeCCLayerTreeHostClient.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <public/WebCompositor.h>

using namespace WebCore;
using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace {

class MockCCLayerTreeHost : public CCLayerTreeHost {
public:
    MockCCLayerTreeHost()
        : CCLayerTreeHost(&m_fakeClient, CCLayerTreeSettings())
    {
        initialize();
    }

    MOCK_METHOD0(acquireLayerTextures, void());

private:
    FakeCCLayerTreeHostClient m_fakeClient;
};


class TextureLayerChromiumTest : public testing::Test {
protected:
    virtual void SetUp()
    {
        // Initialize without threading support.
        WebKit::WebCompositor::initialize(0);
        m_layerTreeHost = adoptPtr(new MockCCLayerTreeHost);
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
        EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AnyNumber());

        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.clear();
        WebKit::WebCompositor::shutdown();
    }

    OwnPtr<MockCCLayerTreeHost> m_layerTreeHost;
};

TEST_F(TextureLayerChromiumTest, syncImplWhenChangingTextureId)
{
    RefPtr<TextureLayerChromium> testLayer = TextureLayerChromium::create(0);
    ASSERT_TRUE(testLayer);

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AnyNumber());
    m_layerTreeHost->setRootLayer(testLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
    EXPECT_EQ(testLayer->layerTreeHost(), m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    testLayer->setTextureId(1);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    testLayer->setTextureId(2);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    testLayer->setTextureId(0);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
}

TEST_F(TextureLayerChromiumTest, syncImplWhenRemovingFromTree)
{
    RefPtr<LayerChromium> rootLayer = LayerChromium::create();
    ASSERT_TRUE(rootLayer);
    RefPtr<LayerChromium> childLayer = LayerChromium::create();
    ASSERT_TRUE(childLayer);
    rootLayer->addChild(childLayer);
    RefPtr<TextureLayerChromium> testLayer = TextureLayerChromium::create(0);
    ASSERT_TRUE(testLayer);
    testLayer->setTextureId(0);
    childLayer->addChild(testLayer);

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AnyNumber());
    m_layerTreeHost->setRootLayer(rootLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    testLayer->removeFromParent();
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    childLayer->addChild(testLayer);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(0);
    testLayer->setTextureId(1);
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

    EXPECT_CALL(*m_layerTreeHost, acquireLayerTextures()).Times(AtLeast(1));
    testLayer->removeFromParent();
    Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
}

} // anonymous namespace
