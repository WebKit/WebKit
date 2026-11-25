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

#import "config.h"
#import <wtf/text/StringFunctions.h>

#import "Helpers/Test.h"
#import <Foundation/Foundation.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/AtomString.h>
#import <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(WTF_StringFunctions, CreateNSString)
{
    struct Expectation {
        RefPtr<StringImpl> string;
        RetainPtr<NSString> nsString; // createNSString(); null converts to @"", not nil.
        RetainPtr<NSString> nilIfEmpty;
        RetainPtr<NSString> nilIfNull;
    };
    Expectation expectations[] {
        { nullptr, @"", nil, nil },
        { emptyString(), @"", nil, @"" },
        { "ASCII"_s, @"ASCII", @"ASCII", @"ASCII" },
        { StringImpl::fromLatin1("caf\xE9"), @"café", @"café", @"café" },
        { u"Строка"_str, @"Строка", @"Строка", @"Строка" },
        { AtomStringImpl::add("atom"_s).ptr(), @"atom", @"atom", @"atom" },
    };
    for (auto& expected : expectations) {
        RefPtr<StringImpl> pointer = expected.string;
        EXPECT_TRUE([expected.nsString.get() isEqualToString:createNSString(pointer).get()]);
        EXPECT_TRUE([expected.nsString.get() isEqualToString:createNSString(pointer.get()).get()]);
        auto nilIfEmpty = nsStringNilIfEmpty(pointer);
        EXPECT_EQ(!expected.nilIfEmpty, !nilIfEmpty);
        if (expected.nilIfEmpty)
            EXPECT_TRUE([expected.nilIfEmpty.get() isEqualToString:nilIfEmpty.get()]);
        auto nilIfNull = nsStringNilIfNull(pointer);
        EXPECT_EQ(!expected.nilIfNull, !nilIfNull);
        if (expected.nilIfNull)
            EXPECT_TRUE([expected.nilIfNull.get() isEqualToString:nilIfNull.get()]);
    }

    RefPtr<StringImpl> null;
    EXPECT_NOT_NULL(createNSString(null).get());
    EXPECT_EQ(0u, [createNSString(null).get() length]);
    EXPECT_NULL(nsStringNilIfNull(null).get());
    EXPECT_NULL(nsStringNilIfEmpty(RefPtr<StringImpl> { emptyString() }).get());
    EXPECT_NOT_NULL(nsStringNilIfNull(RefPtr<StringImpl> { emptyString() }).get());
}

TEST(WTF_StringFunctions, CreateFromNSString)
{
    EXPECT_NULL(StringImpl::create(static_cast<NSString *>(nil)));
    struct Expectation {
        RetainPtr<NSString> nsString;
        std::u16string_view expected;
    };
    for (auto& expectation : std::initializer_list<Expectation> { { @"", u"" }, { @"ASCII", u"ASCII" }, { @"café", u"café" }, { @"Строка", u"Строка" } }) {
        RefPtr<StringImpl> actual = StringImpl::create(expectation.nsString.get());
        EXPECT_NOT_NULL(actual);
        EXPECT_TRUE(equal(actual, std::span<const char16_t> { expectation.expected.data(), expectation.expected.size() }));
    }
    EXPECT_TRUE(StringImpl::create(@"ASCII")->is8Bit());
}

} // namespace TestWebKitAPI
