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
#include "TransferString.h"

namespace TestWebKitAPI {

// Tests that String -> IPC::TransferString -> String is the same as String.
// Tests that StringView -> IPC::TransferString -> String is the same as String.
TEST(TransferStringTests, CreateFromString)
{
    Vector<Latin1Character> longLatin1Data(1024*1024, 'a');
    Vector<char16_t> longUnicodeData(1024*1200, u'a');

    String subcases[] = {
        { },
        ""_s,
        "ab"_s,
        String { longLatin1Data },
        String { longUnicodeData },
        longUnicodeData.span().first(0), // Empty unicode.
    };
    bool bools[] = { false, true };
    for (bool releaseToCopy : bools) {
        for (auto& subcase : subcases) {
            SCOPED_TRACE(::testing::Message() << "releaseToCopy: " << releaseToCopy << " subcase: \"" << subcase << "\"");
            {
                auto ts = IPC::TransferString::create(subcase);
                EXPECT_TRUE(ts.has_value());
                auto string = releaseToCopy ? WTF::move(*ts).releaseToCopy() : WTF::move(*ts).release();
                EXPECT_EQ(string, subcase);
            }
            {
                auto ts = IPC::TransferString::create(StringView { subcase });
                EXPECT_TRUE(ts.has_value());
                auto string = releaseToCopy ? WTF::move(*ts).releaseToCopy() : WTF::move(*ts).release();
                EXPECT_EQ(string, subcase);
            }
        }
    }
}

}
