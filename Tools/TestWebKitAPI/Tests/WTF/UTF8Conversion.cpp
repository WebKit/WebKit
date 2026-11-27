/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 *     * Neither the name of Apple Inc. nor the names of its
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

#include "Helpers/Test.h"

namespace TestWebKitAPI {

template<typename... ValueTypes> constexpr auto char8Array(ValueTypes... values)
{
    return std::array<char8_t, sizeof...(values)> { static_cast<char8_t>(values)... };
}

template<typename... ValueTypes> constexpr auto char16Array(ValueTypes... values)
{
    return std::array<char16_t, sizeof...(values)> { static_cast<char16_t>(values)... };
}

template<typename... ValueTypes> constexpr auto latin1Array(ValueTypes... values)
{
    return std::array<Latin1Character, sizeof...(values)> { static_cast<Latin1Character>(values)... };
}

template<typename CharacterType> const char* serialize(const WTF::Unicode::ConversionResult<CharacterType>& result)
{
    using namespace WTF::Unicode;

    std::ostringstream stream;
    auto separator = "";
    stream << std::hex << std::uppercase;
    for (auto character : result.buffer)
        stream << std::exchange(separator, " ") << std::setfill('0') << std::setw(sizeof(CharacterType) * 2) << static_cast<unsigned>(character);
    switch (result.code) {
    case ConversionResultCode::Success:
        break;
    case ConversionResultCode::SourceInvalid:
        stream << std::exchange(separator, " ") << "source invalid";
        break;
    case ConversionResultCode::TargetExhausted:
        stream << std::exchange(separator, " ") << "target exhausted";
        break;
    default:
        stream << std::exchange(separator, " ") << "bad result code";
    }

    static std::string singleGlobalResult;
    singleGlobalResult = stream.str();
    return singleGlobalResult.c_str();
}

static const char* serialize(WTF::Unicode::CheckedUTF8 result)
{
    std::ostringstream stream;
    stream << result.characters.size() << " UTF-8, " << result.lengthUTF16 << " UTF-16";
    bool isAllASCII = true;
    for (auto character : result.characters) {
        if (!isASCII(character))
            isAllASCII = false;
    }
    if (isAllASCII != result.isAllASCII)
        stream << ", bad ASCII flag";

    static std::string singleGlobalResult;
    singleGlobalResult = stream.str();
    return singleGlobalResult.c_str();
}

TEST(WTF_UTF8Conversion, UTF8ToUTF16)
{
    using namespace WTF::Unicode;

    std::array<char16_t, 32> buffer;
    std::array<char16_t, 1> buffer1;
    std::array<char16_t, 2> buffer2;

    EXPECT_STREQ("", serialize(convert(char8Array(), buffer)));
    EXPECT_STREQ("0000", serialize(convert(char8Array(0), buffer)));
    EXPECT_STREQ("0061", serialize(convert(char8Array('a'), buffer)));

    EXPECT_STREQ("D7FF", serialize(convert(char8Array(0xED, 0x9F, 0xBF), buffer)));
    EXPECT_STREQ("E000", serialize(convert(char8Array(0xEE, 0x80, 0x80), buffer)));
    EXPECT_STREQ("FFFD", serialize(convert(char8Array(0xEF, 0xBF, 0xBD), buffer)));
    EXPECT_STREQ("FFFE", serialize(convert(char8Array(0xEF, 0xBF, 0xBE), buffer)));
    EXPECT_STREQ("FFFF", serialize(convert(char8Array(0xEF, 0xBF, 0xBF), buffer)));
    EXPECT_STREQ("D800 DC00", serialize(convert(char8Array(0xF0, 0x90, 0x80, 0x80), buffer)));
    EXPECT_STREQ("DBFF DFFF", serialize(convert(char8Array(0xF4, 0x8F, 0xBF, 0xBF), buffer)));

    EXPECT_STREQ("0000 0000", serialize(convert(char8Array(0, 0), buffer)));
    EXPECT_STREQ("0061 0000", serialize(convert(char8Array('a', 0), buffer)));
    EXPECT_STREQ("D7FF 0000", serialize(convert(char8Array(0xED, 0x9F, 0xBF, 0), buffer)));

    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0x80), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xA0, 0x80), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xAF, 0xBF), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xB0, 0x80), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xBF, 0xBF), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0x80, 0), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xA0, 0x80, 0), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xF4, 0x90, 0x80, 0x80), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xA0, 0x80, 0xED, 0xBF, 0xBF), buffer)));

    EXPECT_STREQ("0080 source invalid", serialize(convert(char8Array(0xC2, 0x80, 0x80), buffer)));

    EXPECT_STREQ("", serialize(convert(char8Array(), buffer1)));
    EXPECT_STREQ("0000", serialize(convert(char8Array(0), buffer1)));
    EXPECT_STREQ("0061", serialize(convert(char8Array('a'), buffer1)));

    EXPECT_STREQ("D7FF", serialize(convert(char8Array(0xED, 0x9F, 0xBF), buffer1)));
    EXPECT_STREQ("E000", serialize(convert(char8Array(0xEE, 0x80, 0x80), buffer1)));
    EXPECT_STREQ("FFFD", serialize(convert(char8Array(0xEF, 0xBF, 0xBD), buffer1)));
    EXPECT_STREQ("FFFE", serialize(convert(char8Array(0xEF, 0xBF, 0xBE), buffer1)));
    EXPECT_STREQ("FFFF", serialize(convert(char8Array(0xEF, 0xBF, 0xBF), buffer1)));

    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0x80), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xA0, 0x80), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xAF, 0xBF), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xB0, 0x80), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xBF, 0xBF), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xF4, 0x90, 0x80, 0x80), buffer1)));

    EXPECT_STREQ("0000 target exhausted", serialize(convert(char8Array(0, 0), buffer1)));
    EXPECT_STREQ("0061 target exhausted", serialize(convert(char8Array('a', 0), buffer1)));
    EXPECT_STREQ("D7FF target exhausted", serialize(convert(char8Array(0xED, 0x9F, 0xBF, 0), buffer1)));

    EXPECT_STREQ("target exhausted", serialize(convert(char8Array(0xF0, 0x90, 0x80, 0x80), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convert(char8Array(0xF4, 0x8F, 0xBF, 0xBF), buffer1)));

    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0x80, 0), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xA0, 0x80, 0), buffer1)));

    EXPECT_STREQ("", serialize(convert(char8Array(), buffer2)));
    EXPECT_STREQ("0000", serialize(convert(char8Array(0), buffer2)));
    EXPECT_STREQ("0061", serialize(convert(char8Array('a'), buffer2)));

    EXPECT_STREQ("D7FF", serialize(convert(char8Array(0xED, 0x9F, 0xBF), buffer2)));
    EXPECT_STREQ("E000", serialize(convert(char8Array(0xEE, 0x80, 0x80), buffer2)));
    EXPECT_STREQ("FFFD", serialize(convert(char8Array(0xEF, 0xBF, 0xBD), buffer2)));
    EXPECT_STREQ("FFFE", serialize(convert(char8Array(0xEF, 0xBF, 0xBE), buffer2)));
    EXPECT_STREQ("FFFF", serialize(convert(char8Array(0xEF, 0xBF, 0xBF), buffer2)));
    EXPECT_STREQ("D800 DC00", serialize(convert(char8Array(0xF0, 0x90, 0x80, 0x80), buffer2)));
    EXPECT_STREQ("DBFF DFFF", serialize(convert(char8Array(0xF4, 0x8F, 0xBF, 0xBF), buffer2)));

    EXPECT_STREQ("0000 0000", serialize(convert(char8Array(0, 0), buffer2)));
    EXPECT_STREQ("0061 0000", serialize(convert(char8Array('a', 0), buffer2)));
    EXPECT_STREQ("D7FF 0000", serialize(convert(char8Array(0xED, 0x9F, 0xBF, 0), buffer2)));

    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0x80), buffer2)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xA0, 0x80), buffer2)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xAF, 0xBF), buffer2)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xB0, 0x80), buffer2)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xBF, 0xBF), buffer2)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xF4, 0x90, 0x80, 0x80), buffer2)));

    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0x80, 0), buffer2)));
    EXPECT_STREQ("source invalid", serialize(convert(char8Array(0xED, 0xA0, 0x80, 0), buffer2)));
}

TEST(WTF_UTF8Conversion, UTF16ToUTF8)
{
    using namespace WTF::Unicode;

    std::array<char8_t, 32> buffer;
    std::array<char8_t, 1> buffer1;

    EXPECT_STREQ("", serialize(convert(char16Array(), buffer)));
    EXPECT_STREQ("00", serialize(convert(char16Array(0), buffer)));
    EXPECT_STREQ("61", serialize(convert(char16Array('a'), buffer)));

    EXPECT_STREQ("ED 9F BF", serialize(convert(char16Array(0xD7FF), buffer)));
    EXPECT_STREQ("EE 80 80", serialize(convert(char16Array(0xE000), buffer)));
    EXPECT_STREQ("EF BF BD", serialize(convert(char16Array(0xFFFD), buffer)));
    EXPECT_STREQ("EF BF BE", serialize(convert(char16Array(0xFFFE), buffer)));
    EXPECT_STREQ("EF BF BF", serialize(convert(char16Array(0xFFFF), buffer)));
    EXPECT_STREQ("F0 90 80 80", serialize(convert(char16Array(0xD800, 0xDC00), buffer)));
    EXPECT_STREQ("F4 8F BF BF", serialize(convert(char16Array(0xDBFF, 0xDFFF), buffer)));

    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xD800), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xDBFF), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xDC00), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xDFFF), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xD800, 0xD800), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xDC00, 0xD800), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xDC00, 0xDC00), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xD800, 0), buffer)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xD800, 0xE000), buffer)));

    EXPECT_STREQ("", serialize(convert(char16Array(), buffer1)));
    EXPECT_STREQ("00", serialize(convert(char16Array(0), buffer1)));
    EXPECT_STREQ("61", serialize(convert(char16Array('a'), buffer1)));

    EXPECT_STREQ("target exhausted", serialize(convert(char16Array(0xD7FF), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convert(char16Array(0xE000), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convert(char16Array(0xFFFD), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convert(char16Array(0xFFFE), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convert(char16Array(0xFFFF), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convert(char16Array(0xD800, 0xDC00), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convert(char16Array(0xDBFF, 0xDFFF), buffer1)));

    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xD800), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xDBFF), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xDC00), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xDFFF), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xD800, 0xD800), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xDC00, 0xD800), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xDC00, 0xDC00), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xD800, 0), buffer1)));
    EXPECT_STREQ("source invalid", serialize(convert(char16Array(0xD800, 0xE000), buffer1)));
}

TEST(WTF_UTF8Conversion, Latin1ToUTF8)
{
    using namespace WTF::Unicode;

    std::array<char8_t, 32> buffer;
    std::array<char8_t, 1> buffer1;

    EXPECT_STREQ("", serialize(convert(latin1Array(), buffer)));
    EXPECT_STREQ("00", serialize(convert(latin1Array(0), buffer)));
    EXPECT_STREQ("61", serialize(convert(latin1Array('a'), buffer)));
    EXPECT_STREQ("61 62", serialize(convert(latin1Array('a', 'b'), buffer)));
    EXPECT_STREQ("7F", serialize(convert(latin1Array(0x7F), buffer)));
    EXPECT_STREQ("7F 62", serialize(convert(latin1Array(0x7F, 'b'), buffer)));
    EXPECT_STREQ("C2 80", serialize(convert(latin1Array(0x80), buffer)));
    EXPECT_STREQ("C3 BF", serialize(convert(latin1Array(0xFF), buffer)));
    EXPECT_STREQ("C2 80 C3 BF", serialize(convert(latin1Array(0x80, 0xFF), buffer)));

    EXPECT_STREQ("", serialize(convert(latin1Array(), buffer1)));
    EXPECT_STREQ("00", serialize(convert(latin1Array(0), buffer1)));
    EXPECT_STREQ("61", serialize(convert(latin1Array('a'), buffer1)));
    EXPECT_STREQ("61 target exhausted", serialize(convert(latin1Array('a', 'b'), buffer1)));
    EXPECT_STREQ("7F", serialize(convert(latin1Array(0x7F), buffer1)));
    EXPECT_STREQ("7F target exhausted", serialize(convert(latin1Array(0x7F, 'b'), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convert(latin1Array(0x80), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convert(latin1Array(0xFF), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convert(latin1Array(0x80, 0xFF), buffer1)));
}

TEST(WTF_UTF8Conversion, Latin1ToUTF8LongStrings)
{
    using namespace WTF::Unicode;

    Vector<Latin1Character> source;
    Vector<char8_t> expected;
    for (unsigned i = 0; i < 200; ++i) {
        auto character = Latin1Character(i % 7 ? 'a' + i % 26 : 0x80 + i % 128);
        source.append(character);
        if (isASCII(character))
            expected.append(char8_t(character));
        else {
            expected.append(char8_t(0xC0 | (character >> 6)));
            expected.append(char8_t(0x80 | (character & 0x3F)));
        }
    }

    Vector<char8_t> buffer(FillWith { }, 2 * source.size(), char8_t(0));
    auto result = convert(source.span(), buffer.mutableSpan());
    EXPECT_EQ(ConversionResultCode::Success, result.code);
    EXPECT_TRUE(std::ranges::equal(expected.span(), result.buffer));
    EXPECT_EQ(source.size(), result.sourceLength);

    for (size_t bufferSize : { expected.size(), expected.size() - 1, size_t(100), size_t(101) }) {
        result = convert(source.span(), buffer.mutableSpan().first(bufferSize));
        EXPECT_EQ(bufferSize == expected.size() ? ConversionResultCode::Success : ConversionResultCode::TargetExhausted, result.code);
        EXPECT_GE(bufferSize, result.buffer.size());
        EXPECT_LE(bufferSize - 1, result.buffer.size());
        EXPECT_TRUE(std::ranges::equal(expected.span().first(result.buffer.size()), result.buffer));
    }

    Vector<Latin1Character> asciiSource(FillWith { }, 100, Latin1Character('a'));
    result = convert(asciiSource.span(), buffer.mutableSpan());
    EXPECT_EQ(100u, result.buffer.size());
    EXPECT_EQ(100u, result.sourceLength);
    result = convert(asciiSource.span(), buffer.mutableSpan().first(99));
    EXPECT_EQ(ConversionResultCode::TargetExhausted, result.code);
    EXPECT_EQ(99u, result.buffer.size());
    EXPECT_EQ(99u, result.sourceLength);
}

TEST(WTF_UTF8Conversion, EqualUTF16ToUTF8)
{
    using namespace WTF::Unicode;

    EXPECT_TRUE(equal(char16Array(), char8Array()));
    EXPECT_TRUE(equal(char16Array(0), char8Array(0)));
    EXPECT_TRUE(equal(char16Array('a'), char8Array('a')));
    EXPECT_FALSE(equal(char16Array(0x0080), char8Array(0x80)));
    EXPECT_TRUE(equal(char16Array(0xD7FF), char8Array(0xED, 0x9F, 0xBF)));
    EXPECT_TRUE(equal(char16Array(0xE000), char8Array(0xEE, 0x80, 0x80)));
    EXPECT_TRUE(equal(char16Array(0xFFFD), char8Array(0xEF, 0xBF, 0xBD)));
    EXPECT_TRUE(equal(char16Array(0xFFFE), char8Array(0xEF, 0xBF, 0xBE)));
    EXPECT_TRUE(equal(char16Array(0xFFFF), char8Array(0xEF, 0xBF, 0xBF)));
    EXPECT_TRUE(equal(char16Array(0xD800, 0xDC00), char8Array(0xF0, 0x90, 0x80, 0x80)));
    EXPECT_TRUE(equal(char16Array(0xDBFF, 0xDFFF), char8Array(0xF4, 0x8F, 0xBF, 0xBF)));

    EXPECT_FALSE(equal(char16Array(), char8Array(0)));
    EXPECT_FALSE(equal(char16Array(0), char8Array(1)));
    EXPECT_FALSE(equal(char16Array(0), char8Array(1)));
    EXPECT_FALSE(equal(char16Array(1), char8Array(0)));
    EXPECT_FALSE(equal(char16Array(0xD800), char8Array()));
    EXPECT_FALSE(equal(char16Array(0xD800), char8Array(0xED, 0xA0, 0x80)));
    EXPECT_FALSE(equal(char16Array(0xDC00), char8Array(0xED, 0xB0, 0x80)));
    EXPECT_FALSE(equal(char16Array(0xD800, 0xDC00), char8Array(0xED, 0xA0, 0x80, 0xED, 0xB0, 0x80)));
    EXPECT_FALSE(equal(char16Array(0xDFFF), char8Array(0xED, 0xBF, 0xBF)));
    EXPECT_FALSE(equal(char16Array(0xD800, 0xD800), char8Array(0xED, 0xA0, 0x80, 0xED, 0xA0, 0x80)));
    EXPECT_FALSE(equal(char16Array(0xDC00, 0xD800), char8Array(0xED, 0xB0, 0x80, 0xED, 0xA0, 0x80)));
    EXPECT_FALSE(equal(char16Array(0xDC00, 0xDC00), char8Array(0xED, 0xB0, 0x80, 0xED, 0xB0, 0x80)));
    EXPECT_FALSE(equal(char16Array(0xD800, 0), char8Array(0xED, 0xA0, 0x80, 0x00)));

    // Properly encoded U+FFFD on both sides should compare as equal.
    // This is a real code point, not a decoding error.
    EXPECT_TRUE(equal(char16Array(0xFFFD), char8Array(0xEF, 0xBF, 0xBD)));

    // Invalid sequences should not compare as equal, even if both sides fail.
    // next() returns sentinelCodePoint (U_SENTINEL) for these, which is an
    // internal error signal — not U+FFFD — so no code point was produced.

    // Different invalid sequences (lone UTF-16 surrogate vs lone UTF-8 continuation byte).
    EXPECT_FALSE(equal(char16Array(0xD800), char8Array(0x80)));
    EXPECT_FALSE(equal(char16Array(0xDC00), char8Array(0x80)));
    EXPECT_FALSE(equal(char16Array(0xD800), char8Array(0xFE)));

    // Same invalid UTF-8 byte on both sides (via UTF-16 surrogates that map to same sentinel).
    EXPECT_FALSE(equal(char16Array(0xD800), char8Array(0xED, 0xA0, 0x80)));
    EXPECT_FALSE(equal(char16Array(0xDBFF), char8Array(0xED, 0xAF, 0xBF)));
    EXPECT_FALSE(equal(char16Array(0xDC00), char8Array(0xED, 0xB0, 0x80)));

    // Truncated UTF-8 multi-byte sequences.
    EXPECT_FALSE(equal(char16Array(0xD800), char8Array(0xC2)));
    EXPECT_FALSE(equal(char16Array(0xD800), char8Array(0xE0, 0x80)));
    EXPECT_FALSE(equal(char16Array(0xD800), char8Array(0xF0, 0x90, 0x80)));

    // Overlong UTF-8 encoding (2-byte encoding of a character that fits in 1 byte).
    EXPECT_FALSE(equal(char16Array(0xD800), char8Array(0xC0, 0x80)));
    EXPECT_FALSE(equal(char16Array(0xD800), char8Array(0xC1, 0xBF)));

    // Out-of-range UTF-8 (above U+10FFFF).
    EXPECT_FALSE(equal(char16Array(0xD800), char8Array(0xF4, 0x90, 0x80, 0x80)));

    // Invalid sequence surrounded by valid data: "a" + invalid + "b" vs "a" + invalid + "b".
    EXPECT_FALSE(equal(char16Array('a', 0xD800, 'b'), char8Array('a', 0x80, 'b')));
    EXPECT_FALSE(equal(char16Array('a', 0xD800, 'b'), char8Array('a', 0xFE, 'b')));

    // Valid data followed by invalid should not match valid data alone.
    EXPECT_FALSE(equal(char16Array('a', 0xD800), char8Array('a')));
    EXPECT_FALSE(equal(char16Array('a'), char8Array('a', 0x80)));
}

TEST(WTF_UTF8Conversion, EqualLatin1ToUTF8)
{
    using namespace WTF::Unicode;

    EXPECT_TRUE(equal(latin1Array(), char8Array()));
    EXPECT_TRUE(equal(latin1Array(0), char8Array(0)));
    EXPECT_TRUE(equal(latin1Array(0x61), char8Array(0x61)));
    EXPECT_TRUE(equal(latin1Array(0x61, 0x62), char8Array(0x61, 0x62)));
    EXPECT_TRUE(equal(latin1Array(0x7F), char8Array(0x7F)));
    EXPECT_TRUE(equal(latin1Array(0x7F, 0x62), char8Array(0x7F, 0x62)));
    EXPECT_TRUE(equal(latin1Array(0x80), char8Array(0xC2, 0x80)));
    EXPECT_TRUE(equal(latin1Array(0xFF), char8Array(0xC3, 0xBF)));
    EXPECT_TRUE(equal(latin1Array(0x80, 0xFF), char8Array(0xC2, 0x80, 0xC3, 0xBF)));

    EXPECT_FALSE(equal(latin1Array(), char8Array(0)));
    EXPECT_FALSE(equal(latin1Array(0), char8Array(1)));
    EXPECT_FALSE(equal(latin1Array(0), char8Array(1)));
    EXPECT_FALSE(equal(latin1Array(1), char8Array(0)));

    // Latin1 never produces sentinelCodePoint, but invalid UTF-8 does.
    EXPECT_FALSE(equal(latin1Array(0x80), char8Array(0x80)));
    EXPECT_FALSE(equal(latin1Array(0xFF), char8Array(0xFE)));
    EXPECT_FALSE(equal(latin1Array('a'), char8Array(0x80)));
    EXPECT_FALSE(equal(latin1Array('a', 'b'), char8Array('a', 0x80)));
}

TEST(WTF_UTF8Conversion, UTF8ToUTF16ReplacingInvalidSequences)
{
    using namespace WTF::Unicode;

    std::array<char16_t, 32> buffer;
    std::array<char16_t, 1> buffer1;
    std::array<char16_t, 2> buffer2;

    EXPECT_STREQ("", serialize(convertReplacingInvalidSequences(char8Array(), buffer)));
    EXPECT_STREQ("0000", serialize(convertReplacingInvalidSequences(char8Array(0), buffer)));
    EXPECT_STREQ("0061", serialize(convertReplacingInvalidSequences(char8Array('a'), buffer)));

    EXPECT_STREQ("D7FF", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0x9F, 0xBF), buffer)));
    EXPECT_STREQ("E000", serialize(convertReplacingInvalidSequences(char8Array(0xEE, 0x80, 0x80), buffer)));
    EXPECT_STREQ("FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xEF, 0xBF, 0xBD), buffer)));
    EXPECT_STREQ("FFFE", serialize(convertReplacingInvalidSequences(char8Array(0xEF, 0xBF, 0xBE), buffer)));
    EXPECT_STREQ("FFFF", serialize(convertReplacingInvalidSequences(char8Array(0xEF, 0xBF, 0xBF), buffer)));
    EXPECT_STREQ("D800 DC00", serialize(convertReplacingInvalidSequences(char8Array(0xF0, 0x90, 0x80, 0x80), buffer)));
    EXPECT_STREQ("DBFF DFFF", serialize(convertReplacingInvalidSequences(char8Array(0xF4, 0x8F, 0xBF, 0xBF), buffer)));

    EXPECT_STREQ("0000 0000", serialize(convertReplacingInvalidSequences(char8Array(0, 0), buffer)));
    EXPECT_STREQ("0061 0000", serialize(convertReplacingInvalidSequences(char8Array('a', 0), buffer)));
    EXPECT_STREQ("D7FF 0000", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0x9F, 0xBF, 0), buffer)));

    EXPECT_STREQ("FFFD", serialize(convertReplacingInvalidSequences(char8Array(0x80), buffer)));
    EXPECT_STREQ("FFFD 0000", serialize(convertReplacingInvalidSequences(char8Array(0x80, 0), buffer)));

    EXPECT_STREQ("FFFD FFFD FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xA0, 0x80), buffer)));
    EXPECT_STREQ("FFFD FFFD FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xAF, 0xBF), buffer)));
    EXPECT_STREQ("FFFD FFFD FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xB0, 0x80), buffer)));
    EXPECT_STREQ("FFFD FFFD FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xBF, 0xBF), buffer)));
    EXPECT_STREQ("FFFD FFFD FFFD FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xF4, 0x90, 0x80, 0x80), buffer)));
    EXPECT_STREQ("FFFD FFFD FFFD FFFD FFFD FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xA0, 0x80, 0xED, 0xBF, 0xBF), buffer)));

    EXPECT_STREQ("FFFD FFFD FFFD 0000", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xA0, 0x80, 0), buffer)));

    EXPECT_STREQ("0080 FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xC2, 0x80, 0x80), buffer)));

    EXPECT_STREQ("", serialize(convertReplacingInvalidSequences(char8Array(), buffer1)));
    EXPECT_STREQ("0000", serialize(convertReplacingInvalidSequences(char8Array(0), buffer1)));
    EXPECT_STREQ("0061", serialize(convertReplacingInvalidSequences(char8Array('a'), buffer1)));

    EXPECT_STREQ("D7FF", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0x9F, 0xBF), buffer1)));
    EXPECT_STREQ("E000", serialize(convertReplacingInvalidSequences(char8Array(0xEE, 0x80, 0x80), buffer1)));
    EXPECT_STREQ("FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xEF, 0xBF, 0xBD), buffer1)));
    EXPECT_STREQ("FFFE", serialize(convertReplacingInvalidSequences(char8Array(0xEF, 0xBF, 0xBE), buffer1)));
    EXPECT_STREQ("FFFF", serialize(convertReplacingInvalidSequences(char8Array(0xEF, 0xBF, 0xBF), buffer1)));

    EXPECT_STREQ("FFFD", serialize(convertReplacingInvalidSequences(char8Array(0x80), buffer1)));

    EXPECT_STREQ("FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xA0, 0x80), buffer1)));
    EXPECT_STREQ("FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xAF, 0xBF), buffer1)));
    EXPECT_STREQ("FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xB0, 0x80), buffer1)));
    EXPECT_STREQ("FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xBF, 0xBF), buffer1)));

    EXPECT_STREQ("0000 target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0, 0), buffer1)));
    EXPECT_STREQ("0061 target exhausted", serialize(convertReplacingInvalidSequences(char8Array('a', 0), buffer1)));
    EXPECT_STREQ("D7FF target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0x9F, 0xBF, 0), buffer1)));

    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xF0, 0x90, 0x80, 0x80), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xF4, 0x8F, 0xBF, 0xBF), buffer1)));

    EXPECT_STREQ("FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xF4, 0x90, 0x80, 0x80), buffer1)));

    EXPECT_STREQ("FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0x80, 0), buffer1)));
    EXPECT_STREQ("FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xA0, 0x80, 0), buffer1)));

    EXPECT_STREQ("", serialize(convertReplacingInvalidSequences(char8Array(), buffer2)));
    EXPECT_STREQ("0000", serialize(convertReplacingInvalidSequences(char8Array(0), buffer2)));
    EXPECT_STREQ("0061", serialize(convertReplacingInvalidSequences(char8Array('a'), buffer2)));

    EXPECT_STREQ("D7FF", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0x9F, 0xBF), buffer2)));
    EXPECT_STREQ("E000", serialize(convertReplacingInvalidSequences(char8Array(0xEE, 0x80, 0x80), buffer2)));
    EXPECT_STREQ("FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xEF, 0xBF, 0xBD), buffer2)));
    EXPECT_STREQ("FFFE", serialize(convertReplacingInvalidSequences(char8Array(0xEF, 0xBF, 0xBE), buffer2)));
    EXPECT_STREQ("FFFF", serialize(convertReplacingInvalidSequences(char8Array(0xEF, 0xBF, 0xBF), buffer2)));
    EXPECT_STREQ("D800 DC00", serialize(convertReplacingInvalidSequences(char8Array(0xF0, 0x90, 0x80, 0x80), buffer2)));
    EXPECT_STREQ("DBFF DFFF", serialize(convertReplacingInvalidSequences(char8Array(0xF4, 0x8F, 0xBF, 0xBF), buffer2)));

    EXPECT_STREQ("0000 0000", serialize(convertReplacingInvalidSequences(char8Array(0, 0), buffer2)));
    EXPECT_STREQ("0061 0000", serialize(convertReplacingInvalidSequences(char8Array('a', 0), buffer2)));
    EXPECT_STREQ("D7FF 0000", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0x9F, 0xBF, 0), buffer2)));

    EXPECT_STREQ("FFFD", serialize(convertReplacingInvalidSequences(char8Array(0x80), buffer2)));
    EXPECT_STREQ("FFFD 0000", serialize(convertReplacingInvalidSequences(char8Array(0x80, 0), buffer2)));

    EXPECT_STREQ("FFFD FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xA0, 0x80), buffer2)));
    EXPECT_STREQ("FFFD FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xAF, 0xBF), buffer2)));
    EXPECT_STREQ("FFFD FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xB0, 0x80), buffer2)));
    EXPECT_STREQ("FFFD FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xBF, 0xBF), buffer2)));
    EXPECT_STREQ("FFFD FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xF4, 0x90, 0x80, 0x80), buffer2)));
    EXPECT_STREQ("FFFD FFFD target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xED, 0xA0, 0x80, 0), buffer2)));
}

TEST(WTF_UTF8Conversion, UTF16ToUTF8ReplacingUnpairedSurrogates)
{
    using namespace WTF::Unicode;

    std::array<char8_t, 32> buffer;
    std::array<char8_t, 1> buffer1;

    EXPECT_STREQ("", serialize(convertReplacingUnpairedSurrogates(char16Array(), buffer)));
    EXPECT_STREQ("00", serialize(convertReplacingUnpairedSurrogates(char16Array(0), buffer)));
    EXPECT_STREQ("61", serialize(convertReplacingUnpairedSurrogates(char16Array('a'), buffer)));

    EXPECT_STREQ("ED 9F BF", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD7FF), buffer)));
    EXPECT_STREQ("EE 80 80", serialize(convertReplacingUnpairedSurrogates(char16Array(0xE000), buffer)));
    EXPECT_STREQ("EF BF BD", serialize(convertReplacingUnpairedSurrogates(char16Array(0xFFFD), buffer)));
    EXPECT_STREQ("EF BF BE", serialize(convertReplacingUnpairedSurrogates(char16Array(0xFFFE), buffer)));
    EXPECT_STREQ("EF BF BF", serialize(convertReplacingUnpairedSurrogates(char16Array(0xFFFF), buffer)));
    EXPECT_STREQ("F0 90 80 80", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD800, 0xDC00), buffer)));
    EXPECT_STREQ("F4 8F BF BF", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDBFF, 0xDFFF), buffer)));

    EXPECT_STREQ("EF BF BD", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD800), buffer)));
    EXPECT_STREQ("EF BF BD", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDBFF), buffer)));
    EXPECT_STREQ("EF BF BD", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDC00), buffer)));
    EXPECT_STREQ("EF BF BD", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDFFF), buffer)));
    EXPECT_STREQ("EF BF BD EF BF BD", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD800, 0xD800), buffer)));
    EXPECT_STREQ("EF BF BD EF BF BD", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDC00, 0xD800), buffer)));
    EXPECT_STREQ("EF BF BD EF BF BD", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDC00, 0xDC00), buffer)));
    EXPECT_STREQ("EF BF BD 00", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD800, 0), buffer)));
    EXPECT_STREQ("EF BF BD EE 80 80", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD800, 0xE000), buffer)));

    EXPECT_STREQ("", serialize(convertReplacingUnpairedSurrogates(char16Array(), buffer1)));
    EXPECT_STREQ("00", serialize(convertReplacingUnpairedSurrogates(char16Array(0), buffer1)));
    EXPECT_STREQ("61", serialize(convertReplacingUnpairedSurrogates(char16Array('a'), buffer1)));

    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD7FF), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xE000), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xFFFD), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xFFFE), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xFFFF), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD800, 0xDC00), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDBFF, 0xDFFF), buffer1)));

    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD800), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDBFF), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDC00), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDFFF), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD800, 0xD800), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDC00, 0xD800), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xDC00, 0xDC00), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD800, 0), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD800, 0xE000), buffer1)));
}

TEST(WTF_UTF8Conversion, ReplaceInvalidSequencesShouldNotCorruptValidSupplementaryCharacters)
{
    using namespace WTF::Unicode;

    // UTF-16 to UTF-8: valid U+10000 (surrogate pair) needs 4 UTF-8 bytes.
    // A 3-byte buffer cannot hold it. Should report TargetExhausted, not silently write U+FFFD.
    std::array<char8_t, 3> utf8Buffer3;
    EXPECT_STREQ("target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array(0xD800, 0xDC00), utf8Buffer3)));

    // Same but with an ASCII prefix: 'a' takes 1 byte, leaving 2. U+10000 needs 4. Should stop after 'a'.
    EXPECT_STREQ("61 target exhausted", serialize(convertReplacingUnpairedSurrogates(char16Array('a', 0xD800, 0xDC00), utf8Buffer3)));

    // UTF-8 to UTF-16: valid U+10000 (F0 90 80 80) needs 2 UTF-16 units.
    // A 1-unit buffer cannot hold it. Should report TargetExhausted, not silently write U+FFFD.
    std::array<char16_t, 1> utf16Buffer1;
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char8Array(0xF0, 0x90, 0x80, 0x80), utf16Buffer1)));

    // Same but with an ASCII prefix: 'a' takes 1 unit, leaving 1. U+10000 needs 2. Should stop after 'a'.
    std::array<char16_t, 2> utf16Buffer2;
    EXPECT_STREQ("0061 target exhausted", serialize(convertReplacingInvalidSequences(char8Array('a', 0xF0, 0x90, 0x80, 0x80), utf16Buffer2)));
}

TEST(WTF_UTF8Conversion, CheckUTF8)
{
    using namespace WTF::Unicode;

    EXPECT_STREQ("0 UTF-8, 0 UTF-16", serialize(checkUTF8(char8Array())));
    EXPECT_STREQ("1 UTF-8, 1 UTF-16", serialize(checkUTF8(char8Array(0))));
    EXPECT_STREQ("1 UTF-8, 1 UTF-16", serialize(checkUTF8(char8Array('a'))));

    EXPECT_STREQ("3 UTF-8, 1 UTF-16", serialize(checkUTF8(char8Array(0xED, 0x9F, 0xBF))));
    EXPECT_STREQ("3 UTF-8, 1 UTF-16", serialize(checkUTF8(char8Array(0xEE, 0x80, 0x80))));
    EXPECT_STREQ("3 UTF-8, 1 UTF-16", serialize(checkUTF8(char8Array(0xEF, 0xBF, 0xBD))));
    EXPECT_STREQ("3 UTF-8, 1 UTF-16", serialize(checkUTF8(char8Array(0xEF, 0xBF, 0xBE))));
    EXPECT_STREQ("3 UTF-8, 1 UTF-16", serialize(checkUTF8(char8Array(0xEF, 0xBF, 0xBF))));
    EXPECT_STREQ("4 UTF-8, 2 UTF-16", serialize(checkUTF8(char8Array(0xF0, 0x90, 0x80, 0x80))));
    EXPECT_STREQ("4 UTF-8, 2 UTF-16", serialize(checkUTF8(char8Array(0xF4, 0x8F, 0xBF, 0xBF))));

    EXPECT_STREQ("2 UTF-8, 2 UTF-16", serialize(checkUTF8(char8Array(0, 0))));
    EXPECT_STREQ("2 UTF-8, 2 UTF-16", serialize(checkUTF8(char8Array('a', 0))));
    EXPECT_STREQ("4 UTF-8, 2 UTF-16", serialize(checkUTF8(char8Array(0xED, 0x9F, 0xBF, 0))));

    EXPECT_STREQ("0 UTF-8, 0 UTF-16", serialize(checkUTF8(char8Array(0x80))));
    EXPECT_STREQ("0 UTF-8, 0 UTF-16", serialize(checkUTF8(char8Array(0xED, 0xA0, 0x80))));
    EXPECT_STREQ("0 UTF-8, 0 UTF-16", serialize(checkUTF8(char8Array(0xED, 0xAF, 0xBF))));
    EXPECT_STREQ("0 UTF-8, 0 UTF-16", serialize(checkUTF8(char8Array(0xED, 0xB0, 0x80))));
    EXPECT_STREQ("0 UTF-8, 0 UTF-16", serialize(checkUTF8(char8Array(0xED, 0xBF, 0xBF))));
    EXPECT_STREQ("0 UTF-8, 0 UTF-16", serialize(checkUTF8(char8Array(0x80, 0))));
    EXPECT_STREQ("0 UTF-8, 0 UTF-16", serialize(checkUTF8(char8Array(0xED, 0xA0, 0x80, 0))));
    EXPECT_STREQ("0 UTF-8, 0 UTF-16", serialize(checkUTF8(char8Array(0xF4, 0x90, 0x80, 0x80))));
    EXPECT_STREQ("0 UTF-8, 0 UTF-16", serialize(checkUTF8(char8Array(0xED, 0xA0, 0x80, 0xED, 0xBF, 0xBF))));

    EXPECT_STREQ("1 UTF-8, 1 UTF-16", serialize(checkUTF8(char8Array(0x00, 0x80))));
    EXPECT_STREQ("2 UTF-8, 1 UTF-16", serialize(checkUTF8(char8Array(0xC2, 0x80, 0x80))));
}

static std::string serializeWithSourceLength(const WTF::Unicode::ConversionResult<char8_t>& result)
{
    std::ostringstream stream;
    stream << result.sourceLength << ':' << std::hex << std::uppercase;
    auto separator = "";
    for (auto byte : result.buffer)
        stream << std::exchange(separator, " ") << std::setfill('0') << std::setw(2) << static_cast<unsigned>(byte);
    return stream.str();
}

TEST(WTF_UTF8Conversion, Latin1ToUTF8SourceLength)
{
    using namespace WTF::Unicode;

    auto convertInto = [](std::span<const Latin1Character> source, size_t bufferSize) {
        std::array<char8_t, 16> buffer { };
        return serializeWithSourceLength(convert(source, std::span { buffer }.first(bufferSize)));
    };

    EXPECT_EQ("0:", convertInto(latin1Array(), 4));
    EXPECT_EQ("1:61", convertInto(latin1Array('a', 0xE9), 2));
    EXPECT_EQ("2:61 C3 A9", convertInto(latin1Array('a', 0xE9), 3));
    EXPECT_EQ("2:61 C3 A9", convertInto(latin1Array('a', 0xE9), 4));

    Vector<Latin1Character> longSource(FillWith { }, 100, Latin1Character('a'));
    longSource[50] = 0xE9;
    Vector<char8_t> longBuffer(FillWith { }, 200, char8_t(0));
    auto result = convert(longSource.span(), longBuffer.mutableSpan().first(60));
    EXPECT_EQ(59u, result.sourceLength);
    EXPECT_EQ(60u, result.buffer.size());
    result = convert(longSource.span(), longBuffer.mutableSpan());
    EXPECT_EQ(100u, result.sourceLength);
    EXPECT_EQ(101u, result.buffer.size());
}

TEST(WTF_UTF8Conversion, UTF16ToUTF8ReplacingUnpairedSurrogatesSourceLength)
{
    using namespace WTF::Unicode;

    auto convertInto = [](std::span<const char16_t> source, size_t bufferSize) {
        std::array<char8_t, 16> buffer { };
        return serializeWithSourceLength(convertReplacingUnpairedSurrogates(source, std::span { buffer }.first(bufferSize)));
    };

    EXPECT_EQ("0:", convertInto(char16Array(), 4));
    EXPECT_EQ("1:61", convertInto(char16Array('a', 'b'), 1));
    EXPECT_EQ("0:", convertInto(char16Array(0xE9), 1));
    EXPECT_EQ("1:C3 A9", convertInto(char16Array(0xE9), 2));
    EXPECT_EQ("0:", convertInto(char16Array(0xD800), 2));
    EXPECT_EQ("1:EF BF BD", convertInto(char16Array(0xD800), 3));
    EXPECT_EQ("3:61 EF BF BD 62", convertInto(char16Array('a', 0xDC00, 'b'), 5));
    EXPECT_EQ("2:EF BF BD EF BF BD", convertInto(char16Array(0xDC00, 0xD800), 6));
    EXPECT_EQ("0:", convertInto(char16Array(0xD83D, 0xDE00), 3));
    EXPECT_EQ("2:F0 9F 98 80", convertInto(char16Array(0xD83D, 0xDE00), 4));
    EXPECT_EQ("1:61", convertInto(char16Array('a', 0xD83D, 0xDE00, 'b'), 4));

    Vector<char16_t> longSource(FillWith { }, 100, char16_t('a'));
    longSource[70] = 0xDBFF;
    Vector<char8_t> longBuffer(FillWith { }, 200, char8_t(0));
    auto result = convertReplacingUnpairedSurrogates(longSource.span(), longBuffer.mutableSpan());
    EXPECT_EQ(100u, result.sourceLength);
    EXPECT_EQ(102u, result.buffer.size());
    EXPECT_EQ(0xEF, longBuffer[70]);
    result = convertReplacingUnpairedSurrogates(longSource.span(), longBuffer.mutableSpan().first(72));
    EXPECT_EQ(70u, result.sourceLength);
    EXPECT_EQ(70u, result.buffer.size());
}

template<typename SourceCharacterType, typename BufferCharacterType>
static void expectBufferHoldsConversionOfSourceLength(WTF::Unicode::ConversionResult<BufferCharacterType> (*conversion)(std::span<const SourceCharacterType>, std::span<BufferCharacterType>), const Vector<Vector<SourceCharacterType>>& sources)
{
    using namespace WTF::Unicode;

    for (auto& source : sources) {
        for (size_t bufferSize = 0; bufferSize <= 12; ++bufferSize) {
            std::array<BufferCharacterType, 12> buffer { };
            auto result = conversion(source.span(), std::span { buffer }.first(bufferSize));
            EXPECT_EQ(result.code == ConversionResultCode::Success, result.sourceLength == source.size());
            std::array<BufferCharacterType, 32> prefixBuffer { };
            auto prefix = conversion(source.span().first(result.sourceLength), prefixBuffer);
            EXPECT_EQ(ConversionResultCode::Success, prefix.code);
            EXPECT_TRUE(std::ranges::equal(prefix.buffer, result.buffer));
        }
    }
}

TEST(WTF_UTF8Conversion, BufferHoldsConversionOfSourceLength)
{
    using namespace WTF::Unicode;

    Vector<Vector<Latin1Character>> latin1Sources {
        { }, { 'a' }, { 0 }, { 0x7F }, { 0x80 }, { 0xE9 }, { 0xFF }, { 'a', 0xE9 }, { 0xE9, 'a' }, { 'a', 0xE9, 'b', 0xFF, 'c' },
    };
    expectBufferHoldsConversionOfSourceLength<Latin1Character, char8_t>(convert, latin1Sources);

    Vector<Vector<char16_t>> utf16Sources {
        { }, { 0 }, { 'a' }, { 0xD7FF }, { 0xE000 }, { 0xFFFD }, { 0xFFFE }, { 0xFFFF }, { 0xD800, 0xDC00 }, { 0xDBFF, 0xDFFF },
        { 0xD800 }, { 0xDBFF }, { 0xDC00 }, { 0xDFFF }, { 0xD800, 0xD800 }, { 0xDC00, 0xD800 }, { 0xDC00, 0xDC00 }, { 0xD800, 0 },
        { 0xD800, 0xE000 }, { 'a', 0xD800, 0xDC00 }, { 'a', 0xE9, 0x4E00, 0xD83D, 0xDE00, 0xDC00, 'b' },
    };
    expectBufferHoldsConversionOfSourceLength<char16_t, char8_t>(convert, utf16Sources);
    expectBufferHoldsConversionOfSourceLength<char16_t, char8_t>(convertReplacingUnpairedSurrogates, utf16Sources);

    Vector<Vector<char8_t>> utf8Sources {
        { }, { 'a' }, { 0xC3, 0xA9 }, { 0xE4, 0xB8, 0x80 }, { 0xF0, 0x9F, 0x98, 0x80 }, { 0x80 }, { 0xED, 0xA0, 0x80 },
        { 0xF0, 0x90, 0x80 }, { 0xC2, 0x80, 0x80 }, { 'a', 0xC3, 0xA9, 0xFF, 'b' }, { 'a', 0xF0, 0x9F, 0x98, 0x80, 'b' },
    };
    expectBufferHoldsConversionOfSourceLength<char8_t, char16_t>(convert, utf8Sources);
    expectBufferHoldsConversionOfSourceLength<char8_t, char16_t>(convertReplacingInvalidSequences, utf8Sources);
}

} // namespace
