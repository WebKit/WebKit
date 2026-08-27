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

#import "config.h"

#import "Helpers/Utilities.h"
#import <WebCore/SharedVideoFrameInfo.h>
#include <wtf/Vector.h>

namespace TestWebKitAPI {

TEST(WebCore, SharedVideoFramePlaneAlphaSize)
{
    Vector<uint8_t> data(128);
    WebCore::SharedVideoFrameInfo info {
        'v0a8', 1, 2, 1, 1, 1, 1, 1
    };
    info.encode(data.mutableSpan());
    EXPECT_EQ(info.storageSize(), 45u);

    auto info2 = WebCore::SharedVideoFrameInfo::decode(data.span());
    EXPECT_TRUE(info2);
    EXPECT_EQ(info2->storageSize(), 45u);

    data[28] = 2;

    info2 = WebCore::SharedVideoFrameInfo::decode(data.span());
    EXPECT_TRUE(info2);
    EXPECT_EQ(info2->storageSize(), 47u);

    info2 = WebCore::SharedVideoFrameInfo::decode(data.span().subspan(0, 28));
    EXPECT_FALSE(info2);
}

}; // namespace TestWebKitAPI
