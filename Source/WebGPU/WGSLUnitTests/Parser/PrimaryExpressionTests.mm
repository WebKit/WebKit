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

#import "CallableExpression.h"
#import "IdentifierExpression.h"
#import "LiteralExpressions.h"
#import "ParserPrivate.h"
#import <XCTest/XCTest.h>

static Expected<UniqueRef<WGSL::AST::Expression>, WGSL::Error> parseLCharPrimaryExpression(const String& input)
{
    WGSL::Lexer<LChar> lexer(input);
    WGSL::Parser parser(lexer);

    return parser.parsePrimaryExpression();
}

@interface PrimaryExpressionTests : XCTestCase

@end

@implementation PrimaryExpressionTests

- (void)setUp {
    [super setUp];
    [self setContinueAfterFailure:NO];
}

- (void)testVec4Constructor {
    auto parseResult = parseLCharPrimaryExpression("vec4<f32>(1.0, 1.0, 1.0, 1.0)"_s);
    XCTAssert(parseResult);

    auto expr = WTFMove(*parseResult);
    XCTAssert(expr->isCallableExpression());
    const auto& callExpr = downcast<WGSL::AST::CallableExpression>(expr.get());

    const auto& vecArguments = callExpr.arguments();
    XCTAssert(vecArguments.size() == 4);
    // All arguments should be of type AbstractFloat and value 1.0
    for (const auto& arg : vecArguments) {
        XCTAssert(arg.get().isAbstractFloatLiteral());
        const auto& floatLiteral = downcast<WGSL::AST::AbstractFloatLiteral>(arg.get());
        XCTAssert(floatLiteral.value() == 1.0);
    }
}

- (void)testArrayOfIntWithSizeConstructor {
    auto parseResult = parseLCharPrimaryExpression("array<f32, 4>(10, 10, 10, 10)"_s);
    XCTAssert(parseResult);

    auto expr = WTFMove(*parseResult);
    XCTAssert(expr->isCallableExpression());
    const auto& callExpr = downcast<WGSL::AST::CallableExpression>(expr.get());

    const auto& vecArguments = callExpr.arguments();
    XCTAssert(vecArguments.size() == 4);
    // All arguments should be of type AbstractInt and value 10
    for (const auto& arg : vecArguments) {
        XCTAssert(arg.get().isAbstractIntLiteral());
        const auto& intLiteral = downcast<WGSL::AST::AbstractIntLiteral>(arg.get());
        XCTAssert(intLiteral.value() == 10);
    }
}

- (void)testArrayOfVec4WithSize {
    auto parseResult = parseLCharPrimaryExpression(
        "array<vec2<f32>, 3>(\n"
        "    vec2<f32>(0.5, 0.5),\n"
        "    vec2<f32>(0.5, 0.5),\n"
        "    vec2<f32>(0.5, 0.5))"_s);
    XCTAssert(parseResult);

    auto expr = WTFMove(*parseResult);
    XCTAssert(expr->isCallableExpression());
    const auto& callExpr = downcast<WGSL::AST::CallableExpression>(expr.get());

    const auto& callArgs = callExpr.arguments();
    XCTAssert(callArgs.size() == 3);

    // All arguments should be of type vec2<f32>
    for (const auto& arg : callArgs) {
        XCTAssert(arg.get().isCallableExpression());
        const auto& callExpr = downcast<WGSL::AST::CallableExpression>(arg.get());

        XCTAssert(callExpr.target().isVec());
        const auto& vecTarget = downcast<WGSL::AST::VecType>(callExpr.target());
        XCTAssert(vecTarget.size() == 2);

        // All arguments should be of type AbstractFloat and value 0.5.
        for (const auto& vecArg : callExpr.arguments()) {
            XCTAssert(vecArg.get().isAbstractFloatLiteral());
            const auto& floatLiteral = downcast<WGSL::AST::AbstractFloatLiteral>(vecArg.get());
            XCTAssert(floatLiteral.value() == 0.5);
        }
    }
}

@end
