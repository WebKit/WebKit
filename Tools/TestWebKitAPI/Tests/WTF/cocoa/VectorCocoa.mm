/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import <wtf/Vector.h>

#if USE(FOUNDATION)

#import <Foundation/Foundation.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static RetainPtr<NSString> mapString(RetainPtr<NSString> string)
{
    return string;
}

TEST(WTF_Vector, CompactMapStaticFunctionReturnRetainPtr)
{
    @autoreleasepool {
        Vector<RetainPtr<NSString>> vector { @"one", @"two", nil, @"three" };

        static_assert(std::is_same<decltype(WTF::compactMap(vector, mapString)), typename WTF::Vector<RetainPtr<NSString>>>::value, "WTF::compactMap returns Vector<RetainPtr<>>");
        auto mapped = WTF::compactMap(vector, mapString);

        EXPECT_EQ(3U, mapped.size());
        EXPECT_TRUE([mapped[0] isEqualToString:@"one"]);
        EXPECT_TRUE([mapped[1] isEqualToString:@"two"]);
        EXPECT_TRUE([mapped[2] isEqualToString:@"three"]);
    }
}

} // namespace TestWebKitAPI

#endif // USE(FOUNDATION)
