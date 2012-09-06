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

#include "CCRenderPass.h"

#include "CCCheckerboardDrawQuad.h"
#include "CCGeometryTestUtils.h"
#include <gtest/gtest.h>
#include <public/WebFilterOperations.h>
#include <public/WebTransformationMatrix.h>

using WebKit::WebFilterOperation;
using WebKit::WebFilterOperations;
using WebKit::WebTransformationMatrix;

using namespace WebCore;

namespace {

class CCTestRenderPass : public CCRenderPass {
public:
    CCQuadList& quadList() { return m_quadList; }
    CCSharedQuadStateList& sharedQuadStateList() { return m_sharedQuadStateList; }
};

struct CCRenderPassSize {
    // If you add a new field to this class, make sure to add it to the copy() tests.
    int m_id;
    CCQuadList m_quadList;
    CCSharedQuadStateList m_sharedQuadStateList;
    WebKit::WebTransformationMatrix m_transformToRootTarget;
    IntRect m_outputRect;
    FloatRect m_damageRect;
    bool m_hasTransparentBackground;
    bool m_hasOcclusionFromOutsideTargetSurface;
    WebKit::WebFilterOperations m_filters;
    WebKit::WebFilterOperations m_backgroundFilters;
};

TEST(CCRenderPassTest, copyShouldBeIdenticalExceptIdAndQuads)
{
    int id = 3;
    IntRect outputRect(45, 22, 120, 13);
    WebTransformationMatrix transformToRoot(1, 0.5, 0.5, -0.5, -1, 0);

    OwnPtr<CCRenderPass> pass(CCRenderPass::create(id, outputRect, transformToRoot));

    IntRect damageRect(56, 123, 19, 43);
    bool hasTransparentBackground = true;
    bool hasOcclusionFromOutsideTargetSurface = true;
    WebFilterOperations filters;
    WebFilterOperations backgroundFilters;

    filters.append(WebFilterOperation::createGrayscaleFilter(0.2));
    backgroundFilters.append(WebFilterOperation::createInvertFilter(0.2));

    pass->setDamageRect(damageRect);
    pass->setHasTransparentBackground(hasTransparentBackground);
    pass->setHasOcclusionFromOutsideTargetSurface(hasOcclusionFromOutsideTargetSurface);
    pass->setFilters(filters);
    pass->setBackgroundFilters(backgroundFilters);

    // Stick a quad in the pass, this should not get copied.
    CCTestRenderPass* testPass = static_cast<CCTestRenderPass*>(pass.get());
    testPass->sharedQuadStateList().append(CCSharedQuadState::create(WebTransformationMatrix(), IntRect(), IntRect(), 1, false));
    testPass->quadList().append(CCCheckerboardDrawQuad::create(testPass->sharedQuadStateList().last().get(), IntRect()));

    int newId = 63;

    OwnPtr<CCRenderPass> copy(pass->copy(newId));
    EXPECT_EQ(newId, copy->id());
    EXPECT_RECT_EQ(pass->outputRect(), copy->outputRect());
    EXPECT_EQ(pass->transformToRootTarget(), copy->transformToRootTarget());
    EXPECT_RECT_EQ(pass->damageRect(), copy->damageRect());
    EXPECT_EQ(pass->hasTransparentBackground(), copy->hasTransparentBackground());
    EXPECT_EQ(pass->hasOcclusionFromOutsideTargetSurface(), copy->hasOcclusionFromOutsideTargetSurface());
    EXPECT_EQ(pass->filters(), copy->filters());
    EXPECT_EQ(pass->backgroundFilters(), copy->backgroundFilters());
    EXPECT_EQ(0u, copy->quadList().size());

    EXPECT_EQ(sizeof(CCRenderPassSize), sizeof(CCRenderPass));
}

} // namespace
