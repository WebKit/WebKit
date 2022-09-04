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
#import "Parser.h"

#import "AssignmentStatement.h"
#import "CallableExpression.h"
#import "IdentifierExpression.h"
#import "Lexer.h"
#import "LiteralExpressions.h"
#import "ReturnStatement.h"
#import "StructureAccess.h"
#import "VariableStatement.h"
#import "WGSL.h"
#import <XCTest/XCTest.h>
#import <wtf/DataLog.h>

@interface WGSLParserTests : XCTestCase

@end

@implementation WGSLParserTests

- (void)testParsingStruct {
    auto shader = WGSL::parseLChar(
        "struct B {\n"
        "    a: i32;\n"
        "}"_s);

    if (!shader.has_value())
        dataLogLn(shader.error());
    XCTAssert(shader.has_value());
    XCTAssert(!shader->directives().size());
    XCTAssert(shader->structs().size() == 1);
    XCTAssert(shader->globalVars().isEmpty());
    XCTAssert(shader->functions().isEmpty());
    WGSL::AST::StructDecl& str = shader->structs()[0];
    XCTAssert(str.name() == "B"_s);
    XCTAssert(str.attributes().isEmpty());
    XCTAssert(str.members().size() == 1);
    XCTAssert(str.members()[0]->attributes().isEmpty());
    XCTAssert(str.members()[0]->name() == "a"_s);
    XCTAssert(str.members()[0]->type().isNamed());
    WGSL::AST::NamedType& memberType = downcast<WGSL::AST::NamedType>(str.members()[0]->type());
    XCTAssert(memberType.name() == "i32"_s);
}

- (void)testParsingGlobalVariable {
    auto shader = WGSL::parseLChar(
        "@group(0) @binding(0)\n"
        "var<storage, read_write> x: B;\n"_s);

    if (!shader.has_value())
        dataLogLn(shader.error());
    XCTAssert(shader.has_value());
    XCTAssert(!shader->directives().size());
    XCTAssert(shader->structs().isEmpty());
    XCTAssert(shader->globalVars().size() == 1);
    XCTAssert(shader->functions().isEmpty());
    WGSL::AST::VariableDecl& var = shader->globalVars()[0];
    XCTAssert(var.attributes().size() == 2);
    XCTAssert(var.attributes()[0]->isGroup());
    XCTAssert(!downcast<WGSL::AST::GroupAttribute>(var.attributes()[0].get()).group());
    XCTAssert(var.attributes()[1]->isBinding());
    XCTAssert(!downcast<WGSL::AST::BindingAttribute>(var.attributes()[1].get()).binding());
    XCTAssert(var.name() == "x"_s);
    XCTAssert(var.maybeQualifier());
    XCTAssert(var.maybeQualifier()->storageClass() == WGSL::AST::StorageClass::Storage);
    XCTAssert(var.maybeQualifier()->accessMode() == WGSL::AST::AccessMode::ReadWrite);
    XCTAssert(var.maybeTypeDecl());
    XCTAssert(var.maybeTypeDecl()->isNamed());
    WGSL::AST::NamedType& namedType = downcast<WGSL::AST::NamedType>(*var.maybeTypeDecl());
    XCTAssert(namedType.name() == "B"_s);
    XCTAssert(!var.maybeInitializer());
}

- (void)testParsingFunctionDecl {
    auto shader = WGSL::parseLChar(
        "@compute\n"
        "fn main() {\n"
        "    x.a = 42i;\n"
        "}"_s);

    if (!shader.has_value())
        dataLogLn(shader.error());
    XCTAssert(shader.has_value());
    XCTAssert(!shader->directives().size());
    XCTAssert(shader->structs().isEmpty());
    XCTAssert(shader->globalVars().isEmpty());
    XCTAssert(shader->functions().size() == 1);
    WGSL::AST::FunctionDecl& func = shader->functions()[0];
    XCTAssert(func.attributes().size() == 1);
    XCTAssert(func.attributes()[0]->isStage());
    XCTAssert(downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get()).stage() == WGSL::AST::StageAttribute::Stage::Compute);
    XCTAssert(func.name() == "main"_s);
    XCTAssert(func.parameters().isEmpty());
    XCTAssert(func.returnAttributes().isEmpty());
    XCTAssert(func.maybeReturnType() == nullptr);
    XCTAssert(func.body().statements().size() == 1);
    XCTAssert(func.body().statements()[0]->isAssignment());
    WGSL::AST::AssignmentStatement& stmt = downcast<WGSL::AST::AssignmentStatement>(func.body().statements()[0].get());
    XCTAssert(stmt.maybeLhs());
    XCTAssert(stmt.maybeLhs()->isStructureAccess());
    WGSL::AST::StructureAccess& structAccess = downcast<WGSL::AST::StructureAccess>(*stmt.maybeLhs());
    XCTAssert(structAccess.base()->isIdentifier());
    WGSL::AST::IdentifierExpression base = downcast<WGSL::AST::IdentifierExpression>(structAccess.base().get());
    XCTAssert(base.identifier() == "x"_s);
    XCTAssert(structAccess.fieldName() == "a"_s);
    XCTAssert(stmt.rhs().isInt32Literal());
    WGSL::AST::Int32Literal& rhs = downcast<WGSL::AST::Int32Literal>(stmt.rhs());
    XCTAssert(rhs.value() == 42);
}

- (void)testTrivialGraphicsShader {
    auto shader = WGSL::parseLChar(
        "@vertex\n"
        "fn vertexShader(@location(0) x: vec4<f32>) -> @builtin(position) vec4<f32> {\n"
        "    return x;\n"
        "}\n\n"
        "@fragment\n"
        "fn fragmentShader() -> @location(0) vec4<f32> {\n"
        "    return vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
        "}"_s);

    if (!shader.has_value())
        dataLogLn(shader.error());
    XCTAssert(shader.has_value());
    XCTAssert(!shader->directives().size());
    XCTAssert(shader->structs().isEmpty());
    XCTAssert(shader->globalVars().isEmpty());
    XCTAssert(shader->functions().size() == 2);

    {
        WGSL::AST::FunctionDecl& func = shader->functions()[0];
        XCTAssert(func.attributes().size() == 1);
        XCTAssert(func.attributes()[0]->isStage());
        XCTAssert(downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get()).stage() == WGSL::AST::StageAttribute::Stage::Vertex);
        XCTAssert(func.name() == "vertexShader"_s);
        XCTAssert(func.parameters().size() == 1);
        XCTAssert(func.parameters()[0]->name() == "x"_s);
        XCTAssert(func.parameters()[0]->attributes().size() == 1);
        XCTAssert(func.parameters()[0]->attributes()[0]->isLocation());
        XCTAssert(!downcast<WGSL::AST::LocationAttribute>(func.parameters()[0]->attributes()[0].get()).location());
        XCTAssert(func.parameters()[0]->type().isParameterized());
        WGSL::AST::ParameterizedType& paramType = downcast<WGSL::AST::ParameterizedType>(func.parameters()[0]->type());
        XCTAssert(paramType.base() == WGSL::AST::ParameterizedType::Base::Vec4);
        XCTAssert(paramType.elementType().isNamed());
        XCTAssert(downcast<WGSL::AST::NamedType>(paramType.elementType()).name() == "f32"_s);
        XCTAssert(func.returnAttributes().size() == 1);
        XCTAssert(func.returnAttributes()[0]->isBuiltin());
        XCTAssert(downcast<WGSL::AST::BuiltinAttribute>(func.returnAttributes()[0].get()).name() == "position"_s);
        XCTAssert(func.maybeReturnType());
        XCTAssert(func.maybeReturnType()->isParameterized());
        XCTAssert(func.body().statements().size() == 1);
        XCTAssert(func.body().statements()[0]->isReturn());
        WGSL::AST::ReturnStatement& stmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0].get());
        XCTAssert(stmt.maybeExpression());
        XCTAssert(stmt.maybeExpression()->isIdentifier());
    }

    {
        WGSL::AST::FunctionDecl& func = shader->functions()[1];
        XCTAssert(func.attributes().size() == 1);
        XCTAssert(func.attributes()[0]->isStage());
        XCTAssert(downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get()).stage() == WGSL::AST::StageAttribute::Stage::Fragment);
        XCTAssert(func.name() == "fragmentShader"_s);
        XCTAssert(func.parameters().isEmpty());
        XCTAssert(func.returnAttributes().size() == 1);
        XCTAssert(func.returnAttributes()[0]->isLocation());
        XCTAssert(!downcast<WGSL::AST::LocationAttribute>(func.returnAttributes()[0].get()).location());
        XCTAssert(func.maybeReturnType());
        XCTAssert(func.maybeReturnType()->isParameterized());
        XCTAssert(func.body().statements().size() == 1);
        XCTAssert(func.body().statements()[0]->isReturn());
        WGSL::AST::ReturnStatement& stmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0].get());
        XCTAssert(stmt.maybeExpression());
        XCTAssert(stmt.maybeExpression()->isCallableExpression());
        WGSL::AST::CallableExpression& expr = downcast<WGSL::AST::CallableExpression>(*stmt.maybeExpression());
        XCTAssert(expr.target().isParameterized());
        XCTAssert(expr.arguments().size() == 4);
        XCTAssert(expr.arguments()[0].get().isAbstractFloatLiteral());
        XCTAssert(expr.arguments()[1].get().isAbstractFloatLiteral());
        XCTAssert(expr.arguments()[2].get().isAbstractFloatLiteral());
        XCTAssert(expr.arguments()[3].get().isAbstractFloatLiteral());
    }
}

- (void) testParsingLocalVariable {
    auto shader = WGSL::parseLChar(
        "@vertex\n"
        "fn main() -> vec4<f32> {\n"
        "    var x = vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
        "    return x;\n"
        "}"_s);

    if (!shader.has_value())
        dataLogLn(shader.error());
    XCTAssert(shader.has_value());
    XCTAssertEqual(shader->directives().size(), 0ull);
    XCTAssertEqual(shader->structs().size(), 0ull);
    XCTAssertEqual(shader->globalVars().size(), 0ull);
    XCTAssertEqual(shader->functions().size(), 1ull);

    {
        WGSL::AST::FunctionDecl& func = shader->functions()[0];
        // @vertex
        XCTAssertEqual(func.attributes().size(), 1u);
        XCTAssert(func.attributes()[0]->isStage());
        XCTAssertEqual(downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get()).stage(), WGSL::AST::StageAttribute::Stage::Vertex);

        // fn main() -> vec4<f32> {
        XCTAssert(func.name() == "main"_s);
        XCTAssertEqual(func.parameters().size(), 0u);
        XCTAssertEqual(func.returnAttributes().size(), 0u);
        XCTAssert(func.maybeReturnType());
        XCTAssert(func.maybeReturnType()->isParameterized());
        XCTAssertEqual(func.body().statements().size(), 2u);

        // var x = vec4<f32>(0.4, 0.4, 0.8, 1.0);
        XCTAssert(func.body().statements()[0]->isVariable());
        WGSL::AST::VariableStatement& varStmt = downcast<WGSL::AST::VariableStatement>(func.body().statements()[0].get());
        WGSL::AST::VariableDecl& varDecl = downcast<WGSL::AST::VariableDecl>(varStmt.declaration());
        XCTAssertEqual(varDecl.name(), "x"_s);
        XCTAssertEqual(varDecl.attributes().size(), 0u);
        XCTAssertEqual(varDecl.maybeQualifier(), nullptr);
        XCTAssertEqual(varDecl.maybeTypeDecl(), nullptr);
        XCTAssert(varDecl.maybeInitializer());
        WGSL::AST::CallableExpression& varInitExpr = downcast<WGSL::AST::CallableExpression>(*varDecl.maybeInitializer());
        XCTAssert(varInitExpr.target().isParameterized());
        XCTAssert(varInitExpr.arguments().size() == 4);
        XCTAssert(varInitExpr.arguments()[0].get().isAbstractFloatLiteral());
        XCTAssert(varInitExpr.arguments()[1].get().isAbstractFloatLiteral());
        XCTAssert(varInitExpr.arguments()[2].get().isAbstractFloatLiteral());
        XCTAssert(varInitExpr.arguments()[3].get().isAbstractFloatLiteral());

        // return x;
        XCTAssert(func.body().statements()[1]->isReturn());
        WGSL::AST::ReturnStatement& retStmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[1].get());
        XCTAssert(retStmt.maybeExpression());
        XCTAssert(retStmt.maybeExpression()->isIdentifier());
        WGSL::AST::IdentifierExpression retExpr = downcast<WGSL::AST::IdentifierExpression>(*retStmt.maybeExpression());
        XCTAssert(retExpr.identifier() == "x"_s);
    }
}

@end
