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

#import "Test.h"
#import "TransferString.h"
#import <Foundation/Foundation.h>

namespace TestWebKitAPI {

// Tests that NSString -> TransferString -> String is same as NSString -> String.
TEST(TransferStringTests, CreateFromNSString)
{
    Vector<Latin1Character> longLatin1Data(1024*1024, 'a');
    Vector<char16_t> longUnicodeData(1024*1200, u'a');

    RetainPtr<NSString> subcases[] = {
        nil,
        adoptNS(@""),
        adoptNS(@"ab"),
        adoptNS([[NSString alloc] initWithBytes:longLatin1Data.span().data() length:longLatin1Data.span().size() encoding:NSISOLatin1StringEncoding]),
        adoptNS([[NSString alloc] initWithBytes:longLatin1Data.span().data() length:longLatin1Data.span().size() encoding:NSUTF8StringEncoding]),
        adoptNS([[NSString alloc] initWithBytes:longUnicodeData.span().data() length:longUnicodeData.span().size() encoding:NSUnicodeStringEncoding])
    };

    bool bools[] = { false, true };
    for (bool releaseToCopy : bools) {
        for (auto& subcase : subcases) {
            String wtfString { subcase.get() };
            SCOPED_TRACE(::testing::Message() << "releaseToCopy: " << releaseToCopy << " subcase: \"" << wtfString << "\"" << " ptr: " << static_cast<void*>(subcase.get()));
            auto ts = IPC::TransferString::create(subcase.get());
            EXPECT_TRUE(ts.has_value());
            auto string = releaseToCopy ? WTF::move(*ts).releaseToCopy() : WTF::move(*ts).release();
            ASSERT_TRUE(string.has_value());
            EXPECT_EQ(*string, wtfString);
        }
    }
}

}
