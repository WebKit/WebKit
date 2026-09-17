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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import <wtf/text/CString.h>

#import <Foundation/Foundation.h>
#import <array>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>

namespace TestWebKitAPI {

// A requires-expression on a concrete type is evaluated eagerly, so the absence of the member has to
// be checked through a template parameter to be a substitution failure rather than an error.
template<typename StringType> concept HasCreateNSString = requires(const StringType& string) {
    string.createNSString();
};

template<typename StringType> concept HasNSStringConstructor = requires(NSString *string) {
    StringType { string };
};

TEST(WTF, CStringWithEncodingFromNSString)
{
    UTF8CString utf8String { @"Water🍉Melon" };
    EXPECT_TRUE(utf8String == UTF8CString { u8"Water🍉Melon"_span });

    // nil becomes a null string, like String(NSString *), rather than the empty string that
    // createNSString() produces going the other way.
    UTF8CString nullString { (NSString *)nil };
    EXPECT_TRUE(nullString.isNull());
    UTF8CString emptyString { @"" };
    EXPECT_FALSE(emptyString.isNull());
    EXPECT_TRUE(emptyString.isEmpty());

    // Only UTF-8 can be converted to without the conversion silently failing.
    static_assert(HasNSStringConstructor<UTF8CString>);
    static_assert(!HasNSStringConstructor<Latin1CString>);
    static_assert(!HasNSStringConstructor<ASCIICString>);
}

TEST(WTF, CStringWithEncodingCreateNSString)
{
    // The encoding is in the type, so each alias picks the right NSStringEncoding.
    UTF8CString utf8String { u8"Water🍉Melon"_span };
    EXPECT_TRUE([utf8String.createNSString().get() isEqualToString:@"Water🍉Melon"]);

    constexpr auto latin1Cafe = WTF::toArray<Latin1Character>({ 'c', 'a', 'f', 0xE9 });
    Latin1CString latin1String { std::span<const Latin1Character> { latin1Cafe } };
    EXPECT_TRUE([latin1String.createNSString().get() isEqualToString:@"café"]);

    ASCIICString asciiString { "cafe"_s };
    EXPECT_TRUE([asciiString.createNSString().get() isEqualToString:@"cafe"]);

    // Null and empty strings both become an empty NSString, like String::createNSString().
    UTF8CString nullString;
    EXPECT_TRUE([nullString.createNSString().get() isEqualToString:@""]);
    UTF8CString emptyString { u8""_span };
    EXPECT_TRUE([emptyString.createNSString().get() isEqualToString:@""]);

    // An untyped CString has no encoding to convert from, so it has no createNSString().
    static_assert(HasCreateNSString<UTF8CString>);
    static_assert(HasCreateNSString<Latin1CString>);
    static_assert(HasCreateNSString<ASCIICString>);
    static_assert(!HasCreateNSString<CString>);
}

} // namespace TestWebKitAPI
