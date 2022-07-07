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

#import "LiteralExpressions.h"
#import "ParserPrivate.h"
#import <XCTest/XCTest.h>

static Expected<UniqueRef<WGSL::AST::TypeDecl>, WGSL::Error> parseLCharTypeDecl(const String& input)
{
    WGSL::Lexer<LChar> lexer(input);
    WGSL::Parser parser(lexer);

    return parser.parseTypeDecl();
}

static void testNamedType(const String& type)
{
    auto declResult = parseLCharTypeDecl(type);
    XCTAssert(declResult);

    const auto& decl = (*declResult).get();
    XCTAssertEqual(decl.span(), WGSL::SourceSpan(0, 0, 0, type.length()));

    XCTAssert(decl.isNamed());
    const auto& namedDecl = downcast<WGSL::AST::NamedType>(decl);
    XCTAssertEqual(namedDecl.name(), type);
}

@interface TypeDeclTests : XCTestCase

@end

@implementation TypeDeclTests

- (void)setUp {
    [super setUp];
    self.continueAfterFailure = NO;
}

- (void)testBool {
    testNamedType("bool"_s);
}

- (void)testI32 {
    testNamedType("i32"_s);
}

- (void)testU32 {
    testNamedType("u32"_s);
}

- (void)testF32 {
    testNamedType("f32"_s);
}

- (void)testIdentifier {
    testNamedType("some_type_alias"_s);
}

- (void)testVec4 {
    const auto declResult = parseLCharTypeDecl("vec4<f32>"_s);
    XCTAssert(declResult);
    const auto& decl = (*declResult).get();
    XCTAssertEqual(decl.span(), WGSL::SourceSpan(0, 0, 0, 9));

    XCTAssert(decl.isVec());
    const auto& vecDecl = downcast<WGSL::AST::VecType>(decl);

    XCTAssertEqual(vecDecl.size(), 4);

    const auto& elementType = vecDecl.elementType();
    XCTAssert(elementType.isNamed());
    XCTAssertEqual(elementType.span(), WGSL::SourceSpan(0, 5, 5, 3));

    const auto& namedElementType = downcast<WGSL::AST::NamedType>(elementType);
    XCTAssertEqual(namedElementType.name(), "f32"_s);
}

- (void)testVecWithoutElementType {
    const auto declResult = parseLCharTypeDecl("vec4"_s);
    XCTAssert(!declResult);

    const auto& error = declResult.error();
    XCTAssertEqual(error.lineNumber(), 0u);
    XCTAssertEqual(error.lineOffset(), 4u);
    XCTAssertEqual(error.offset(), 4u);
    XCTAssertEqual(error.length(), 0u);
    XCTAssert(error.message() == "Expected <"_s);
}

- (void)testVecWithNonIdentifierType {
    auto declResult = parseLCharTypeDecl("vec4<1234>"_s);
    XCTAssert(!declResult);

    const auto& error = declResult.error();
    XCTAssert(error.message() == "Expected <"_s);
}

- (void)testArrayOfF32 {
    auto declResult = parseLCharTypeDecl("array<f32, 4>"_s);
    XCTAssert(declResult);

    const auto& decl = (*declResult).get();
    XCTAssert(decl.isArray());
    const auto& arrayDecl = downcast<WGSL::AST::ArrayType>(decl);

    const auto& elementType = arrayDecl.elementType();
    XCTAssert(elementType.isNamed());
    const auto& namedElementType = downcast<WGSL::AST::NamedType>(elementType);
    XCTAssert(namedElementType.name() == "f32"_s);

    XCTAssert(arrayDecl.maybeSizeExpression());
}

- (void)testArrayOfVec2OfF32 {
    auto declResult = parseLCharTypeDecl("array<vec2<f32>, 4>"_s);
    XCTAssert(declResult);

    const auto& decl = (*declResult).get();
    XCTAssert(decl.isArray());
    const auto& arrayDecl = downcast<WGSL::AST::ArrayType>(decl);

    const auto& elementType = arrayDecl.elementType();
    XCTAssert(elementType.isVec());
    const auto& vecElementType = downcast<WGSL::AST::VecType>(elementType);
    XCTAssert(vecElementType.size() == 2);

    XCTAssert(arrayDecl.maybeSizeExpression());
    const auto& sizeExpr = *arrayDecl.maybeSizeExpression();
    XCTAssert(sizeExpr.isAbstractIntLiteral());
    const auto& sizeLiteral = downcast<WGSL::AST::AbstractIntLiteral>(sizeExpr);
    XCTAssert(sizeLiteral.value() == 4);
}

- (void)testArrayOfF32WithoutSize {
    auto declResult = parseLCharTypeDecl("array<f32>"_s);
    XCTAssert(declResult);

    const auto& decl = (*declResult).get();
    XCTAssert(decl.isArray());
    const auto& arrayDecl = downcast<WGSL::AST::ArrayType>(decl);

    const auto& elementType = arrayDecl.elementType();
    XCTAssert(elementType.isNamed());
    const auto& namedElementType = downcast<WGSL::AST::NamedType>(elementType);
    XCTAssert(namedElementType.name() == "f32"_s);

    XCTAssert(!arrayDecl.maybeSizeExpression());
}

- (void)testArrayWithoutParameters {
    auto declResult = parseLCharTypeDecl("array"_s);
    XCTAssert(!declResult);

    const auto& error = declResult.error();
    XCTAssert(error.message() == "Expected <"_s);
}

@end
