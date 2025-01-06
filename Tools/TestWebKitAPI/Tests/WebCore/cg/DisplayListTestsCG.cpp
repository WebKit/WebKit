/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#include <WebCore/ControlFactory.h>
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListReplayer.h>
#include <WebCore/DisplayListResourceHeap.h>
#include <WebCore/Filter.h>
#include <WebCore/Gradient.h>
#include <WebCore/GraphicsContextCG.h>

namespace TestWebKitAPI {
using namespace WebCore;
using DisplayList::DisplayList;
using namespace DisplayList;

constexpr CGFloat contextWidth = 100;
constexpr CGFloat contextHeight = 100;

TEST(DisplayListTests, ReplayWithMissingResource)
{
    FloatRect contextBounds { 0, 0, contextWidth, contextHeight };

    auto colorSpace = DestinationColorSpace::SRGB();
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    GraphicsContextCG context { cgContext.get() };

    auto imageBuffer = ImageBuffer::create({ 100, 100 }, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, colorSpace, ImageBufferPixelFormat::BGRA8);
    auto imageBufferIdentifier = imageBuffer->renderingResourceIdentifier();

    DisplayList list;

    list.append(SetInlineFillColor(Color::green));
    list.append(FillRect(contextBounds, GraphicsContext::RequiresClipToRect::Yes));
    list.append(DrawImageBuffer(imageBufferIdentifier, contextBounds, contextBounds, ImagePaintingOptions { }));
    list.append(SetInlineStroke(Color::red));
    list.append(StrokeLine(FloatPoint { 0, contextHeight }, FloatPoint { contextWidth, 0 }));

    {
        Replayer replayer { context, list };
        auto result = replayer.replay();
        EXPECT_EQ(result.reasonForStopping, StopReplayReason::MissingCachedResource);
        EXPECT_EQ(result.missingCachedResourceIdentifier, imageBufferIdentifier);
    }

    {
        ResourceHeap resourceHeap;
        resourceHeap.add(imageBuffer.releaseNonNull());

        Replayer replayer { context, list.items(), resourceHeap, ControlFactory::shared() };
        auto result = replayer.replay();
        EXPECT_EQ(result.reasonForStopping, StopReplayReason::ReplayedAllItems);
        EXPECT_EQ(result.missingCachedResourceIdentifier, std::nullopt);
    }
}

TEST(DisplayListTests, ItemValidationFailure)
{
    auto colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.get(), kCGImageAlphaPremultipliedLast));
    GraphicsContextCG context { cgContext.get() };

    auto runTestWithInvalidIdentifier = [&](std::optional<RenderingResourceIdentifier> identifier) {
        EXPECT_TRUE(!identifier);

        DisplayList list;
        list.append(ClipToImageBuffer(identifier, FloatRect { }));

        Replayer replayer { context, list };
        auto result = replayer.replay();
        EXPECT_EQ(result.missingCachedResourceIdentifier, std::nullopt);
        EXPECT_EQ(result.reasonForStopping, StopReplayReason::InvalidItemOrExtent);
    };

    runTestWithInvalidIdentifier(std::nullopt);
}

} // namespace TestWebKitAPI

#endif // USE(CG)
