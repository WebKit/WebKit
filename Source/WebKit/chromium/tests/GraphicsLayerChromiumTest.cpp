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

#include "CCAnimationTestCommon.h"
#include "GraphicsLayer.h"
#include <gtest/gtest.h>
#include <wtf/PassOwnPtr.h>

using namespace WebCore;
using namespace WebKitTests;

namespace {

class MockGraphicsLayerClient : public GraphicsLayerClient {
  public:
    virtual void notifyAnimationStarted(const GraphicsLayer*, double time) { }
    virtual void notifySyncRequired(const GraphicsLayer*) { }
    virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const IntRect& inClip) { }
    virtual bool showDebugBorders(const GraphicsLayer*) const { return false; }
    virtual bool showRepaintCounter(const GraphicsLayer*) const { return false; }
};

TEST(GraphicsLayerChromiumTest, updateLayerPreserves3DWithAnimations)
{
    MockGraphicsLayerClient client;
    OwnPtr<GraphicsLayerChromium> graphicsLayer = static_pointer_cast<GraphicsLayerChromium>(GraphicsLayer::create(&client));
    ASSERT_TRUE(graphicsLayer.get());

    LayerChromium* platformLayer = static_cast<LayerChromium*>(graphicsLayer->platformLayer());
    ASSERT_TRUE(platformLayer);

    ASSERT_FALSE(platformLayer->hasActiveAnimation());

    OwnPtr<CCActiveAnimation> floatAnimation(CCActiveAnimation::create(adoptPtr(new FakeFloatAnimationCurve), 0, 1, CCActiveAnimation::Opacity));
    platformLayer->layerAnimationController()->add(floatAnimation.release());

    ASSERT_TRUE(platformLayer->hasActiveAnimation());

    graphicsLayer->setPreserves3D(true);

    platformLayer = static_cast<LayerChromium*>(graphicsLayer->platformLayer());
    ASSERT_TRUE(platformLayer);

    ASSERT_TRUE(platformLayer->hasActiveAnimation());
    platformLayer->removeAnimation(0);
    ASSERT_FALSE(platformLayer->hasActiveAnimation());

    graphicsLayer->setPreserves3D(false);

    platformLayer = static_cast<LayerChromium*>(graphicsLayer->platformLayer());
    ASSERT_TRUE(platformLayer);

    ASSERT_FALSE(platformLayer->hasActiveAnimation());
}

} // namespace
