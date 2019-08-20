/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WTFStringUtilities.h"

#include <wtf/unicode/CharacterNames.h>

namespace TestWebKitAPI {

static void expectBuilderContent(const String& expected, const StringBuilder& builder)
{
    // Not using builder.toString() or builder.toStringPreserveCapacity() because they all
    // change internal state of builder.
    if (builder.is8Bit())
        EXPECT_EQ(expected, String(builder.characters8(), builder.length()));
    else
        EXPECT_EQ(expected, String(builder.characters16(), builder.length()));
}

void expectEmpty(const StringBuilder& builder)
{
    EXPECT_EQ(0U, builder.length());
    EXPECT_TRUE(builder.isEmpty());
    EXPECT_EQ(0, builder.characters8());
}

TEST(StringBuilderTest, DefaultConstructor)
{
    StringBuilder builder;
    expectEmpty(builder);
}

TEST(StringBuilderTest, Append)
{
    StringBuilder builder;
    builder.append(String("0123456789"));
    expectBuilderContent("0123456789", builder);
    builder.append("abcd");
    expectBuilderContent("0123456789abcd", builder);
    builder.appendCharacters("efgh", 3);
    expectBuilderContent("0123456789abcdefg", builder);
    builder.append("");
    expectBuilderContent("0123456789abcdefg", builder);
    builder.append('#');
    expectBuilderContent("0123456789abcdefg#", builder);

    builder.toString(); // Test after reifyString().
    StringBuilder builder1;
    builder.appendCharacters("", 0);
    expectBuilderContent("0123456789abcdefg#", builder);
    builder1.appendCharacters(builder.characters8(), builder.length());
    builder1.append("XYZ");
    builder.appendCharacters(builder1.characters8(), builder1.length());
    expectBuilderContent("0123456789abcdefg#0123456789abcdefg#XYZ", builder);

    StringBuilder builder2;
    builder2.reserveCapacity(100);
    builder2.append("xyz");
    const LChar* characters = builder2.characters8();
    builder2.append("0123456789");
    EXPECT_EQ(characters, builder2.characters8());
    builder2.toStringPreserveCapacity(); // Test after reifyString with buffer preserved.
    builder2.append("abcd");
    EXPECT_EQ(characters, builder2.characters8());

    // Test appending UChar32 characters to StringBuilder.
    StringBuilder builderForUChar32Append;
    UChar32 frakturAChar = 0x1D504;
    builderForUChar32Append.appendCharacter(frakturAChar); // The fraktur A is not in the BMP, so it's two UTF-16 code units long.
    EXPECT_EQ(2U, builderForUChar32Append.length());
    builderForUChar32Append.appendCharacter(static_cast<UChar32>('A'));
    EXPECT_EQ(3U, builderForUChar32Append.length());
    const UChar resultArray[] = { U16_LEAD(frakturAChar), U16_TRAIL(frakturAChar), 'A' };
    expectBuilderContent(String(resultArray, WTF_ARRAY_LENGTH(resultArray)), builderForUChar32Append);
    {
        StringBuilder builder;
        StringBuilder builder2;
        UChar32 frakturAChar = 0x1D504;
        const UChar data[] = { U16_LEAD(frakturAChar), U16_TRAIL(frakturAChar) };
        builder2.appendCharacters(data, 2);
        EXPECT_EQ(2U, builder2.length());
        String result2 = builder2.toString();
        EXPECT_EQ(2U, result2.length());
        builder.append(builder2);
        builder.appendCharacters(data, 2);
        EXPECT_EQ(4U, builder.length());
        const UChar resultArray[] = { U16_LEAD(frakturAChar), U16_TRAIL(frakturAChar), U16_LEAD(frakturAChar), U16_TRAIL(frakturAChar) };
        expectBuilderContent(String(resultArray, WTF_ARRAY_LENGTH(resultArray)), builder);
    }
}

TEST(StringBuilderTest, VariadicAppend)
{
    {
        StringBuilder builder;
        builder.append(String("0123456789"));
        expectBuilderContent("0123456789", builder);
        builder.append("abcd");
        expectBuilderContent("0123456789abcd", builder);
        builder.append('e');
        expectBuilderContent("0123456789abcde", builder);
        builder.append("");
        expectBuilderContent("0123456789abcde", builder);
    }

    {
        StringBuilder builder;
        builder.append(String("0123456789"), "abcd", 'e', "");
        expectBuilderContent("0123456789abcde", builder);
        builder.append(String("A"), "B", 'C', "");
        expectBuilderContent("0123456789abcdeABC", builder);
    }

    {
        StringBuilder builder;
        builder.append(String("0123456789"), "abcd", bullseye, "");
        expectBuilderContent("0123456789abcd" + String(&bullseye, 1), builder);
        builder.append(String("A"), "B", 'C', "");
        expectBuilderContent("0123456789abcd" + String(&bullseye, 1) + "ABC", builder);
    }

    {
        // Test where we upconvert the StringBuilder from 8-bit to 16-bit, and don't fit in the existing capacity.
        StringBuilder builder;
        builder.append(String("0123456789"), "abcd", 'e', "");
        expectBuilderContent("0123456789abcde", builder);
        EXPECT_TRUE(builder.is8Bit());
        EXPECT_LT(builder.capacity(), builder.length() + 3);
        builder.append(String("A"), "B", bullseye, "");
        expectBuilderContent("0123456789abcdeAB" + String(&bullseye, 1), builder);
    }

    {
        // Test where we upconvert the StringBuilder from 8-bit to 16-bit, but would have fit in the capacity if the upconvert wasn't necessary.
        StringBuilder builder;
        builder.append(String("0123456789"), "abcd", 'e', "");
        expectBuilderContent("0123456789abcde", builder);
        builder.reserveCapacity(32);
        EXPECT_TRUE(builder.is8Bit());
        EXPECT_GE(builder.capacity(), builder.length() + 3);
        builder.append(String("A"), "B", bullseye, "");
        expectBuilderContent("0123456789abcdeAB" + String(&bullseye, 1), builder);
    }
}

TEST(StringBuilderTest, ToString)
{
    StringBuilder builder;
    builder.append("0123456789");
    String string = builder.toString();
    EXPECT_EQ(String("0123456789"), string);
    EXPECT_EQ(string.impl(), builder.toString().impl());

    // Changing the StringBuilder should not affect the original result of toString().
    builder.append("abcdefghijklmnopqrstuvwxyz");
    EXPECT_EQ(String("0123456789"), string);

    // Changing the StringBuilder should not affect the original result of toString() in case the capacity is not changed.
    builder.reserveCapacity(200);
    string = builder.toString();
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyz"), string);
    builder.append("ABC");
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyz"), string);

    // Changing the original result of toString() should not affect the content of the StringBuilder.
    String string1 = builder.toString();
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABC"), string1);
    string1.append("DEF");
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABC"), builder.toString());
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABCDEF"), string1);

    // Resizing the StringBuilder should not affect the original result of toString().
    string1 = builder.toString();
    builder.resize(10);
    builder.append("###");
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABC"), string1);
}

TEST(StringBuilderTest, ToStringPreserveCapacity)
{
    StringBuilder builder;
    builder.append("0123456789");
    unsigned capacity = builder.capacity();
    String string = builder.toStringPreserveCapacity();
    EXPECT_EQ(capacity, builder.capacity());
    EXPECT_EQ(String("0123456789"), string);
    EXPECT_EQ(string.impl(), builder.toStringPreserveCapacity().impl());
    EXPECT_EQ(string.characters8(), builder.characters8());

    // Changing the StringBuilder should not affect the original result of toStringPreserveCapacity().
    builder.append("abcdefghijklmnopqrstuvwxyz");
    EXPECT_EQ(String("0123456789"), string);

    // Changing the StringBuilder should not affect the original result of toStringPreserveCapacity() in case the capacity is not changed.
    builder.reserveCapacity(200);
    capacity = builder.capacity();
    string = builder.toStringPreserveCapacity();
    EXPECT_EQ(capacity, builder.capacity());
    EXPECT_EQ(string.characters8(), builder.characters8());
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyz"), string);
    builder.append("ABC");
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyz"), string);

    // Changing the original result of toStringPreserveCapacity() should not affect the content of the StringBuilder.
    capacity = builder.capacity();
    String string1 = builder.toStringPreserveCapacity();
    EXPECT_EQ(capacity, builder.capacity());
    EXPECT_EQ(string1.characters8(), builder.characters8());
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABC"), string1);
    string1.append("DEF");
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABC"), builder.toStringPreserveCapacity());
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABCDEF"), string1);

    // Resizing the StringBuilder should not affect the original result of toStringPreserveCapacity().
    capacity = builder.capacity();
    string1 = builder.toStringPreserveCapacity();
    EXPECT_EQ(capacity, builder.capacity());
    EXPECT_EQ(string.characters8(), builder.characters8());
    builder.resize(10);
    builder.append("###");
    EXPECT_EQ(String("0123456789abcdefghijklmnopqrstuvwxyzABC"), string1);
}

TEST(StringBuilderTest, Clear)
{
    StringBuilder builder;
    builder.append("0123456789");
    builder.clear();
    expectEmpty(builder);
}

TEST(StringBuilderTest, Array)
{
    StringBuilder builder;
    builder.append("0123456789");
    EXPECT_EQ('0', static_cast<char>(builder[0]));
    EXPECT_EQ('9', static_cast<char>(builder[9]));
    builder.toString(); // Test after reifyString().
    EXPECT_EQ('0', static_cast<char>(builder[0]));
    EXPECT_EQ('9', static_cast<char>(builder[9]));
}

TEST(StringBuilderTest, Resize)
{
    StringBuilder builder;
    builder.append("0123456789");
    builder.resize(10);
    EXPECT_EQ(10U, builder.length());
    expectBuilderContent("0123456789", builder);
    builder.resize(8);
    EXPECT_EQ(8U, builder.length());
    expectBuilderContent("01234567", builder);

    builder.toString();
    builder.resize(7);
    EXPECT_EQ(7U, builder.length());
    expectBuilderContent("0123456", builder);
    builder.resize(0);
    expectEmpty(builder);
}

TEST(StringBuilderTest, Equal)
{
    StringBuilder builder1;
    StringBuilder builder2;
    EXPECT_TRUE(builder1 == builder2);
    EXPECT_TRUE(equal(builder1, static_cast<LChar*>(0), 0));
    EXPECT_TRUE(builder1 == String());
    EXPECT_TRUE(String() == builder1);
    EXPECT_TRUE(builder1 != String("abc"));

    builder1.append("123");
    builder1.reserveCapacity(32);
    builder2.append("123");
    builder1.reserveCapacity(64);
    EXPECT_TRUE(builder1 == builder2);
    EXPECT_TRUE(builder1 == String("123"));
    EXPECT_TRUE(String("123") == builder1);

    builder2.append("456");
    EXPECT_TRUE(builder1 != builder2);
    EXPECT_TRUE(builder2 != builder1);
    EXPECT_TRUE(String("123") != builder2);
    EXPECT_TRUE(builder2 != String("123"));
    builder2.toString(); // Test after reifyString().
    EXPECT_TRUE(builder1 != builder2);

    builder2.resize(3);
    EXPECT_TRUE(builder1 == builder2);

    builder1.toString(); // Test after reifyString().
    EXPECT_TRUE(builder1 == builder2);
}

TEST(StringBuilderTest, CanShrink)
{
    StringBuilder builder;
    builder.reserveCapacity(256);
    EXPECT_TRUE(builder.canShrink());
    for (int i = 0; i < 256; i++)
        builder.append('x');
    EXPECT_EQ(builder.length(), builder.capacity());
    EXPECT_FALSE(builder.canShrink());
}

TEST(StringBuilderTest, ToAtomString)
{
    StringBuilder builder;
    builder.append("123");
    AtomString atomString = builder.toAtomString();
    EXPECT_EQ(String("123"), atomString);

    builder.reserveCapacity(256);
    EXPECT_TRUE(builder.canShrink());
    for (int i = builder.length(); i < 128; i++)
        builder.append('x');
    AtomString atomString1 = builder.toAtomString();
    EXPECT_EQ(128u, atomString1.length());
    EXPECT_EQ('x', atomString1[127]);

    // Later change of builder should not affect the atomic string.
    for (int i = builder.length(); i < 256; i++)
        builder.append('x');
    EXPECT_EQ(128u, atomString1.length());

    EXPECT_FALSE(builder.canShrink());
    String string = builder.toString();
    AtomString atomString2 = builder.toAtomString();
    // They should share the same StringImpl.
    EXPECT_EQ(atomString2.impl(), string.impl());
}

TEST(StringBuilderTest, ToAtomStringOnEmpty)
{
    { // Default constructed.
        StringBuilder builder;
        AtomString atomString = builder.toAtomString();
        EXPECT_EQ(emptyAtom(), atomString);
    }
    { // With capacity.
        StringBuilder builder;
        builder.reserveCapacity(64);
        AtomString atomString = builder.toAtomString();
        EXPECT_EQ(emptyAtom(), atomString);
    }
    { // AtomString constructed from a null string.
        StringBuilder builder;
        builder.append(String());
        AtomString atomString = builder.toAtomString();
        EXPECT_EQ(emptyAtom(), atomString);
    }
    { // AtomString constructed from an empty string.
        StringBuilder builder;
        builder.append(emptyString());
        AtomString atomString = builder.toAtomString();
        EXPECT_EQ(emptyAtom(), atomString);
    }
    { // AtomString constructed from an empty StringBuilder.
        StringBuilder builder;
        StringBuilder emptyBuilder;
        builder.append(emptyBuilder);
        AtomString atomString = builder.toAtomString();
        EXPECT_EQ(emptyAtom(), atomString);
    }
    { // AtomString constructed from an empty char* string.
        StringBuilder builder;
        builder.appendCharacters("", 0);
        AtomString atomString = builder.toAtomString();
        EXPECT_EQ(emptyAtom(), atomString);
    }
    { // Cleared StringBuilder.
        StringBuilder builder;
        builder.appendLiteral("WebKit");
        builder.clear();
        AtomString atomString = builder.toAtomString();
        EXPECT_EQ(emptyAtom(), atomString);
    }
}

} // namespace
