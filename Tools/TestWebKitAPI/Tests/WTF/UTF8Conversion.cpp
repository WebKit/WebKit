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

#include "Test.h"

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
    return std::array<LChar, sizeof...(values)> { static_cast<LChar>(values)... };
}

template<typename CharacterType> const char* serialize(const WTF::Unicode::ConversionResult<CharacterType>& result)
{
    using namespace WTF::Unicode;

    std::ostringstream stream;
    auto separator = "";
    stream << std::hex << std::uppercase;
    bool isAllASCII = true;
    for (auto character : result.buffer) {
        stream << std::exchange(separator, " ") << std::setfill('0') << std::setw(sizeof(CharacterType) * 2) << static_cast<unsigned>(character);
        if (!isASCII(character))
            isAllASCII = false;
    }
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
    if (isAllASCII != result.isAllASCII)
        stream << std::exchange(separator, " ") << "bad ASCII flag";

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

static const char* serialize(WTF::Unicode::UTF16LengthWithHash result)
{
    if (!result.lengthUTF16 && !result.hash)
        return "source invalid";

    std::ostringstream stream;
    stream << result.lengthUTF16 << " UTF-16, " << std::hex << std::uppercase << std::setfill('0') << std::setw(6) << result.hash;

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

    EXPECT_STREQ("FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xF0, 0x90, 0x80, 0x80), buffer1)));
    EXPECT_STREQ("FFFD", serialize(convertReplacingInvalidSequences(char8Array(0xF4, 0x8F, 0xBF, 0xBF), buffer1)));

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

TEST(WTF_UTF8Conversion, UTF16ToUTF8ReplacingInvalidSequences)
{
    using namespace WTF::Unicode;

    std::array<char8_t, 32> buffer;
    std::array<char8_t, 1> buffer1;

    EXPECT_STREQ("", serialize(convertReplacingInvalidSequences(char16Array(), buffer)));
    EXPECT_STREQ("00", serialize(convertReplacingInvalidSequences(char16Array(0), buffer)));
    EXPECT_STREQ("61", serialize(convertReplacingInvalidSequences(char16Array('a'), buffer)));

    EXPECT_STREQ("ED 9F BF", serialize(convertReplacingInvalidSequences(char16Array(0xD7FF), buffer)));
    EXPECT_STREQ("EE 80 80", serialize(convertReplacingInvalidSequences(char16Array(0xE000), buffer)));
    EXPECT_STREQ("EF BF BD", serialize(convertReplacingInvalidSequences(char16Array(0xFFFD), buffer)));
    EXPECT_STREQ("EF BF BE", serialize(convertReplacingInvalidSequences(char16Array(0xFFFE), buffer)));
    EXPECT_STREQ("EF BF BF", serialize(convertReplacingInvalidSequences(char16Array(0xFFFF), buffer)));
    EXPECT_STREQ("F0 90 80 80", serialize(convertReplacingInvalidSequences(char16Array(0xD800, 0xDC00), buffer)));
    EXPECT_STREQ("F4 8F BF BF", serialize(convertReplacingInvalidSequences(char16Array(0xDBFF, 0xDFFF), buffer)));

    EXPECT_STREQ("EF BF BD", serialize(convertReplacingInvalidSequences(char16Array(0xD800), buffer)));
    EXPECT_STREQ("EF BF BD", serialize(convertReplacingInvalidSequences(char16Array(0xDBFF), buffer)));
    EXPECT_STREQ("EF BF BD", serialize(convertReplacingInvalidSequences(char16Array(0xDC00), buffer)));
    EXPECT_STREQ("EF BF BD", serialize(convertReplacingInvalidSequences(char16Array(0xDFFF), buffer)));
    EXPECT_STREQ("EF BF BD EF BF BD", serialize(convertReplacingInvalidSequences(char16Array(0xD800, 0xD800), buffer)));
    EXPECT_STREQ("EF BF BD EF BF BD", serialize(convertReplacingInvalidSequences(char16Array(0xDC00, 0xD800), buffer)));
    EXPECT_STREQ("EF BF BD EF BF BD", serialize(convertReplacingInvalidSequences(char16Array(0xDC00, 0xDC00), buffer)));
    EXPECT_STREQ("EF BF BD 00", serialize(convertReplacingInvalidSequences(char16Array(0xD800, 0), buffer)));
    EXPECT_STREQ("EF BF BD EE 80 80", serialize(convertReplacingInvalidSequences(char16Array(0xD800, 0xE000), buffer)));

    EXPECT_STREQ("", serialize(convertReplacingInvalidSequences(char16Array(), buffer1)));
    EXPECT_STREQ("00", serialize(convertReplacingInvalidSequences(char16Array(0), buffer1)));
    EXPECT_STREQ("61", serialize(convertReplacingInvalidSequences(char16Array('a'), buffer1)));

    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xD7FF), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xE000), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xFFFD), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xFFFE), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xFFFF), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xD800, 0xDC00), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xDBFF, 0xDFFF), buffer1)));

    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xD800), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xDBFF), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xDC00), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xDFFF), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xD800, 0xD800), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xDC00, 0xD800), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xDC00, 0xDC00), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xD800, 0), buffer1)));
    EXPECT_STREQ("target exhausted", serialize(convertReplacingInvalidSequences(char16Array(0xD800, 0xE000), buffer1)));
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

TEST(WTF_UTF8Conversion, ComputeUTF16LengthWithHash)
{
    using namespace WTF::Unicode;

    EXPECT_STREQ("0 UTF-16, EC889E", serialize(computeUTF16LengthWithHash(char8Array())));
    EXPECT_STREQ("1 UTF-16, 3ABF44", serialize(computeUTF16LengthWithHash(char8Array(0))));
    EXPECT_STREQ("1 UTF-16, 95343B", serialize(computeUTF16LengthWithHash(char8Array('a'))));

    EXPECT_STREQ("1 UTF-16, C9438B", serialize(computeUTF16LengthWithHash(char8Array(0xED, 0x9F, 0xBF))));
    EXPECT_STREQ("1 UTF-16, 5AA931", serialize(computeUTF16LengthWithHash(char8Array(0xEE, 0x80, 0x80))));
    EXPECT_STREQ("1 UTF-16, 4AB82F", serialize(computeUTF16LengthWithHash(char8Array(0xEF, 0xBF, 0xBD))));
    EXPECT_STREQ("1 UTF-16, A9541A", serialize(computeUTF16LengthWithHash(char8Array(0xEF, 0xBF, 0xBE))));
    EXPECT_STREQ("1 UTF-16, 39215E", serialize(computeUTF16LengthWithHash(char8Array(0xEF, 0xBF, 0xBF))));
    EXPECT_STREQ("2 UTF-16, 2F4E65", serialize(computeUTF16LengthWithHash(char8Array(0xF0, 0x90, 0x80, 0x80))));
    EXPECT_STREQ("2 UTF-16, F121B5", serialize(computeUTF16LengthWithHash(char8Array(0xF4, 0x8F, 0xBF, 0xBF))));

    EXPECT_STREQ("2 UTF-16, 6F6D50", serialize(computeUTF16LengthWithHash(char8Array(0, 0))));
    EXPECT_STREQ("2 UTF-16, 36A996", serialize(computeUTF16LengthWithHash(char8Array('a', 0))));
    EXPECT_STREQ("2 UTF-16, 2E15E1", serialize(computeUTF16LengthWithHash(char8Array(0xED, 0x9F, 0xBF, 0))));

    EXPECT_STREQ("source invalid", serialize(computeUTF16LengthWithHash(char8Array(0x80))));
    EXPECT_STREQ("source invalid", serialize(computeUTF16LengthWithHash(char8Array(0xED, 0xA0, 0x80))));
    EXPECT_STREQ("source invalid", serialize(computeUTF16LengthWithHash(char8Array(0xED, 0xAF, 0xBF))));
    EXPECT_STREQ("source invalid", serialize(computeUTF16LengthWithHash(char8Array(0xED, 0xB0, 0x80))));
    EXPECT_STREQ("source invalid", serialize(computeUTF16LengthWithHash(char8Array(0xED, 0xBF, 0xBF))));
    EXPECT_STREQ("source invalid", serialize(computeUTF16LengthWithHash(char8Array(0x80, 0))));
    EXPECT_STREQ("source invalid", serialize(computeUTF16LengthWithHash(char8Array(0xED, 0xA0, 0x80, 0))));
    EXPECT_STREQ("source invalid", serialize(computeUTF16LengthWithHash(char8Array(0xF4, 0x90, 0x80, 0x80))));
    EXPECT_STREQ("source invalid", serialize(computeUTF16LengthWithHash(char8Array(0xED, 0xA0, 0x80, 0xED, 0xBF, 0xBF))));

    EXPECT_STREQ("source invalid", serialize(computeUTF16LengthWithHash(char8Array(0x00, 0x80))));
    EXPECT_STREQ("source invalid", serialize(computeUTF16LengthWithHash(char8Array(0xC2, 0x80, 0x80))));
}

} // namespace
