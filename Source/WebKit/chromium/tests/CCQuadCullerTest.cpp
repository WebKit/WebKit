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

#include "cc/CCQuadCuller.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class CCQuadCullerTest : public testing::Test {
};

class TestDrawQuad : public CCDrawQuad {
public:
    TestDrawQuad(const CCSharedQuadState* state, Material m, const IntRect& rect, const IntRect& opaqueRect)
    : CCDrawQuad(state, m, rect)
    {
        m_opaqueRect = opaqueRect;
    }

    static PassOwnPtr<TestDrawQuad> create(const CCSharedQuadState* state, Material m, const IntRect& rect, const IntRect& opaqueRect)
    {
        return adoptPtr(new TestDrawQuad(state, m, rect, opaqueRect));
    }
};

void setQuads(CCSharedQuadState* rootState, CCSharedQuadState* childState, CCQuadList& quadList, const IntRect& childOpaqueRect = IntRect())
{
    quadList.clear();

    quadList.append(TestDrawQuad::create(rootState, CCDrawQuad::TiledContent, IntRect(IntPoint(), IntSize(100, 100)), childOpaqueRect));
    quadList.append(TestDrawQuad::create(rootState, CCDrawQuad::TiledContent, IntRect(IntPoint(100, 0), IntSize(100, 100)), childOpaqueRect));
    quadList.append(TestDrawQuad::create(rootState, CCDrawQuad::TiledContent, IntRect(IntPoint(200, 0), IntSize(100, 100)), childOpaqueRect));
    quadList.append(TestDrawQuad::create(rootState, CCDrawQuad::TiledContent, IntRect(IntPoint(0, 100), IntSize(100, 100)), childOpaqueRect));
    quadList.append(TestDrawQuad::create(rootState, CCDrawQuad::TiledContent, IntRect(IntPoint(100, 100), IntSize(100, 100)), childOpaqueRect));
    quadList.append(TestDrawQuad::create(rootState, CCDrawQuad::TiledContent, IntRect(IntPoint(200, 100), IntSize(100, 100)), childOpaqueRect));
    quadList.append(TestDrawQuad::create(rootState, CCDrawQuad::TiledContent, IntRect(IntPoint(0, 200), IntSize(100, 100)), childOpaqueRect));
    quadList.append(TestDrawQuad::create(rootState, CCDrawQuad::TiledContent, IntRect(IntPoint(100, 200), IntSize(100, 100)), childOpaqueRect));
    quadList.append(TestDrawQuad::create(rootState, CCDrawQuad::TiledContent, IntRect(IntPoint(200, 200), IntSize(100, 100)), childOpaqueRect));

    quadList.append(TestDrawQuad::create(childState, CCDrawQuad::TiledContent, IntRect(IntPoint(), IntSize(100, 100)), childOpaqueRect));
    quadList.append(TestDrawQuad::create(childState, CCDrawQuad::TiledContent, IntRect(IntPoint(100, 0), IntSize(100, 100)), childOpaqueRect));
    quadList.append(TestDrawQuad::create(childState, CCDrawQuad::TiledContent, IntRect(IntPoint(0, 100), IntSize(100, 100)), childOpaqueRect));
    quadList.append(TestDrawQuad::create(childState, CCDrawQuad::TiledContent, IntRect(IntPoint(100, 100), IntSize(100, 100)), childOpaqueRect));
}

#define DECLARE_AND_INITIALIZE_TEST_QUADS               \
    CCQuadList quadList;                                \
    TransformationMatrix childTransform;                \
    IntSize rootSize = IntSize(300, 300);               \
    IntRect rootRect = IntRect(IntPoint(), rootSize);   \
    IntSize childSize = IntSize(200, 200);              \
    IntRect childRect = IntRect(IntPoint(), childSize);

TEST(CCQuadCullerTest, verifyCullChildLinesUpTopLeft)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCSharedQuadState> rootState = CCSharedQuadState::create(TransformationMatrix(), TransformationMatrix(), rootRect, IntRect(), 1.0, true);
    OwnPtr<CCSharedQuadState> childState = CCSharedQuadState::create(childTransform, TransformationMatrix(), childRect, IntRect(), 1.0, true);

    setQuads(rootState.get(), childState.get(), quadList);
    EXPECT_EQ(quadList.size(), 13u);
    CCQuadCuller::cullOccludedQuads(quadList);
    EXPECT_EQ(quadList.size(), 9u);
}

TEST(CCQuadCullerTest, verifyCullWhenChildOpacityNotOne)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCSharedQuadState> rootState = CCSharedQuadState::create(TransformationMatrix(), TransformationMatrix(), rootRect, IntRect(), 1.0, true);
    OwnPtr<CCSharedQuadState> childState = CCSharedQuadState::create(childTransform, TransformationMatrix(), childRect, IntRect(), 0.9, true);

    setQuads(rootState.get(), childState.get(), quadList);
    EXPECT_EQ(quadList.size(), 13u);
    CCQuadCuller::cullOccludedQuads(quadList);
    EXPECT_EQ(quadList.size(), 13u);
}

TEST(CCQuadCullerTest, verifyCullWhenChildOpaqueFlagFalse)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    OwnPtr<CCSharedQuadState> rootState = CCSharedQuadState::create(TransformationMatrix(), TransformationMatrix(), rootRect, IntRect(), 1.0, true);
    OwnPtr<CCSharedQuadState> childState = CCSharedQuadState::create(childTransform, TransformationMatrix(), childRect, IntRect(), 1.0, false);

    setQuads(rootState.get(), childState.get(), quadList);
    EXPECT_EQ(quadList.size(), 13u);
    CCQuadCuller::cullOccludedQuads(quadList);
    EXPECT_EQ(quadList.size(), 13u);
}

TEST(CCQuadCullerTest, verifyCullCenterTileOnly)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.translate(50, 50);

    OwnPtr<CCSharedQuadState> rootState = CCSharedQuadState::create(TransformationMatrix(), TransformationMatrix(), rootRect, IntRect(), 1.0, true);
    OwnPtr<CCSharedQuadState> childState = CCSharedQuadState::create(childTransform, TransformationMatrix(), childRect, IntRect(), 1.0, true);

    setQuads(rootState.get(), childState.get(), quadList);
    EXPECT_EQ(quadList.size(), 13u);
    CCQuadCuller::cullOccludedQuads(quadList);
    EXPECT_EQ(quadList.size(), 12u);
}

TEST(CCQuadCullerTest, verifyCullChildLinesUpBottomRight)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.translate(100, 100);

    OwnPtr<CCSharedQuadState> rootState = CCSharedQuadState::create(TransformationMatrix(), TransformationMatrix(), rootRect, IntRect(), 1.0, true);
    OwnPtr<CCSharedQuadState> childState = CCSharedQuadState::create(childTransform, TransformationMatrix(), childRect, IntRect(), 1.0, true);

    setQuads(rootState.get(), childState.get(), quadList);
    EXPECT_EQ(quadList.size(), 13u);
    CCQuadCuller::cullOccludedQuads(quadList);
    EXPECT_EQ(quadList.size(), 9u);
}

TEST(CCQuadCullerTest, verifyCullSubRegion)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.translate(50, 50);

    OwnPtr<CCSharedQuadState> rootState = CCSharedQuadState::create(TransformationMatrix(), TransformationMatrix(), rootRect, IntRect(), 1.0, true);
    OwnPtr<CCSharedQuadState> childState = CCSharedQuadState::create(childTransform, TransformationMatrix(), childRect, IntRect(), 1.0, false);
    IntRect childOpaqueRect(childRect.x() + childRect.width() / 4, childRect.y() + childRect.height() / 4, childRect.width() / 2, childRect.height() / 2);

    setQuads(rootState.get(), childState.get(), quadList, childOpaqueRect);
    EXPECT_EQ(quadList.size(), 13u);
    CCQuadCuller::cullOccludedQuads(quadList);
    EXPECT_EQ(quadList.size(), 12u);
}

TEST(CCQuadCullerTest, verifyCullSubRegion2)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.translate(50, 10);

    OwnPtr<CCSharedQuadState> rootState = CCSharedQuadState::create(TransformationMatrix(), TransformationMatrix(), rootRect, IntRect(), 1.0, true);
    OwnPtr<CCSharedQuadState> childState = CCSharedQuadState::create(childTransform, TransformationMatrix(), childRect, IntRect(), 1.0, false);
    IntRect childOpaqueRect(childRect.x() + childRect.width() / 4, childRect.y() + childRect.height() / 4, childRect.width() / 2, childRect.height() * 3 / 4);

    setQuads(rootState.get(), childState.get(), quadList, childOpaqueRect);
    EXPECT_EQ(quadList.size(), 13u);
    CCQuadCuller::cullOccludedQuads(quadList);
    EXPECT_EQ(quadList.size(), 12u);
}

TEST(CCQuadCullerTest, verifyCullSubRegionCheckOvercull)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    childTransform.translate(50, 49);

    OwnPtr<CCSharedQuadState> rootState = CCSharedQuadState::create(TransformationMatrix(), TransformationMatrix(), rootRect, IntRect(), 1.0, true);
    OwnPtr<CCSharedQuadState> childState = CCSharedQuadState::create(childTransform, TransformationMatrix(), childRect, IntRect(), 1.0, false);
    IntRect childOpaqueRect(childRect.x() + childRect.width() / 4, childRect.y() + childRect.height() / 4, childRect.width() / 2, childRect.height() / 2);

    setQuads(rootState.get(), childState.get(), quadList, childOpaqueRect);
    EXPECT_EQ(quadList.size(), 13u);
    CCQuadCuller::cullOccludedQuads(quadList);
    EXPECT_EQ(quadList.size(), 13u);
}

TEST(CCQuadCullerTest, verifyNonAxisAlignedQuadsDontOcclude)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    // Use a small rotation so as to not disturb the geometry significantly.
    childTransform.rotate(1);

    OwnPtr<CCSharedQuadState> rootState = CCSharedQuadState::create(TransformationMatrix(), TransformationMatrix(), rootRect, IntRect(), 1.0, true);
    OwnPtr<CCSharedQuadState> childState = CCSharedQuadState::create(childTransform, TransformationMatrix(), childRect, IntRect(), 1.0, true);

    setQuads(rootState.get(), childState.get(), quadList);
    EXPECT_EQ(quadList.size(), 13u);
    CCQuadCuller::cullOccludedQuads(quadList);
    EXPECT_EQ(quadList.size(), 13u);
}

// This test requires some explanation: here we are rotating the quads to be culled.
// The 2x2 tile child layer remains in the top-left corner, unrotated, but the 3x3
// tile parent layer is rotated by 1 degree. Of the four tiles the child would
// normally occlude, three will move (slightly) out from under the child layer, and
// one moves further under the child. Only this last tile should be culled.
TEST(CCQuadCullerTest, verifyNonAxisAlignedQuadsSafelyCulled)
{
    DECLARE_AND_INITIALIZE_TEST_QUADS

    // Use a small rotation so as to not disturb the geometry significantly.
    TransformationMatrix parentTransform;
    parentTransform.rotate(1);

    OwnPtr<CCSharedQuadState> rootState = CCSharedQuadState::create(parentTransform, TransformationMatrix(), rootRect, IntRect(), 1.0, true);
    OwnPtr<CCSharedQuadState> childState = CCSharedQuadState::create(TransformationMatrix(), TransformationMatrix(), childRect, IntRect(), 1.0, true);

    setQuads(rootState.get(), childState.get(), quadList);
    EXPECT_EQ(quadList.size(), 13u);
    CCQuadCuller::cullOccludedQuads(quadList);
    EXPECT_EQ(quadList.size(), 12u);
}

} // namespace
