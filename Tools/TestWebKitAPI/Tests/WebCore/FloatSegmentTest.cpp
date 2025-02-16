/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Test.h"
#include "WebCoreTestUtilities.h"
#include <WebCore/FloatSegment.h>

namespace TestWebKitAPI {
using namespace WebCore;

TEST(FloatSegment, DilateWithDifference)
{
    // Empty a.
    {
        Vector<FloatSegment> intersections { { 77.f, 88.f } };
        auto result = differenceWithDilation({ 0, 0 }, WTFMove(intersections), 0.5f);
        Vector<FloatSegment> expected { };
        EXPECT_EQ(result, expected);
    }
    // Empty bs.
    {
        Vector<FloatSegment> intersections { };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.5f);
        Vector<FloatSegment> expected { { 0.f, 170.f } };
        EXPECT_EQ(result, expected);
    }
    // First element removed due to dilation filtering.
    {
        Vector<FloatSegment> intersections { { 1.0f, 2.f } };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.5f);
        Vector<FloatSegment> expected { { 2.5f, 170.f } };
        EXPECT_EQ(result, expected);
    }
    // Last element removed due to dilation filtering.
    {
        Vector<FloatSegment> intersections { { 66.f, 169.7f } };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.5f);
        Vector<FloatSegment> expected { { 0.f, 65.5f } };
        EXPECT_EQ(result, expected);
    }
    // All removed.
    {
        Vector<FloatSegment> intersections { { 0.5f, 80.f }, { 80.5f, 169.5f } };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.5f);
        Vector<FloatSegment> expected { };
        EXPECT_EQ(result, expected);
    }
    // All removed.
    {
        Vector<FloatSegment> intersections { { 0.0f, 80.f }, { 80.f, 180.f } };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.f);
        Vector<FloatSegment> expected { };
        EXPECT_EQ(result, expected);
    }
    // Normal operation.
    {
        Vector<FloatSegment> intersections { { 1.1f, 2.f } };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.5f);
        Vector<FloatSegment> expected { { 0.f, 0.6f }, { 2.5f, 170.f } };
        EXPECT_EQ(result, expected);
    }
    {
        Vector<FloatSegment> intersections { { 0.f, 1.f }, { 5.f, 77.f } };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.5f);
        Vector<FloatSegment> expected { { 1.5f, 4.5f }, { 77.5f, 170.f } };
        EXPECT_EQ(result, expected);
    }
    {
        Vector<FloatSegment> intersections { { 0.f, 1.f } };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.5f);
        Vector<FloatSegment> expected { { 1.5f, 170.f } };
        EXPECT_EQ(result, expected);
    }
    {
        Vector<FloatSegment> intersections { { 2.f, 6.f }, { 8.f, 77.f }, { 3.f, 4.f } };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.5f);
        Vector<FloatSegment> expected { { 0.f, 1.5f }, { 6.5f, 7.5f }, { 77.5f, 170.f } };
        EXPECT_EQ(result, expected);
    }
    {
        Vector<FloatSegment> intersections { { 2.f, 6.f }, { 8.f, 77.f }, { 3.f, 4.f } };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.f);
        Vector<FloatSegment> expected { { 0.f, 2.f }, { 6.f, 8.f }, { 77.f, 170.f } };
        EXPECT_EQ(result, expected);
    }
    // Out of range, after end.
    {
        Vector<FloatSegment> intersections { { 180.f, 188.f } };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.5f);
        Vector<FloatSegment> expected { { 0.f, 170.f } };
        EXPECT_EQ(result, expected);
    }
    // Out of range, before begin.
    {
        Vector<FloatSegment> intersections { { -188.f, -180.f } };
        auto result = differenceWithDilation({ 0, 170.f }, WTFMove(intersections), 0.5f);
        Vector<FloatSegment> expected { { 0.f, 170.f } };
        EXPECT_EQ(result, expected);
    }
}

}
