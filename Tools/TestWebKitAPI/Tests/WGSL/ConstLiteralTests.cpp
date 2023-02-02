/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#include "config.h"

#include "AST.h"
#include "ParserPrivate.h"

static Expected<UniqueRef<WGSL::AST::Expression>, WGSL::Error> parseLCharPrimaryExpression(const String& input)
{
    WGSL::Lexer<LChar> lexer(input);
    WGSL::Parser parser(lexer);

    return parser.parsePrimaryExpression();
}

template<class NumberType>
struct ConstLiteralTestCase {
    String input;
    NumberType expectedValue;
};

namespace TestWGSLAPI {

TEST(WGSLConstLiteralTests, BoolLiteral)
{
    using namespace WGSL;

    const ConstLiteralTestCase<bool> testCases[] = {
        { "true"_s, true },
        { "false"_s, false }
    };

    for (const auto& testCase : testCases) {
        auto parseResult = parseLCharPrimaryExpression(testCase.input);
        EXPECT_TRUE(parseResult);

        auto expr = WTFMove(*parseResult);
        EXPECT_TRUE(is<AST::BoolLiteral>(expr));

        const auto& intLiteral = downcast<AST::BoolLiteral>(expr.get());
        EXPECT_EQ(intLiteral.value(), testCase.expectedValue);
        auto inputLength = testCase.input.length();
        EXPECT_EQ(intLiteral.span(), SourceSpan(0, inputLength, inputLength, 0));
    }
}

TEST(WGSLConstLiteralTests, AbstractIntegerLiteralDecimal)
{
    using namespace WGSL;

    const ConstLiteralTestCase<int64_t> testCases[] = {
        { "0"_s, 0LL },
        { "1"_s, 1LL },
        { "12345"_s, 12345LL },
    };

    for (const auto& testCase : testCases) {
        auto parseResult = parseLCharPrimaryExpression(testCase.input);
        EXPECT_TRUE(parseResult);

        auto expr = WTFMove(*parseResult);
        EXPECT_TRUE(is<AST::AbstractIntegerLiteral>(expr));

        const auto& intLiteral = downcast<AST::AbstractIntegerLiteral>(expr.get());
        EXPECT_EQ(intLiteral.value(), testCase.expectedValue);
        auto inputLength = testCase.input.length();
        EXPECT_EQ(intLiteral.span(), SourceSpan(0, inputLength, inputLength, 0));
    }
}

TEST(WGSLConstLiteralTests, AbstractIntegerLiteralHex)
{
    using namespace WGSL;

    const ConstLiteralTestCase<int64_t> testCases[] = {
        { "0x0"_s, 0x0LL },
        { "0x1"_s, 0x1LL },
        { "0x12345"_s, 0x12345LL },
    };

    for (const auto& testCase : testCases) {
        auto parseResult = parseLCharPrimaryExpression(testCase.input);
        EXPECT_TRUE(parseResult);

        auto expr = WTFMove(*parseResult);
        EXPECT_TRUE(is<AST::AbstractIntegerLiteral>(expr));

        const auto& intLiteral = downcast<AST::AbstractIntegerLiteral>(expr.get());
        EXPECT_EQ(intLiteral.value(), testCase.expectedValue);
        auto inputLength = testCase.input.length();
        EXPECT_EQ(intLiteral.span(), WGSL::SourceSpan(0, inputLength, inputLength, 0));
    }
}

TEST(WGSLConstLiteralTests, AbstractFloatLiteralDec)
{
    using namespace WGSL;

    const ConstLiteralTestCase<double> testCases[] = {
        { "0.0"_s, 0.0 },
        { "0.1"_s, 0.1 },
        { "2612.0324"_s, 2612.0324 },
        { "0.00000001"_s, 0.00000001 },
        { "1234.5678e-3"_s, 1234.5678e-3 },
        { "01."_s, 01. },
        { ".01"_s, .01 },
        { "1e-3"_s, 1e-3 }
    };

    for (const auto& testCase : testCases) {
        auto parseResult = parseLCharPrimaryExpression(testCase.input);
        EXPECT_TRUE(parseResult);

        auto expr = WTFMove(*parseResult);
        EXPECT_TRUE(is<AST::AbstractFloatLiteral>(expr));

        const auto& floatLiteral = downcast<AST::AbstractFloatLiteral>(expr.get());
        EXPECT_EQ(floatLiteral.value(), testCase.expectedValue);
        auto inputLength = testCase.input.length();
        EXPECT_EQ(floatLiteral.span(), SourceSpan(0, inputLength, inputLength, 0));
    }
}

} // namespace TestWGSLAPI
