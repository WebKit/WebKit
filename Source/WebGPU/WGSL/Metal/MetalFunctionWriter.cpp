/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "MetalFunctionWriter.h"

#include "API.h"
#include "AST.h"
#include "ASTStringDumper.h"
#include "ASTVisitor.h"
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/SetForScope.h>
#include <wtf/text/StringBuilder.h>

namespace WGSL {

namespace Metal {

class FunctionDefinitionWriter : public AST::Visitor {
public:
    FunctionDefinitionWriter(AST::ShaderModule& shaderModule, StringBuilder& stringBuilder)
        : m_stringBuilder(stringBuilder)
        , m_shaderModule(shaderModule)
    {
    }

    virtual ~FunctionDefinitionWriter() = default;

    using AST::Visitor::visit;

    void visit(AST::ShaderModule&) override;

    void visit(AST::Attribute&) override;
    void visit(AST::BuiltinAttribute&) override;
    void visit(AST::LocationAttribute&) override;
    void visit(AST::StageAttribute&) override;
    void visit(AST::GroupAttribute&) override;
    void visit(AST::BindingAttribute&) override;

    void visit(AST::Function&) override;
    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;

    void visit(AST::AbstractFloatLiteral&) override;
    void visit(AST::AbstractIntegerLiteral&) override;
    void visit(AST::BinaryExpression&) override;
    void visit(AST::CallExpression&) override;
    void visit(AST::Expression&) override;
    void visit(AST::FieldAccessExpression&) override;
    void visit(AST::Float32Literal&) override;
    void visit(AST::IdentifierExpression&) override;
    void visit(AST::IndexAccessExpression&) override;
    void visit(AST::PointerDereferenceExpression&) override;
    void visit(AST::Signed32Literal&) override;
    void visit(AST::Unsigned32Literal&) override;
    void visit(AST::UnaryExpression&) override;

    void visit(AST::Statement&) override;
    void visit(AST::AssignmentStatement&) override;
    void visit(AST::ReturnStatement&) override;

    void visit(AST::ArrayTypeName&) override;
    void visit(AST::NamedTypeName&) override;
    void visit(AST::ParameterizedTypeName&) override;
    void visit(AST::ReferenceTypeName&) override;
    void visit(AST::StructTypeName&) override;

    void visit(AST::ParameterValue&) override;
    void visitArgumentBufferParameter(AST::ParameterValue&);

private:
    StringBuilder& m_stringBuilder;
    AST::ShaderModule& m_shaderModule;
    Indentation<4> m_indent { 0 };
    std::optional<AST::StructureRole> m_structRole;
    std::optional<AST::StageAttribute::Stage> m_entryPointStage;
    std::optional<String> m_suffix;
};

void FunctionDefinitionWriter::visit(AST::ShaderModule& shaderModule)
{
    AST::Visitor::visit(shaderModule);
}

void FunctionDefinitionWriter::visit(AST::Function& functionDefinition)
{
    // FIXME: visit return attributes
    for (auto& attribute : functionDefinition.attributes()) {
        checkErrorAndVisit(attribute);
        m_stringBuilder.append(" ");
    }

    if (functionDefinition.maybeReturnType())
        checkErrorAndVisit(*functionDefinition.maybeReturnType());
    else
        m_stringBuilder.append("void");

    m_stringBuilder.append(" ", functionDefinition.name(), "(");
    bool first = true;
    for (auto& parameter : functionDefinition.parameters()) {
        if (!first)
            m_stringBuilder.append(", ");
        switch (parameter.role()) {
        case AST::ParameterRole::UserDefined:
            checkErrorAndVisit(parameter);
            break;
        case AST::ParameterRole::StageIn:
            checkErrorAndVisit(parameter);
            m_stringBuilder.append(" [[stage_in]]");
            break;
        case AST::ParameterRole::BindGroup:
            visitArgumentBufferParameter(parameter);
            break;
        }
        first = false;
    }
    // Clear the flag set while serializing StageAttribute
    m_entryPointStage = std::nullopt;

    m_stringBuilder.append(")\n");
    m_stringBuilder.append("{\n");
    IndentationScope scope(m_indent);
    checkErrorAndVisit(functionDefinition.body());
    m_stringBuilder.append("}\n\n");
}

void FunctionDefinitionWriter::visit(AST::Structure& structDecl)
{
    // FIXME: visit struct attributes
    m_structRole = { structDecl.role() };
    m_stringBuilder.append(m_indent, "struct ", structDecl.name(), " {\n");
    {
        IndentationScope scope(m_indent);
        for (auto& member : structDecl.members()) {
            m_stringBuilder.append(m_indent);
            visit(member.type());
            m_stringBuilder.append(" ", member.name());
            if (m_suffix.has_value()) {
                m_stringBuilder.append(*m_suffix);
                m_suffix.reset();
            }
            for (auto &attribute : member.attributes()) {
                m_stringBuilder.append(" ");
                visit(attribute);
            }
            m_stringBuilder.append(";\n");
        }
    }
    m_stringBuilder.append(m_indent, "};\n\n");
    m_structRole = std::nullopt;
}

void FunctionDefinitionWriter::visit(AST::Variable& variableDecl)
{
    ASSERT(variableDecl.maybeTypeName());

    m_stringBuilder.append(m_indent);
    visit(*variableDecl.maybeTypeName());
    m_stringBuilder.append(" ", variableDecl.name());
    if (variableDecl.maybeInitializer()) {
        m_stringBuilder.append(" = ");
        visit(*variableDecl.maybeInitializer());
    }
    m_stringBuilder.append(";\n");
}

void FunctionDefinitionWriter::visit(AST::Attribute& attribute)
{
    AST::Visitor::visit(attribute);
}

void FunctionDefinitionWriter::visit(AST::BuiltinAttribute& builtin)
{
    // FIXME: we should replace this with something more efficient, like a trie
    if (builtin.name() == "vertex_index"_s) {
        m_stringBuilder.append("[[vertex_id]]");
        return;
    }

    if (builtin.name() == "position"_s) {
        m_stringBuilder.append("[[position]]");
        return;
    }

    if (builtin.name() == "global_invocation_id"_s) {
        m_stringBuilder.append("[[thread_position_in_grid]]");
        return;
    }

    ASSERT_NOT_REACHED();
}

void FunctionDefinitionWriter::visit(AST::StageAttribute& stage)
{
    m_entryPointStage = { stage.stage() };
    switch (stage.stage()) {
    case AST::StageAttribute::Stage::Vertex:
        m_stringBuilder.append("[[vertex]]");
        break;
    case AST::StageAttribute::Stage::Fragment:
        m_stringBuilder.append("[[fragment]]");
        break;
    case AST::StageAttribute::Stage::Compute:
        m_stringBuilder.append("[[kernel]]");
        break;
    }
}

void FunctionDefinitionWriter::visit(AST::GroupAttribute& group)
{
    unsigned bufferIndex = group.group();
    if (m_entryPointStage.has_value() && *m_entryPointStage == AST::StageAttribute::Stage::Vertex) {
        auto max = m_shaderModule.configuration().maxBuffersPlusVertexBuffersForVertexStage;
        bufferIndex = vertexBufferIndexForBindGroup(bufferIndex, max);
    }
    m_stringBuilder.append("[[buffer(", bufferIndex, ")]]");
}

void FunctionDefinitionWriter::visit(AST::BindingAttribute& binding)
{
    m_stringBuilder.append("[[id(", binding.binding(), ")]]");
}

void FunctionDefinitionWriter::visit(AST::LocationAttribute& location)
{
    if (m_structRole.has_value()) {
        auto role = *m_structRole;
        switch (role) {
        case AST::StructureRole::UserDefined:
            break;
        case AST::StructureRole::VertexOutput:
        case AST::StructureRole::FragmentInput:
            m_stringBuilder.append("[[user(loc", location.location(), ")]]");
            return;
        case AST::StructureRole::BindGroup:
            return;
        case AST::StructureRole::VertexInput:
        case AST::StructureRole::ComputeInput:
            // FIXME: not sure if these should actually be attributes or not
            break;
        }
    }
    m_stringBuilder.append("[[attribute(", location.location(), ")]]");
}

void FunctionDefinitionWriter::visit(AST::ArrayTypeName& type)
{
    ASSERT(type.maybeElementType());

    if (!type.maybeElementCount()) {
        visit(*type.maybeElementType());
        m_suffix = { "[1]"_s };
        return;
    }

    m_stringBuilder.append("array<");
    visit(*type.maybeElementType());
    m_stringBuilder.append(", ");
    visit(*type.maybeElementCount());
    m_stringBuilder.append(">");
}

void FunctionDefinitionWriter::visit(AST::NamedTypeName& type)
{
    if (type.name() == "i32"_s)
        m_stringBuilder.append("int");
    else if (type.name() == "f32"_s)
        m_stringBuilder.append("float");
    else if (type.name() == "u32"_s)
        m_stringBuilder.append("unsigned");
    else
        m_stringBuilder.append(type.name());
}

void FunctionDefinitionWriter::visit(AST::ParameterizedTypeName& type)
{
    const auto& vec = [&](size_t size) {
        m_stringBuilder.append("vec<");
        visit(type.elementType());
        m_stringBuilder.append(", ", size, ">");
    };

    const auto& matrix = [&](size_t rows, size_t columns) {
        m_stringBuilder.append("matrix<");
        visit(type.elementType());
        m_stringBuilder.append(", ", columns, ", ", rows, ">");
    };

    switch (type.base()) {
    case AST::ParameterizedTypeName::Base::Vec2:
        vec(2);
        break;
    case AST::ParameterizedTypeName::Base::Vec3:
        vec(3);
        break;
    case AST::ParameterizedTypeName::Base::Vec4:
        vec(4);
        break;

    // FIXME: Implement the following types
    case AST::ParameterizedTypeName::Base::Mat2x2:
        matrix(2, 2);
        break;
    case AST::ParameterizedTypeName::Base::Mat2x3:
        matrix(2, 3);
        break;
    case AST::ParameterizedTypeName::Base::Mat2x4:
        matrix(2, 4);
        break;
    case AST::ParameterizedTypeName::Base::Mat3x2:
        matrix(3, 2);
        break;
    case AST::ParameterizedTypeName::Base::Mat3x3:
        matrix(3, 3);
        break;
    case AST::ParameterizedTypeName::Base::Mat3x4:
        matrix(3, 4);
        break;
    case AST::ParameterizedTypeName::Base::Mat4x2:
        matrix(4, 2);
        break;
    case AST::ParameterizedTypeName::Base::Mat4x3:
        matrix(4, 3);
        break;
    case AST::ParameterizedTypeName::Base::Mat4x4:
        matrix(4, 4);
        break;
    }
}

void FunctionDefinitionWriter::visit(AST::ReferenceTypeName& type)
{
    // FIXME: We can't assume this will always be device. The ReferenceType should
    // have knowledge about the memory region
    m_stringBuilder.append("device ");
    visit(type.type());
    m_stringBuilder.append("&");
}

void FunctionDefinitionWriter::visit(AST::StructTypeName& structType)
{
    m_stringBuilder.append(structType.structure().name());
}

void FunctionDefinitionWriter::visit(AST::ParameterValue& parameter)
{
    visit(parameter.typeName());
    m_stringBuilder.append(" ", parameter.name());
    for (auto& attribute : parameter.attributes()) {
        m_stringBuilder.append(" ");
        checkErrorAndVisit(attribute);
    }
}

void FunctionDefinitionWriter::visitArgumentBufferParameter(AST::ParameterValue& parameter)
{
    m_stringBuilder.append("constant ");
    visit(parameter.typeName());
    m_stringBuilder.append("& ", parameter.name());
    for (auto& attribute : parameter.attributes()) {
        m_stringBuilder.append(" ");
        checkErrorAndVisit(attribute);
    }
}

void FunctionDefinitionWriter::visit(AST::Expression& expression)
{
    AST::Visitor::visit(expression);
}

void FunctionDefinitionWriter::visit(AST::CallExpression& call)
{
    bool first = true;
    if (is<AST::ArrayTypeName>(call.target())) {
        m_stringBuilder.append("{\n");
        {
            IndentationScope scope(m_indent);
            for (auto& argument : call.arguments()) {
                m_stringBuilder.append(m_indent);
                visit(argument);
                m_stringBuilder.append(",\n");
                first = false;
            }
        }
        m_stringBuilder.append(m_indent, "}");
    } else {
        visit(call.target());
        m_stringBuilder.append("(");
        for (auto& argument : call.arguments()) {
            if (!first)
                m_stringBuilder.append(", ");
            visit(argument);
            first = false;
        }
        m_stringBuilder.append(")");
    }
}

void FunctionDefinitionWriter::visit(AST::UnaryExpression& unary)
{
    switch (unary.operation()) {
    case AST::UnaryOperation::Negate:
        m_stringBuilder.append("-");
        break;

    case AST::UnaryOperation::AddressOf:
    case AST::UnaryOperation::Complement:
    case AST::UnaryOperation::Dereference:
    case AST::UnaryOperation::Not:
        // FIXME: Implement these
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    visit(unary.expression());
}

void FunctionDefinitionWriter::visit(AST::BinaryExpression& binary)
{
    visit(binary.leftExpression());
    switch (binary.operation()) {
    case AST::BinaryOperation::Add:
        m_stringBuilder.append(" + ");
        break;
    case AST::BinaryOperation::Subtract:
        m_stringBuilder.append(" - ");
        break;
    case AST::BinaryOperation::Multiply:
        m_stringBuilder.append(" * ");
        break;

    case AST::BinaryOperation::Divide:
    case AST::BinaryOperation::Modulo:
    case AST::BinaryOperation::And:
    case AST::BinaryOperation::Or:
    case AST::BinaryOperation::Xor:
    case AST::BinaryOperation::LeftShift:
    case AST::BinaryOperation::RightShift:
    case AST::BinaryOperation::Equal:
    case AST::BinaryOperation::NotEqual:
    case AST::BinaryOperation::GreaterThan:
    case AST::BinaryOperation::GreaterEqual:
    case AST::BinaryOperation::LessThan:
    case AST::BinaryOperation::LessEqual:
    case AST::BinaryOperation::ShortCircuitAnd:
    case AST::BinaryOperation::ShortCircuitOr:
        // FIXME: Implement these
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    visit(binary.rightExpression());
}

void FunctionDefinitionWriter::visit(AST::PointerDereferenceExpression& pointerDereference)
{
    m_stringBuilder.append("(*");
    visit(pointerDereference.target());
    m_stringBuilder.append(")");
}
void FunctionDefinitionWriter::visit(AST::IndexAccessExpression& access)
{
    visit(access.base());
    m_stringBuilder.append("[");
    visit(access.index());
    m_stringBuilder.append("]");
}

void FunctionDefinitionWriter::visit(AST::IdentifierExpression& identifier)
{
    m_stringBuilder.append(identifier.identifier());
}

void FunctionDefinitionWriter::visit(AST::FieldAccessExpression& access)
{
    visit(access.base());
    m_stringBuilder.append(".", access.fieldName());
}

void FunctionDefinitionWriter::visit(AST::AbstractIntegerLiteral& literal)
{
    // FIXME: this might not serialize all values correctly
    m_stringBuilder.append(literal.value());
}

void FunctionDefinitionWriter::visit(AST::Signed32Literal& literal)
{
    // FIXME: this might not serialize all values correctly
    m_stringBuilder.append(literal.value());
}

void FunctionDefinitionWriter::visit(AST::Unsigned32Literal& literal)
{
    // FIXME: this might not serialize all values correctly
    m_stringBuilder.append(literal.value());
}

void FunctionDefinitionWriter::visit(AST::AbstractFloatLiteral& literal)
{
    // FIXME: this might not serialize all values correctly
    m_stringBuilder.append(literal.value());
}

void FunctionDefinitionWriter::visit(AST::Float32Literal& literal)
{
    // FIXME: this might not serialize all values correctly
    m_stringBuilder.append(literal.value());
}

void FunctionDefinitionWriter::visit(AST::Statement& statement)
{
    AST::Visitor::visit(statement);
}

void FunctionDefinitionWriter::visit(AST::AssignmentStatement& assignment)
{
    m_stringBuilder.append(m_indent);
    visit(assignment.lhs());
    m_stringBuilder.append(" = ");
    visit(assignment.rhs());
    m_stringBuilder.append(";\n");
}

void FunctionDefinitionWriter::visit(AST::ReturnStatement& statement)
{
    m_stringBuilder.append(m_indent, "return");
    if (statement.maybeExpression()) {
        m_stringBuilder.append(" ");
        visit(*statement.maybeExpression());
    }
    m_stringBuilder.append(";\n");
}

RenderMetalFunctionEntryPoints emitMetalFunctions(StringBuilder& stringBuilder, AST::ShaderModule& module)
{
    FunctionDefinitionWriter functionDefinitionWriter(module, stringBuilder);
    functionDefinitionWriter.visit(module);

    // FIXME: return the actual entry points
    return { String(""_s), String(""_s) };
}

} // namespace Metal
} // namespace WGSL
