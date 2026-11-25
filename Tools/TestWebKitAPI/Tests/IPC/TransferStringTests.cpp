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

#include "IPCTestUtilities.h"
#include "Helpers/Test.h"
#include "TransferString.h"

namespace TestWebKitAPI {

// Tests that String -> IPC::TransferString -> String is the same as String.
// Tests that StringView -> IPC::TransferString -> String is the same as String.
TEST(TransferStringTests, CreateFromString)
{
    Vector<Latin1Character> longLatin1Data(FillWith { }, 1024*1024, 'a');
    Vector<char16_t> longUnicodeData(FillWith { }, 1024*1200, u'a');

    RefPtr<StringImpl> subcases[] = {
        { },
        ""_s,
        "ab"_s,
        StringImpl::create(longLatin1Data),
        StringImpl::create(longUnicodeData),
        StringImpl::create(longUnicodeData.span().first(0)), // Empty unicode.
    };
    bool bools[] = { false, true };
    for (bool releaseToCopy : bools) {
        for (auto& subcase : subcases) {
            {
                SCOPED_TRACE(::testing::Message() << "TransferString(String) releaseToCopy: " << releaseToCopy << " subcase: \"" << subcase << "\"");
                auto ts = IPC::TransferString::create(subcase.get());
                EXPECT_TRUE(ts.has_value());
                auto string = releaseToCopy ? WTF::move(*ts).releaseToCopy() : WTF::move(*ts).release();
                EXPECT_EQ(string, subcase);
            }
            {
                SCOPED_TRACE(::testing::Message() << "TransferString(StringView) releaseToCopy: " << releaseToCopy << " subcase: \"" << subcase << "\"");
                auto ts = IPC::TransferString::create(StringView { subcase });
                EXPECT_TRUE(ts.has_value());
                auto string = releaseToCopy ? WTF::move(*ts).releaseToCopy() : WTF::move(*ts).release();
                EXPECT_EQ(string, subcase);
            }

            {
                SCOPED_TRACE(::testing::Message() << "TransferString(String) IPC encode/decode, releaseToCopy: " << releaseToCopy << " subcase: \"" << subcase << "\"");
                auto ts = IPC::TransferString::create(subcase.get());
                EXPECT_TRUE(ts.has_value());
                auto tsAfterIPC = copyViaEncoder(*ts);
                ASSERT_TRUE(tsAfterIPC.has_value());
                auto string = releaseToCopy ? WTF::move(*tsAfterIPC).releaseToCopy() : WTF::move(*tsAfterIPC).release();
                EXPECT_EQ(string, subcase);
            }

            {
                SCOPED_TRACE(::testing::Message() << "TransferString(StringView) IPC encode/decode, releaseToCopy: " << releaseToCopy << " subcase: \"" << subcase << "\"");
                auto ts = IPC::TransferString::create(StringView { subcase });
                EXPECT_TRUE(ts.has_value());
                auto tsAfterIPC = copyViaEncoder(*ts);
                ASSERT_TRUE(tsAfterIPC.has_value());
                auto string = releaseToCopy ? WTF::move(*tsAfterIPC).releaseToCopy() : WTF::move(*tsAfterIPC).release();
                EXPECT_EQ(string, subcase);
            }
        }
    }
}

// Characterization: TransferString built from inline IPC data. A std::monostate is a null string. A span is copied;
// a span with a null data pointer (as decoded for an empty string) is the empty string, not a null string.
// Written against StringView so that the same source compiles with String and with RefPtr<StringImpl>.
static IPC::TransferString::IPCData makeTransferStringTestIPCData(unsigned subcase)
{
    static constexpr Latin1Character latin1Characters[] = { 'a', 'b' };
    static constexpr char16_t utf16Characters[] = { u'a', 0x263A };
    switch (subcase) {
    case 0:
        return std::monostate { };
    case 1:
        return std::span<const Latin1Character> { };
    case 2:
        return std::span<const char16_t> { };
    case 3:
        return std::span<const Latin1Character> { latin1Characters }.first(0);
    case 4:
        return std::span<const char16_t> { utf16Characters }.first(0);
    case 5:
        return std::span<const Latin1Character> { latin1Characters };
    default:
        return std::span<const char16_t> { utf16Characters };
    }
}

TEST(TransferStringTests, CreateFromIPCData)
{
    struct Expectation {
        const char* name;
        bool isNull;
        unsigned length;
        bool is8Bit;
    };
    static constexpr Expectation expectations[] = {
        { "monostate", true, 0, true },
        { "null Latin-1 span", false, 0, true },
        { "null UTF-16 span", false, 0, true },
        { "empty Latin-1 span", false, 0, true },
        { "empty UTF-16 span", false, 0, true },
        { "Latin-1 span", false, 2, true },
        { "UTF-16 span", false, 2, false },
    };
    for (bool releaseToCopy : { false, true }) {
        for (unsigned subcase = 0; subcase < std::size(expectations); ++subcase) {
            auto& expected = expectations[subcase];
            for (bool viaIPC : { false, true }) {
                SCOPED_TRACE(::testing::Message() << "subcase: " << expected.name << " releaseToCopy: " << releaseToCopy << " viaIPC: " << viaIPC);
                IPC::TransferString transferString { makeTransferStringTestIPCData(subcase) };
                if (viaIPC) {
                    auto afterIPC = copyViaEncoder(transferString);
                    ASSERT_TRUE(afterIPC.has_value());
                    transferString = WTF::move(*afterIPC);
                }
                auto string = releaseToCopy ? WTF::move(transferString).releaseToCopy() : WTF::move(transferString).release();
                ASSERT_TRUE(string.has_value());
                StringView view { *string };
                EXPECT_EQ(view.isNull(), expected.isNull);
                EXPECT_EQ(view.length(), expected.length);
                if (expected.length) {
                    EXPECT_EQ(view.is8Bit(), expected.is8Bit);
                    EXPECT_EQ(view[0], u'a');
                }
            }
        }
    }
}

}
