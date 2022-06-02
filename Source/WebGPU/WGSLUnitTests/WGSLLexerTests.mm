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

@interface WGSLLexerTests : XCTestCase

@end

@implementation WGSLLexerTests

- (void)testLexerOnSingleTokens {
    auto checkSingleToken = [] (const char* string, WGSL::TokenType type) {
        WGSL::Lexer<LChar> lexer(string);
        WGSL::Token result = lexer.lex();
        XCTAssertEqual(result.m_type, type);
        return result;
    };
    auto checkSingleLiteral = [&] (const char* string, WGSL::TokenType type, double literalValue) {
        WGSL::Token result = checkSingleToken(string, type);
        XCTAssertEqual(result.m_literalValue, literalValue);
    };

    checkSingleToken("", WGSL::TokenType::EndOfFile);
    checkSingleLiteral("1", WGSL::TokenType::IntegerLiteral, 1);
    checkSingleLiteral("0", WGSL::TokenType::IntegerLiteral, 0);
    checkSingleLiteral("142", WGSL::TokenType::IntegerLiteral, 142);
    checkSingleLiteral("1.1", WGSL::TokenType::DecimalFloatLiteral, 1.1);
    checkSingleLiteral("0.4", WGSL::TokenType::DecimalFloatLiteral, 0.4);
    checkSingleLiteral("0123.456", WGSL::TokenType::DecimalFloatLiteral, 0123.456);
    checkSingleToken("0123", WGSL::TokenType::Invalid);
    checkSingleLiteral("0123.", WGSL::TokenType::DecimalFloatLiteral, 123);
    checkSingleLiteral(".456", WGSL::TokenType::DecimalFloatLiteral, 0.456);
    checkSingleToken(".", WGSL::TokenType::Period);
    checkSingleLiteral("42f", WGSL::TokenType::DecimalFloatLiteral, 42);
    checkSingleLiteral("42e0f", WGSL::TokenType::DecimalFloatLiteral, 42);
    checkSingleLiteral("042e0f", WGSL::TokenType::DecimalFloatLiteral, 42);
    checkSingleToken("042f", WGSL::TokenType::Invalid);
    checkSingleLiteral("42e-3", WGSL::TokenType::DecimalFloatLiteral, 42e-3);
    checkSingleLiteral("42e-a", WGSL::TokenType::IntegerLiteral, 42);
}

- (void) testLexerOnComputeShader {
    WGSL::Lexer<LChar> lexer("@block struct B {\n"
                             "    a: i32;\n"
                             "}\n"
                             "\n"
                             "@group(0) @binding(0)\n"
                             "var<storage, read_write> x: B;\n"
                             "\n"
                             "@stage(compute)\n"
                             "fn main() {\n"
                             "    x.a = 42;\n"
                             "}");

    unsigned lineNumber = 0;
    auto checkNextToken = [&] (WGSL::TokenType type) {
        WGSL::Token result = lexer.lex();
        XCTAssertEqual(result.m_type, type);
        XCTAssertEqual(result.m_span.m_line, lineNumber);
        return result;
    };
    auto checkNextTokenIsIdentifier = [&] (const char* ident) {
        WGSL::Token result = checkNextToken(WGSL::TokenType::Identifier);
        XCTAssertEqual(result.m_ident, ident);
    };
    auto checkNextTokenIsLiteral = [&] (WGSL::TokenType type, double literalValue) {
        WGSL::Token result = checkNextToken(type);
        XCTAssertEqual(result.m_literalValue, literalValue);
    };

    // @block struct B {
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("block");
    checkNextToken(WGSL::TokenType::KeywordStruct);
    checkNextTokenIsIdentifier("B");
    checkNextToken(WGSL::TokenType::BraceLeft);

    // a: i32;
    ++lineNumber;
    checkNextTokenIsIdentifier("a");
    checkNextToken(WGSL::TokenType::Colon);
    checkNextToken(WGSL::TokenType::KeywordI32);
    checkNextToken(WGSL::TokenType::Semicolon);

    // }
    ++lineNumber;
    checkNextToken(WGSL::TokenType::BraceRight);

    // @group(0) @binding(0)
    lineNumber += 2;
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("group");
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsLiteral(WGSL::TokenType::IntegerLiteral, 0);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("binding");
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
    checkNextTokenIsIdentifier("x");
    checkNextToken(WGSL::TokenType::Colon);
    checkNextTokenIsIdentifier("B");
    checkNextToken(WGSL::TokenType::Semicolon);

    // @stage(compute)
    lineNumber += 2;
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("stage");
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsIdentifier("compute");
    checkNextToken(WGSL::TokenType::ParenRight);

    // fn main() {
    ++lineNumber;
    checkNextToken(WGSL::TokenType::KeywordFn);
    checkNextTokenIsIdentifier("main");
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextToken(WGSL::TokenType::BraceLeft);

    // x.a = 42;
    ++lineNumber;
    checkNextTokenIsIdentifier("x");
    checkNextToken(WGSL::TokenType::Period);
    checkNextTokenIsIdentifier("a");
    checkNextToken(WGSL::TokenType::Equal);
    checkNextTokenIsLiteral(WGSL::TokenType::IntegerLiteral, 42);
    checkNextToken(WGSL::TokenType::Semicolon);

    // }
    ++lineNumber;
    checkNextToken(WGSL::TokenType::BraceRight);
    checkNextToken(WGSL::TokenType::EndOfFile);
}

- (void) testLexerOnGraphicsShader {
    WGSL::Lexer<LChar> lexer("@stage(vertex)\n"
                             "fn vertexShader(@location(0) x: vec4<f32>) -> @builtin(position) vec4<f32> {\n"
                             "    return x;\n"
                             "}\n"
                             "\n"
                             "@stage(fragment)\n"
                             "fn fragmentShader() -> @location(0) vec4<f32> {\n"
                             "    return vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
                             "}");

    unsigned lineNumber = 0;
    auto checkNextToken = [&] (WGSL::TokenType type) {
        WGSL::Token result = lexer.lex();
        XCTAssertEqual(result.m_type, type);
        XCTAssertEqual(result.m_span.m_line, lineNumber);
        return result;
    };
    auto checkNextTokenIsIdentifier = [&] (const char* ident) {
        WGSL::Token result = checkNextToken(WGSL::TokenType::Identifier);
        XCTAssertEqual(result.m_ident, ident);
    };
    auto checkNextTokenIsLiteral = [&] (WGSL::TokenType type, double literalValue) {
        WGSL::Token result = checkNextToken(type);
        XCTAssertEqual(result.m_literalValue, literalValue);
    };

    // @stage(vertex)
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("stage");
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsIdentifier("vertex");
    checkNextToken(WGSL::TokenType::ParenRight);

    ++lineNumber;
    // fn vertexShader(@location(0) x: vec4<f32>) -> @builtin(position) vec4<f32> {
    checkNextToken(WGSL::TokenType::KeywordFn);
    checkNextTokenIsIdentifier("vertexShader");
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("location");
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsLiteral(WGSL::TokenType::IntegerLiteral, 0);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextTokenIsIdentifier("x");
    checkNextToken(WGSL::TokenType::Colon);
    checkNextTokenIsIdentifier("vec4");
    checkNextToken(WGSL::TokenType::LT);
    checkNextToken(WGSL::TokenType::KeywordF32);
    checkNextToken(WGSL::TokenType::GT);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextToken(WGSL::TokenType::Arrow);
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("builtin");
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsIdentifier("position");
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextTokenIsIdentifier("vec4");
    checkNextToken(WGSL::TokenType::LT);
    checkNextToken(WGSL::TokenType::KeywordF32);
    checkNextToken(WGSL::TokenType::GT);
    checkNextToken(WGSL::TokenType::BraceLeft);

    // return x;
    ++lineNumber;
    checkNextToken(WGSL::TokenType::KeywordReturn);
    checkNextTokenIsIdentifier("x");
    checkNextToken(WGSL::TokenType::Semicolon);

    // }
    ++lineNumber;
    checkNextToken(WGSL::TokenType::BraceRight);

    // @stage(fragment)
    lineNumber += 2;
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("stage");
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsIdentifier("fragment");
    checkNextToken(WGSL::TokenType::ParenRight);

    // fn fragmentShader() -> @location(0) vec4<f32> {
    ++lineNumber;
    checkNextToken(WGSL::TokenType::KeywordFn);
    checkNextTokenIsIdentifier("fragmentShader");
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextToken(WGSL::TokenType::Arrow);
    checkNextToken(WGSL::TokenType::Attribute);
    checkNextTokenIsIdentifier("location");
    checkNextToken(WGSL::TokenType::ParenLeft);
    checkNextTokenIsLiteral(WGSL::TokenType::IntegerLiteral, 0);
    checkNextToken(WGSL::TokenType::ParenRight);
    checkNextTokenIsIdentifier("vec4");
    checkNextToken(WGSL::TokenType::LT);
    checkNextToken(WGSL::TokenType::KeywordF32);
    checkNextToken(WGSL::TokenType::GT);
    checkNextToken(WGSL::TokenType::BraceLeft);

    // return vec4<f32>(0.4, 0.4, 0.8, 1.0);
    ++lineNumber;
    checkNextToken(WGSL::TokenType::KeywordReturn);
    checkNextTokenIsIdentifier("vec4");
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

@end
