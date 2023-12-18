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
#include "Lexer.h"

namespace TestWGSLAPI {

class TestLexer : public WGSL::Lexer<LChar> {
    using Base = WGSL::Lexer<LChar>;

public:
    TestLexer(const String& input)
        : Base(input)
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


static WGSL::Token checkSingleToken(const String& string, WGSL::TokenType type)
{
    TestLexer lexer(string);
    WGSL::Token result = lexer.lex();
    ASSERT(result.type == type);
    EXPECT_EQ(result.type, type);
    return result;
}

static void checkSingleLiteral(const String& string, WGSL::TokenType type, double literalValue)
{
    WGSL::Token result = checkSingleToken(string, type);
    EXPECT_EQ(result.literalValue, literalValue);
}

static WGSL::Token checkNextTokenIs(TestLexer& lexer, WGSL::TokenType type, unsigned lineNumber)
{
    WGSL::Token result = lexer.lex();
    EXPECT_EQ(result.type, type);
    EXPECT_EQ(result.span.line, lineNumber);
    return result;
}

static void checkNextTokenIsIdentifier(TestLexer& lexer, const String& ident, unsigned lineNumber)
{
    WGSL::Token result = checkNextTokenIs(lexer, WGSL::TokenType::Identifier, lineNumber);
    EXPECT_EQ(result.ident, ident);
}

static void checkNextTokenIsLiteral(TestLexer& lexer, WGSL::TokenType type, double literalValue, unsigned lineNumber)
{
    WGSL::Token result = checkNextTokenIs(lexer, type, lineNumber);
    EXPECT_EQ(result.literalValue, literalValue);
}

static void checkNextTokensAreBuiltinAttr(TestLexer& lexer, const String& attr, unsigned lineNumber)
{
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "builtin"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, attr, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
};

TEST(WGSLLexerTests, Comments)
{
    using WGSL::TokenType;

    checkSingleToken("// This is line-ending comment.\n"
        "/* This is a block comment\n"
        "   that spans lines.\n"
        "   /* Block comments can nest.\n"
        "    */\n"
        "   But all block comments must terminate.\n"
        "*/"_s, TokenType::EndOfFile);
}

TEST(WGSLLexerTests, SingleTokens)
{
    using WGSL::TokenType;

    checkSingleToken(""_s, TokenType::EndOfFile);
    checkSingleLiteral("1"_s, TokenType::IntegerLiteral, 1);
    checkSingleLiteral("0"_s, TokenType::IntegerLiteral, 0);
    checkSingleLiteral("142"_s, TokenType::IntegerLiteral, 142);
    checkSingleLiteral("142f"_s, TokenType::FloatLiteral, 142.0);
    checkSingleLiteral("1.1"_s, TokenType::AbstractFloatLiteral, 1.1);
    checkSingleLiteral("0.4"_s, TokenType::AbstractFloatLiteral, 0.4);
    checkSingleLiteral("0123.456"_s, TokenType::AbstractFloatLiteral, 0123.456);
    checkSingleToken("0123"_s, TokenType::Invalid);
    checkSingleLiteral("0123."_s, TokenType::AbstractFloatLiteral, 123);
    checkSingleLiteral(".456"_s, TokenType::AbstractFloatLiteral, 0.456);
    checkSingleToken("."_s, TokenType::Period);
    checkSingleLiteral("42f"_s, TokenType::FloatLiteral, 42);
    checkSingleLiteral("42e0f"_s, TokenType::FloatLiteral, 42);
    checkSingleLiteral("042e0f"_s, TokenType::FloatLiteral, 42);
    checkSingleToken("042f"_s, TokenType::Invalid);
    checkSingleLiteral("0123.f"_s, TokenType::FloatLiteral, 123);
    checkSingleLiteral(".456f"_s, TokenType::FloatLiteral, 0.456f);
    checkSingleLiteral("42e-3"_s, TokenType::AbstractFloatLiteral, 42e-3);
    checkSingleLiteral("42e-3f"_s, TokenType::FloatLiteral, 42e-3f);
}

TEST(WGSLLexerTests, KeywordTokens)
{
    using WGSL::TokenType;

    checkSingleToken("alias"_s, TokenType::KeywordAlias);
    checkSingleToken("break"_s, TokenType::KeywordBreak);
    checkSingleToken("case"_s, TokenType::KeywordCase);
    checkSingleToken("const"_s, TokenType::KeywordConst);
    checkSingleToken("const_assert"_s, TokenType::KeywordConstAssert);
    checkSingleToken("continue"_s, TokenType::KeywordContinue);
    checkSingleToken("default"_s, TokenType::KeywordDefault);
    checkSingleToken("diagnostic"_s, TokenType::KeywordDiagnostic);
    checkSingleToken("discard"_s, TokenType::KeywordDiscard);
    checkSingleToken("else"_s, TokenType::KeywordElse);
    checkSingleToken("enable"_s, TokenType::KeywordEnable);
    checkSingleToken("false"_s, TokenType::KeywordFalse);
    checkSingleToken("fn"_s, TokenType::KeywordFn);
    checkSingleToken("for"_s, TokenType::KeywordFor);
    checkSingleToken("if"_s, TokenType::KeywordIf);
    checkSingleToken("let"_s, TokenType::KeywordLet);
    checkSingleToken("loop"_s, TokenType::KeywordLoop);
    checkSingleToken("override"_s, TokenType::KeywordOverride);
    checkSingleToken("requires"_s, TokenType::KeywordRequires);
    checkSingleToken("return"_s, TokenType::KeywordReturn);
    checkSingleToken("struct"_s, TokenType::KeywordStruct);
    checkSingleToken("switch"_s, TokenType::KeywordSwitch);
    checkSingleToken("true"_s, TokenType::KeywordTrue);
    checkSingleToken("var"_s, TokenType::KeywordVar);
    checkSingleToken("while"_s, TokenType::KeywordWhile);
}

TEST(WGSLLexerTests, SpecialTokens)
{
    using WGSL::TokenType;

    checkSingleToken("->"_s, TokenType::Arrow);
    checkSingleToken("@"_s, TokenType::Attribute);
    checkSingleToken("!"_s, TokenType::Bang);
    checkSingleToken("!="_s, TokenType::BangEq);
    checkSingleToken("{"_s, TokenType::BraceLeft);
    checkSingleToken("}"_s, TokenType::BraceRight);
    checkSingleToken("["_s, TokenType::BracketLeft);
    checkSingleToken("]"_s, TokenType::BracketRight);
    checkSingleToken(":"_s, TokenType::Colon);
    checkSingleToken(","_s, TokenType::Comma);
    checkSingleToken("="_s, TokenType::Equal);
    checkSingleToken("=="_s, TokenType::EqEq);
    checkSingleToken(">"_s, TokenType::Gt);
    checkSingleToken(">="_s, TokenType::GtEq);
    checkSingleToken(">>"_s, TokenType::GtGt);
    checkSingleToken(">>="_s, TokenType::GtGtEq);
    checkSingleToken("<"_s, TokenType::Lt);
    checkSingleToken("<="_s, TokenType::LtEq);
    checkSingleToken("<<"_s, TokenType::LtLt);
    checkSingleToken("<<="_s, TokenType::LtLtEq);
    checkSingleToken("-"_s, TokenType::Minus);
    checkSingleToken("--"_s, TokenType::MinusMinus);
    checkSingleToken("-="_s, TokenType::MinusEq);
    checkSingleToken("%"_s, TokenType::Modulo);
    checkSingleToken("%="_s, TokenType::ModuloEq);
    checkSingleToken("."_s, TokenType::Period);
    checkSingleToken("("_s, TokenType::ParenLeft);
    checkSingleToken(")"_s, TokenType::ParenRight);
    checkSingleToken(";"_s, TokenType::Semicolon);
    checkSingleToken("/"_s, TokenType::Slash);
    checkSingleToken("/="_s, TokenType::SlashEq);
    checkSingleToken("*"_s, TokenType::Star);
    checkSingleToken("*="_s, TokenType::StarEq);
    checkSingleToken("+"_s, TokenType::Plus);
    checkSingleToken("++"_s, TokenType::PlusPlus);
    checkSingleToken("+="_s, TokenType::PlusEq);
    checkSingleToken("&"_s, TokenType::And);
    checkSingleToken("&&"_s, TokenType::AndAnd);
    checkSingleToken("&="_s, TokenType::AndEq);
    checkSingleToken("|"_s, TokenType::Or);
    checkSingleToken("||"_s, TokenType::OrOr);
    checkSingleToken("|="_s, TokenType::OrEq);
    checkSingleToken("^"_s, TokenType::Xor);
    checkSingleToken("^="_s, TokenType::XorEq);
    checkSingleToken("~"_s, TokenType::Tilde);
    checkSingleToken("_"_s, TokenType::Underbar);
}

TEST(WGSLLexerTests, ComputeShader)
{
    TestLexer lexer(
        "@block struct B {\n"
        "    a: i32,\n"
        "}\n"
        "\n"
        "@group(0) @binding(0)\n"
        "var<storage, read_write> x: B;\n"
        "\n"
        "@compute\n"
        "fn main() {\n"
        "    x.a = 42;\n"
        "}"_s);

    unsigned lineNumber = 1;
    // @block struct B {
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "block"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordStruct, lineNumber);
    checkNextTokenIsIdentifier(lexer, "B"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    // a: i32;
    ++lineNumber;
    checkNextTokenIsIdentifier(lexer, "a"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Colon, lineNumber);
    checkNextTokenIsIdentifier(lexer, "i32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);

    // }
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::BraceRight, lineNumber);

    // @group(0) @binding(0)
    lineNumber += 2;
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "group"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::IntegerLiteral, 0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "binding"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::IntegerLiteral, 0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);

    // var<storage, read_write> x: B;
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordVar, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "storage"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsIdentifier(lexer, "read_write"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "x"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Colon, lineNumber);
    checkNextTokenIsIdentifier(lexer, "B"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    // @compute
    lineNumber += 2;
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "compute"_s, lineNumber);

    // fn main() {
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordFn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "main"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    // x.a = 42;
    ++lineNumber;
    checkNextTokenIsIdentifier(lexer, "x"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Period, lineNumber);
    checkNextTokenIsIdentifier(lexer, "a"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Equal, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::IntegerLiteral, 42, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    // }
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::BraceRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::EndOfFile, lineNumber);
}

TEST(WGSLLexerTests, GraphicsShader)
{
    TestLexer lexer(
        "@vertex\n"
        "fn vertexShader(@location(0) x: vec4<f32>) -> @builtin(position) vec4<f32> {\n"
        "    return x;\n"
        "}\n"
        "\n"
        "@fragment\n"
        "fn fragmentShader() -> @location(0) vec4<f32> {\n"
        "    return vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
        "}"_s);

    unsigned lineNumber = 1;

    // @vertex
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vertex"_s, lineNumber);

    ++lineNumber;
    // fn vertexShader(@location(0) x: vec4<f32>) -> @builtin(position) vec4<f32> {
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordFn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vertexShader"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "location"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::IntegerLiteral, 0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIsIdentifier(lexer, "x"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Colon, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Arrow, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "builtin"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, "position"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    // return x;
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordReturn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "x"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    // }
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::BraceRight, lineNumber);

    // @fragment
    lineNumber += 2;
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "fragment"_s, lineNumber);

    // fn fragmentShader() -> @location(0) vec4<f32> {
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordFn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "fragmentShader"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Arrow, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "location"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::IntegerLiteral, 0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    // return vec4<f32>(0.4, 0.4, 0.8, 1.0);
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordReturn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.4, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.4, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.8, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 1.0, lineNumber);
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
        "@vertex\n"
        "fn main(\n"
        "    @builtin(vertex_index) VertexIndex : u32\n"
        ") -> @builtin(position) vec4<f32> {\n"
        "    var pos = array<vec2<f32>, 3>(\n"
        "        vec2<f32>(0.0, 0.5),\n"
        "        vec2<f32>(-0.5, -0.5),\n"
        "        vec2<f32>(0.5, -0.5)\n"
        "    );\n\n"
        "    return vec4<f32>(pos[VertexIndex] + vec2<f32>(0.5, 0.5), 0.0, 1.0);\n"
        "}\n"_s);

    unsigned lineNumber = 1;

    // @vertex
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vertex"_s, lineNumber);

    ++lineNumber;
    // fn main(
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordFn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "main"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);

    ++lineNumber;
    //    @builtin(vertex_index) VertexIndex : u32
    checkNextTokensAreBuiltinAttr(lexer, "vertex_index"_s, lineNumber);
    checkNextTokenIsIdentifier(lexer, "VertexIndex"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Colon, lineNumber);
    checkNextTokenIsIdentifier(lexer, "u32"_s, lineNumber);

    ++lineNumber;
    // ) -> @builtin(position) vec4<f32> {
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Arrow, lineNumber);
    checkNextTokensAreBuiltinAttr(lexer, "position"_s, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    ++lineNumber;
    //    var pos = array<vec2<f32>, 3>(
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordVar, lineNumber);
    checkNextTokenIsIdentifier(lexer, "pos"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Equal, lineNumber);
    checkNextTokenIsIdentifier(lexer, "array"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec2"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::IntegerLiteral, 3, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);

    ++lineNumber;
    //        vec2<f32>(0.0, 0.5),
    checkNextTokenIsIdentifier(lexer, "vec2"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);

    ++lineNumber;
    //        vec2<f32>(-0.5, -0.5),
    checkNextTokenIsIdentifier(lexer, "vec2"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Minus, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Minus, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);

    ++lineNumber;
    //        vec2<f32>(0.5, -0.5)
    checkNextTokenIsIdentifier(lexer, "vec2"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Minus, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);

    ++lineNumber;
    //    );
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    lineNumber += 2;
    //    return vec4<f32>(pos[VertexIndex] + vec2<f32>(0.5, 0.5), 0.0, 1.0);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordReturn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, "pos"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BracketLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, "VertexIndex"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BracketRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Plus, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec2"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Lt, lineNumber);
    checkNextTokenIsIdentifier(lexer, "f32"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Gt, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 0.0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::AbstractFloatLiteral, 1.0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    ++lineNumber;
    // }
    checkNextTokenIs(lexer, WGSL::TokenType::BraceRight, lineNumber);
}

}
