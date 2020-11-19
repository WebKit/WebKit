/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListReplayer.h>
#include <WebCore/Gradient.h>

namespace TestWebKitAPI {
using namespace WebCore;
using DisplayList::DisplayList;
using namespace DisplayList;

TEST(DisplayListTests, ReplayWithMissingResource)
{
    constexpr CGFloat contextWidth = 100;
    constexpr CGFloat contextHeight = 100;

    FloatRect contextBounds { 0, 0, contextWidth, contextHeight };

    auto colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.get(), kCGImageAlphaPremultipliedLast));
    GraphicsContext context { cgContext.get() };

    auto imageBufferIdentifier = RenderingResourceIdentifier::generate();

    DisplayList list;

    list.append<SetInlineFillColor>(Color::green);
    list.append<FillRect>(contextBounds);
    list.append<DrawImageBuffer>(imageBufferIdentifier, contextBounds, contextBounds, ImagePaintingOptions { });
    list.append<SetInlineStrokeColor>(Color::red);
    list.append<StrokeLine>(FloatPoint { 0, contextHeight }, FloatPoint { contextWidth, 0 });

    {
        Replayer replayer { context, list };
        auto result = replayer.replay();
        EXPECT_LT(result.numberOfBytesRead, list.sizeInBytes());
        EXPECT_EQ(result.reasonForStopping, StopReplayReason::MissingCachedResource);
    }

    {
        auto imageBuffer = ImageBuffer::create({ 100, 100 }, RenderingMode::Unaccelerated);
        ImageBufferHashMap imageBufferMap;
        imageBufferMap.set(imageBufferIdentifier, imageBuffer.releaseNonNull());

        Replayer replayer { context, list, &imageBufferMap };
        auto result = replayer.replay();
        EXPECT_EQ(result.numberOfBytesRead, list.sizeInBytes());
        EXPECT_EQ(result.reasonForStopping, StopReplayReason::ReplayedAllItems);
    }
}

} // namespace TestWebKitAPI

#endif // USE(CG)
