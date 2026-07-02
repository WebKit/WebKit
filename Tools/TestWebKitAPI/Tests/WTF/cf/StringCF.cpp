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
    String string(static_cast<CFStringRef>(nullptr));
    EXPECT_TRUE(string.isNull());
}

TEST(StringCF, ConstructFromEmpty)
{
    String string(CFSTR(""));
    EXPECT_FALSE(string.isNull());
    EXPECT_TRUE(string.isEmpty());
    EXPECT_TRUE(string.is8Bit());
}

TEST(StringCF, ConstructFromLatin1)
{
    String string(CFSTR("Hello, world!"));
    EXPECT_EQ(string.length(), 13u);
    EXPECT_TRUE(string.is8Bit());
    EXPECT_EQ(string, "Hello, world!"_s);
}

TEST(StringCF, ConstructFromLatin1WithHighBytes)
{
    // Create a CFString with Latin-1 characters above ASCII range (e.g. U+00E9 = é).
    const uint8_t latin1Bytes[] = { 'r', 0xE9, 's', 'u', 'm', 0xE9 }; // "résumé" in Latin-1
    RetainPtr cfString = adoptCF(CFStringCreateWithBytes(kCFAllocatorDefault, latin1Bytes, sizeof(latin1Bytes), kCFStringEncodingISOLatin1, false));

    String string(cfString.get());
    EXPECT_EQ(string.length(), 6u);
    EXPECT_FALSE(string.isNull());
    EXPECT_EQ(string, String::fromUTF8("résumé"));
}

TEST(StringCF, ConstructFromUTF16WithLatin1Characters)
{
    // Create a long CFString from UTF-16 characters that are all in the Latin-1 range.
    // Use a long string so CF is likely to expose its internal UTF-16 buffer via
    // CFStringGetCharactersSpan, exercising the create8BitIfPossible narrowing path.
    Vector<char16_t> characters(FillWith { }, 4096, 'A');
    RetainPtr cfString = adoptCF(CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(characters.span().data()), characters.size()));

    String string(cfString.get());
    EXPECT_EQ(string.length(), 4096u);
    EXPECT_TRUE(string.is8Bit());
    EXPECT_EQ(string, String(Vector<Latin1Character>(FillWith { }, 4096, 'A')));
}

TEST(StringCF, ConstructFromUTF16WithHighLatin1Characters)
{
    // Create a CFString from UTF-16 characters that include high Latin-1 chars (U+00E9).
    const char16_t characters[] = { 'r', 0x00E9, 's', 'u', 'm', 0x00E9 }; // "résumé"
    RetainPtr cfString = adoptCF(CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(characters), std::size(characters)));

    String string(cfString.get());
    EXPECT_EQ(string.length(), 6u);
    EXPECT_FALSE(string.isNull());
    EXPECT_EQ(string, String::fromUTF8("résumé"));
}

TEST(StringCF, ConstructFromUTF16WithNonLatin1Characters)
{
    // Create a CFString with characters outside Latin-1 range (e.g. U+4E16 = 世, U+754C = 界).
    const char16_t characters[] = { 0x4E16, 0x754C }; // "世界"
    RetainPtr cfString = adoptCF(CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(characters), std::size(characters)));

    String string(cfString.get());
    EXPECT_EQ(string.length(), 2u);
    EXPECT_FALSE(string.is8Bit());
    EXPECT_EQ(string[0], 0x4E16);
    EXPECT_EQ(string[1], 0x754C);
}

TEST(StringCF, ConstructFromUTF16MixedLatin1AndNonLatin1)
{
    // Mix of Latin-1 and non-Latin-1 characters — should stay 16-bit.
    const char16_t characters[] = { 'H', 'i', 0x4E16 }; // "Hi世"
    RetainPtr cfString = adoptCF(CFStringCreateWithCharacters(kCFAllocatorDefault, reinterpret_cast<const UniChar*>(characters), std::size(characters)));

    String string(cfString.get());
    EXPECT_EQ(string.length(), 3u);
    EXPECT_FALSE(string.is8Bit());
    EXPECT_EQ(string[0], 'H');
    EXPECT_EQ(string[1], 'i');
    EXPECT_EQ(string[2], 0x4E16);
}

TEST(StringCF, RoundTrip)
{
    String original("WebKit"_s);
    auto cfString = original.createCFString();
    String roundTripped(cfString.get());
    EXPECT_EQ(original, roundTripped);
    EXPECT_TRUE(roundTripped.is8Bit());
}

} // namespace TestWebKitAPI
