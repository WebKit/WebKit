/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if USE(CG)

#include <WebCore/ColorSpace.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/GraphicsContextCG.h>

namespace TestWebKitAPI {
using namespace WebCore;
using DisplayList::DisplayList;
using namespace DisplayList;

constexpr CGFloat contextWidth = 1;
constexpr CGFloat contextHeight = 1;

// Before the replay depth limit, replaying a deep enough chain overflowed the stack.
TEST(DisplayListRecursionTests, DeeplyNestedDrawDisplayListStopsAtDepthLimit)
{
    constexpr unsigned depth = 1000;
    FloatRect rect(0, 0, contextWidth, contextHeight);

    Ref displayList = DisplayList::create({ FillRectWithColor(rect, Color::red) });
    for (unsigned i = 0; i < depth; ++i)
        displayList = DisplayList::create({ DrawDisplayList(displayList.copyRef()) });
    displayList = DisplayList::create({ FillRectWithColor(rect, Color::green), DrawDisplayList(displayList.copyRef()) });

    auto colorSpace = ColorSpace::SRGB();
    RetainPtr cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    GraphicsContextCG ctx(cgContext.get());

    ctx.drawDisplayList(displayList);

    // The context should have one green pixel.
    CGContextFlush(cgContext.get());
    uint8_t* data = static_cast<uint8_t*>(CGBitmapContextGetData(cgContext.get()));
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 255);
    EXPECT_EQ(data[2], 0);
}

} // namespace TestWebKitAPI

#endif // USE(CG)
