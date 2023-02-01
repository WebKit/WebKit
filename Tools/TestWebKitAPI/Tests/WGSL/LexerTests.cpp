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

static WGSL::Token checkSingleToken(const String& string, WGSL::TokenType type)
{
    WGSL::Lexer<LChar> lexer(string);
    WGSL::Token result = lexer.lex();
    EXPECT_EQ(result.m_type, type);
    return result;
}

static void checkSingleLiteral(const String& string, WGSL::TokenType type, double literalValue)
{
    WGSL::Token result = checkSingleToken(string, type);
    EXPECT_EQ(result.m_literalValue, literalValue);
}

template<typename T>
static WGSL::Token checkNextTokenIs(WGSL::Lexer<T>& lexer, WGSL::TokenType type, unsigned lineNumber)
{
    WGSL::Token result = lexer.lex();
    EXPECT_EQ(result.m_type, type);
    EXPECT_EQ(result.m_span.m_line, lineNumber);
    return result;
}

template<typename T>
static void checkNextTokenIsIdentifier(WGSL::Lexer<T>& lexer, const String& ident, unsigned lineNumber)
{
    WGSL::Token result = checkNextTokenIs(lexer, WGSL::TokenType::Identifier, lineNumber);
    EXPECT_EQ(result.m_ident, ident);
}

template<typename T>
static void checkNextTokenIsLiteral(WGSL::Lexer<T>& lexer, WGSL::TokenType type, double literalValue, unsigned lineNumber)
{
    WGSL::Token result = checkNextTokenIs(lexer, type, lineNumber);
    EXPECT_EQ(result.m_literalValue, literalValue);
}

template<typename T>
static void checkNextTokensAreBuiltinAttr(WGSL::Lexer<T>& lexer, const String& attr, unsigned lineNumber)
{
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "builtin"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, attr, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
};

TEST(WGSLLexerTests, SingleTokens)
{
    using WGSL::TokenType;

    checkSingleToken(""_s, TokenType::EndOfFile);
    checkSingleLiteral("1"_s, TokenType::IntegerLiteral, 1);
    checkSingleLiteral("0"_s, TokenType::IntegerLiteral, 0);
    checkSingleLiteral("142"_s, TokenType::IntegerLiteral, 142);
    checkSingleLiteral("1.1"_s, TokenType::DecimalFloatLiteral, 1.1);
    checkSingleLiteral("0.4"_s, TokenType::DecimalFloatLiteral, 0.4);
    checkSingleLiteral("0123.456"_s, TokenType::DecimalFloatLiteral, 0123.456);
    checkSingleToken("0123"_s, TokenType::Invalid);
    checkSingleLiteral("0123."_s, TokenType::DecimalFloatLiteral, 123);
    checkSingleLiteral(".456"_s, TokenType::DecimalFloatLiteral, 0.456);
    checkSingleToken("."_s, TokenType::Period);
    checkSingleLiteral("42f"_s, TokenType::DecimalFloatLiteral, 42);
    checkSingleLiteral("42e0f"_s, TokenType::DecimalFloatLiteral, 42);
    checkSingleLiteral("042e0f"_s, TokenType::DecimalFloatLiteral, 42);
    checkSingleToken("042f"_s, TokenType::Invalid);
    checkSingleLiteral("42e-3"_s, TokenType::DecimalFloatLiteral, 42e-3);
    checkSingleLiteral("42e-a"_s, TokenType::IntegerLiteral, 42);
}

TEST(WGSLLexerTests, KeywordTokens)
{
    using WGSL::TokenType;

    checkSingleToken("array"_s, TokenType::KeywordArray);
    checkSingleToken("fn"_s, TokenType::KeywordFn);
    checkSingleToken("function"_s, TokenType::KeywordFunction);
    checkSingleToken("private"_s, TokenType::KeywordPrivate);
    checkSingleToken("read"_s, TokenType::KeywordRead);
    checkSingleToken("read_write"_s, TokenType::KeywordReadWrite);
    checkSingleToken("return"_s, TokenType::KeywordReturn);
    checkSingleToken("storage"_s, TokenType::KeywordStorage);
    checkSingleToken("struct"_s, TokenType::KeywordStruct);
    checkSingleToken("uniform"_s, TokenType::KeywordUniform);
    checkSingleToken("var"_s, TokenType::KeywordVar);
    checkSingleToken("workgroup"_s, TokenType::KeywordWorkgroup);
    checkSingleToken("write"_s, TokenType::KeywordWrite);
    checkSingleToken("i32"_s, TokenType::KeywordI32);
    checkSingleToken("u32"_s, TokenType::KeywordU32);
    checkSingleToken("f32"_s, TokenType::KeywordF32);
    checkSingleToken("bool"_s, TokenType::KeywordBool);
}

TEST(WGSLLexerTests, SpecialTokens)
{
    using WGSL::TokenType;

    checkSingleToken("->"_s, TokenType::Arrow);
    checkSingleToken("@"_s, TokenType::Attribute);
    checkSingleToken("{"_s, TokenType::BraceLeft);
    checkSingleToken("}"_s, TokenType::BraceRight);
    checkSingleToken("["_s, TokenType::BracketLeft);
    checkSingleToken("]"_s, TokenType::BracketRight);
    checkSingleToken(":"_s, TokenType::Colon);
    checkSingleToken(","_s, TokenType::Comma);
    checkSingleToken("="_s, TokenType::Equal);
    checkSingleToken(">"_s, TokenType::GT);
    checkSingleToken("<"_s, TokenType::LT);
    checkSingleToken("-"_s, TokenType::Minus);
    checkSingleToken("--"_s, TokenType::MinusMinus);
    checkSingleToken("."_s, TokenType::Period);
    checkSingleToken("("_s, TokenType::ParenLeft);
    checkSingleToken(")"_s, TokenType::ParenRight);
    checkSingleToken(";"_s, TokenType::Semicolon);
}

TEST(WGSLLexerTests, ComputeShader)
{
    WGSL::Lexer<LChar> lexer(
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

    unsigned lineNumber = 0;
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
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordI32, lineNumber);
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
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordStorage, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordReadWrite, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
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
    WGSL::Lexer<LChar> lexer(
        "@vertex\n"
        "fn vertexShader(@location(0) x: vec4<f32>) -> @builtin(position) vec4<f32> {\n"
        "    return x;\n"
        "}\n"
        "\n"
        "@fragment\n"
        "fn fragmentShader() -> @location(0) vec4<f32> {\n"
        "    return vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
        "}"_s);

    unsigned lineNumber = 0;

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
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordF32, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Arrow, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Attribute, lineNumber);
    checkNextTokenIsIdentifier(lexer, "builtin"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, "position"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordF32, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
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
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordF32, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    // return vec4<f32>(0.4, 0.4, 0.8, 1.0);
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordReturn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordF32, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.4, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.4, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.8, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 1.0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    // }
    ++lineNumber;
    checkNextTokenIs(lexer, WGSL::TokenType::BraceRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::EndOfFile, lineNumber);
}

TEST(WGSLLexerTests, TriangleVert)
{
    WGSL::Lexer<LChar> lexer(
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

    unsigned lineNumber = 0;

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
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordU32, lineNumber);

    ++lineNumber;
    // ) -> @builtin(position) vec4<f32> {
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Arrow, lineNumber);
    checkNextTokensAreBuiltinAttr(lexer, "position"_s, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordF32, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BraceLeft, lineNumber);

    ++lineNumber;
    //    var pos = array<vec2<f32>, 3>(
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordVar, lineNumber);
    checkNextTokenIsIdentifier(lexer, "pos"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Equal, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordArray, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec2"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordF32, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::IntegerLiteral, 3, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);

    ++lineNumber;
    //        vec2<f32>(0.0, 0.5),
    checkNextTokenIsIdentifier(lexer, "vec2"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordF32, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);

    ++lineNumber;
    //        vec2<f32>(-0.5, -0.5),
    checkNextTokenIsIdentifier(lexer, "vec2"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordF32, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Minus, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Minus, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);

    ++lineNumber;
    //        vec2<f32>(0.5, -0.5)
    checkNextTokenIsIdentifier(lexer, "vec2"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordF32, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Minus, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);

    ++lineNumber;
    //    );
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    lineNumber += 2;
    //    return vec4<f32>(pos[VertexIndex] + vec2<f32>(0.5, 0.5), 0.0, 1.0);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordReturn, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec4"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordF32, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, "pos"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BracketLeft, lineNumber);
    checkNextTokenIsIdentifier(lexer, "VertexIndex"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::BracketRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Plus, lineNumber);
    checkNextTokenIsIdentifier(lexer, "vec2"_s, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::LT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::KeywordF32, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::GT, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenLeft, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.5, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 0.0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Comma, lineNumber);
    checkNextTokenIsLiteral(lexer, WGSL::TokenType::DecimalFloatLiteral, 1.0, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::ParenRight, lineNumber);
    checkNextTokenIs(lexer, WGSL::TokenType::Semicolon, lineNumber);

    ++lineNumber;
    // }
    checkNextTokenIs(lexer, WGSL::TokenType::BraceRight, lineNumber);
}

}
