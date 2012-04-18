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

#include "cc/CCSolidColorLayerImpl.h"

#include "CCLayerTestCommon.h"
#include "MockCCQuadCuller.h"
#include "cc/CCSingleThreadProxy.h"
#include "cc/CCSolidColorDrawQuad.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;
using namespace CCLayerTestCommon;

namespace {

TEST(CCSolidColorLayerImplTest, verifyTilingCompleteAndNoOverlap)
{
    DebugScopedSetImplThread scopedImplThread;

    MockCCQuadCuller quadCuller;
    IntSize layerSize = IntSize(800, 600);
    IntRect visibleLayerRect = IntRect(IntPoint(), layerSize);

    OwnPtr<CCSolidColorLayerImpl> layer = CCSolidColorLayerImpl::create(0);
    layer->setVisibleLayerRect(visibleLayerRect);
    layer->setBounds(layerSize);

    OwnPtr<CCSharedQuadState> sharedQuadState = layer->createSharedQuadState();
    bool hadMissingTiles = false;
    layer->appendQuads(quadCuller, sharedQuadState.get(), hadMissingTiles);

    verifyQuadsExactlyCoverRect(quadCuller.quadList(), visibleLayerRect);
}

TEST(CCSolidColorLayerImplTest, verifyCorrectBackgroundColorInQuad)
{
    DebugScopedSetImplThread scopedImplThread;

    const Color testColor = 0xFFA55AFF;

    MockCCQuadCuller quadCuller;
    IntSize layerSize = IntSize(100, 100);
    IntRect visibleLayerRect = IntRect(IntPoint(), layerSize);

    OwnPtr<CCSolidColorLayerImpl> layer = CCSolidColorLayerImpl::create(0);
    layer->setVisibleLayerRect(visibleLayerRect);
    layer->setBounds(layerSize);
    layer->setBackgroundColor(testColor);

    OwnPtr<CCSharedQuadState> sharedQuadState = layer->createSharedQuadState();
    bool hadMissingTiles = false;
    layer->appendQuads(quadCuller, sharedQuadState.get(), hadMissingTiles);

    ASSERT_EQ(quadCuller.quadList().size(), 1U);
    EXPECT_EQ(quadCuller.quadList()[0]->toSolidColorDrawQuad()->color(), testColor);
}

TEST(CCSolidColorLayerImplTest, verifyCorrectOpacityInQuad)
{
    DebugScopedSetImplThread scopedImplThread;

    const float opacity = 0.5f;

    MockCCQuadCuller quadCuller;
    IntSize layerSize = IntSize(100, 100);
    IntRect visibleLayerRect = IntRect(IntPoint(), layerSize);

    OwnPtr<CCSolidColorLayerImpl> layer = CCSolidColorLayerImpl::create(0);
    layer->setVisibleLayerRect(visibleLayerRect);
    layer->setBounds(layerSize);
    layer->setDrawOpacity(opacity);

    OwnPtr<CCSharedQuadState> sharedQuadState = layer->createSharedQuadState();
    bool hadMissingTiles = false;
    layer->appendQuads(quadCuller, sharedQuadState.get(), hadMissingTiles);

    ASSERT_EQ(quadCuller.quadList().size(), 1U);
    EXPECT_EQ(opacity, quadCuller.quadList()[0]->toSolidColorDrawQuad()->opacity());
}

} // namespace
