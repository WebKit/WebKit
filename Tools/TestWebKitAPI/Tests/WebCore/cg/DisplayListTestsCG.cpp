/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include <WebCore/DestinationColorSpace.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListReplayer.h>
#include <WebCore/DisplayListResourceHeap.h>
#include <WebCore/Gradient.h>
#include <WebCore/GraphicsContextCG.h>

namespace TestWebKitAPI {
using namespace WebCore;
using DisplayList::DisplayList;
using namespace DisplayList;

constexpr size_t globalItemBufferCapacity = 1 << 12;
static uint8_t globalItemBuffer[globalItemBufferCapacity];
constexpr CGFloat contextWidth = 100;
constexpr CGFloat contextHeight = 100;

TEST(DisplayListTests, ReplayWithMissingResource)
{
    FloatRect contextBounds { 0, 0, contextWidth, contextHeight };

    auto colorSpace = DestinationColorSpace::SRGB();
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.platformColorSpace(), kCGImageAlphaPremultipliedLast));
    GraphicsContextCG context { cgContext.get() };

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
        EXPECT_EQ(result.missingCachedResourceIdentifier, imageBufferIdentifier);
    }

    {
        auto imageBuffer = ImageBuffer::create({ 100, 100 }, RenderingMode::Unaccelerated, 1, colorSpace, PixelFormat::BGRA8);
        LocalResourceHeap resourceHeap;
        resourceHeap.add(imageBufferIdentifier, imageBuffer.releaseNonNull());

        Replayer replayer { context, list, &resourceHeap };
        auto result = replayer.replay();
        EXPECT_EQ(result.numberOfBytesRead, list.sizeInBytes());
        EXPECT_EQ(result.reasonForStopping, StopReplayReason::ReplayedAllItems);
        EXPECT_EQ(result.missingCachedResourceIdentifier, std::nullopt);
    }
}

static ItemBufferIdentifier globalBufferIdentifier = ItemBufferIdentifier::generate();

class ReadingClient : public ItemBufferReadingClient {
private:
    std::optional<ItemHandle> WARN_UNUSED_RETURN decodeItem(const uint8_t*, size_t, ItemType type, uint8_t*) final
    {
        EXPECT_EQ(type, ItemType::FillPath);
        return std::nullopt;
    }
};

class WritingClient : public ItemBufferWritingClient {
private:
    ItemBufferHandle createItemBuffer(size_t capacity) final
    {
        EXPECT_LT(capacity, globalItemBufferCapacity);
        return { globalBufferIdentifier, globalItemBuffer, globalItemBufferCapacity };
    }

    RefPtr<SharedBuffer> encodeItemOutOfLine(const DisplayListItem&) const final
    {
        return SharedBuffer::create();
    }
};

TEST(DisplayListTests, OutOfLineItemDecodingFailure)
{
    FloatRect contextBounds { 0, 0, contextWidth, contextHeight };

    auto colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.get(), kCGImageAlphaPremultipliedLast));
    GraphicsContextCG context { cgContext.get() };

    DisplayList originalList;
    WritingClient writer;
    originalList.setItemBufferWritingClient(&writer);

    Path path;
    path.moveTo({ 10., 10. });
    path.addLineTo({ 50., 50. });
    path.addLineTo({ 10., 10. });
    originalList.append<SetInlineStrokeColor>(Color::blue);
    originalList.append<FillPath>(WTFMove(path));

    DisplayList shallowCopy {{ ItemBufferHandle { globalBufferIdentifier, globalItemBuffer, originalList.sizeInBytes() } }};
    ReadingClient reader;
    shallowCopy.setItemBufferReadingClient(&reader);

    Replayer replayer { context, shallowCopy };
    auto result = replayer.replay();
    EXPECT_GT(result.numberOfBytesRead, 0U);
    EXPECT_EQ(result.nextDestinationImageBuffer, std::nullopt);
    EXPECT_EQ(result.missingCachedResourceIdentifier, std::nullopt);
    EXPECT_EQ(result.reasonForStopping, StopReplayReason::InvalidItemOrExtent);
}

TEST(DisplayListTests, InlineItemValidationFailure)
{
    FloatRect contextBounds { 0, 0, contextWidth, contextHeight };

    auto colorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto cgContext = adoptCF(CGBitmapContextCreate(nullptr, contextWidth, contextHeight, 8, 4 * contextWidth, colorSpace.get(), kCGImageAlphaPremultipliedLast));
    GraphicsContextCG context { cgContext.get() };

    auto runTestWithInvalidIdentifier = [&](GraphicsContextFlushIdentifier identifier) {
        EXPECT_FALSE(identifier.isValid());

        DisplayList list;
        ReadingClient reader;
        list.setItemBufferReadingClient(&reader);
        list.append<FlushContext>(identifier);

        Replayer replayer { context, list };
        auto result = replayer.replay();
        EXPECT_EQ(result.numberOfBytesRead, 0U);
        EXPECT_EQ(result.nextDestinationImageBuffer, std::nullopt);
        EXPECT_EQ(result.missingCachedResourceIdentifier, std::nullopt);
        EXPECT_EQ(result.reasonForStopping, StopReplayReason::InvalidItemOrExtent);
    };

    runTestWithInvalidIdentifier(GraphicsContextFlushIdentifier { });
    runTestWithInvalidIdentifier(GraphicsContextFlushIdentifier { WTF::HashTableDeletedValue });
}

} // namespace TestWebKitAPI

#endif // USE(CG)
