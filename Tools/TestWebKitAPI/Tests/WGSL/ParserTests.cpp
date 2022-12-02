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
#include "ASTAttribute.h"
#include "ASTTypeDecl.h"
#include "Parser.h"

#include "AST.h"
#include "Lexer.h"
#include "TestWGSLAPI.h"
#include "WGSL.h"

static void checkBuiltin(WGSL::AST::Attribute& attr, ASCIILiteral attrName)
{
    EXPECT_TRUE(is<WGSL::AST::BuiltinAttribute>(attr));
    EXPECT_EQ(downcast<WGSL::AST::BuiltinAttribute>(attr).name(), attrName);
}

static void checkIntLiteral(WGSL::AST::Expression& node, int32_t value)
{
    EXPECT_TRUE(is<WGSL::AST::AbstractIntLiteral>(node));
    auto& intLiteral = downcast<WGSL::AST::AbstractIntLiteral>(node);
    EXPECT_EQ(intLiteral.value(), value);
}

static void checkVecType(WGSL::AST::TypeDecl& type, WGSL::AST::ParameterizedType::Base vecType, ASCIILiteral paramTypeName)
{
    EXPECT_TRUE(is<WGSL::AST::ParameterizedType>(type));
    auto& parameterizedType = downcast<WGSL::AST::ParameterizedType>(type);
    EXPECT_EQ(parameterizedType.base(), vecType);
    EXPECT_TRUE(is<WGSL::AST::NamedType>(parameterizedType.elementType()));
    EXPECT_EQ(downcast<WGSL::AST::NamedType>(parameterizedType.elementType()).name(), paramTypeName);
}

static void checkVec2F32Type(WGSL::AST::TypeDecl& type)
{
    checkVecType(type, WGSL::AST::ParameterizedType::Base::Vec2, "f32"_s);
}

static void checkVec4F32Type(WGSL::AST::TypeDecl& type)
{
    checkVecType(type, WGSL::AST::ParameterizedType::Base::Vec4, "f32"_s);
}

namespace TestWGSLAPI {

TEST(WGSLParserTests, Struct)
{
    auto shader = WGSL::parseLChar(
        "struct B {\n"
        "    a: i32;\n"
        "}"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_EQ(shader->structs().size(), 1u);
    EXPECT_TRUE(shader->globalVars().isEmpty());
    EXPECT_TRUE(shader->functions().isEmpty());
    auto& str = shader->structs()[0];
    EXPECT_EQ(str.name(), "B"_s);
    EXPECT_TRUE(str.attributes().isEmpty());
    EXPECT_EQ(str.members().size(), 1u);
    EXPECT_TRUE(str.members()[0].attributes().isEmpty());
    EXPECT_EQ(str.members()[0].name(), "a"_s);
    EXPECT_TRUE(is<WGSL::AST::NamedType>(str.members()[0].type()));
    auto& memberType = downcast<WGSL::AST::NamedType>(str.members()[0].type());
    EXPECT_EQ(memberType.name(), "i32"_s);
}

TEST(WGSLParserTests, GlobalVariable)
{
    auto shader = WGSL::parseLChar(
        "@group(0) @binding(0)\n"
        "var<storage, read_write> x: B;\n"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structs().isEmpty());
    EXPECT_EQ(shader->globalVars().size(), 1u);
    EXPECT_TRUE(shader->functions().isEmpty());
    auto& var = shader->globalVars()[0];
    EXPECT_EQ(var.attributes().size(), 2u);
    EXPECT_TRUE(is<WGSL::AST::GroupAttribute>(var.attributes()[0]));
    EXPECT_FALSE(downcast<WGSL::AST::GroupAttribute>(var.attributes()[0]).group());
    EXPECT_TRUE(is<WGSL::AST::BindingAttribute>(var.attributes()[1]));
    EXPECT_FALSE(downcast<WGSL::AST::BindingAttribute>(var.attributes()[1]).binding());
    EXPECT_EQ(var.name(), "x"_s);
    EXPECT_TRUE(var.maybeQualifier());
    EXPECT_EQ(var.maybeQualifier()->storageClass(), WGSL::AST::StorageClass::Storage);
    EXPECT_EQ(var.maybeQualifier()->accessMode(), WGSL::AST::AccessMode::ReadWrite);
    EXPECT_TRUE(var.maybeTypeDecl());
    EXPECT_TRUE(is<WGSL::AST::NamedType>(var.maybeTypeDecl()));
    auto& namedType = downcast<WGSL::AST::NamedType>(*var.maybeTypeDecl());
    EXPECT_EQ(namedType.name(), "B"_s);
    EXPECT_FALSE(var.maybeInitializer());
}

TEST(WGSLParserTests, FunctionDecl)
{
    auto shader = WGSL::parseLChar(
        "@compute\n"
        "fn main() {\n"
        "    x.a = 42i;\n"
        "}"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_FALSE(shader->directives().size());
    EXPECT_TRUE(shader->structs().isEmpty());
    EXPECT_TRUE(shader->globalVars().isEmpty());
    EXPECT_EQ(shader->functions().size(), 1u);
    auto& func = shader->functions()[0];
    EXPECT_EQ(func.attributes().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::StageAttribute>(func.attributes()[0]));
    EXPECT_EQ(downcast<WGSL::AST::StageAttribute>(func.attributes()[0]).stage(), WGSL::AST::StageAttribute::Stage::Compute);
    EXPECT_EQ(func.name(), "main"_s);
    EXPECT_TRUE(func.parameters().isEmpty());
    EXPECT_TRUE(func.returnAttributes().isEmpty());
    EXPECT_FALSE(func.maybeReturnType());
    EXPECT_EQ(func.body().statements().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::AssignmentStatement>(func.body().statements()[0]));
    auto& stmt = downcast<WGSL::AST::AssignmentStatement>(func.body().statements()[0]);
    EXPECT_TRUE(stmt.maybeLhs());
    EXPECT_TRUE(is<WGSL::AST::StructureAccess>(stmt.maybeLhs()));
    auto& structAccess = downcast<WGSL::AST::StructureAccess>(*stmt.maybeLhs());
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(structAccess.base()));
    auto& base = downcast<WGSL::AST::IdentifierExpression>(structAccess.base());
    EXPECT_EQ(base.identifier(), "x"_s);
    EXPECT_EQ(structAccess.fieldName(), "a"_s);
    EXPECT_TRUE(is<WGSL::AST::Int32Literal>(stmt.rhs()));
    auto& rhs = downcast<WGSL::AST::Int32Literal>(stmt.rhs());
    EXPECT_EQ(rhs.value(), 42);
}

TEST(WGSLParserTests, TrivialGraphicsShader)
{
    auto shader = WGSL::parseLChar(
        "@vertex\n"
        "fn vertexShader(@location(0) x: vec4<f32>) -> @builtin(position) vec4<f32> {\n"
        "    return x;\n"
        "}\n\n"
        "@fragment\n"
        "fn fragmentShader() -> @location(0) vec4<f32> {\n"
        "    return vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
        "}"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_FALSE(shader->directives().size());
    EXPECT_TRUE(shader->structs().isEmpty());
    EXPECT_TRUE(shader->globalVars().isEmpty());
    EXPECT_EQ(shader->functions().size(), 2u);

    {
        auto& func = shader->functions()[0];
        EXPECT_EQ(func.attributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::StageAttribute>(func.attributes()[0]));
        EXPECT_EQ(downcast<WGSL::AST::StageAttribute>(func.attributes()[0]).stage(), WGSL::AST::StageAttribute::Stage::Vertex);
        EXPECT_EQ(func.name(), "vertexShader"_s);
        EXPECT_EQ(func.parameters().size(), 1u);
        EXPECT_EQ(func.parameters()[0].name(), "x"_s);
        EXPECT_EQ(func.parameters()[0].attributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::LocationAttribute>(func.parameters()[0].attributes()[0]));
        EXPECT_EQ(downcast<WGSL::AST::LocationAttribute>(func.parameters()[0].attributes()[0]).location(), 0U);
        EXPECT_TRUE(is<WGSL::AST::ParameterizedType>(func.parameters()[0].type()));
        auto& paramType = downcast<WGSL::AST::ParameterizedType>(func.parameters()[0].type());
        EXPECT_EQ(paramType.base(), WGSL::AST::ParameterizedType::Base::Vec4);
        EXPECT_TRUE(is<WGSL::AST::NamedType>(paramType.elementType()));
        EXPECT_EQ(downcast<WGSL::AST::NamedType>(paramType.elementType()).name(), "f32"_s);
        EXPECT_EQ(func.returnAttributes().size(), 1u);
        checkBuiltin(func.returnAttributes()[0], "position"_s);
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedType>(func.maybeReturnType()));
        EXPECT_EQ(func.body().statements().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(func.body().statements()[0]));
        auto& stmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0]);
        EXPECT_TRUE(stmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(stmt.maybeExpression()));
    }

    {
        auto& func = shader->functions()[1];
        EXPECT_EQ(func.attributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::StageAttribute>(func.attributes()[0]));
        EXPECT_EQ(downcast<WGSL::AST::StageAttribute>(func.attributes()[0]).stage(), WGSL::AST::StageAttribute::Stage::Fragment);
        EXPECT_EQ(func.name(), "fragmentShader"_s);
        EXPECT_TRUE(func.parameters().isEmpty());
        EXPECT_EQ(func.returnAttributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::LocationAttribute>(func.returnAttributes()[0]));
        EXPECT_FALSE(downcast<WGSL::AST::LocationAttribute>(func.returnAttributes()[0]).location());
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedType>(func.maybeReturnType()));
        EXPECT_EQ(func.body().statements().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(func.body().statements()[0]));
        auto& stmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0]);
        EXPECT_TRUE(stmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::CallableExpression>(stmt.maybeExpression()));
        auto& expr = downcast<WGSL::AST::CallableExpression>(*stmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedType>(expr.target()));
        EXPECT_EQ(expr.arguments().size(), 4u);
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(expr.arguments()[0]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(expr.arguments()[1]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(expr.arguments()[2]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(expr.arguments()[3]));
    }
}

#pragma mark -
#pragma mark Declarations

TEST(WGSLParserTests, LocalVariable)
{
    auto shader = WGSL::parseLChar(
        "@vertex\n"
        "fn main() -> vec4<f32> {\n"
        "    var x = vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
        "    return x;\n"
        "}"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structs().isEmpty());
    EXPECT_TRUE(shader->globalVars().isEmpty());
    EXPECT_EQ(shader->functions().size(), 1u);

    {
        auto& func = shader->functions()[0];
        // @vertex
        EXPECT_EQ(func.attributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::StageAttribute>(func.attributes()[0]));
        EXPECT_EQ(downcast<WGSL::AST::StageAttribute>(func.attributes()[0]).stage(), WGSL::AST::StageAttribute::Stage::Vertex);

        // fn main() -> vec4<f32> {
        EXPECT_EQ(func.name(), "main"_s);
        EXPECT_TRUE(func.parameters().isEmpty());
        EXPECT_TRUE(func.returnAttributes().isEmpty());
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedType>(func.maybeReturnType()));
        EXPECT_EQ(func.body().statements().size(), 2u);

        // var x = vec4<f32>(0.4, 0.4, 0.8, 1.0);
        EXPECT_TRUE(is<WGSL::AST::VariableStatement>(func.body().statements()[0]));
        auto& varStmt = downcast<WGSL::AST::VariableStatement>(func.body().statements()[0]);
        auto& varDecl = downcast<WGSL::AST::VariableDecl>(varStmt.declaration());
        EXPECT_EQ(varDecl.name(), "x"_s);
        EXPECT_TRUE(varDecl.attributes().isEmpty());
        EXPECT_EQ(varDecl.maybeQualifier(), nullptr);
        EXPECT_EQ(varDecl.maybeTypeDecl(), nullptr);
        EXPECT_TRUE(varDecl.maybeInitializer());
        auto& varInitExpr = downcast<WGSL::AST::CallableExpression>(*varDecl.maybeInitializer());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedType>(varInitExpr.target()));
        EXPECT_EQ(varInitExpr.arguments().size(), 4u);
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(varInitExpr.arguments()[0]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(varInitExpr.arguments()[1]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(varInitExpr.arguments()[2]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(varInitExpr.arguments()[3]));

        // return x;
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(func.body().statements()[1]));
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[1]);
        EXPECT_TRUE(retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(retStmt.maybeExpression()));
        auto& retExpr = downcast<WGSL::AST::IdentifierExpression>(*retStmt.maybeExpression());
        EXPECT_EQ(retExpr.identifier(), "x"_s);
    }
}

#pragma mark -
#pragma mark Expressions

TEST(WGSLParserTests, ArrayAccess)
{
    auto shader = WGSL::parseLChar("fn test() { return x[42i]; }"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structs().isEmpty());
    EXPECT_TRUE(shader->globalVars().isEmpty());
    EXPECT_EQ(shader->functions().size(), 1u);

    {
        auto& func = shader->functions()[0];
        // fn test() { ... }
        EXPECT_EQ(func.name(), "test"_s);
        EXPECT_TRUE(func.parameters().isEmpty());
        EXPECT_TRUE(func.returnAttributes().isEmpty());
        EXPECT_FALSE(func.maybeReturnType());

        EXPECT_EQ(func.body().statements().size(), 1u);
        // return x[42];
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(func.body().statements()[0]));
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0]);
        EXPECT_TRUE(retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::ArrayAccess>(retStmt.maybeExpression()));
        auto& arrayAccess = downcast<WGSL::AST::ArrayAccess>(*retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(arrayAccess.base()));
        auto& base = downcast<WGSL::AST::IdentifierExpression>(arrayAccess.base());
        EXPECT_EQ(base.identifier(), "x"_s);
        EXPECT_TRUE(is<WGSL::AST::Int32Literal>(arrayAccess.index()));
        auto& index = downcast<WGSL::AST::Int32Literal>(arrayAccess.index());
        EXPECT_EQ(index.value(), 42);
    }
}

TEST(WGSLParserTests, UnaryExpression)
{
    auto shader = WGSL::parseLChar(
        "fn negate(x: f32) -> f32 {\n"
        "    return -x;\n"
        "}"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structs().isEmpty());
    EXPECT_TRUE(shader->globalVars().isEmpty());
    EXPECT_EQ(shader->functions().size(), 1u);

    {
        auto& func = shader->functions()[0];
        // @vertex
        EXPECT_TRUE(func.attributes().isEmpty());

        // fn negate(x: f32) -> f32 {
        EXPECT_EQ(func.name(), "negate"_s);
        EXPECT_EQ(func.parameters().size(), 1u);
        EXPECT_TRUE(func.returnAttributes().isEmpty());
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::NamedType>(func.maybeReturnType()));

        EXPECT_EQ(func.body().statements().size(), 1u);
        // return x;
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(func.body().statements()[0]));
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0]);
        EXPECT_TRUE(retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::UnaryExpression>(retStmt.maybeExpression()));
        auto& retExpr = downcast<WGSL::AST::UnaryExpression>(*retStmt.maybeExpression());
        EXPECT_EQ(retExpr.operation(), WGSL::AST::UnaryOperation::Negate);
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(retExpr.expression()));
        auto& negateExpr = downcast<WGSL::AST::IdentifierExpression>(retExpr.expression());
        EXPECT_EQ(negateExpr.identifier(), "x"_s);
    }
}

#pragma mark -
#pragma mark WebGPU Example Shaders

TEST(WGSLParserTests, TriangleVert)
{
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

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structs().isEmpty());
    EXPECT_TRUE(shader->globalVars().isEmpty());
    EXPECT_EQ(shader->functions().size(), 1u);

    // fn main(...)
    {
        auto& func = shader->functions()[0];
        // @vertex
        EXPECT_EQ(func.attributes().size(), 1u);

        // fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
        EXPECT_EQ(func.name(), "main"_s);
        EXPECT_EQ(func.parameters().size(), 1u);
        EXPECT_EQ(func.returnAttributes().size(), 1u);
        EXPECT_TRUE(func.maybeReturnType());
        checkVec4F32Type(*func.maybeReturnType());
        EXPECT_EQ(func.returnAttributes().size(), 1u);
        checkBuiltin(func.returnAttributes()[0], "position"_s);
    }

    // var pos = array<vec2<f32>, 3>(...);
    {
        auto& func = shader->functions()[0];
        EXPECT_GE(func.body().statements().size(), 1u);
        auto& stmt = func.body().statements()[0];
        EXPECT_TRUE(is<WGSL::AST::VariableStatement>(stmt));
        auto& varStmt = downcast<WGSL::AST::VariableStatement>(func.body().statements()[0]);
        auto& varDecl = downcast<WGSL::AST::VariableDecl>(varStmt.declaration());
        EXPECT_EQ(varDecl.name(), "pos"_s);
        EXPECT_TRUE(varDecl.attributes().isEmpty());
        EXPECT_EQ(varDecl.maybeQualifier(), nullptr);
        EXPECT_EQ(varDecl.maybeTypeDecl(), nullptr);
        EXPECT_TRUE(varDecl.maybeInitializer());
        auto& varInitExpr = downcast<WGSL::AST::CallableExpression>(*varDecl.maybeInitializer());
        EXPECT_TRUE(is<WGSL::AST::ArrayType>(varInitExpr.target()));
        auto& varInitArrayType = downcast<WGSL::AST::ArrayType>(varInitExpr.target());
        EXPECT_TRUE(varInitArrayType.maybeElementType());
        checkVec2F32Type(*varInitArrayType.maybeElementType());
        EXPECT_TRUE(varInitArrayType.maybeElementCount());
        EXPECT_TRUE(is<WGSL::AST::AbstractIntLiteral>(varInitArrayType.maybeElementCount()));
        checkIntLiteral(*varInitArrayType.maybeElementCount(), 3);
    }

    // return vec4<f32>(..);
    {
        auto& func = shader->functions()[0];
        EXPECT_GE(func.body().statements().size(), 1u);
        auto& stmt = func.body().statements()[1];
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(stmt));
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(stmt);
        EXPECT_TRUE(retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::CallableExpression>(retStmt.maybeExpression()));
        auto& expr = downcast<WGSL::AST::CallableExpression>(*retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedType>(expr.target()));
    }
}

TEST(WGSLParserTests, RedFrag)
{
    auto shader = WGSL::parseLChar(
        "@fragment\n"
        "fn main() -> @location(0) vec4<f32> {\n"
        "    return vec4<f32>(1.0, 0.0, 0.0, 1.0);\n"
        "}\n"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structs().isEmpty());
    EXPECT_TRUE(shader->globalVars().isEmpty());
    EXPECT_EQ(shader->functions().size(), 1u);
}

}
