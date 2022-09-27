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

#import "ArrayAccess.h"
#import "AssignmentStatement.h"
#import "CallableExpression.h"
#import "IdentifierExpression.h"
#import "Lexer.h"
#import "LiteralExpressions.h"
#import "ReturnStatement.h"
#import "StructureAccess.h"
#import "UnaryExpression.h"
#import "VariableStatement.h"
#import "WGSL.h"
#import <XCTest/XCTest.h>
#import <wtf/DataLog.h>

#define ASSERT_BUILTIN(attr, attr_name) \
    do { \
        XCTAssertTrue((attr)->isBuiltin()); \
        XCTAssertEqual(downcast<WGSL::AST::BuiltinAttribute>((attr).get()).name(), (attr_name)); \
    } while (false)

#define ASSERT_INT_LITERAL(node, int_value) \
    do { \
        XCTAssertTrue((node).isAbstractIntLiteral()); \
        auto& intLiteral = downcast<WGSL::AST::AbstractIntLiteral>(node); \
        XCTAssertEqual(intLiteral.value(), (int_value)); \
    } while (false)

#define ASSERT_VEC_T(type, vec_type, param_type) \
    do { \
        XCTAssertTrue((type).isParameterized()); \
        auto& paramType = downcast<WGSL::AST::ParameterizedType>((type)); \
        XCTAssertEqual(paramType.base(), WGSL::AST::ParameterizedType::Base::vec_type); \
        XCTAssertTrue(paramType.elementType().isNamed()); \
        XCTAssertEqual(downcast<WGSL::AST::NamedType>(paramType.elementType()).name(), (param_type)); \
    } while (false)


#define ASSERT_VEC2_F32(type) ASSERT_VEC_T(type, Vec2, "f32"_s)
#define ASSERT_VEC4_F32(type) ASSERT_VEC_T(type, Vec4, "f32"_s)

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
    XCTAssertTrue(shader.has_value());
    XCTAssertTrue(shader->directives().isEmpty());
    XCTAssertEqual(shader->structs().size(), 1u);
    XCTAssertTrue(shader->globalVars().isEmpty());
    XCTAssertTrue(shader->functions().isEmpty());
    auto& str = shader->structs()[0].get();
    XCTAssertEqual(str.name(), "B"_s);
    XCTAssertTrue(str.attributes().isEmpty());
    XCTAssertEqual(str.members().size(), 1u);
    XCTAssertTrue(str.members()[0]->attributes().isEmpty());
    XCTAssertEqual(str.members()[0]->name(), "a"_s);
    XCTAssertTrue(str.members()[0]->type().isNamed());
    auto& memberType = downcast<WGSL::AST::NamedType>(str.members()[0]->type());
    XCTAssertEqual(memberType.name(), "i32"_s);
}

- (void)testParsingGlobalVariable {
    auto shader = WGSL::parseLChar(
        "@group(0) @binding(0)\n"
        "var<storage, read_write> x: B;\n"_s);

    if (!shader.has_value())
        dataLogLn(shader.error());
    XCTAssertTrue(shader.has_value());
    XCTAssertTrue(shader->directives().isEmpty());
    XCTAssertTrue(shader->structs().isEmpty());
    XCTAssertEqual(shader->globalVars().size(), 1u);
    XCTAssertTrue(shader->functions().isEmpty());
    auto& var = shader->globalVars()[0].get();
    XCTAssertEqual(var.attributes().size(), 2u);
    XCTAssertTrue(var.attributes()[0]->isGroup());
    XCTAssertEqual(downcast<WGSL::AST::GroupAttribute>(var.attributes()[0].get()).group(), 0u);
    XCTAssertTrue(var.attributes()[1]->isBinding());
    XCTAssertEqual(downcast<WGSL::AST::BindingAttribute>(var.attributes()[1].get()).binding(), 0u);
    XCTAssertEqual(var.name(), "x"_s);
    XCTAssertTrue(var.maybeQualifier());
    XCTAssertEqual(var.maybeQualifier()->storageClass(), WGSL::AST::StorageClass::Storage);
    XCTAssertEqual(var.maybeQualifier()->accessMode(), WGSL::AST::AccessMode::ReadWrite);
    XCTAssertTrue(var.maybeTypeDecl());
    XCTAssertTrue(var.maybeTypeDecl()->isNamed());
    auto& namedType = downcast<WGSL::AST::NamedType>(*var.maybeTypeDecl());
    XCTAssertEqual(namedType.name(), "B"_s);
    XCTAssertFalse(var.maybeInitializer());
}

- (void)testParsingFunctionDecl {
    auto shader = WGSL::parseLChar(
        "@compute\n"
        "fn main() {\n"
        "    x.a = 42i;\n"
        "}"_s);

    if (!shader.has_value())
        dataLogLn(shader.error());
    XCTAssertTrue(shader.has_value());
    XCTAssertTrue(shader->directives().isEmpty());
    XCTAssertTrue(shader->structs().isEmpty());
    XCTAssertTrue(shader->globalVars().isEmpty());
    XCTAssertEqual(shader->functions().size(), 1u);
    auto& func = shader->functions()[0].get();
    XCTAssertEqual(func.attributes().size(), 1u);
    XCTAssertTrue(func.attributes()[0]->isStage());
    XCTAssertEqual(downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get()).stage(), WGSL::AST::StageAttribute::Stage::Compute);
    XCTAssertEqual(func.name(), "main"_s);
    XCTAssertTrue(func.parameters().isEmpty());
    XCTAssertTrue(func.returnAttributes().isEmpty());
    XCTAssertEqual(func.maybeReturnType(), nullptr);
    XCTAssertEqual(func.body().statements().size(), 1u);
    XCTAssertTrue(func.body().statements()[0]->isAssignment());
    auto& stmt = downcast<WGSL::AST::AssignmentStatement>(func.body().statements()[0].get());
    XCTAssertTrue(stmt.maybeLhs());
    XCTAssertTrue(stmt.maybeLhs()->isStructureAccess());
    auto& structAccess = downcast<WGSL::AST::StructureAccess>(*stmt.maybeLhs());
    XCTAssertTrue(structAccess.base().isIdentifier());
    auto& base = downcast<WGSL::AST::IdentifierExpression>(structAccess.base());
    XCTAssertEqual(base.identifier(), "x"_s);
    XCTAssertEqual(structAccess.fieldName(), "a"_s);
    XCTAssertTrue(stmt.rhs().isInt32Literal());
    auto& rhs = downcast<WGSL::AST::Int32Literal>(stmt.rhs());
    XCTAssertEqual(rhs.value(), 42);
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
    XCTAssertTrue(shader.has_value());
    XCTAssertTrue(shader->directives().isEmpty());
    XCTAssertTrue(shader->structs().isEmpty());
    XCTAssertTrue(shader->globalVars().isEmpty());
    XCTAssertEqual(shader->functions().size(), 2u);

    {
        auto& func = shader->functions()[0].get();
        XCTAssertEqual(func.attributes().size(), 1u);
        XCTAssertTrue(func.attributes()[0]->isStage());
        XCTAssertEqual(downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get()).stage(), WGSL::AST::StageAttribute::Stage::Vertex);
        XCTAssertEqual(func.name(), "vertexShader"_s);
        XCTAssertEqual(func.parameters().size(), 1u);
        XCTAssertEqual(func.parameters()[0]->name(), "x"_s);
        XCTAssertEqual(func.parameters()[0]->attributes().size(), 1u);
        XCTAssertTrue(func.parameters()[0]->attributes()[0]->isLocation());
        XCTAssertEqual(downcast<WGSL::AST::LocationAttribute>(func.parameters()[0]->attributes()[0].get()).location(), 0u);
        XCTAssertTrue(func.parameters()[0]->type().isParameterized());
        auto& paramType = downcast<WGSL::AST::ParameterizedType>(func.parameters()[0]->type());
        XCTAssertEqual(paramType.base(), WGSL::AST::ParameterizedType::Base::Vec4);
        XCTAssertTrue(paramType.elementType().isNamed());
        XCTAssertEqual(downcast<WGSL::AST::NamedType>(paramType.elementType()).name(), "f32"_s);
        XCTAssertEqual(func.returnAttributes().size(), 1u);
        XCTAssertTrue(func.returnAttributes()[0]->isBuiltin());
        XCTAssertEqual(downcast<WGSL::AST::BuiltinAttribute>(func.returnAttributes()[0].get()).name(), "position"_s);
        XCTAssertTrue(func.maybeReturnType());
        XCTAssertTrue(func.maybeReturnType()->isParameterized());
        XCTAssertEqual(func.body().statements().size(), 1u);
        XCTAssertTrue(func.body().statements()[0]->isReturn());
        auto& stmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0].get());
        XCTAssertTrue(stmt.maybeExpression());
        XCTAssertTrue(stmt.maybeExpression()->isIdentifier());
    }

    {
        auto& func = shader->functions()[1].get();
        XCTAssertEqual(func.attributes().size(), 1u);
        XCTAssertTrue(func.attributes()[0]->isStage());
        XCTAssertEqual(downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get()).stage(), WGSL::AST::StageAttribute::Stage::Fragment);
        XCTAssertEqual(func.name(), "fragmentShader"_s);
        XCTAssertTrue(func.parameters().isEmpty());
        XCTAssertEqual(func.returnAttributes().size(), 1u);
        XCTAssertTrue(func.returnAttributes()[0]->isLocation());
        XCTAssertEqual(downcast<WGSL::AST::LocationAttribute>(func.returnAttributes()[0].get()).location(), 0u);
        XCTAssertTrue(func.maybeReturnType());
        XCTAssertTrue(func.maybeReturnType()->isParameterized());
        XCTAssertEqual(func.body().statements().size(), 1u);
        XCTAssertTrue(func.body().statements()[0]->isReturn());
        auto& stmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0].get());
        XCTAssertTrue(stmt.maybeExpression());
        XCTAssertTrue(stmt.maybeExpression()->isCallableExpression());
        auto& expr = downcast<WGSL::AST::CallableExpression>(*stmt.maybeExpression());
        XCTAssertTrue(expr.target().isParameterized());
        XCTAssertEqual(expr.arguments().size(), 4u);
        XCTAssertTrue(expr.arguments()[0].get().isAbstractFloatLiteral());
        XCTAssertTrue(expr.arguments()[1].get().isAbstractFloatLiteral());
        XCTAssertTrue(expr.arguments()[2].get().isAbstractFloatLiteral());
        XCTAssertTrue(expr.arguments()[3].get().isAbstractFloatLiteral());
    }
}

#pragma mark -
#pragma mark Declarations

- (void) testParsingLocalVariable {
    auto shader = WGSL::parseLChar(
        "@vertex\n"
        "fn main() -> vec4<f32> {\n"
        "    var x = vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
        "    return x;\n"
        "}"_s);

    if (!shader.has_value())
        dataLogLn(shader.error());
    XCTAssertTrue(shader.has_value());
    XCTAssertTrue(shader->directives().isEmpty());
    XCTAssertTrue(shader->structs().isEmpty());
    XCTAssertTrue(shader->globalVars().isEmpty());
    XCTAssertEqual(shader->functions().size(), 1u);

    {
        auto& func = shader->functions()[0].get();
        // @vertex
        XCTAssertEqual(func.attributes().size(), 1u);
        XCTAssertTrue(func.attributes()[0]->isStage());
        XCTAssertEqual(downcast<WGSL::AST::StageAttribute>(func.attributes()[0].get()).stage(), WGSL::AST::StageAttribute::Stage::Vertex);

        // fn main() -> vec4<f32> {
        XCTAssertEqual(func.name(), "main"_s);
        XCTAssertTrue(func.parameters().isEmpty());
        XCTAssertTrue(func.returnAttributes().isEmpty());
        XCTAssertTrue(func.maybeReturnType());
        XCTAssertTrue(func.maybeReturnType()->isParameterized());
        XCTAssertEqual(func.body().statements().size(), 2u);

        // var x = vec4<f32>(0.4, 0.4, 0.8, 1.0);
        XCTAssertTrue(func.body().statements()[0]->isVariable());
        auto& varStmt = downcast<WGSL::AST::VariableStatement>(func.body().statements()[0].get());
        auto& varDecl = downcast<WGSL::AST::VariableDecl>(varStmt.declaration());
        XCTAssertEqual(varDecl.name(), "x"_s);
        XCTAssertTrue(varDecl.attributes().isEmpty());
        XCTAssertEqual(varDecl.maybeQualifier(), nullptr);
        XCTAssertEqual(varDecl.maybeTypeDecl(), nullptr);
        XCTAssertTrue(varDecl.maybeInitializer());
        auto& varInitExpr = downcast<WGSL::AST::CallableExpression>(*varDecl.maybeInitializer());
        XCTAssertTrue(varInitExpr.target().isParameterized());
        XCTAssertEqual(varInitExpr.arguments().size(), 4u);
        XCTAssertTrue(varInitExpr.arguments()[0].get().isAbstractFloatLiteral());
        XCTAssertTrue(varInitExpr.arguments()[1].get().isAbstractFloatLiteral());
        XCTAssertTrue(varInitExpr.arguments()[2].get().isAbstractFloatLiteral());
        XCTAssertTrue(varInitExpr.arguments()[3].get().isAbstractFloatLiteral());

        // return x;
        XCTAssertTrue(func.body().statements()[1]->isReturn());
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[1].get());
        XCTAssertTrue(retStmt.maybeExpression());
        XCTAssertTrue(retStmt.maybeExpression()->isIdentifier());
        auto& retExpr = downcast<WGSL::AST::IdentifierExpression>(*retStmt.maybeExpression());
        XCTAssertEqual(retExpr.identifier(), "x"_s);
    }
}

#pragma mark -
#pragma mark Expressions

- (void) testParsingArrayAccess {
    auto shader = WGSL::parseLChar("fn test() { return x[42i]; }"_s);

    if (!shader.has_value())
        dataLogLn(shader.error());
    XCTAssertTrue(shader.has_value());
    XCTAssertTrue(shader->directives().isEmpty());
    XCTAssertTrue(shader->structs().isEmpty());
    XCTAssertTrue(shader->globalVars().isEmpty());
    XCTAssertEqual(shader->functions().size(), 1u);

    {
        auto& func = shader->functions()[0].get();
        // fn test() { ... }
        XCTAssertEqual(func.name(), "test"_s);
        XCTAssertTrue(func.parameters().isEmpty());
        XCTAssertTrue(func.returnAttributes().isEmpty());
        XCTAssertFalse(func.maybeReturnType());

        XCTAssertEqual(func.body().statements().size(), 1u);
        // return x[42];
        XCTAssertTrue(func.body().statements()[0]->isReturn());
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0].get());
        XCTAssertTrue(retStmt.maybeExpression());
        XCTAssertTrue(retStmt.maybeExpression()->isArrayAccess());
        auto& arrayAccess = downcast<WGSL::AST::ArrayAccess>(*retStmt.maybeExpression());
        XCTAssertTrue(arrayAccess.base()->isIdentifier());
        auto& base = downcast<WGSL::AST::IdentifierExpression>(arrayAccess.base().get());
        XCTAssertEqual(base.identifier(), "x"_s);
        XCTAssertTrue(arrayAccess.index()->isInt32Literal());
        auto& index = downcast<WGSL::AST::Int32Literal>(arrayAccess.index().get());
        XCTAssertEqual(index.value(), 42);
    }
}

- (void) testParsingUnaryExpression {
    auto shader = WGSL::parseLChar(
        "fn negate(x: f32) -> f32 {\n"
        "    return -x;\n"
        "}"_s);

    if (!shader.has_value())
        dataLogLn(shader.error());
    XCTAssertTrue(shader.has_value());
    XCTAssertTrue(shader->directives().isEmpty());
    XCTAssertTrue(shader->structs().isEmpty());
    XCTAssertTrue(shader->globalVars().isEmpty());
    XCTAssertEqual(shader->functions().size(), 1u);

    {
        WGSL::AST::FunctionDecl& func = shader->functions()[0];
        // @vertex
        XCTAssertTrue(func.attributes().isEmpty());

        // fn negate(x: f32) -> f32 {
        XCTAssertEqual(func.name(), "negate"_s);
        XCTAssertEqual(func.parameters().size(), 1u);
        XCTAssertTrue(func.returnAttributes().isEmpty());
        XCTAssertTrue(func.maybeReturnType());
        XCTAssertTrue(func.maybeReturnType()->isNamed());

        XCTAssertEqual(func.body().statements().size(), 1u);
        // return x;
        XCTAssertTrue(func.body().statements()[0]->isReturn());
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0].get());
        XCTAssertTrue(retStmt.maybeExpression());
        XCTAssertTrue(retStmt.maybeExpression()->isUnaryExpression());
        auto& retExpr = downcast<WGSL::AST::UnaryExpression>(*retStmt.maybeExpression());
        XCTAssertEqual(retExpr.operation(), WGSL::AST::UnaryOperation::Negate);
        XCTAssertTrue(retExpr.expression().isIdentifier());
        auto& negateExpr = downcast<WGSL::AST::IdentifierExpression>(retExpr.expression());
        XCTAssertEqual(negateExpr.identifier(), "x"_s);
    }
}

#pragma mark -
#pragma mark WebGPU Example Shaders

- (void) testParsingTriangleVert {
    auto shader = WGSL::parseLChar(
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

    if (!shader.has_value()) {
        auto& error = shader.error();
        XCTFail(@"%u:%u length:%u: %s\n", error.lineNumber(), error.lineOffset(), error.length(), error.message().characters8());
    }
    XCTAssertTrue(shader.has_value());
    XCTAssertTrue(shader->directives().isEmpty());
    XCTAssertTrue(shader->structs().isEmpty());
    XCTAssertTrue(shader->globalVars().isEmpty());
    XCTAssertEqual(shader->functions().size(), 1u);

    // fn main(...)
    {
        auto& func = shader->functions()[0].get();
        // @vertex
        XCTAssertEqual(func.attributes().size(), 1u);

        // fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
        XCTAssertEqual(func.name(), "main"_s);
        XCTAssertEqual(func.parameters().size(), 1u);
        XCTAssertEqual(func.returnAttributes().size(), 1u);
        XCTAssertTrue(func.maybeReturnType());
        ASSERT_VEC4_F32(*func.maybeReturnType());
        XCTAssertEqual(func.returnAttributes().size(), 1u);
        ASSERT_BUILTIN(func.returnAttributes()[0], "position"_s);
    }

    // var pos = array<vec2<f32>, 3>(...);
    {
        auto& func = shader->functions()[0].get();
        XCTAssertGreaterThanOrEqual(func.body().statements().size(), 1u);
        auto& stmt = func.body().statements()[0].get();
        XCTAssertTrue(stmt.isVariable());
        auto& varStmt = downcast<WGSL::AST::VariableStatement>(func.body().statements()[0].get());
        auto& varDecl = downcast<WGSL::AST::VariableDecl>(varStmt.declaration());
        XCTAssertEqual(varDecl.name(), "pos"_s);
        XCTAssertTrue(varDecl.attributes().isEmpty());
        XCTAssertEqual(varDecl.maybeQualifier(), nullptr);
        XCTAssertEqual(varDecl.maybeTypeDecl(), nullptr);
        XCTAssertTrue(varDecl.maybeInitializer());
        auto& varInitExpr = downcast<WGSL::AST::CallableExpression>(*varDecl.maybeInitializer());
        XCTAssertTrue(varInitExpr.target().isArray());
        auto& varInitArrayType = downcast<WGSL::AST::ArrayType>(varInitExpr.target());
        XCTAssertTrue(varInitArrayType.maybeElementType());
        ASSERT_VEC2_F32(*varInitArrayType.maybeElementType());
        XCTAssertTrue(varInitArrayType.maybeElementCount());
        XCTAssertTrue(varInitArrayType.maybeElementCount()->isAbstractIntLiteral());
        ASSERT_INT_LITERAL(*varInitArrayType.maybeElementCount(), 3);
    }

    // return vec4<f32>(..);
    {
        auto& func = shader->functions()[0].get();
        XCTAssertTrue(func.body().statements().size() >= 1u);
        auto& stmt = func.body().statements()[1].get();
        XCTAssertTrue(stmt.isReturn());
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(stmt);
        XCTAssertTrue(retStmt.maybeExpression());
        XCTAssertTrue(retStmt.maybeExpression()->isCallableExpression());
        auto& expr = downcast<WGSL::AST::CallableExpression>(*retStmt.maybeExpression());
        XCTAssertTrue(expr.target().isParameterized());
    }
}

- (void) testParsingRedFrag {
    auto shader = WGSL::parseLChar(
        "@fragment\n"
        "fn main() -> @location(0) vec4<f32> {\n"
        "    return vec4<f32>(1.0, 0.0, 0.0, 1.0);\n"
        "}\n"_s);

    if (!shader.has_value())
        dataLogLn(shader.error());
    XCTAssertTrue(shader.has_value());
    XCTAssertTrue(shader->directives().isEmpty());
    XCTAssertTrue(shader->structs().isEmpty());
    XCTAssertTrue(shader->globalVars().isEmpty());
    XCTAssertEqual(shader->functions().size(), 1u);
}

@end
