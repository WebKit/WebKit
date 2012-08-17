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

#include "CCRenderSurfaceFilters.h"

#include "CompositorFakeWebGraphicsContext3D.h"
#include <gtest/gtest.h>
#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>
#include <wtf/RefPtr.h>

using namespace WebCore;
using namespace WebKit;

namespace {

// Checks whether op can be combined with a following color matrix.
bool isCombined(const WebFilterOperation& op)
{
    WebFilterOperations filters;
    filters.append(op);
    filters.append(WebFilterOperation::createBrightnessFilter(0)); // brightness(0) is identity.
    WebFilterOperations optimized = CCRenderSurfaceFilters::optimize(filters);
    return optimized.size() == 1;
}

TEST(CCRenderSurfaceFiltersTest, testColorMatrixFiltersCombined)
{
    // Several filters should always combine for any amount between 0 and 1:
    // grayscale, saturate, invert, contrast, opacity.
    EXPECT_TRUE(isCombined(WebFilterOperation::createGrayscaleFilter(0)));
    // Note that we use 0.3f to avoid "argument is truncated from 'double' to
    // 'float'" warnings on Windows. 0.5 is exactly representable as a float, so
    // there is no warning.
    EXPECT_TRUE(isCombined(WebFilterOperation::createGrayscaleFilter(0.3f)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createGrayscaleFilter(0.5)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createGrayscaleFilter(1)));

    EXPECT_TRUE(isCombined(WebFilterOperation::createSaturateFilter(0)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createSaturateFilter(0.3f)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createSaturateFilter(0.5)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createSaturateFilter(1)));

    EXPECT_TRUE(isCombined(WebFilterOperation::createInvertFilter(0)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createInvertFilter(0.3f)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createInvertFilter(0.5)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createInvertFilter(1)));

    EXPECT_TRUE(isCombined(WebFilterOperation::createContrastFilter(0)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createContrastFilter(0.3f)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createContrastFilter(0.5)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createContrastFilter(1)));

    EXPECT_TRUE(isCombined(WebFilterOperation::createOpacityFilter(0)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createOpacityFilter(0.3f)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createOpacityFilter(0.5)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createOpacityFilter(1)));

    // Several filters should never combine: brightness(amount > 0), blur, drop-shadow.
    EXPECT_FALSE(isCombined(WebFilterOperation::createBrightnessFilter(0.5)));
    EXPECT_FALSE(isCombined(WebFilterOperation::createBrightnessFilter(1)));
    EXPECT_FALSE(isCombined(WebFilterOperation::createBlurFilter(3)));
    EXPECT_FALSE(isCombined(WebFilterOperation::createDropShadowFilter(WebPoint(2, 2), 3, 0xffffffff)));

    // sepia and hue may or may not combine depending on the value.
    EXPECT_TRUE(isCombined(WebFilterOperation::createSepiaFilter(0)));
    EXPECT_FALSE(isCombined(WebFilterOperation::createSepiaFilter(1)));
    EXPECT_TRUE(isCombined(WebFilterOperation::createHueRotateFilter(0)));
    EXPECT_FALSE(isCombined(WebFilterOperation::createHueRotateFilter(180)));

    float matrix1[20] = {
        1, 0, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0,
    };
    EXPECT_TRUE(isCombined(WebFilterOperation::createColorMatrixFilter(matrix1)));

    float matrix2[20] = {
        1, 1, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0,
    };
    EXPECT_FALSE(isCombined(WebFilterOperation::createColorMatrixFilter(matrix2)));

    float matrix3[20] = {
        0.25, 0, 0, 0, 255*0.75,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0,
    };
    EXPECT_TRUE(isCombined(WebFilterOperation::createColorMatrixFilter(matrix3)));

    float matrix4[20] = {
        -0.25, 0.75, 0, 0, 255*0.25,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0,
    };
    EXPECT_TRUE(isCombined(WebFilterOperation::createColorMatrixFilter(matrix4)));
}

TEST(CCRenderSurfaceFiltersTest, testOptimize)
{
    WebFilterOperation combines(WebFilterOperation::createBrightnessFilter(0));
    WebFilterOperation doesntCombine(WebFilterOperation::createBrightnessFilter(1));

    WebFilterOperations filters;
    WebFilterOperations optimized = CCRenderSurfaceFilters::optimize(filters);
    EXPECT_EQ(0u, optimized.size());

    filters.append(combines);
    optimized = CCRenderSurfaceFilters::optimize(filters);
    EXPECT_EQ(1u, optimized.size());

    filters.append(combines);
    optimized = CCRenderSurfaceFilters::optimize(filters);
    EXPECT_EQ(1u, optimized.size());

    filters.append(doesntCombine);
    optimized = CCRenderSurfaceFilters::optimize(filters);
    EXPECT_EQ(1u, optimized.size());

    filters.append(combines);
    optimized = CCRenderSurfaceFilters::optimize(filters);
    EXPECT_EQ(2u, optimized.size());

    filters.append(doesntCombine);
    optimized = CCRenderSurfaceFilters::optimize(filters);
    EXPECT_EQ(2u, optimized.size());

    filters.append(doesntCombine);
    optimized = CCRenderSurfaceFilters::optimize(filters);
    EXPECT_EQ(3u, optimized.size());
}

} // namespace
