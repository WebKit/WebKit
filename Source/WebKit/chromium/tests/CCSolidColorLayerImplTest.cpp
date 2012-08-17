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

#include "CCSolidColorLayerImpl.h"

#include "CCLayerTestCommon.h"
#include "CCSingleThreadProxy.h"
#include "CCSolidColorDrawQuad.h"
#include "MockCCQuadCuller.h"
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
    IntRect visibleContentRect = IntRect(IntPoint(), layerSize);

    OwnPtr<CCSolidColorLayerImpl> layer = CCSolidColorLayerImpl::create(1);
    layer->setVisibleContentRect(visibleContentRect);
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->createRenderSurface();
    layer->setRenderTarget(layer.get());

    OwnPtr<CCSharedQuadState> sharedQuadState = layer->createSharedQuadState(0);
    bool hadMissingTiles = false;
    layer->appendQuads(quadCuller, sharedQuadState.get(), hadMissingTiles);

    verifyQuadsExactlyCoverRect(quadCuller.quadList(), visibleContentRect);
}

TEST(CCSolidColorLayerImplTest, verifyCorrectBackgroundColorInQuad)
{
    DebugScopedSetImplThread scopedImplThread;

    SkColor testColor = 0xFFA55AFF;

    MockCCQuadCuller quadCuller;
    IntSize layerSize = IntSize(100, 100);
    IntRect visibleContentRect = IntRect(IntPoint(), layerSize);

    OwnPtr<CCSolidColorLayerImpl> layer = CCSolidColorLayerImpl::create(1);
    layer->setVisibleContentRect(visibleContentRect);
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->setBackgroundColor(testColor);
    layer->createRenderSurface();
    layer->setRenderTarget(layer.get());

    OwnPtr<CCSharedQuadState> sharedQuadState = layer->createSharedQuadState(0);
    bool hadMissingTiles = false;
    layer->appendQuads(quadCuller, sharedQuadState.get(), hadMissingTiles);

    ASSERT_EQ(quadCuller.quadList().size(), 1U);
    EXPECT_EQ(CCSolidColorDrawQuad::materialCast(quadCuller.quadList()[0].get())->color(), testColor);
}

TEST(CCSolidColorLayerImplTest, verifyCorrectOpacityInQuad)
{
    DebugScopedSetImplThread scopedImplThread;

    const float opacity = 0.5f;

    MockCCQuadCuller quadCuller;
    IntSize layerSize = IntSize(100, 100);
    IntRect visibleContentRect = IntRect(IntPoint(), layerSize);

    OwnPtr<CCSolidColorLayerImpl> layer = CCSolidColorLayerImpl::create(1);
    layer->setVisibleContentRect(visibleContentRect);
    layer->setBounds(layerSize);
    layer->setContentBounds(layerSize);
    layer->setDrawOpacity(opacity);
    layer->createRenderSurface();
    layer->setRenderTarget(layer.get());

    OwnPtr<CCSharedQuadState> sharedQuadState = layer->createSharedQuadState(0);
    bool hadMissingTiles = false;
    layer->appendQuads(quadCuller, sharedQuadState.get(), hadMissingTiles);

    ASSERT_EQ(quadCuller.quadList().size(), 1U);
    EXPECT_EQ(opacity, CCSolidColorDrawQuad::materialCast(quadCuller.quadList()[0].get())->opacity());
}

} // namespace
