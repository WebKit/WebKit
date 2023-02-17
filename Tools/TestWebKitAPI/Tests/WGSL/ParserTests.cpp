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
#include "ASTTypeName.h"
#include "Parser.h"
#include "ParserPrivate.h"

#include "AST.h"
#include "Lexer.h"
#include "TestWGSLAPI.h"
#include "WGSLShaderModule.h"

#include <wtf/Assertions.h>

static void checkBuiltin(WGSL::AST::Attribute& attr, ASCIILiteral attrName)
{
    EXPECT_TRUE(is<WGSL::AST::BuiltinAttribute>(attr));
    EXPECT_EQ(downcast<WGSL::AST::BuiltinAttribute>(attr).name(), attrName);
}

static void checkIntLiteral(WGSL::AST::Expression& node, int32_t value)
{
    EXPECT_TRUE(is<WGSL::AST::AbstractIntegerLiteral>(node));
    auto& intLiteral = downcast<WGSL::AST::AbstractIntegerLiteral>(node);
    EXPECT_EQ(intLiteral.value(), value);
}

static void checkVecType(WGSL::AST::TypeName& type, WGSL::AST::ParameterizedTypeName::Base vecType, ASCIILiteral paramTypeName)
{
    EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(type));
    auto& parameterizedType = downcast<WGSL::AST::ParameterizedTypeName>(type);
    EXPECT_EQ(parameterizedType.base(), vecType);
    EXPECT_TRUE(is<WGSL::AST::NamedTypeName>(parameterizedType.elementType()));
    EXPECT_EQ(downcast<WGSL::AST::NamedTypeName>(parameterizedType.elementType()).name(), paramTypeName);
}

static void checkVec2F32Type(WGSL::AST::TypeName& type)
{
    checkVecType(type, WGSL::AST::ParameterizedTypeName::Base::Vec2, "f32"_s);
}

static void checkVec4F32Type(WGSL::AST::TypeName& type)
{
    checkVecType(type, WGSL::AST::ParameterizedTypeName::Base::Vec4, "f32"_s);
}

namespace TestWGSLAPI {

inline Expected<WGSL::ShaderModule, WGSL::Error> parse(const String& wgsl)
{
    WGSL::ShaderModule shaderModule(wgsl, { 8 });
    auto maybeError = WGSL::parse(shaderModule);
    if (maybeError.has_value())
        return makeUnexpected(*maybeError);
    return { WTFMove(shaderModule) };
}

static void testStruct(ASCIILiteral program, const Vector<String>& fieldNames, const Vector<String>& typeNames)
{
    ASSERT(fieldNames.size() == typeNames.size());

    auto shader = parse(program);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_EQ(shader->structures().size(), 1u);
    EXPECT_TRUE(shader->variables().isEmpty());
    EXPECT_TRUE(shader->functions().isEmpty());
    auto& str = shader->structures()[0];
    EXPECT_EQ(str.name(), "B"_s);
    EXPECT_TRUE(str.attributes().isEmpty());

    EXPECT_EQ(str.members().size(), fieldNames.size());
    for (unsigned i = 0; i < fieldNames.size(); ++i) {
        EXPECT_TRUE(str.members()[i].attributes().isEmpty());
        EXPECT_EQ(str.members()[i].name(), fieldNames[i]);
        EXPECT_TRUE(is<WGSL::AST::NamedTypeName>(str.members()[i].type()));
        auto& memberType = downcast<WGSL::AST::NamedTypeName>(str.members()[i].type());
        EXPECT_EQ(memberType.name(), typeNames[i]);
    }
}

TEST(WGSLParserTests, SourceLifecycle)
{
    Expected<WGSL::ShaderModule, WGSL::Error> shader = ([&]() {
        auto source = makeString(
            "@group(0)"_s,
            " "_s,
            "@binding(0)"_s,
            "var<storage, read_write>"_s,
            " "_s,
            "x: B;"_s
        );
        return parse(source);
    })();

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_EQ(shader->variables().size(), 1u);
    EXPECT_TRUE(shader->functions().isEmpty());
    auto& var = shader->variables()[0];
    EXPECT_EQ(var.attributes().size(), 2u);
    EXPECT_TRUE(is<WGSL::AST::GroupAttribute>(var.attributes()[0]));
    EXPECT_FALSE(downcast<WGSL::AST::GroupAttribute>(var.attributes()[0]).group());
    EXPECT_TRUE(is<WGSL::AST::BindingAttribute>(var.attributes()[1]));
    EXPECT_FALSE(downcast<WGSL::AST::BindingAttribute>(var.attributes()[1]).binding());
    EXPECT_EQ(var.name(), "x"_s);
    EXPECT_TRUE(var.maybeQualifier());
    EXPECT_EQ(var.maybeQualifier()->storageClass(), WGSL::AST::StorageClass::Storage);
    EXPECT_EQ(var.maybeQualifier()->accessMode(), WGSL::AST::AccessMode::ReadWrite);
    EXPECT_TRUE(var.maybeTypeName());
    EXPECT_TRUE(is<WGSL::AST::NamedTypeName>(var.maybeTypeName()));
    auto& namedType = downcast<WGSL::AST::NamedTypeName>(*var.maybeTypeName());
    EXPECT_EQ(namedType.name(), "B"_s);
    EXPECT_FALSE(var.maybeInitializer());
}

TEST(WGSLParserTests, Struct)
{
    // 1 field, without trailing comma
    testStruct(
        "struct B {\n"
        "    a: i32\n"
        "}"_s, { "a"_s }, { "i32"_s });

    // 1 field, with trailing comma
    testStruct(
        "struct B {\n"
        "    a: i32,\n"
        "}"_s, { "a"_s }, { "i32"_s });

    // 2 fields, without trailing comma
    testStruct(
        "struct B {\n"
        "    a: i32,\n"
        "    b: f32\n"
        "}"_s, { "a"_s, "b"_s }, { "i32"_s, "f32"_s });

    // 2 fields, with trailing comma
    testStruct(
        "struct B {\n"
        "    a: i32,\n"
        "    b: f32,\n"
        "}"_s, { "a"_s, "b"_s }, { "i32"_s, "f32"_s });
}

TEST(WGSLParserTests, GlobalVariable)
{
    auto shader = parse(
        "@group(0) @binding(0)\n"
        "var<storage, read_write> x: B;\n"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_EQ(shader->variables().size(), 1u);
    EXPECT_TRUE(shader->functions().isEmpty());
    auto& var = shader->variables()[0];
    EXPECT_EQ(var.attributes().size(), 2u);
    EXPECT_TRUE(is<WGSL::AST::GroupAttribute>(var.attributes()[0]));
    auto& groupAttribute = downcast<WGSL::AST::GroupAttribute>(var.attributes()[0]);
    EXPECT_EQ(groupAttribute.group(), 0u);
    EXPECT_TRUE(is<WGSL::AST::BindingAttribute>(var.attributes()[1]));
    auto& bindingAttribute = downcast<WGSL::AST::BindingAttribute>(var.attributes()[1]);
    EXPECT_EQ(bindingAttribute.binding(), 0u);
    EXPECT_EQ(var.name(), "x"_s);
    EXPECT_TRUE(var.maybeQualifier());
    EXPECT_EQ(var.maybeQualifier()->storageClass(), WGSL::AST::StorageClass::Storage);
    EXPECT_EQ(var.maybeQualifier()->accessMode(), WGSL::AST::AccessMode::ReadWrite);
    EXPECT_TRUE(var.maybeTypeName());
    EXPECT_TRUE(is<WGSL::AST::NamedTypeName>(var.maybeTypeName()));
    auto& namedType = downcast<WGSL::AST::NamedTypeName>(*var.maybeTypeName());
    EXPECT_EQ(namedType.name(), "B"_s);
    EXPECT_FALSE(var.maybeInitializer());
}

TEST(WGSLParserTests, FunctionDecl)
{
    auto shader = parse(
        "@compute\n"
        "fn main() {\n"
        "    x.a = 42i;\n"
        "}"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_FALSE(shader->directives().size());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
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
    EXPECT_TRUE(is<WGSL::AST::FieldAccessExpression>(stmt.lhs()));
    auto& structAccess = downcast<WGSL::AST::FieldAccessExpression>(stmt.lhs());
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(structAccess.base()));
    auto& base = downcast<WGSL::AST::IdentifierExpression>(structAccess.base());
    EXPECT_EQ(base.identifier(), "x"_s);
    EXPECT_EQ(structAccess.fieldName(), "a"_s);
    EXPECT_TRUE(is<WGSL::AST::Signed32Literal>(stmt.rhs()));
    auto& rhs = downcast<WGSL::AST::Signed32Literal>(stmt.rhs());
    EXPECT_EQ(rhs.value(), 42);
}

TEST(WGSLParserTests, TrivialGraphicsShader)
{
    auto shader = parse(
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
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
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
        auto& locationAttribute = downcast<WGSL::AST::LocationAttribute>(func.parameters()[0].attributes()[0]);
        EXPECT_EQ(locationAttribute.location(), 0u);
        EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(func.parameters()[0].typeName()));
        auto& paramType = downcast<WGSL::AST::ParameterizedTypeName>(func.parameters()[0].typeName());
        EXPECT_EQ(paramType.base(), WGSL::AST::ParameterizedTypeName::Base::Vec4);
        EXPECT_TRUE(is<WGSL::AST::NamedTypeName>(paramType.elementType()));
        EXPECT_EQ(downcast<WGSL::AST::NamedTypeName>(paramType.elementType()).name(), "f32"_s);
        EXPECT_EQ(func.returnAttributes().size(), 1u);
        checkBuiltin(func.returnAttributes()[0], "position"_s);
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(func.maybeReturnType()));
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
        auto& locationAttribute = downcast<WGSL::AST::LocationAttribute>(func.returnAttributes()[0]);
        EXPECT_EQ(locationAttribute.location(), 0u);
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(func.maybeReturnType()));
        EXPECT_EQ(func.body().statements().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(func.body().statements()[0]));
        auto& stmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0]);
        EXPECT_TRUE(stmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::CallExpression>(stmt.maybeExpression()));
        auto& expr = downcast<WGSL::AST::CallExpression>(*stmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(expr.target()));
        EXPECT_EQ(expr.arguments().size(), 4u);
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(expr.arguments()[0]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(expr.arguments()[1]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(expr.arguments()[2]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(expr.arguments()[3]));
    }
}

#pragma mark -
#pragma mark Declarations

TEST(WGSLParserTests, GlobalConstant)
{
    auto shader = parse("const x: i32 = 1;\n"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_EQ(shader->variables().size(), 1u);
    EXPECT_TRUE(shader->functions().isEmpty());

    {
        // const x: i32 = 1
        auto& constant = shader->variables()[0];
        EXPECT_EQ(constant.flavor(), WGSL::AST::VariableFlavor::Const);
        EXPECT_EQ(constant.name(), "x"_s);
        EXPECT_TRUE(constant.maybeTypeName());
        EXPECT_TRUE(is<WGSL::AST::NamedTypeName>(*constant.maybeTypeName()));
        auto& typeName = downcast<WGSL::AST::NamedTypeName>(*constant.maybeTypeName());
        EXPECT_EQ(typeName.name().id(), "i32"_s);
        EXPECT_TRUE(constant.maybeInitializer());
        EXPECT_TRUE(is<WGSL::AST::AbstractIntegerLiteral>(*constant.maybeInitializer()));
    }
}

TEST(WGSLParserTests, LocalConstant)
{
    auto shader = parse(
        "@vertex\n"
        "fn main() -> vec4<f32> {\n"
        "    const x = vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
        "    return x;\n"
        "}"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
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
        EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(func.maybeReturnType()));
        EXPECT_EQ(func.body().statements().size(), 2u);

        // const x = vec4<f32>(0.4, 0.4, 0.8, 1.0);
        EXPECT_TRUE(is<WGSL::AST::VariableStatement>(func.body().statements()[0]));
        auto& varStmt = downcast<WGSL::AST::VariableStatement>(func.body().statements()[0]);
        auto& constant = varStmt.variable();
        EXPECT_EQ(constant.flavor(), WGSL::AST::VariableFlavor::Const);
        EXPECT_EQ(constant.name(), "x"_s);
        EXPECT_EQ(constant.maybeTypeName(), nullptr);
        EXPECT_TRUE(constant.maybeInitializer());
        EXPECT_TRUE(is<WGSL::AST::CallExpression>(*constant.maybeInitializer()));
        auto& constantInitExpr = downcast<WGSL::AST::CallExpression>(*constant.maybeInitializer());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(constantInitExpr.target()));
        EXPECT_EQ(constantInitExpr.arguments().size(), 4u);
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(constantInitExpr.arguments()[0]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(constantInitExpr.arguments()[1]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(constantInitExpr.arguments()[2]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(constantInitExpr.arguments()[3]));

        // return x;
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(func.body().statements()[1]));
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[1]);
        EXPECT_TRUE(retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(retStmt.maybeExpression()));
        auto& retExpr = downcast<WGSL::AST::IdentifierExpression>(*retStmt.maybeExpression());
        EXPECT_EQ(retExpr.identifier(), "x"_s);
    }
}

TEST(WGSLParserTests, LocalLet)
{
    auto shader = parse(
        "@vertex\n"
        "fn main() -> vec4<f32> {\n"
        "    let x = vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
        "    return x;\n"
        "}"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
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
        EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(func.maybeReturnType()));
        EXPECT_EQ(func.body().statements().size(), 2u);

        // lex x = vec4<f32>(0.4, 0.4, 0.8, 1.0);
        EXPECT_TRUE(is<WGSL::AST::VariableStatement>(func.body().statements()[0]));
        auto& varStmt = downcast<WGSL::AST::VariableStatement>(func.body().statements()[0]);
        auto& let = varStmt.variable();
        EXPECT_EQ(let.flavor(), WGSL::AST::VariableFlavor::Let);
        EXPECT_EQ(let.name(), "x"_s);
        EXPECT_EQ(let.maybeTypeName(), nullptr);
        EXPECT_TRUE(let.maybeInitializer());
        EXPECT_TRUE(is<WGSL::AST::CallExpression>(*let.maybeInitializer()));
        auto& letInitExpr = downcast<WGSL::AST::CallExpression>(*let.maybeInitializer());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(letInitExpr.target()));
        EXPECT_EQ(letInitExpr.arguments().size(), 4u);
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(letInitExpr.arguments()[0]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(letInitExpr.arguments()[1]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(letInitExpr.arguments()[2]));
        EXPECT_TRUE(is<WGSL::AST::AbstractFloatLiteral>(letInitExpr.arguments()[3]));

        // return x;
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(func.body().statements()[1]));
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[1]);
        EXPECT_TRUE(retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(retStmt.maybeExpression()));
        auto& retExpr = downcast<WGSL::AST::IdentifierExpression>(*retStmt.maybeExpression());
        EXPECT_EQ(retExpr.identifier(), "x"_s);
    }
}

TEST(WGSLParserTests, GlobalOverride)
{
    auto shader = parse("override x: i32;\n"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_EQ(shader->variables().size(), 1u);
    EXPECT_TRUE(shader->functions().isEmpty());

    {
        // override x: i32
        auto& override = shader->variables()[0];
        EXPECT_TRUE(override.attributes().isEmpty());
        EXPECT_EQ(override.flavor(), WGSL::AST::VariableFlavor::Override);
        EXPECT_EQ(override.name(), "x"_s);
        EXPECT_TRUE(override.maybeTypeName());
        EXPECT_TRUE(is<WGSL::AST::NamedTypeName>(*override.maybeTypeName()));
        auto& typeName = downcast<WGSL::AST::NamedTypeName>(*override.maybeTypeName());
        EXPECT_EQ(typeName.name().id(), "i32"_s);
        EXPECT_EQ(override.maybeInitializer(), nullptr);
    }
}

TEST(WGSLParserTests, LocalVariable)
{
    auto shader = parse(
        "@vertex\n"
        "fn main() -> vec4<f32> {\n"
        "    var x = vec4<f32>(0.4, 0.4, 0.8, 1.0);\n"
        "    return x;\n"
        "}"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
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
        EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(func.maybeReturnType()));
        EXPECT_EQ(func.body().statements().size(), 2u);

        // var x = vec4<f32>(0.4, 0.4, 0.8, 1.0);
        EXPECT_TRUE(is<WGSL::AST::VariableStatement>(func.body().statements()[0]));
        auto& varStmt = downcast<WGSL::AST::VariableStatement>(func.body().statements()[0]);
        auto& var = varStmt.variable();
        EXPECT_EQ(var.flavor(), WGSL::AST::VariableFlavor::Var);
        EXPECT_EQ(var.name(), "x"_s);
        EXPECT_TRUE(var.attributes().isEmpty());
        EXPECT_EQ(var.maybeQualifier(), nullptr);
        EXPECT_EQ(var.maybeTypeName(), nullptr);
        EXPECT_TRUE(var.maybeInitializer());
        auto& varInitExpr = downcast<WGSL::AST::CallExpression>(*var.maybeInitializer());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(varInitExpr.target()));
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
    auto shader = parse("fn test() { return x[42i]; }"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
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
        EXPECT_TRUE(is<WGSL::AST::IndexAccessExpression>(retStmt.maybeExpression()));
        auto& arrayAccess = downcast<WGSL::AST::IndexAccessExpression>(*retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(arrayAccess.base()));
        auto& base = downcast<WGSL::AST::IdentifierExpression>(arrayAccess.base());
        EXPECT_EQ(base.identifier(), "x"_s);
        EXPECT_TRUE(is<WGSL::AST::Signed32Literal>(arrayAccess.index()));
        auto& index = downcast<WGSL::AST::Signed32Literal>(arrayAccess.index());
        EXPECT_EQ(index.value(), 42);
    }
}

TEST(WGSLParserTests, UnaryExpression)
{
    auto shader = parse(
        "fn negate(x: f32) -> f32 {\n"
        "    return -x;\n"
        "}"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
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
        EXPECT_TRUE(is<WGSL::AST::NamedTypeName>(func.maybeReturnType()));

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

static void testUnaryExpressionX(ASCIILiteral program, WGSL::AST::UnaryOperation op)
{
    EXPECT_EXPRESSION(expression, program);
    EXPECT_TRUE(is<WGSL::AST::UnaryExpression>(expression.get()));
    auto& unaryExpression = downcast<WGSL::AST::UnaryExpression>(expression.get());

    EXPECT_EQ(unaryExpression.operation(), op);
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(unaryExpression.expression()));
    auto& expr = downcast<WGSL::AST::IdentifierExpression>(unaryExpression.expression());
    EXPECT_EQ(expr.identifier(), "x"_s);
}

TEST(WGSLParserTests, UnaryExpression2)
{
    testUnaryExpressionX("&x"_s, WGSL::AST::UnaryOperation::AddressOf);
    testUnaryExpressionX("~x"_s, WGSL::AST::UnaryOperation::Complement);
    testUnaryExpressionX("*x"_s, WGSL::AST::UnaryOperation::Dereference);
    testUnaryExpressionX("-x"_s, WGSL::AST::UnaryOperation::Negate);
    testUnaryExpressionX("!x"_s, WGSL::AST::UnaryOperation::Not);
}

static void testBinaryExpressionXY(ASCIILiteral program, WGSL::AST::BinaryOperation op, const Vector<ASCIILiteral>& ids)
{
    EXPECT_EQ(ids.size(), 2u);

    EXPECT_EXPRESSION(expression, program);
    EXPECT_TRUE(is<WGSL::AST::BinaryExpression>(expression.get()));
    auto& binaryExpression = downcast<WGSL::AST::BinaryExpression>(expression.get());

    EXPECT_EQ(binaryExpression.operation(), op);
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(binaryExpression.leftExpression()));
    auto& lhs = downcast<WGSL::AST::IdentifierExpression>(binaryExpression.leftExpression());
    EXPECT_EQ(lhs.identifier(), ids[0]);
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(binaryExpression.rightExpression()));
    auto& rhs = downcast<WGSL::AST::IdentifierExpression>(binaryExpression.rightExpression());
    EXPECT_EQ(rhs.identifier(), ids[1]);
}

static void testBinaryExpressionXYZ(ASCIILiteral program, const Vector<WGSL::AST::BinaryOperation>& ops, const Vector<ASCIILiteral>& ids)
{
    EXPECT_EQ(ops.size(), 2u);
    EXPECT_EQ(ids.size(), 3u);

    EXPECT_EXPRESSION(expression, program);
    EXPECT_TRUE(is<WGSL::AST::BinaryExpression>(expression.get()));
    auto& binaryExpression = downcast<WGSL::AST::BinaryExpression>(expression.get());

    auto& complex = is<WGSL::AST::BinaryExpression>(binaryExpression.leftExpression()) ?
        binaryExpression.leftExpression() : binaryExpression.rightExpression();
    auto& simple = is<WGSL::AST::BinaryExpression>(binaryExpression.leftExpression()) ?
        binaryExpression.rightExpression() : binaryExpression.leftExpression();
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(simple));

    {
        auto& binaryExpression2 = downcast<WGSL::AST::BinaryExpression>(complex);

        EXPECT_EQ(binaryExpression2.operation(), ops[0]);
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(binaryExpression2.leftExpression()));
        auto& lhs = downcast<WGSL::AST::IdentifierExpression>(binaryExpression2.leftExpression());
        EXPECT_EQ(lhs.identifier(), ids[0]);
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(binaryExpression2.rightExpression()));
        auto& rhs = downcast<WGSL::AST::IdentifierExpression>(binaryExpression2.rightExpression());
        EXPECT_EQ(rhs.identifier(), ids[1]);
    }

    {
        EXPECT_EQ(binaryExpression.operation(), ops[1]);
        auto& rhs = downcast<WGSL::AST::IdentifierExpression>(simple);
        EXPECT_EQ(rhs.identifier(), ids[2]);
    }
}

TEST(WGSLParserTests, BinaryExpression)
{
    auto shader = parse(
        "fn add(x: f32, y: f32) -> f32 {\n"
        "    return x + y;\n"
        "}"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
    EXPECT_EQ(shader->functions().size(), 1u);

    {
        auto& func = shader->functions()[0];
        EXPECT_TRUE(func.attributes().isEmpty());

        // fn add(x: f32, y: f32) -> f32 {
        EXPECT_EQ(func.name(), "add"_s);
        EXPECT_EQ(func.parameters().size(), 2u);
        EXPECT_TRUE(func.returnAttributes().isEmpty());
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::NamedTypeName>(func.maybeReturnType()));

        EXPECT_EQ(func.body().statements().size(), 1u);

        // return x + y;
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(func.body().statements()[0]));
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0]);
        EXPECT_TRUE(retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::BinaryExpression>(retStmt.maybeExpression()));
        auto& binaryExpression = downcast<WGSL::AST::BinaryExpression>(*retStmt.maybeExpression());
        EXPECT_EQ(binaryExpression.operation(), WGSL::AST::BinaryOperation::Add);

        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(binaryExpression.leftExpression()));
        auto& lhs = downcast<WGSL::AST::IdentifierExpression>(binaryExpression.leftExpression());
        EXPECT_EQ(lhs.identifier(), "x"_s);

        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(binaryExpression.rightExpression()));
        auto& rhs = downcast<WGSL::AST::IdentifierExpression>(binaryExpression.rightExpression());
        EXPECT_EQ(rhs.identifier(), "y"_s);
    }
}

TEST(WGSLParserTests, BinaryExpression2)
{
    testBinaryExpressionXYZ("x - y + z"_s,
        { WGSL::AST::BinaryOperation::Subtract, WGSL::AST::BinaryOperation::Add },
        { "x"_s, "y"_s, "z"_s });

    testBinaryExpressionXYZ("x + y * z"_s,
        { WGSL::AST::BinaryOperation::Multiply, WGSL::AST::BinaryOperation::Add },
        { "y"_s, "z"_s, "x"_s });

    testBinaryExpressionXYZ("x / y + z"_s,
        { WGSL::AST::BinaryOperation::Divide, WGSL::AST::BinaryOperation::Add },
        { "x"_s, "y"_s, "z"_s });
}

// FIXME: Evaluate more complex expressions involving << and forced ().
TEST(WGSLParserTests, BinaryLeftShiftExpression)
{
    testBinaryExpressionXY("x << y"_s, WGSL::AST::BinaryOperation::LeftShift, { "x"_s, "y"_s });
}

// FIXME: Evaluate more complex expressions involving >> and forced ().
TEST(WGSLParserTests, BinaryRightShiftExpression)
{
    testBinaryExpressionXY("x >> y"_s, WGSL::AST::BinaryOperation::RightShift, { "x"_s, "y"_s });
}

// FIXME: Test failing cases, such as, "x & y | z"
TEST(WGSLParserTests, BinaryBitwiseExpression)
{
    testBinaryExpressionXYZ("x & y & z"_s,
        { WGSL::AST::BinaryOperation::And, WGSL::AST::BinaryOperation::And },
        { "x"_s, "y"_s, "z"_s });

    testBinaryExpressionXYZ("x | y | z"_s,
        { WGSL::AST::BinaryOperation::Or, WGSL::AST::BinaryOperation::Or },
        { "x"_s, "y"_s, "z"_s });

    testBinaryExpressionXYZ("x ^ y ^ z"_s,
        { WGSL::AST::BinaryOperation::Xor, WGSL::AST::BinaryOperation::Xor },
        { "x"_s, "y"_s, "z"_s });
}

// FIXME: Test failing cases, such as, "x < y > z"
TEST(WGSLParserTests, RelationalExpression)
{
    testBinaryExpressionXY("x == y"_s, WGSL::AST::BinaryOperation::Equal,        { "x"_s, "y"_s });
    testBinaryExpressionXY("x != y"_s, WGSL::AST::BinaryOperation::NotEqual,     { "x"_s, "y"_s });
    testBinaryExpressionXY("x > y"_s,  WGSL::AST::BinaryOperation::GreaterThan,  { "x"_s, "y"_s });
    testBinaryExpressionXY("x >= y"_s, WGSL::AST::BinaryOperation::GreaterEqual, { "x"_s, "y"_s });
    testBinaryExpressionXY("x < y"_s,  WGSL::AST::BinaryOperation::LessThan,     { "x"_s, "y"_s });
    testBinaryExpressionXY("x <= y"_s, WGSL::AST::BinaryOperation::LessEqual,    { "x"_s, "y"_s });
}

// FIXME: Test failing cases, such as, "x && y || z"
TEST(WGSLParserTest, ShortCircuitAndExpression)
{
    testBinaryExpressionXY("x && y"_s, WGSL::AST::BinaryOperation::ShortCircuitAnd, { "x"_s, "y"_s });
    testBinaryExpressionXYZ("x && y && z"_s,
        { WGSL::AST::BinaryOperation::ShortCircuitAnd, WGSL::AST::BinaryOperation::ShortCircuitAnd },
        { "x"_s, "y"_s, "z"_s });
}

// FIXME: Test failing cases, such as, "x || y && z"
TEST(WGSLParserTest, ShortCircuitOrExpression)
{
    testBinaryExpressionXY("x || y"_s, WGSL::AST::BinaryOperation::ShortCircuitOr, { "x"_s, "y"_s });
    testBinaryExpressionXYZ("x || y || z"_s,
        { WGSL::AST::BinaryOperation::ShortCircuitOr, WGSL::AST::BinaryOperation::ShortCircuitOr },
        { "x"_s, "y"_s, "z"_s });
}

#pragma mark -
#pragma mark WebGPU Example Shaders

TEST(WGSLParserTests, TriangleVert)
{
    auto shader = parse(
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
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
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
        auto& var = varStmt.variable();
        EXPECT_EQ(var.name(), "pos"_s);
        EXPECT_TRUE(var.attributes().isEmpty());
        EXPECT_EQ(var.maybeQualifier(), nullptr);
        EXPECT_EQ(var.maybeTypeName(), nullptr);
        EXPECT_TRUE(var.maybeInitializer());
        EXPECT_TRUE(is<WGSL::AST::CallExpression>(var.maybeInitializer()));
        auto& varInitExpr = downcast<WGSL::AST::CallExpression>(*var.maybeInitializer());
        EXPECT_TRUE(is<WGSL::AST::ArrayTypeName>(varInitExpr.target()));
        auto& varInitArrayType = downcast<WGSL::AST::ArrayTypeName>(varInitExpr.target());
        EXPECT_TRUE(varInitArrayType.maybeElementType());
        checkVec2F32Type(*varInitArrayType.maybeElementType());
        EXPECT_TRUE(varInitArrayType.maybeElementCount());
        EXPECT_TRUE(is<WGSL::AST::AbstractIntegerLiteral>(varInitArrayType.maybeElementCount()));
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
        EXPECT_TRUE(is<WGSL::AST::CallExpression>(retStmt.maybeExpression()));
        auto& expr = downcast<WGSL::AST::CallExpression>(*retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(expr.target()));
    }
}

TEST(WGSLParserTests, VectorWithOutComponentType)
{
    auto shader = parse(
        "@vertex\n"
        "fn main() {\n"
        "    x = vec4(1.0);\n"
        "}\n"_s);

    EXPECT_SHADER(shader);
    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
    EXPECT_EQ(shader->functions().size(), 1u);

    auto& func = shader->functions()[0];
    EXPECT_EQ(func.body().statements().size(), 1u);

    // x = vec4(1.0);
    EXPECT_TRUE(is<WGSL::AST::AssignmentStatement>(func.body().statements()[0]));
    auto& stmt = downcast<WGSL::AST::AssignmentStatement>(func.body().statements()[0]);
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(stmt.lhs()));
    auto& id = downcast<WGSL::AST::IdentifierExpression>(stmt.lhs());
    EXPECT_EQ(id.identifier(), "x"_s);

    EXPECT_TRUE(is<WGSL::AST::CallExpression>(stmt.rhs()));
    auto& constructor = downcast<WGSL::AST::CallExpression>(stmt.rhs());
    EXPECT_TRUE(is<WGSL::AST::NamedTypeName>(constructor.target()));
    auto& vec4 = downcast<WGSL::AST::NamedTypeName>(constructor.target());
    EXPECT_EQ(vec4.name().id(), "vec4"_s);
}

TEST(WGSLParserTests, VectorWithComponentType)
{
    auto shader = parse(
        "@vertex\n"
        "fn main() {\n"
        "    x = vec4<f32>(1.0);\n"
        "}\n"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
    EXPECT_EQ(shader->functions().size(), 1u);

    auto& func = shader->functions()[0];
    EXPECT_EQ(func.body().statements().size(), 1u);

    // x = vec4<f32>(1.0);
    EXPECT_TRUE(is<WGSL::AST::AssignmentStatement>(func.body().statements()[0]));
    auto& stmt = downcast<WGSL::AST::AssignmentStatement>(func.body().statements()[0]);
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(stmt.lhs()));
    auto& id = downcast<WGSL::AST::IdentifierExpression>(stmt.lhs());
    EXPECT_EQ(id.identifier(), "x"_s);

    EXPECT_TRUE(is<WGSL::AST::CallExpression>(stmt.rhs()));
    auto& constructor = downcast<WGSL::AST::CallExpression>(stmt.rhs());
    EXPECT_TRUE(is<WGSL::AST::ParameterizedTypeName>(constructor.target()));
    checkVec4F32Type(constructor.target());
}

TEST(WGSLParserTests, RedFrag)
{
    auto shader = parse(
        "@fragment\n"
        "fn main() -> @location(0) vec4<f32> {\n"
        "    return vec4<f32>(1.0, 0.0, 0.0, 1.0);\n"
        "}\n"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_TRUE(shader->structures().isEmpty());
    EXPECT_TRUE(shader->variables().isEmpty());
    EXPECT_EQ(shader->functions().size(), 1u);
}

}
