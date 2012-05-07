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

#include "LinkHighlightLayerDelegate.h"

#include "AnimationIdVendor.h"
#include "GraphicsLayerChromium.h"
#include "GraphicsLayerClient.h"
#include "IntRect.h"
#include "Path.h"
#include "TransformationMatrix.h"
#include <gtest/gtest.h>
#include <wtf/PassOwnPtr.h>

using namespace WebCore;

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
public:

    virtual void notifyAnimationStarted(const GraphicsLayer*, double time) { }
    virtual void notifySyncRequired(const GraphicsLayer*) { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) { }
    virtual bool showDebugBorders(const GraphicsLayer*) const { return false; }
    virtual bool showRepaintCounter(const GraphicsLayer*) const { return false; }
};

TEST(LinkHighlightLayerTest, verifyLinkHighlightLayer)
{
    Path highlightPath;
    highlightPath.addRect(FloatRect(5, 6, 12, 8));
    IntRect pathBoundingRect = enclosingIntRect(highlightPath.boundingRect());

    RefPtr<LinkHighlightLayerDelegate> highlightLayerDelegate = LinkHighlightLayerDelegate::create(0, highlightPath, AnimationIdVendor::LinkHighlightAnimationId, AnimationIdVendor::getNextGroupId());
    ASSERT_TRUE(highlightLayerDelegate.get());
    ContentLayerChromium* contentLayer = highlightLayerDelegate->getContentLayer();
    ASSERT_TRUE(contentLayer);

    EXPECT_EQ(pathBoundingRect.size(), contentLayer->bounds());
    EXPECT_TRUE(contentLayer->transform().isIdentityOrTranslation());
    EXPECT_TRUE(contentLayer->transform().isIntegerTranslation());

    TransformationMatrix::DecomposedType decomposition;
    EXPECT_TRUE(contentLayer->transform().decompose(decomposition));

    FloatPoint expectedTranslation(pathBoundingRect.x() + pathBoundingRect.width() / 2, pathBoundingRect.y() + pathBoundingRect.height() / 2);
    EXPECT_EQ(FloatPoint(decomposition.translateX, decomposition.translateY), expectedTranslation);
}

TEST(LinkHighlightLayerTest, verifyGraphicsLayerChromiumEmbedding)
{
    MockGraphicsLayerClient client;
    OwnPtr<GraphicsLayerChromium> graphicsLayer = static_pointer_cast<GraphicsLayerChromium>(GraphicsLayer::create(&client));
    ASSERT_TRUE(graphicsLayer.get());

    Path highlightPath;
    highlightPath.addRect(FloatRect(5, 5, 10, 8));

    // Neither of the following operations should crash.
    graphicsLayer->addLinkHighlightLayer(highlightPath);
    graphicsLayer->didFinishLinkHighlightLayer();
}

} // namespace
