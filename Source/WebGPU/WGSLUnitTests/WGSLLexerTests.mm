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

#import "config.h"
#import "Lexer.h"

#import <XCTest/XCTest.h>

static WGSL::Token checkSingleToken(const String& string, WGSL::TokenType type)
{
    WGSL::Lexer<LChar> lexer(string);
    WGSL::Token result = lexer.lex();
    XCTAssertEqual(result.m_type, type);
    return result;
};

@interface WGSLLexerTests : XCTestCase

@end

@implementation WGSLLexerTests

- (void)testLexerOnSingleTokens {
    auto checkSingleToken = [] (const String& string, WGSL::TokenType type) {
        WGSL::Lexer<LChar> lexer(string);
        WGSL::Token result = lexer.lex();
        XCTAssertEqual(result.m_type, type);
        return result;
    };
    auto checkSingleLiteral = [&] (const String& string, WGSL::TokenType type, double literalValue) {
        WGSL::Token result = checkSingleToken(string, type);
        XCTAssertEqual(result.m_literalValue, literalValue);
    };

    checkSingleToken(""_s, WGSL::TokenType::EndOfFile);
    checkSingleLiteral("1"_s, WGSL::TokenType::IntegerLiteral, 1);
    checkSingleLiteral("0"_s, WGSL::TokenType::IntegerLiteral, 0);
    checkSingleLiteral("142"_s, WGSL::TokenType::IntegerLiteral, 142);
    checkSingleLiteral("1.1"_s, WGSL::TokenType::DecimalFloatLiteral, 1.1);
    checkSingleLiteral("0.4"_s, WGSL::TokenType::DecimalFloatLiteral, 0.4);
    checkSingleLiteral("0123.456"_s, WGSL::TokenType::DecimalFloatLiteral, 0123.456);
    checkSingleToken("0123"_s, WGSL::TokenType::Invalid);
    checkSingleLiteral("0123."_s, WGSL::TokenType::DecimalFloatLiteral, 123);
    checkSingleLiteral(".456"_s, WGSL::TokenType::DecimalFloatLiteral, 0.456);
    checkSingleToken("."_s, WGSL::TokenType::Period);
    checkSingleLiteral("42f"_s, WGSL::TokenType::DecimalFloatLiteral, 42);
    checkSingleLiteral("42e0f"_s, WGSL::TokenType::DecimalFloatLiteral, 42);
    checkSingleLiteral("042e0f"_s, WGSL::TokenType::DecimalFloatLiteral, 42);
    checkSingleToken("042f"_s, WGSL::TokenType::Invalid);
    checkSingleLiteral("42e-3"_s, WGSL::TokenType::DecimalFloatLiteral, 42e-3);
    checkSingleLiteral("42e-a"_s, WGSL::TokenType::IntegerLiteral, 42);
}

- (void) testLexerOfKeywordTokens {
    using WGSL::TokenType;

    checkSingleToken("array"_s,      TokenType::KeywordArray);
    checkSingleToken("fn"_s,         TokenType::KeywordFn);
    checkSingleToken("function"_s,   TokenType::KeywordFunction);
    checkSingleToken("private"_s,    TokenType::KeywordPrivate);
    checkSingleToken("read"_s,       TokenType::KeywordRead);
    checkSingleToken("read_write"_s, TokenType::KeywordReadWrite);
    checkSingleToken("return"_s,     TokenType::KeywordReturn);
    checkSingleToken("storage"_s,    TokenType::KeywordStorage);
    checkSingleToken("struct"_s,     TokenType::KeywordStruct);
    checkSingleToken("uniform"_s,    TokenType::KeywordUniform);
    checkSingleToken("var"_s,        TokenType::KeywordVar);
    checkSingleToken("workgroup"_s,  TokenType::KeywordWorkgroup);
    checkSingleToken("write"_s,      TokenType::KeywordWrite);
    checkSingleToken("i32"_s,        TokenType::KeywordI32);
    checkSingleToken("u32"_s,        TokenType::KeywordU32);
    checkSingleToken("f32"_s,        TokenType::KeywordF32);
    checkSingleToken("bool"_s,       TokenType::KeywordBool);
}

- (void) testLexerOfSpecialTokens {
    checkSingleToken("->"_s, WGSL::TokenType::Arrow);
    checkSingleToken("@"_s,  WGSL::TokenType::Attribute);
    checkSingleToken("{"_s,  WGSL::TokenType::BraceLeft);
    checkSingleToken("}"_s,  WGSL::TokenType::BraceRight);
    checkSingleToken("["_s,  WGSL::TokenType::BracketLeft);
    checkSingleToken("]"_s,  WGSL::TokenType::BracketRight);
    checkSingleToken(":"_s,  WGSL::TokenType::Colon);
    checkSingleToken(","_s,  WGSL::TokenType::Comma);
    checkSingleToken("="_s,  WGSL::TokenType::Equal);
    checkSingleToken(">"_s,  WGSL::TokenType::GT);
    checkSingleToken("<"_s,  WGSL::TokenType::LT);
    checkSingleToken("-"_s,  WGSL::TokenType::Minus);
    checkSingleToken("--"_s, WGSL::TokenType::MinusMinus);
    checkSingleToken("."_s,  WGSL::TokenType::Period);
    checkSingleToken("("_s,  WGSL::TokenType::ParenLeft);
    checkSingleToken(")"_s,  WGSL::TokenType::ParenRight);
    checkSingleToken(";"_s,  WGSL::TokenType::Semicolon);
}

- (void) testLexerOnComputeShader {
    WGSL::Lexer<LChar> lexer(
        "@block struct B {\n"
        "    a: i32;\n"
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
    auto checkNextToken = [&] (WGSL::TokenType type) {
        WGSL::Token result = lexer.lex();
        XCTAssertEqual(result.m_type, type);
        XCTAssertEqual(result.m_span.m_line, lineNumber);
        return result;
    };
    auto checkNextTokenIsIdentifier = [&] (const String& ident) {
        WGSL::Token result = checkNextToken(WGSL::TokenType::Identifier);
        XCTAssertEqual(result.m_ident, ident);
    };
    auto checkNextTokenIsLiteral = [&] (WGSL::TokenType type, double literalValue) {
        WGSL::Token result = checkNextToken(type);
        XCTAssertEqual(result.m_literalValue, literalValue);
    };

    // @block struct B {
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("block"_s);
    checkNextToken(WGSL::TokenType::KeywordStruct);
    checkNextTokenIsIdentifier("B"_s);
    checkNextToken(WGSL::TokenType::BraceLeft);

    // a: i32;
    ++lineNumber;
    checkNextTokenIsIdentifier("a"_s);
    checkNextToken(WGSL::TokenType::Colon);
    checkNextToken(WGSL::TokenType::KeywordI32);
    checkNextToken(WGSL::TokenType::Semicolon);

    // }
    ++lineNumber;
    checkNextToken(WGSL::TokenType::BraceRight);

    // @group(0) @binding(0)
    lineNumber += 2;
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("group"_s);
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsLiteral(WGSL::TokenType::IntegerLiteral, 0);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("binding"_s);
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsLiteral(WGSL::TokenType::IntegerLiteral, 0);
    checkNextToken(WGSL::TokenType::ParenRight);

    // var<storage, read_write> x: B;
    ++lineNumber;
    checkNextToken(WGSL::TokenType::KeywordVar);
    checkNextToken(WGSL::TokenType::LT);
    checkNextToken(WGSL::TokenType::KeywordStorage);
    checkNextToken(WGSL::TokenType::Comma);
    checkNextToken(WGSL::TokenType::KeywordReadWrite);
    checkNextToken(WGSL::TokenType::GT);
    checkNextTokenIsIdentifier("x"_s);
    checkNextToken(WGSL::TokenType::Colon);
    checkNextTokenIsIdentifier("B"_s);
    checkNextToken(WGSL::TokenType::Semicolon);

    // @compute
    lineNumber += 2;
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("compute"_s);

    // fn main() {
    ++lineNumber;
    checkNextToken(WGSL::TokenType::KeywordFn);
    checkNextTokenIsIdentifier("main"_s);
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextToken(WGSL::TokenType::BraceLeft);

    // x.a = 42;
    ++lineNumber;
    checkNextTokenIsIdentifier("x"_s);
    checkNextToken(WGSL::TokenType::Period);
    checkNextTokenIsIdentifier("a"_s);
    checkNextToken(WGSL::TokenType::Equal);
    checkNextTokenIsLiteral(WGSL::TokenType::IntegerLiteral, 42);
    checkNextToken(WGSL::TokenType::Semicolon);

    // }
    ++lineNumber;
    checkNextToken(WGSL::TokenType::BraceRight);
    checkNextToken(WGSL::TokenType::EndOfFile);
}

- (void) testLexerOnGraphicsShader {
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
    auto checkNextToken = [&] (WGSL::TokenType type) {
        WGSL::Token result = lexer.lex();
        XCTAssertEqual(result.m_type, type);
        XCTAssertEqual(result.m_span.m_line, lineNumber);
        return result;
    };
    auto checkNextTokenIsIdentifier = [&] (const String& ident) {
        WGSL::Token result = checkNextToken(WGSL::TokenType::Identifier);
        XCTAssertEqual(result.m_ident, ident);
    };
    auto checkNextTokenIsLiteral = [&] (WGSL::TokenType type, double literalValue) {
        WGSL::Token result = checkNextToken(type);
        XCTAssertEqual(result.m_literalValue, literalValue);
    };

    // @vertex
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("vertex"_s);

    ++lineNumber;
    // fn vertexShader(@location(0) x: vec4<f32>) -> @builtin(position) vec4<f32> {
    checkNextToken(WGSL::TokenType::KeywordFn);
    checkNextTokenIsIdentifier("vertexShader"_s);
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("location"_s);
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsLiteral(WGSL::TokenType::IntegerLiteral, 0);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextTokenIsIdentifier("x"_s);
    checkNextToken(WGSL::TokenType::Colon);
    checkNextTokenIsIdentifier("vec4"_s);
    checkNextToken(WGSL::TokenType::LT);
    checkNextToken(WGSL::TokenType::KeywordF32);
    checkNextToken(WGSL::TokenType::GT);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextToken(WGSL::TokenType::Arrow);
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("builtin"_s);
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsIdentifier("position"_s);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextTokenIsIdentifier("vec4"_s);
    checkNextToken(WGSL::TokenType::LT);
    checkNextToken(WGSL::TokenType::KeywordF32);
    checkNextToken(WGSL::TokenType::GT);
    checkNextToken(WGSL::TokenType::BraceLeft);

    // return x;
    ++lineNumber;
    checkNextToken(WGSL::TokenType::KeywordReturn);
    checkNextTokenIsIdentifier("x"_s);
    checkNextToken(WGSL::TokenType::Semicolon);

    // }
    ++lineNumber;
    checkNextToken(WGSL::TokenType::BraceRight);

    // @fragment
    lineNumber += 2;
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("fragment"_s);

    // fn fragmentShader() -> @location(0) vec4<f32> {
    ++lineNumber;
    checkNextToken(WGSL::TokenType::KeywordFn);
    checkNextTokenIsIdentifier("fragmentShader"_s);
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextToken(WGSL::TokenType::Arrow);
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("location"_s);
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsLiteral(WGSL::TokenType::IntegerLiteral, 0);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextTokenIsIdentifier("vec4"_s);
    checkNextToken(WGSL::TokenType::LT);
    checkNextToken(WGSL::TokenType::KeywordF32);
    checkNextToken(WGSL::TokenType::GT);
    checkNextToken(WGSL::TokenType::BraceLeft);

    // return vec4<f32>(0.4, 0.4, 0.8, 1.0);
    ++lineNumber;
    checkNextToken(WGSL::TokenType::KeywordReturn);
    checkNextTokenIsIdentifier("vec4"_s);
    checkNextToken(WGSL::TokenType::LT);
    checkNextToken(WGSL::TokenType::KeywordF32);
    checkNextToken(WGSL::TokenType::GT);
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 0.4);
    checkNextToken(WGSL::TokenType::Comma);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 0.4);
    checkNextToken(WGSL::TokenType::Comma);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 0.8);
    checkNextToken(WGSL::TokenType::Comma);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 1.0);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextToken(WGSL::TokenType::Semicolon);

    // }
    ++lineNumber;
    checkNextToken(WGSL::TokenType::BraceRight);
    checkNextToken(WGSL::TokenType::EndOfFile);
}

- (void) testLexerOnTriangleVert {
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
        "    return vec4<f32>(pos[VertexIndex], 0.0, 1.0);\n"
        "}\n"_s);

    unsigned lineNumber = 0;
    auto checkNextTokenIs = [&](WGSL::TokenType type) {
        WGSL::Token result = lexer.lex();
        XCTAssertEqual(result.m_type, type);
        XCTAssertEqual(result.m_span.m_line, lineNumber);
        return result;
    };
    auto checkNextTokenIsIdentifier = [&](const String& ident) {
        WGSL::Token result = checkNextTokenIs(WGSL::TokenType::Identifier);
        XCTAssertEqual(result.m_ident, ident);
    };
    auto checkNextTokenIsLiteral = [&](WGSL::TokenType type, double literalValue) {
        WGSL::Token result = checkNextTokenIs(type);
        XCTAssertEqual(result.m_literalValue, literalValue);
    };
    auto checkNextTokensAreBuiltinAttr = [&](const String& attr) {
        checkNextTokenIs(WGSL::TokenType::Attribute);
        checkNextTokenIsIdentifier("builtin"_s);
        checkNextTokenIs(WGSL::TokenType::ParenLeft);
        checkNextTokenIsIdentifier(attr);
        checkNextTokenIs(WGSL::TokenType::ParenRight);
    };

    // @vertex
    checkNextTokenIs(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("vertex"_s);

    ++lineNumber;
    // fn main(
    checkNextTokenIs(WGSL::TokenType::KeywordFn);
    checkNextTokenIsIdentifier("main"_s);
    checkNextTokenIs(WGSL::TokenType::ParenLeft);

    ++lineNumber;
    //    @builtin(vertex_index) VertexIndex : u32
    checkNextTokensAreBuiltinAttr("vertex_index"_s);
    checkNextTokenIsIdentifier("VertexIndex"_s);
    checkNextTokenIs(WGSL::TokenType::Colon);
    checkNextTokenIs(WGSL::TokenType::KeywordU32);

    ++lineNumber;
    // ) -> @builtin(position) vec4<f32> {
    checkNextTokenIs(WGSL::TokenType::ParenRight);
    checkNextTokenIs(WGSL::TokenType::Arrow);
    checkNextTokensAreBuiltinAttr("position"_s);
    checkNextTokenIsIdentifier("vec4"_s);
    checkNextTokenIs(WGSL::TokenType::LT);
    checkNextTokenIs(WGSL::TokenType::KeywordF32);
    checkNextTokenIs(WGSL::TokenType::GT);
    checkNextTokenIs(WGSL::TokenType::BraceLeft);

    ++lineNumber;
    //    var pos = array<vec2<f32>, 3>(
    checkNextTokenIs(WGSL::TokenType::KeywordVar);
    checkNextTokenIsIdentifier("pos"_s);
    checkNextTokenIs(WGSL::TokenType::Equal);
    checkNextTokenIs(WGSL::TokenType::KeywordArray);
    checkNextTokenIs(WGSL::TokenType::LT);
    checkNextTokenIsIdentifier("vec2"_s);
    checkNextTokenIs(WGSL::TokenType::LT);
    checkNextTokenIs(WGSL::TokenType::KeywordF32);
    checkNextTokenIs(WGSL::TokenType::GT);
    checkNextTokenIs(WGSL::TokenType::Comma);
    checkNextTokenIsLiteral(WGSL::TokenType::IntegerLiteral, 3);
    checkNextTokenIs(WGSL::TokenType::GT);
    checkNextTokenIs(WGSL::TokenType::ParenLeft);

    ++lineNumber;
    //        vec2<f32>(0.0, 0.5),
    checkNextTokenIsIdentifier("vec2"_s);
    checkNextTokenIs(WGSL::TokenType::LT);
    checkNextTokenIs(WGSL::TokenType::KeywordF32);
    checkNextTokenIs(WGSL::TokenType::GT);
    checkNextTokenIs(WGSL::TokenType::ParenLeft);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 0.0);
    checkNextTokenIs(WGSL::TokenType::Comma);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 0.5);
    checkNextTokenIs(WGSL::TokenType::ParenRight);
    checkNextTokenIs(WGSL::TokenType::Comma);

    ++lineNumber;
    //        vec2<f32>(-0.5, -0.5),
    checkNextTokenIsIdentifier("vec2"_s);
    checkNextTokenIs(WGSL::TokenType::LT);
    checkNextTokenIs(WGSL::TokenType::KeywordF32);
    checkNextTokenIs(WGSL::TokenType::GT);
    checkNextTokenIs(WGSL::TokenType::ParenLeft);
    checkNextTokenIs(WGSL::TokenType::Minus);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 0.5);
    checkNextTokenIs(WGSL::TokenType::Comma);
    checkNextTokenIs(WGSL::TokenType::Minus);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 0.5);
    checkNextTokenIs(WGSL::TokenType::ParenRight);
    checkNextTokenIs(WGSL::TokenType::Comma);

    ++lineNumber;
    //        vec2<f32>(0.5, -0.5)
    checkNextTokenIsIdentifier("vec2"_s);
    checkNextTokenIs(WGSL::TokenType::LT);
    checkNextTokenIs(WGSL::TokenType::KeywordF32);
    checkNextTokenIs(WGSL::TokenType::GT);
    checkNextTokenIs(WGSL::TokenType::ParenLeft);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 0.5);
    checkNextTokenIs(WGSL::TokenType::Comma);
    checkNextTokenIs(WGSL::TokenType::Minus);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 0.5);
    checkNextTokenIs(WGSL::TokenType::ParenRight);

    ++lineNumber;
    //    );
    checkNextTokenIs(WGSL::TokenType::ParenRight);
    checkNextTokenIs(WGSL::TokenType::Semicolon);

    lineNumber += 2;
    //    return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
    checkNextTokenIs(WGSL::TokenType::KeywordReturn);
    checkNextTokenIsIdentifier("vec4"_s);
    checkNextTokenIs(WGSL::TokenType::LT);
    checkNextTokenIs(WGSL::TokenType::KeywordF32);
    checkNextTokenIs(WGSL::TokenType::GT);
    checkNextTokenIs(WGSL::TokenType::ParenLeft);
    checkNextTokenIsIdentifier("pos"_s);
    checkNextTokenIs(WGSL::TokenType::BracketLeft);
    checkNextTokenIsIdentifier("VertexIndex"_s);
    checkNextTokenIs(WGSL::TokenType::BracketRight);
    checkNextTokenIs(WGSL::TokenType::Comma);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 0.0);
    checkNextTokenIs(WGSL::TokenType::Comma);
    checkNextTokenIsLiteral(WGSL::TokenType::DecimalFloatLiteral, 1.0);
    checkNextTokenIs(WGSL::TokenType::ParenRight);
    checkNextTokenIs(WGSL::TokenType::Semicolon);

    ++lineNumber;
    // }
    checkNextTokenIs(WGSL::TokenType::BraceRight);
}

@end
