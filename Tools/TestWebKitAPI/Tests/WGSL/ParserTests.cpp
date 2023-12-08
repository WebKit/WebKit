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
#include "ASTBinaryExpression.h"
#include "ASTCompoundStatement.h"
#include "Parser.h"
#include "ParserPrivate.h"

#include "AST.h"
#include "Lexer.h"
#include "TestWGSLAPI.h"
#include "WGSLShaderModule.h"

#include <wtf/Assertions.h>
#include <wtf/DataLog.h>

static void checkBuiltin(WGSL::AST::Attribute& attr, WGSL::Builtin builtin)
{
    EXPECT_TRUE(is<WGSL::AST::BuiltinAttribute>(attr));
    EXPECT_EQ(downcast<WGSL::AST::BuiltinAttribute>(attr).builtin(), builtin);
}

static void checkIntLiteral(WGSL::AST::Expression& node, int32_t value)
{
    EXPECT_TRUE(is<WGSL::AST::AbstractIntegerLiteral>(node));
    auto& intLiteral = downcast<WGSL::AST::AbstractIntegerLiteral>(node);
    EXPECT_EQ(intLiteral.value(), value);
}

static void checkVecType(WGSL::AST::Expression& type, ASCIILiteral vecType, ASCIILiteral paramTypeName)
{
    EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(type));
    auto& parameterizedType = downcast<WGSL::AST::ElaboratedTypeExpression>(type);
    EXPECT_EQ(parameterizedType.base(), vecType);
    EXPECT_EQ(parameterizedType.arguments().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(parameterizedType.arguments()[0]));
    EXPECT_EQ(downcast<WGSL::AST::IdentifierExpression>(parameterizedType.arguments()[0]).identifier(), paramTypeName);
}

static void checkVec2F32Type(WGSL::AST::Expression& type)
{
    checkVecType(type, "vec2"_s, "f32"_s);
}

static void checkVec4F32Type(WGSL::AST::Expression& type)
{
    checkVecType(type, "vec4"_s, "f32"_s);
}

namespace TestWGSLAPI {

inline Expected<WGSL::ShaderModule, WGSL::FailedCheck> parse(const String& wgsl)
{
    WGSL::ShaderModule shaderModule(wgsl, { 8 });
    auto maybeError = WGSL::parse(shaderModule);
    if (maybeError.has_value())
        return makeUnexpected(*maybeError);
    return { WTFMove(shaderModule) };
}

struct StructAttributeTest {
    enum Kind {
        Align,
        Size,
    };
    Kind kind;
    size_t value;
};

std::optional<unsigned> extractInteger(WGSL::AST::Expression& expression)
{
    switch (expression.kind()) {
    case WGSL::AST::NodeKind::AbstractIntegerLiteral:
        return { static_cast<unsigned>(downcast<WGSL::AST::AbstractIntegerLiteral>(expression).value()) };
    case WGSL::AST::NodeKind::Unsigned32Literal:
        return { static_cast<unsigned>(downcast<WGSL::AST::Unsigned32Literal>(expression).value()) };
    case WGSL::AST::NodeKind::Signed32Literal:
        return { static_cast<unsigned>(downcast<WGSL::AST::Signed32Literal>(expression).value()) };
    default:
        return std::nullopt;
    }
}


static void testStruct(ASCIILiteral program, const Vector<String>& fieldNames, const Vector<String>& typeNames, const Vector<Vector<StructAttributeTest>>& attributeTests = { })
{
    ASSERT(fieldNames.size() == typeNames.size());

    auto shader = parse(program);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Structure>(shader->declarations()[0]));
    auto& str = downcast<WGSL::AST::Structure>(shader->declarations()[0]);
    EXPECT_EQ(str.name(), "B"_s);
    EXPECT_TRUE(str.attributes().isEmpty());

    EXPECT_EQ(str.members().size(), fieldNames.size());
    for (unsigned i = 0; i < fieldNames.size(); ++i) {
        auto& attributes = str.members()[i].attributes();
        if (!attributeTests.size())
            EXPECT_TRUE(attributes.isEmpty());
        else {
            const Vector<StructAttributeTest>& tests = attributeTests[i];
            EXPECT_EQ(tests.size(), attributes.size());
            for (unsigned j = 0; j < tests.size(); ++j) {
                auto& test = tests[j];
                auto& attribute = attributes[j];
                switch (test.kind) {
                case StructAttributeTest::Align: {
                    EXPECT_TRUE(is<WGSL::AST::AlignAttribute>(attribute));
                    auto alignment = extractInteger(downcast<WGSL::AST::AlignAttribute>(attribute).alignment());
                    EXPECT_TRUE(alignment.has_value());
                    EXPECT_EQ(*alignment, test.value);
                    break;
                }
                case StructAttributeTest::Size: {
                    EXPECT_TRUE(is<WGSL::AST::SizeAttribute>(attribute));
                    auto size = extractInteger(downcast<WGSL::AST::SizeAttribute>(attribute).size());
                    EXPECT_TRUE(size.has_value());
                    EXPECT_EQ(*size, test.value);
                    break;
                }
                }
            }
        }
        EXPECT_EQ(str.members()[i].name(), fieldNames[i]);
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(str.members()[i].type()));
        auto& memberType = downcast<WGSL::AST::IdentifierExpression>(str.members()[i].type());
        EXPECT_EQ(memberType.identifier(), typeNames[i]);
    }
}

TEST(WGSLParserTests, SourceLifecycle)
{
    auto shader = ([&]() {
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
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Variable>(shader->declarations()[0]));
    auto& var = downcast<WGSL::AST::Variable>(shader->declarations()[0]);
    EXPECT_EQ(var.attributes().size(), 2u);
    EXPECT_TRUE(is<WGSL::AST::GroupAttribute>(var.attributes()[0]));
    auto group = extractInteger(downcast<WGSL::AST::GroupAttribute>(var.attributes()[0]).group());
    EXPECT_TRUE(group.has_value());
    EXPECT_EQ(*group, 0u);
    EXPECT_TRUE(is<WGSL::AST::BindingAttribute>(var.attributes()[1]));
    auto binding = extractInteger(downcast<WGSL::AST::BindingAttribute>(var.attributes()[1]).binding());
    EXPECT_TRUE(binding.has_value());
    EXPECT_EQ(*binding, 0u);
    EXPECT_EQ(var.name(), "x"_s);
    EXPECT_TRUE(var.maybeQualifier());
    EXPECT_EQ(var.maybeQualifier()->addressSpace(), WGSL::AddressSpace::Storage);
    EXPECT_EQ(var.maybeQualifier()->accessMode(), WGSL::AccessMode::ReadWrite);
    EXPECT_TRUE(var.maybeTypeName());
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(var.maybeTypeName()));
    auto& namedType = downcast<WGSL::AST::IdentifierExpression>(*var.maybeTypeName());
    EXPECT_EQ(namedType.identifier(), "B"_s);
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
        "    @size(8) a: i32,\n"
        "    @align(8) b: f32,\n"
        "}"_s, { "a"_s, "b"_s }, { "i32"_s, "f32"_s }, { { { StructAttributeTest::Size, 8 } }, { { StructAttributeTest::Align, 8 } } });
}

TEST(WGSLParserTests, GlobalVariable)
{
    auto shader = parse(
        "@group(0) @binding(0)\n"
        "var<storage, read_write> x: B;\n"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Variable>(shader->declarations()[0]));
    auto& var = downcast<WGSL::AST::Variable>(shader->declarations()[0]);
    EXPECT_EQ(var.attributes().size(), 2u);
    EXPECT_TRUE(is<WGSL::AST::GroupAttribute>(var.attributes()[0]));
    auto& groupAttribute = downcast<WGSL::AST::GroupAttribute>(var.attributes()[0]);
    auto group = extractInteger(groupAttribute.group());
    EXPECT_TRUE(group.has_value());
    EXPECT_EQ(*group, 0u);
    EXPECT_TRUE(is<WGSL::AST::BindingAttribute>(var.attributes()[1]));
    auto& bindingAttribute = downcast<WGSL::AST::BindingAttribute>(var.attributes()[1]);
    auto binding = extractInteger(bindingAttribute.binding());
    EXPECT_TRUE(binding.has_value());
    EXPECT_EQ(*binding, 0u);
    EXPECT_EQ(var.name(), "x"_s);
    EXPECT_TRUE(var.maybeQualifier());
    EXPECT_EQ(var.maybeQualifier()->addressSpace(), WGSL::AddressSpace::Storage);
    EXPECT_EQ(var.maybeQualifier()->accessMode(), WGSL::AccessMode::ReadWrite);
    EXPECT_TRUE(var.maybeTypeName());
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(var.maybeTypeName()));
    auto& namedType = downcast<WGSL::AST::IdentifierExpression>(*var.maybeTypeName());
    EXPECT_EQ(namedType.identifier(), "B"_s);
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
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
    auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);
    EXPECT_EQ(func.attributes().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::StageAttribute>(func.attributes()[0]));
    EXPECT_EQ(downcast<WGSL::AST::StageAttribute>(func.attributes()[0]).stage(), WGSL::ShaderStage::Compute);
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
    EXPECT_EQ(shader->declarations().size(), 2u);

    {
        EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
        auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);
        EXPECT_EQ(func.attributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::StageAttribute>(func.attributes()[0]));
        EXPECT_EQ(downcast<WGSL::AST::StageAttribute>(func.attributes()[0]).stage(), WGSL::ShaderStage::Vertex);
        EXPECT_EQ(func.name(), "vertexShader"_s);
        EXPECT_EQ(func.parameters().size(), 1u);
        EXPECT_EQ(func.parameters()[0].name(), "x"_s);
        EXPECT_EQ(func.parameters()[0].attributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::LocationAttribute>(func.parameters()[0].attributes()[0]));
        auto& locationAttribute = downcast<WGSL::AST::LocationAttribute>(func.parameters()[0].attributes()[0]);
        auto location = extractInteger(locationAttribute.location());
        EXPECT_TRUE(location.has_value());
        EXPECT_EQ(*location, 0u);
        EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(func.parameters()[0].typeName()));
        auto& paramType = downcast<WGSL::AST::ElaboratedTypeExpression>(func.parameters()[0].typeName());
        EXPECT_EQ(paramType.base(), "vec4"_s);
        EXPECT_EQ(paramType.arguments().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(paramType.arguments()[0]));
        EXPECT_EQ(downcast<WGSL::AST::IdentifierExpression>(paramType.arguments()[0]).identifier(), "f32"_s);
        EXPECT_EQ(func.returnAttributes().size(), 1u);
        checkBuiltin(func.returnAttributes()[0], WGSL::Builtin::Position);
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(func.maybeReturnType()));
        EXPECT_EQ(func.body().statements().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(func.body().statements()[0]));
        auto& stmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0]);
        EXPECT_TRUE(stmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(stmt.maybeExpression()));
    }

    {
        EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[1]));
        auto& func = downcast<WGSL::AST::Function>(shader->declarations()[1]);
        EXPECT_EQ(func.attributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::StageAttribute>(func.attributes()[0]));
        EXPECT_EQ(downcast<WGSL::AST::StageAttribute>(func.attributes()[0]).stage(), WGSL::ShaderStage::Fragment);
        EXPECT_EQ(func.name(), "fragmentShader"_s);
        EXPECT_TRUE(func.parameters().isEmpty());
        EXPECT_EQ(func.returnAttributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::LocationAttribute>(func.returnAttributes()[0]));
        auto& locationAttribute = downcast<WGSL::AST::LocationAttribute>(func.returnAttributes()[0]);
        auto location = extractInteger(locationAttribute.location());
        EXPECT_TRUE(location.has_value());
        EXPECT_EQ(*location, 0u);
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(func.maybeReturnType()));
        EXPECT_EQ(func.body().statements().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(func.body().statements()[0]));
        auto& stmt = downcast<WGSL::AST::ReturnStatement>(func.body().statements()[0]);
        EXPECT_TRUE(stmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::CallExpression>(stmt.maybeExpression()));
        auto& expr = downcast<WGSL::AST::CallExpression>(*stmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(expr.target()));
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
    EXPECT_EQ(shader->declarations().size(), 1u);

    {
        // const x: i32 = 1
        EXPECT_TRUE(is<WGSL::AST::Variable>(shader->declarations()[0]));
        auto& constant = downcast<WGSL::AST::Variable>(shader->declarations()[0]);
        EXPECT_EQ(constant.flavor(), WGSL::AST::VariableFlavor::Const);
        EXPECT_EQ(constant.name(), "x"_s);
        EXPECT_TRUE(constant.maybeTypeName());
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(*constant.maybeTypeName()));
        auto& typeName = downcast<WGSL::AST::IdentifierExpression>(*constant.maybeTypeName());
        EXPECT_EQ(typeName.identifier().id(), "i32"_s);
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
    EXPECT_EQ(shader->declarations().size(), 1u);

    {
        EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
        auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);
        // @vertex
        EXPECT_EQ(func.attributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::StageAttribute>(func.attributes()[0]));
        EXPECT_EQ(downcast<WGSL::AST::StageAttribute>(func.attributes()[0]).stage(), WGSL::ShaderStage::Vertex);

        // fn main() -> vec4<f32> {
        EXPECT_EQ(func.name(), "main"_s);
        EXPECT_TRUE(func.parameters().isEmpty());
        EXPECT_TRUE(func.returnAttributes().isEmpty());
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(func.maybeReturnType()));
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
        EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(constantInitExpr.target()));
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
    EXPECT_EQ(shader->declarations().size(), 1u);

    {
        EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
        auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);
        // @vertex
        EXPECT_EQ(func.attributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::StageAttribute>(func.attributes()[0]));
        EXPECT_EQ(downcast<WGSL::AST::StageAttribute>(func.attributes()[0]).stage(), WGSL::ShaderStage::Vertex);

        // fn main() -> vec4<f32> {
        EXPECT_EQ(func.name(), "main"_s);
        EXPECT_TRUE(func.parameters().isEmpty());
        EXPECT_TRUE(func.returnAttributes().isEmpty());
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(func.maybeReturnType()));
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
        EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(letInitExpr.target()));
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
    EXPECT_EQ(shader->declarations().size(), 1u);

    {
        // override x: i32
        EXPECT_TRUE(is<WGSL::AST::Variable>(shader->declarations()[0]));
        auto& override = downcast<WGSL::AST::Variable>(shader->declarations()[0]);
        EXPECT_TRUE(override.attributes().isEmpty());
        EXPECT_EQ(override.flavor(), WGSL::AST::VariableFlavor::Override);
        EXPECT_EQ(override.name(), "x"_s);
        EXPECT_TRUE(override.maybeTypeName());
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(*override.maybeTypeName()));
        auto& typeName = downcast<WGSL::AST::IdentifierExpression>(*override.maybeTypeName());
        EXPECT_EQ(typeName.identifier().id(), "i32"_s);
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
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
    auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);

    {
        // @vertex
        EXPECT_EQ(func.attributes().size(), 1u);
        EXPECT_TRUE(is<WGSL::AST::StageAttribute>(func.attributes()[0]));
        EXPECT_EQ(downcast<WGSL::AST::StageAttribute>(func.attributes()[0]).stage(), WGSL::ShaderStage::Vertex);

        // fn main() -> vec4<f32> {
        EXPECT_EQ(func.name(), "main"_s);
        EXPECT_TRUE(func.parameters().isEmpty());
        EXPECT_TRUE(func.returnAttributes().isEmpty());
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(func.maybeReturnType()));
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
        EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(varInitExpr.target()));
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
    EXPECT_EQ(shader->declarations().size(), 1u);

    {
        EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
        auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);
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
    EXPECT_EQ(shader->declarations().size(), 1u);

    {
        EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
        auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);
        // @vertex
        EXPECT_TRUE(func.attributes().isEmpty());

        // fn negate(x: f32) -> f32 {
        EXPECT_EQ(func.name(), "negate"_s);
        EXPECT_EQ(func.parameters().size(), 1u);
        EXPECT_TRUE(func.returnAttributes().isEmpty());
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(func.maybeReturnType()));

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
    auto source = makeString(
        "fn f() {\n"_s,
        "_ = "_s, program, ";"_s,
        "}\n"_s
    );
    auto shader = parse(source);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
    auto& function = downcast<WGSL::AST::Function>(shader->declarations()[0]);

    EXPECT_EQ(function.body().statements().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::PhonyAssignmentStatement>(function.body().statements()[0]));
    auto& statement = downcast<WGSL::AST::PhonyAssignmentStatement>(function.body().statements()[0]);

    EXPECT_TRUE(is<WGSL::AST::UnaryExpression>(statement.rhs()));
    auto& unaryExpression = downcast<WGSL::AST::UnaryExpression>(statement.rhs());

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

    auto source = makeString(
        "fn f() {\n"_s,
        "_ = "_s, program, ";"_s,
        "}\n"_s
    );
    auto shader = parse(source);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
    auto& function = downcast<WGSL::AST::Function>(shader->declarations()[0]);

    EXPECT_EQ(function.body().statements().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::PhonyAssignmentStatement>(function.body().statements()[0]));
    auto& statement = downcast<WGSL::AST::PhonyAssignmentStatement>(function.body().statements()[0]);
    EXPECT_TRUE(is<WGSL::AST::BinaryExpression>(statement.rhs()));
    auto& binaryExpression = downcast<WGSL::AST::BinaryExpression>(statement.rhs());

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

    auto source = makeString(
        "fn f() {\n"_s,
        "_ = "_s, program, ";"_s,
        "}\n"_s
    );
    auto shader = parse(source);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
    auto& function = downcast<WGSL::AST::Function>(shader->declarations()[0]);

    EXPECT_EQ(function.body().statements().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::PhonyAssignmentStatement>(function.body().statements()[0]));
    auto& statement = downcast<WGSL::AST::PhonyAssignmentStatement>(function.body().statements()[0]);

    EXPECT_TRUE(is<WGSL::AST::BinaryExpression>(statement.rhs()));
    auto& binaryExpression = downcast<WGSL::AST::BinaryExpression>(statement.rhs());

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
    EXPECT_EQ(shader->declarations().size(), 1u);

    {
        EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
        auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);
        EXPECT_TRUE(func.attributes().isEmpty());

        // fn add(x: f32, y: f32) -> f32 {
        EXPECT_EQ(func.name(), "add"_s);
        EXPECT_EQ(func.parameters().size(), 2u);
        EXPECT_TRUE(func.returnAttributes().isEmpty());
        EXPECT_TRUE(func.maybeReturnType());
        EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(func.maybeReturnType()));

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
    testBinaryExpressionXY("x < y"_s, WGSL::AST::BinaryOperation::LessThan, { "x"_s, "y"_s });
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
#pragma mark Statements

TEST(WGSLParserTest, IfStatement)
{
    auto shader = parse(
        R"(fn foo() {
               if true {
                   return;
               } else if false {
                   return;
               } else {
                   return;
               }
        })"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_TRUE(shader->directives().isEmpty());
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));

    auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);
    EXPECT_GE(func.body().statements().size(), 1u);
    auto& stmt = func.body().statements()[0];
    EXPECT_TRUE(is<WGSL::AST::IfStatement>(stmt));
    auto& ifStmt = downcast<WGSL::AST::IfStatement>(stmt);
    auto& testExpr = ifStmt.test();
    EXPECT_TRUE(is<WGSL::AST::BoolLiteral>(testExpr));
    auto& trueBody = ifStmt.trueBody();
    EXPECT_TRUE(is<WGSL::AST::CompoundStatement>(trueBody));
    EXPECT_TRUE(is<WGSL::AST::IfStatement>(ifStmt.maybeFalseBody()));
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
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
    auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);

    // fn main(...)
    {
        // @vertex
        EXPECT_EQ(func.attributes().size(), 1u);

        // fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
        EXPECT_EQ(func.name(), "main"_s);
        EXPECT_EQ(func.parameters().size(), 1u);
        EXPECT_EQ(func.returnAttributes().size(), 1u);
        EXPECT_TRUE(func.maybeReturnType());
        checkVec4F32Type(*func.maybeReturnType());
        EXPECT_EQ(func.returnAttributes().size(), 1u);
        checkBuiltin(func.returnAttributes()[0], WGSL::Builtin::Position);
    }

    // var pos = array<vec2<f32>, 3>(...);
    {
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
        EXPECT_TRUE(is<WGSL::AST::ArrayTypeExpression>(varInitExpr.target()));
        auto& varInitArrayType = downcast<WGSL::AST::ArrayTypeExpression>(varInitExpr.target());
        EXPECT_TRUE(varInitArrayType.maybeElementType());
        checkVec2F32Type(*varInitArrayType.maybeElementType());
        EXPECT_TRUE(varInitArrayType.maybeElementCount());
        EXPECT_TRUE(is<WGSL::AST::AbstractIntegerLiteral>(varInitArrayType.maybeElementCount()));
        checkIntLiteral(*varInitArrayType.maybeElementCount(), 3);
    }

    // return vec4<f32>(..);
    {
        EXPECT_GE(func.body().statements().size(), 1u);
        auto& stmt = func.body().statements()[1];
        EXPECT_TRUE(is<WGSL::AST::ReturnStatement>(stmt));
        auto& retStmt = downcast<WGSL::AST::ReturnStatement>(stmt);
        EXPECT_TRUE(retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::CallExpression>(retStmt.maybeExpression()));
        auto& expr = downcast<WGSL::AST::CallExpression>(*retStmt.maybeExpression());
        EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(expr.target()));
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
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
    auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);
    EXPECT_EQ(func.body().statements().size(), 1u);

    // x = vec4(1.0);
    EXPECT_TRUE(is<WGSL::AST::AssignmentStatement>(func.body().statements()[0]));
    auto& stmt = downcast<WGSL::AST::AssignmentStatement>(func.body().statements()[0]);
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(stmt.lhs()));
    auto& id = downcast<WGSL::AST::IdentifierExpression>(stmt.lhs());
    EXPECT_EQ(id.identifier(), "x"_s);

    EXPECT_TRUE(is<WGSL::AST::CallExpression>(stmt.rhs()));
    auto& constructor = downcast<WGSL::AST::CallExpression>(stmt.rhs());
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(constructor.target()));
    auto& vec4 = downcast<WGSL::AST::IdentifierExpression>(constructor.target());
    EXPECT_EQ(vec4.identifier().id(), "vec4"_s);
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
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
    auto& func = downcast<WGSL::AST::Function>(shader->declarations()[0]);
    EXPECT_EQ(func.body().statements().size(), 1u);

    // x = vec4<f32>(1.0);
    EXPECT_TRUE(is<WGSL::AST::AssignmentStatement>(func.body().statements()[0]));
    auto& stmt = downcast<WGSL::AST::AssignmentStatement>(func.body().statements()[0]);
    EXPECT_TRUE(is<WGSL::AST::IdentifierExpression>(stmt.lhs()));
    auto& id = downcast<WGSL::AST::IdentifierExpression>(stmt.lhs());
    EXPECT_EQ(id.identifier(), "x"_s);

    EXPECT_TRUE(is<WGSL::AST::CallExpression>(stmt.rhs()));
    auto& constructor = downcast<WGSL::AST::CallExpression>(stmt.rhs());
    EXPECT_TRUE(is<WGSL::AST::ElaboratedTypeExpression>(constructor.target()));
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
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
}

TEST(WGSLParserTests, TrailingSemicolon)
{
    auto shader = parse(
        "fn f(){\n"
        "  ;;;\n"
        "}\n"
        ";;;\n"_s);

    EXPECT_SHADER(shader);
    EXPECT_TRUE(shader.has_value());
    EXPECT_EQ(shader->declarations().size(), 1u);
    EXPECT_TRUE(is<WGSL::AST::Function>(shader->declarations()[0]));
}

TEST(WGSLParserTests, GlobalVarWithoutTypeOrInitializer)
{
    auto shader = parse("var x;"_s);
    EXPECT_FALSE(shader.has_value());
    EXPECT_EQ(shader.error().errors.first().message(), "var declaration requires a type or initializer"_s);
}

TEST(WGSLParserTests, GlobalConstWithoutTypeOrInitializer)
{
    auto shader = parse("const x;"_s);
    EXPECT_FALSE(shader.has_value());
    EXPECT_EQ(shader.error().errors.first().message(), "Expected a =, but got a ;"_s);
}

TEST(WGSLParserTests, GlobalConstWithoutInitializer)
{
    auto shader = parse("const x: i32;"_s);
    EXPECT_FALSE(shader.has_value());
    EXPECT_EQ(shader.error().errors.first().message(), "Expected a =, but got a ;"_s);
}

TEST(WGSLParserTests, GlobalOverrideWithoutTypeOrInitializer)
{
    auto shader = parse("override x;"_s);
    EXPECT_FALSE(shader.has_value());
    EXPECT_EQ(shader.error().errors.first().message(), "override declaration requires a type or initializer"_s);
}

TEST(WGSLParserTests, LocalVarWithoutTypeOrInitializer)
{
    auto shader = parse(
        "fn f() {\n"
        "   var x;\n"
        "}"_s);
    EXPECT_FALSE(shader.has_value());
    EXPECT_EQ(shader.error().errors.first().message(), "var declaration requires a type or initializer"_s);
}

TEST(WGSLParserTests, LocalLetWithoutTypeOrInitializer)
{
    auto shader = parse(
        "fn f() {\n"
        "   let x;\n"
        "}"_s);
    EXPECT_FALSE(shader.has_value());
    EXPECT_EQ(shader.error().errors.first().message(), "Expected a =, but got a ;"_s);
}

TEST(WGSLParserTests, LocalLetWithoutInitializer)
{
    auto shader = parse(
        "fn f() {\n"
        "   let x: i32;\n"
        "}"_s);
    EXPECT_FALSE(shader.has_value());
    EXPECT_EQ(shader.error().errors.first().message(), "Expected a =, but got a ;"_s);
}

TEST(WGSLParserTests, LocalConstWithoutTypeOrInitializer)
{
    auto shader = parse(
        "fn f() {\n"
        "   const x;\n"
        "}"_s);
    EXPECT_FALSE(shader.has_value());
    EXPECT_EQ(shader.error().errors.first().message(), "Expected a =, but got a ;"_s);
}

TEST(WGSLParserTests, LocalConstWithoutInitializer)
{
    auto shader = parse(
        "fn f() {\n"
        "   const x: i32;\n"
        "}"_s);
    EXPECT_FALSE(shader.has_value());
    EXPECT_EQ(shader.error().errors.first().message(), "Expected a =, but got a ;"_s);
}

}
