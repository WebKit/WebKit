/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "Helpers/Test.h"
#include "Lexer.h"

#include "Token.h"

namespace TestWGSLAPI {

class TestLexer : public WGSL::Lexer<Latin1Character> {
    using Base = WGSL::Lexer<Latin1Character>;

public:
    TestLexer(StringImpl* input)
        : Base(WTF::span<Latin1Character>(input))
        , m_tokens(Base::lex())
    {
    }

    WGSL::Token lex()
    {
        return m_tokens[m_index++];
    }

private:
    Vector<WGSL::Token> m_tokens;
    unsigned m_index { 0 };
};


static WGSL::Token checkSingleToken(StringImpl* string, WGSL::TokenType type)
{
    TestLexer lexer(string);
    WGSL::Token result = lexer.lex();
    ASSERT(result.type == type);
    EXPECT_EQ(result.type, type);
    return result;
}

static void checkSingleIntegerLiteral(StringImpl* string, WGSL::TokenType type, int64_t integerValue)
{
    WGSL::Token result = checkSingleToken(string, type);
    EXPECT_EQ(result.integerValue, integerValue);
}
static void checkSingleFloatLiteral(StringImpl* string, WGSL::TokenType type, double floatValue)
{
    WGSL::Token result = checkSingleToken(string, type);
    EXPECT_EQ(result.floatValue, floatValue);
}

static WGSL::Token checkNextTokenIs(TestLexer& lexer, WGSL::TokenType type, unsigned lineNumber)
{
    WGSL::Token result = lexer.lex();
    EXPECT_EQ(result.type, type);
    EXPECT_EQ(result.span.line, lineNumber);
    return result;
}

static void checkNextTokenIsIdentifier(TestLexer& lexer, StringImpl* ident, unsigned lineNumber)
{
    WGSL::Token result = checkNextTokenIs(lexer, WGSL::TokenType::Identifier, lineNumber);
    EXPECT_EQ(result.ident, ident);
}

static void checkNextTokenIsIntegerLiteral(TestLexer& lexer, WGSL::TokenType type, int64_t integerValue, unsigned lineNumber)
{
    WGSL::Token result = checkNextTokenIs(lexer, type, lineNumber);
    EXPECT_EQ(result.integerValue, integerValue);
}

static void checkNextTokenIsFloatLiteral(TestLexer& lexer, WGSL::TokenType type, double floatValue, unsigned lineNumber)
{
    WGSL::Token result = checkNextTokenIs(lexer, type, lineNumber);
    EXPECT_EQ(result.floatValue, floatValue);
}

static void checkNextTokensAreBuiltinAttr(TestLexer& lexer, StringImpl* attr, unsigned lineNumber)
{
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "builtin"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, attr, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
};

TEST(WGSLLexerTests, Comments)
{
    using WGSL::TokenType;

    checkSingleToken(StringImpl::create("// This is line-ending comment.\n"
        "/* This is a block comment\n"
        "   that spans lines.\n"
        "   /* Block comments can nest.\n"
        "    */\n"
        "   But all block comments must terminate.\n"
        "*/"_s).ptr(), TokenType::EndOfFile);
}

TEST(WGSLLexerTests, SingleTokens)
{
    using WGSL::TokenType;

    checkSingleToken(""_si, TokenType::EndOfFile);
    checkSingleIntegerLiteral("1"_si, TokenType::IntegerLiteral, 1);
    checkSingleIntegerLiteral("0"_si, TokenType::IntegerLiteral, 0);
    checkSingleIntegerLiteral("142"_si, TokenType::IntegerLiteral, 142);
    checkSingleFloatLiteral("142f"_si, TokenType::FloatLiteral, 142.0);
    checkSingleFloatLiteral("1.1"_si, TokenType::AbstractFloatLiteral, 1.1);
    checkSingleFloatLiteral("0.4"_si, TokenType::AbstractFloatLiteral, 0.4);
    checkSingleFloatLiteral("0123.456"_si, TokenType::AbstractFloatLiteral, 0123.456);
    checkSingleToken("0123"_si, TokenType::Invalid);
    checkSingleFloatLiteral("0123."_si, TokenType::AbstractFloatLiteral, 123);
    checkSingleFloatLiteral(".456"_si, TokenType::AbstractFloatLiteral, 0.456);
    checkSingleToken("."_si, TokenType::Period);
    checkSingleFloatLiteral("42f"_si, TokenType::FloatLiteral, 42);
    checkSingleFloatLiteral("42e0f"_si, TokenType::FloatLiteral, 42);
    checkSingleFloatLiteral("042e0f"_si, TokenType::FloatLiteral, 42);
    checkSingleToken("042f"_si, TokenType::Invalid);
    checkSingleFloatLiteral("0123.f"_si, TokenType::FloatLiteral, 123);
    checkSingleFloatLiteral(".456f"_si, TokenType::FloatLiteral, 0.456f);
    checkSingleFloatLiteral("42e-3"_si, TokenType::AbstractFloatLiteral, 42e-3);
    checkSingleFloatLiteral("42e-3f"_si, TokenType::FloatLiteral, 42e-3f);
    checkSingleFloatLiteral("34028235000000000000000000000000000000f"_si, TokenType::FloatLiteral, 34028234663852885981170418348451692544.0f);
}

TEST(WGSLLexerTests, KeywordTokens)
{
    using WGSL::TokenType;

    checkSingleToken("alias"_si, TokenType::KeywordAlias);
    checkSingleToken("break"_si, TokenType::KeywordBreak);
    checkSingleToken("case"_si, TokenType::KeywordCase);
    checkSingleToken("const"_si, TokenType::KeywordConst);
    checkSingleToken("const_assert"_si, TokenType::KeywordConstAssert);
    checkSingleToken("continue"_si, TokenType::KeywordContinue);
    checkSingleToken("default"_si, TokenType::KeywordDefault);
    checkSingleToken("diagnostic"_si, TokenType::KeywordDiagnostic);
    checkSingleToken("discard"_si, TokenType::KeywordDiscard);
    checkSingleToken("else"_si, TokenType::KeywordElse);
    checkSingleToken("enable"_si, TokenType::KeywordEnable);
    checkSingleToken("false"_si, TokenType::KeywordFalse);
    checkSingleToken("fn"_si, TokenType::KeywordFn);
    checkSingleToken("for"_si, TokenType::KeywordFor);
    checkSingleToken("if"_si, TokenType::KeywordIf);
    checkSingleToken("let"_si, TokenType::KeywordLet);
    checkSingleToken("loop"_si, TokenType::KeywordLoop);
    checkSingleToken("override"_si, TokenType::KeywordOverride);
    checkSingleToken("requires"_si, TokenType::KeywordRequires);
    checkSingleToken("return"_si, TokenType::KeywordReturn);
    checkSingleToken("struct"_si, TokenType::KeywordStruct);
    checkSingleToken("switch"_si, TokenType::KeywordSwitch);
    checkSingleToken("true"_si, TokenType::KeywordTrue);
    checkSingleToken("var"_si, TokenType::KeywordVar);
    checkSingleToken("while"_si, TokenType::KeywordWhile);
}

TEST(WGSLLexerTests, SpecialTokens)
{
    using WGSL::TokenType;

    checkSingleToken("->"_si, TokenType::Arrow);
    checkSingleToken("@"_si, TokenType::Attribute);
    checkSingleToken("!"_si, TokenType::Bang);
    checkSingleToken("!="_si, TokenType::BangEq);
    checkSingleToken("{"_si, TokenType::BraceLeft);
    checkSingleToken("}"_si, TokenType::BraceRight);
    checkSingleToken("["_si, TokenType::BracketLeft);
    checkSingleToken("]"_si, TokenType::BracketRight);
    checkSingleToken(":"_si, TokenType::Colon);
    checkSingleToken(","_si, TokenType::Comma);
    checkSingleToken("="_si, TokenType::Equal);
    checkSingleToken("=="_si, TokenType::EqEq);
    checkSingleToken(">"_si, TokenType::Gt);
    checkSingleToken(">="_si, TokenType::GtEq);
    checkSingleToken(">>"_si, TokenType::GtGt);
    checkSingleToken(">>="_si, TokenType::GtGtEq);
    checkSingleToken("<"_si, TokenType::Lt);
    checkSingleToken("<="_si, TokenType::LtEq);
    checkSingleToken("<<"_si, TokenType::LtLt);
    checkSingleToken("<<="_si, TokenType::LtLtEq);
    checkSingleToken("-"_si, TokenType::Minus);
    checkSingleToken("--"_si, TokenType::MinusMinus);
    checkSingleToken("-="_si, TokenType::MinusEq);
    checkSingleToken("%"_si, TokenType::Modulo);
    checkSingleToken("%="_si, TokenType::ModuloEq);
    checkSingleToken("."_si, TokenType::Period);
    checkSingleToken("("_si, TokenType::ParenLeft);
    checkSingleToken(")"_si, TokenType::ParenRight);
    checkSingleToken(";"_si, TokenType::Semicolon);
    checkSingleToken("/"_si, TokenType::Slash);
    checkSingleToken("/="_si, TokenType::SlashEq);
    checkSingleToken("*"_si, TokenType::Star);
    checkSingleToken("*="_si, TokenType::StarEq);
    checkSingleToken("+"_si, TokenType::Plus);
    checkSingleToken("++"_si, TokenType::PlusPlus);
    checkSingleToken("+="_si, TokenType::PlusEq);
    checkSingleToken("&"_si, TokenType::And);
    checkSingleToken("&&"_si, TokenType::AndAnd);
    checkSingleToken("&="_si, TokenType::AndEq);
    checkSingleToken("|"_si, TokenType::Or);
    checkSingleToken("||"_si, TokenType::OrOr);
    checkSingleToken("|="_si, TokenType::OrEq);
    checkSingleToken("^"_si, TokenType::Xor);
    checkSingleToken("^="_si, TokenType::XorEq);
    checkSingleToken("~"_si, TokenType::Tilde);
    checkSingleToken("_"_si, TokenType::Underbar);
}

TEST(WGSLLexerTests, ComputeShader)
{
    TestLexer lexer(
        StringImpl::create("@block struct B {\n"
        "    a: i32,\n"
        "}\n"
        "\n"
        "@group(0) @binding(0)\n"
        "var<storage, read_write> x: B;\n"
        "\n"
        "@compute\n"
        "fn main() {\n"
        "    x.a = 42;\n"
        "}"_s).ptr());

    unsigned lineNumber = 1;
    // @block struct B {
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "block"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordStruct, lineNumber);
    checkNextTokenIsIdentifier(lexer, "B"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    // a: i32;
    ++lineNumber;
    checkNextTokenIsIdentifier(lexer, "a"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Colon, lineNumber);
    checkNextTokenIsIdentifier(lexer, "i32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);

    // }
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::BraceRight, lineNumber);

    // @group(0) @binding(0)
    lineNumber += 2;
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "group"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIntegerLiteral(lexer, WGSL::TokenType::IntegerLiteral, 0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "binding"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIntegerLiteral(lexer, WGSL::TokenType::IntegerLiteral, 0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);

    // var<storage, read_write> x: B;
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordVar, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "storage"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsIdentifier(lexer, "read_write"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "x"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Colon, lineNumber);
    checkNextTokenIsIdentifier(lexer, "B"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    // @compute
    lineNumber += 2;
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "compute"_si, lineNumber);

    // fn main() {
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordFn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "main"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    // x.a = 42;
    ++lineNumber;
    checkNextTokenIsIdentifier(lexer, "x"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Period, lineNumber);
    checkNextTokenIsIdentifier(lexer, "a"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Equal, lineNumber);
    checkNextTokenIsIntegerLiteral(lexer, WGSL::TokenType::IntegerLiteral, 42, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    // }
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::BraceRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::EndOfFile, lineNumber);
}

TEST(WGSLLexerTests, GraphicsShader)
{
    TestLexer lexer(
        StringImpl::create("@vertex\n"
        "fn vertexShader(@location(0) x: vec4<f32>) -> @builtin(position) vec4<f32> {\n"
        "    return x;\n"
        "}\n"
        "\n"
        "@fragment\n"
        "fn fragmentShader() -> @location(0) vec4<f32> {\n"
        "    return vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
        "}"_s).ptr());

    unsigned lineNumber = 1;

    // @vertex
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vertex"_si, lineNumber);

    ++lineNumber;
    // fn vertexShader(@location(0) x: vec4<f32>) -> @builtin(position) vec4<f32> {
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordFn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vertexShader"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "location"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIntegerLiteral(lexer, WGSL::TokenType::IntegerLiteral, 0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIsIdentifier(lexer, "x"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Colon, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Arrow, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "builtin"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, "position"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    // return x;
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordReturn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "x"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    // }
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::BraceRight, lineNumber);

    // @fragment
    lineNumber += 2;
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "fragment"_si, lineNumber);

    // fn fragmentShader() -> @location(0) vec4<f32> {
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordFn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "fragmentShader"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Arrow, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "location"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIntegerLiteral(lexer, WGSL::TokenType::IntegerLiteral, 0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    // return vec4<f32>(0.4, 0.4, 0.8, 1.0);
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordReturn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.4, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.4, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.8, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 1.0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    // }
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::BraceRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::EndOfFile, lineNumber);
}

TEST(WGSLLexerTests, TriangleVert)
{
    TestLexer lexer(
        StringImpl::create("@vertex\n"
        "fn main(\n"
        "    @builtin(vertex_index) VertexIndex : u32\n"
        ") -> @builtin(position) vec4<f32> {\n"
        "    var pos = array<vec2<f32>, 3>(\n"
        "        vec2<f32>(0.0, 0.5),\n"
        "        vec2<f32>(-0.5, -0.5),\n"
        "        vec2<f32>(0.5, -0.5)\n"
        "    );\n\n"
        "    return vec4<f32>(pos[VertexIndex] + vec2<f32>(0.5, 0.5), 0.0, 1.0);\n"
        "}\n"_s).ptr());

    unsigned lineNumber = 1;

    // @vertex
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vertex"_si, lineNumber);

    ++lineNumber;
    // fn main(
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordFn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "main"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);

    ++lineNumber;
    //    @builtin(vertex_index) VertexIndex : u32
    checkNextTokensAreBuiltinAttr(lexer, "vertex_index"_si, lineNumber);
    checkNextTokenIsIdentifier(lexer, "VertexIndex"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Colon, lineNumber);
    checkNextTokenIsIdentifier(lexer, "u32"_si, lineNumber);

    ++lineNumber;
    // ) -> @builtin(position) vec4<f32> {
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Arrow, lineNumber);
    checkNextTokensAreBuiltinAttr(lexer, "position"_si, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    ++lineNumber;
    //    var pos = array<vec2<f32>, 3>(
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordVar, lineNumber);
    checkNextTokenIsIdentifier(lexer, "pos"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Equal, lineNumber);
    checkNextTokenIsIdentifier(lexer, "array"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec2"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsIntegerLiteral(lexer, WGSL::TokenType::IntegerLiteral, 3, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);

    ++lineNumber;
    //        vec2<f32>(0.0, 0.5),
    checkNextTokenIsIdentifier(lexer, "vec2"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);

    ++lineNumber;
    //        vec2<f32>(-0.5, -0.5),
    checkNextTokenIsIdentifier(lexer, "vec2"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Minus, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Minus, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);

    ++lineNumber;
    //        vec2<f32>(0.5, -0.5)
    checkNextTokenIsIdentifier(lexer, "vec2"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Minus, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);

    ++lineNumber;
    //    );
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    lineNumber += 2;
    //    return vec4<f32>(pos[VertexIndex] + vec2<f32>(0.5, 0.5), 0.0, 1.0);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordReturn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, "pos"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BracketLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, "VertexIndex"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BracketRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Plus, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec2"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_si, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsFloatLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 1.0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    ++lineNumber;
    // }
    checkNextTokenIs(lexer, WGSL::TokenType::BraceRight, lineNumber);
}

}
