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

#include "config.h"
#include <wtf/FastFloat.h>

#include <bit>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

static void expectSameAsParseJSONDouble(const String& text)
{
    auto characters = text.span8();
    size_t cursor = 0;
    uint64_t mantissa = 0;
    auto integerStart = cursor;
    while (cursor < characters.size() && isASCIIDigit(characters[cursor]))
        mantissa = mantissa * 10 + (characters[cursor++] - '0');
    auto integerDigits = characters.subspan(integerStart, cursor - integerStart);
    std::span<const Latin1Character> fractionDigits;
    if (cursor < characters.size() && characters[cursor] == '.') {
        auto fractionStart = ++cursor;
        while (cursor < characters.size() && isASCIIDigit(characters[cursor]))
            mantissa = mantissa * 10 + (characters[cursor++] - '0');
        fractionDigits = characters.subspan(fractionStart, cursor - fractionStart);
    }
    int64_t exponent = -static_cast<int64_t>(fractionDigits.size());
    if (cursor < characters.size() && characters[cursor] == 'e')
        exponent += parseInteger<int64_t>(characters.subspan(cursor + 1)).value();
    ASSERT_LE(integerDigits.size() + fractionDigits.size(), 19u);

    size_t parsedLength = 0;
    auto expected = WTF::parseJSONDouble(characters, parsedLength);
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(parsedLength, characters.size());

    double actual = WTF::parseJSONDoubleFromDigits(mantissa, exponent);
    EXPECT_EQ(std::bit_cast<uint64_t>(actual), std::bit_cast<uint64_t>(*expected)) << text.ascii().data();
}

TEST(WTF_FastFloat, ParseJSONDoubleFromDigits)
{
    for (auto text : {
        "0"_s, "0.0"_s, "0e999999"_s, "1"_s, "0.1"_s, "0.2"_s, "0.3"_s, "1e23"_s, "1e22"_s, "1e-22"_s, "1e-23"_s,
        "9007199254740992"_s, "9007199254740993"_s, "9007199254740993.0"_s, "9007199254740992.5"_s, "18014398509481985"_s,
        "1.7976931348623157e308"_s, "1.7976931348623159e308"_s, "2.2250738585072011e-308"_s, "2.2250738585072012e-308"_s,
        "4.9e-324"_s, "5e-324"_s, "2.4703282292062327e-324"_s, "2.4703282292062328e-324"_s, "1e-400"_s, "1e400"_s,
        "0.9999999999999999"_s, "3.0000000000000004"_s, "1.0000000000000002"_s, "7.2057594037927933e16"_s,
        "9999999999999999999"_s, "0.0000000123456789"_s, "8.98846567431158e307"_s, "123456789012345678e-330"_s,
    })
        expectSameAsParseJSONDouble(text);

    // 19 digits of the exact midpoint between two adjacent doubles, where rounding is hardest to decide.
    for (auto text : {
        "7.263739997081359414e-310"_s, "8.777431717144332782e-54"_s, "2.186271262837971710e154"_s, "5.484018462189851280e286"_s,
        "9.066255333848780090e169"_s, "7.935783026169824265e-229"_s, "1.183038337750219315e-116"_s, "1.804649593541350100e-30"_s,
        "2.921602186522293193e65"_s, "8.993433993890539265e57"_s, "1.384479774396721571e8"_s, "6.531058957729360491e293"_s,
        "2.075250999038129808e-290"_s, "1.203616367563016997e-308"_s, "9.426640265461310196e290"_s, "3.405786469603087752e143"_s,
        "2.892564228211527036e-132"_s, "1.514071975925667375e211"_s, "4.197141258787924190e-27"_s, "6.102542590556188653e293"_s,
        "1.654000795844023490e-142"_s, "3.825875475747334921e270"_s, "1.945596032543111161e236"_s, "1.386073252777012579e-287"_s,
        "7.410984687618698162e-324"_s, "1.797693134862315608e308"_s, "2.225073858507201630e-308"_s, "1.000000000000000111e0"_s,
    })
        expectSameAsParseJSONDouble(text);

    uint64_t state = 0x123456789abcdefULL;
    auto next = [&] {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state >> 11;
    };
    for (unsigned i = 0; i < 20000; ++i) {
        unsigned digitCount = 1 + next() % 19;
        StringBuilder builder;
        for (unsigned j = 0; j < digitCount; ++j)
            builder.append(static_cast<Latin1Character>('0' + (j ? next() % 10 : 1 + next() % 9)));
        String digits = builder.toString();
        unsigned dot = next() % (digitCount + 1);
        String number = dot && dot < digitCount ? makeString(digits.left(dot), '.', digits.substring(dot)) : digits;
        int exponent = static_cast<int>(next() % 700) - 350;
        expectSameAsParseJSONDouble(makeString(number, 'e', exponent));
        expectSameAsParseJSONDouble(number);
    }
}

} // namespace TestWebKitAPI
