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

#include "Test.h"
#if USE(CG)
#include <CoreGraphics/CGImage.h>
#endif
#include <WebCore/ShareableBitmap.h>

#if OS(DARWIN)

namespace TestWebKitAPI {

static void fillTestPattern(std::span<uint8_t> data, size_t seed)
{
    for (size_t i = 0; i < 5 && i < data.size(); ++i)
        data[i] = seed + i;
    if (data.size() < 12)
        return;
    for (size_t i = 1; i < 6; ++i)
        data[data.size() - i] = seed + i + 77u;
    auto mid = data.size() / 2;
    data[mid] = seed + 99;
}

static void expectTestPattern(std::span<uint8_t> data, size_t seed)
{
    for (size_t i = 0; i < 5 && i < data.size(); ++i)
        EXPECT_EQ(data[i], static_cast<uint8_t>(seed + i));
    if (data.size() < 12)
        return;
    for (size_t i = 1; i < 6; ++i)
        EXPECT_EQ(data[data.size() - i], static_cast<uint8_t>(seed + i + 77u));
    auto mid = data.size() / 2;
    EXPECT_EQ(data[mid], static_cast<uint8_t>(seed + 99));
}

TEST(ShareableBitmap, DISABLED_ensureCOWBothMapsRWSenderWrite)
{
    WebCore::ShareableBitmapConfiguration configuration = { WebCore::IntSize(100, 100) };
    auto bitmap = WebCore::ShareableBitmap::create(configuration);
    fillTestPattern(bitmap->mutableSpan(), 0);
    expectTestPattern(bitmap->mutableSpan(), 0);

    auto handle = bitmap->createHandle(WebCore::SharedMemory::Protection::ReadWrite);
    auto bitmap2 = WebCore::ShareableBitmap::create(WTF::move(*handle));
    expectTestPattern(bitmap2->mutableSpan(), 0);
    fillTestPattern(bitmap->mutableSpan(), 1);
    expectTestPattern(bitmap->mutableSpan(), 1);
    expectTestPattern(bitmap2->mutableSpan(), 0);
}

TEST(ShareableBitmap, DISABLED_ensureCOWBothMapsRWSenderWriteReceiverWrite)
{
    WebCore::ShareableBitmapConfiguration configuration = { WebCore::IntSize(100, 100) };
    auto bitmap = WebCore::ShareableBitmap::create(configuration);
    fillTestPattern(bitmap->mutableSpan(), 0);
    expectTestPattern(bitmap->mutableSpan(), 0);

    auto handle = bitmap->createHandle(WebCore::SharedMemory::Protection::ReadWrite);
    auto bitmap2 = WebCore::ShareableBitmap::create(WTF::move(*handle));
    expectTestPattern(bitmap2->mutableSpan(), 0);
    fillTestPattern(bitmap2->mutableSpan(), 1);
    expectTestPattern(bitmap->mutableSpan(), 0);
    expectTestPattern(bitmap2->mutableSpan(), 1);
}

TEST(ShareableBitmap, DISABLED_ensureCOWBothMapsROSenderWrite)
{
    WebCore::ShareableBitmapConfiguration configuration = { WebCore::IntSize(100, 100) };
    auto bitmap = WebCore::ShareableBitmap::create(configuration);
    fillTestPattern(bitmap->mutableSpan(), 0);
    expectTestPattern(bitmap->mutableSpan(), 0);

    auto handle = bitmap->createReadOnlyHandle();
    auto bitmap2 = *WebCore::ShareableBitmap::createReadOnly(WTF::move(handle));
    expectTestPattern(bitmap2->mutableSpan(), 0);
    fillTestPattern(bitmap->mutableSpan(), 1);
    expectTestPattern(bitmap->mutableSpan(), 1);
    expectTestPattern(bitmap2->mutableSpan(), 0);
}

}

#endif // OS(DARWIN)
