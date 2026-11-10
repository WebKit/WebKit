/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "Helpers/Test.h"
#include <CoreFoundation/CoreFoundation.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

TEST(StringCF, ConstructFromNull)
{
    RefPtr<StringImpl> string(StringImpl::create(static_cast<CFStringRef>(nullptr)));
    EXPECT_TRUE(!string);
}

TEST(StringCF, ConstructFromEmpty)
{
    RefPtr<StringImpl> string(StringImpl::create(CFSTR("")));
    EXPECT_FALSE(!string);
    EXPECT_TRUE(isEmpty(string));
    EXPECT_TRUE(is8Bit(string));
}

TEST(StringCF, ConstructFromLatin1)
{
    RefPtr<StringImpl> string(StringImpl::create(CFSTR("Hello, world!")));
    EXPECT_EQ(length(string), 13u);
    EXPECT_TRUE(is8Bit(string));
    EXPECT_EQ(string, "Hello, world!"_s);
}

TEST(StringCF, ConstructFromLatin1WithHighBytes)
{
    // Create a CFString with Latin-1 characters above ASCII range (e.g. U+00E9 = é).
    const uint8_t latin1Bytes[] = { 'r', 0xE9, 's', 'u', 'm', 0xE9 }; // "résumé" in Latin-1
    RetainPtr cfString = adoptCF(CFStringCreateWithBytes(kCFAllocatorDefault, latin1Bytes, sizeof(latin1Bytes), kCFStringEncodingISOLatin1, false));

    RefPtr<StringImpl> string(StringImpl::create(cfString.get()));
    EXPECT_EQ(length(string), 6u);
    EXPECT_FALSE(!string);
    EXPECT_EQ(string, StringImpl::fromUTF8("résumé"));
}

TEST(StringCF, ConstructFromUTF16WithLatin1Characters)
{
    // Create a long CFString from UTF-16 characters that are all in the Latin-1 range.
    // Use a long string so CF is likely to expose its internal UTF-16 buffer via
    // CFStringGetCharactersSpan, exercising the create8BitIfPossible narrowing path.
    Vector<char16_t> characters(FillWith { }, 4096, 'A');
    RetainPtr cfString = adoptCF(CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(characters.span().data()), characters.size()));

    RefPtr<StringImpl> string(StringImpl::create(cfString.get()));
    EXPECT_EQ(length(string), 4096u);
    EXPECT_TRUE(is8Bit(string));
    EXPECT_EQ(string, StringImpl::create(Vector<Latin1Character>(FillWith { }, 4096, 'A')).ptr());
}

TEST(StringCF, ConstructFromUTF16WithHighLatin1Characters)
{
    // Create a CFString from UTF-16 characters that include high Latin-1 chars (U+00E9).
    const char16_t characters[] = { 'r', 0x00E9, 's', 'u', 'm', 0x00E9 }; // "résumé"
    RetainPtr cfString = adoptCF(CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(characters), std::size(characters)));

    RefPtr<StringImpl> string(StringImpl::create(cfString.get()));
    EXPECT_EQ(length(string), 6u);
    EXPECT_FALSE(!string);
    EXPECT_EQ(string, StringImpl::fromUTF8("résumé"));
}

TEST(StringCF, ConstructFromUTF16WithNonLatin1Characters)
{
    // Create a CFString with characters outside Latin-1 range (e.g. U+4E16 = 世, U+754C = 界).
    const char16_t characters[] = { 0x4E16, 0x754C }; // "世界"
    RetainPtr cfString = adoptCF(CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(characters), std::size(characters)));

    RefPtr<StringImpl> string(StringImpl::create(cfString.get()));
    EXPECT_EQ(length(string), 2u);
    EXPECT_FALSE(is8Bit(string));
    EXPECT_EQ(codeUnitAt(string, 0), 0x4E16);
    EXPECT_EQ(codeUnitAt(string, 1), 0x754C);
}

TEST(StringCF, ConstructFromUTF16MixedLatin1AndNonLatin1)
{
    // Mix of Latin-1 and non-Latin-1 characters — should stay 16-bit.
    const char16_t characters[] = { 'H', 'i', 0x4E16 }; // "Hi世"
    RetainPtr cfString = adoptCF(CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(characters), std::size(characters)));

    RefPtr<StringImpl> string(StringImpl::create(cfString.get()));
    EXPECT_EQ(length(string), 3u);
    EXPECT_FALSE(is8Bit(string));
    EXPECT_EQ(codeUnitAt(string, 0), 'H');
    EXPECT_EQ(codeUnitAt(string, 1), 'i');
    EXPECT_EQ(codeUnitAt(string, 2), 0x4E16);
}

TEST(StringCF, RoundTrip)
{
    RefPtr<StringImpl> original("WebKit"_si);
    auto cfString = createCFString(original);
    RefPtr<StringImpl> roundTripped(StringImpl::create(cfString.get()));
    EXPECT_EQ(original, roundTripped);
    EXPECT_TRUE(is8Bit(roundTripped));
}

} // namespace TestWebKitAPI
