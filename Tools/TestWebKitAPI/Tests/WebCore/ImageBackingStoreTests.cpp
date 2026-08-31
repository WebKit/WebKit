/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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

#include "Helpers/Test.h"
#include <WebCore/ImageBackingStore.h>

namespace TestWebKitAPI {
using namespace WebCore;

// A copied backing store must keep the source's frame rect. The animated image
// decoders store each frame's rect on its backing store and later read it back
// off a *previous* frame to composite the next one: GIFImageDecoder::initFrameBuffer
// and PNGImageDecoder::initFrameBuffer clear the previous frame's rect when its
// disposal method is RestoreToBackground, and WEBPImageDecoder::applyPostProcessing
// indexes the decoded frame by it. Those buffers are copied whenever
// m_frameBufferCache grows (ScalableImageDecoderFrame's copy constructor deep
// copies the backing store), which happens as an animation's frame count grows
// while it is still loading. A copy that dropped the rect would silently turn a
// dispose-to-background into a no-op and leave applyPostProcessing indexing an
// empty rect.
TEST(ImageBackingStore, CopyPreservesFrameRect)
{
    auto store = ImageBackingStore::create(IntSize(32, 32));
    ASSERT_NE(store, nullptr);
    EXPECT_EQ(store->frameRect(), IntRect(0, 0, 32, 32));

    store->setFrameRect(IntRect(4, 8, 16, 12));
    EXPECT_EQ(store->frameRect(), IntRect(4, 8, 16, 12));

    auto copy = ImageBackingStore::create(*store);
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->size(), IntSize(32, 32));
    EXPECT_EQ(copy->frameRect(), IntRect(4, 8, 16, 12));
}

// The default frame rect covers the whole store, and that must survive a copy
// too: a copy that reset it to an empty rect would still differ from the source.
TEST(ImageBackingStore, CopyPreservesDefaultFrameRect)
{
    auto store = ImageBackingStore::create(IntSize(20, 10));
    ASSERT_NE(store, nullptr);

    auto copy = ImageBackingStore::create(*store);
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->frameRect(), IntRect(0, 0, 20, 10));
    EXPECT_FALSE(copy->frameRect().isEmpty());
}

// The copy must be an independent buffer, not a view onto the source's pixels.
TEST(ImageBackingStore, CopyIsIndependent)
{
    auto store = ImageBackingStore::create(IntSize(8, 8));
    ASSERT_NE(store, nullptr);
    store->fillRect(IntRect(0, 0, 8, 8), 255, 0, 0, 255);

    auto copy = ImageBackingStore::create(*store);
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->pixelAt(0, 0), store->pixelAt(0, 0));

    store->fillRect(IntRect(0, 0, 8, 8), 0, 255, 0, 255);
    EXPECT_NE(copy->pixelAt(0, 0), store->pixelAt(0, 0));
}

} // namespace TestWebKitAPI
