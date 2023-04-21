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
#include "CallGraph.h"
#include "Types.h"
#include "WGSLShaderModule.h"

#include <wtf/SortedArrayMap.h>
#include <wtf/text/StringBuilder.h>

namespace WGSL {

namespace Metal {

class FunctionDefinitionWriter : public AST::Visitor {
public:
    FunctionDefinitionWriter(CallGraph& callGraph, StringBuilder& stringBuilder)
        : m_stringBuilder(stringBuilder)
        , m_callGraph(callGraph)
    {
    }

    virtual ~FunctionDefinitionWriter() = default;

    using AST::Visitor::visit;

    void write();

    void visit(AST::Attribute&) override;
    void visit(AST::BuiltinAttribute&) override;
    void visit(AST::LocationAttribute&) override;
    void visit(AST::StageAttribute&) override;
    void visit(AST::GroupAttribute&) override;
    void visit(AST::BindingAttribute&) override;

    void visit(AST::Function&) override;
    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;

    void visit(AST::BoolLiteral&) override;
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
    void visit(AST::CompoundStatement&) override;
    void visit(AST::IfStatement&) override;
    void visit(AST::ReturnStatement&) override;
    void visit(AST::ForStatement&) override;

    void visit(AST::TypeName&) override;

    void visit(AST::Parameter&) override;
    void visitArgumentBufferParameter(AST::Parameter&);

    StringBuilder& stringBuilder() { return m_stringBuilder; }

private:
    void visit(const Type*);
    void visitGlobal(AST::Variable&);
    void serializeVariable(AST::Variable&);

    StringBuilder& m_stringBuilder;
    CallGraph& m_callGraph;
    Indentation<4> m_indent { 0 };
    std::optional<AST::StructureRole> m_structRole;
    std::optional<AST::StageAttribute::Stage> m_entryPointStage;
    std::optional<String> m_suffix;
    unsigned m_functionConstantIndex { 0 };
};

void FunctionDefinitionWriter::write()
{
    for (auto& structure : m_callGraph.ast().structures())
        visit(structure);
    for (auto& variable : m_callGraph.ast().variables())
        visitGlobal(variable);
    for (auto& entryPoint : m_callGraph.entrypoints())
        visit(entryPoint.function);
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
    checkErrorAndVisit(functionDefinition.body());
    m_stringBuilder.append("\n");
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

void FunctionDefinitionWriter::visit(AST::Variable& variable)
{
    serializeVariable(variable);
    m_stringBuilder.append(";\n");
}

void FunctionDefinitionWriter::visitGlobal(AST::Variable& variable)
{
    if (variable.flavor() != AST::VariableFlavor::Override)
        return;

    m_stringBuilder.append("constant ");
    serializeVariable(variable);
    m_stringBuilder.append(" [[function_constant(", m_functionConstantIndex++, ")]];\n");
}

void FunctionDefinitionWriter::serializeVariable(AST::Variable& variable)
{
    if (variable.maybeTypeName())
        visit(*variable.maybeTypeName());
    else {
        ASSERT(variable.maybeInitializer());
        const Type* inferredType = variable.maybeInitializer()->inferredType();
        visit(inferredType);
    }
    m_stringBuilder.append(" ", variable.name());
    if (variable.maybeInitializer()) {
        m_stringBuilder.append(" = ");
        visit(*variable.maybeInitializer());
    }
}

void FunctionDefinitionWriter::visit(AST::Attribute& attribute)
{
    AST::Visitor::visit(attribute);
}

void FunctionDefinitionWriter::visit(AST::BuiltinAttribute& builtin)
{
    // FIXME: we should replace this with something more efficient, like a trie
    static constexpr std::pair<ComparableASCIILiteral, ASCIILiteral> builtinMappings[] {
        { "frag_depth", "depth(any)"_s },
        { "front_facing", "front_facing"_s },
        { "global_invocation_id", "thread_position_in_grid"_s },
        { "instance_index", "instance_id"_s },
        { "local_invocation_id", "thread_position_in_threadgroup"_s },
        { "local_invocation_index", "thread_index_in_threadgroup"_s },
        { "num_workgroups", "threadgroups_per_grid"_s },
        { "position", "position"_s },
        { "sample_index", "sample_id"_s },
        { "sample_mask", "sample_mask"_s },
        { "vertex_index", "vertex_id"_s },
        { "workgroup_id", "threadgroup_position_in_grid"_s },
    };
    static constexpr SortedArrayMap builtins { builtinMappings };

    auto mappedBuiltin = builtins.get(builtin.name().id());
    if (mappedBuiltin) {
        m_stringBuilder.append("[[", mappedBuiltin, "]]");
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
        auto max = m_callGraph.ast().configuration().maxBuffersPlusVertexBuffersForVertexStage;
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

// Types
void FunctionDefinitionWriter::visit(AST::TypeName& type)
{
    // FIXME:Remove this when the type checker is aware of reference types
    if (is<AST::ReferenceTypeName>(type)) {
        // FIXME: We can't assume this will always be device. The ReferenceType should
        // have knowledge about the memory region
        m_stringBuilder.append("device ");
        visit(downcast<AST::ReferenceTypeName>(type).type());
        m_stringBuilder.append("&");
        return;
    }

    visit(type.resolvedType());
}

void FunctionDefinitionWriter::visit(const Type* type)
{
    using namespace WGSL::Types;
    WTF::switchOn(*type,
        [&](const Primitive& primitive) {
            switch (primitive.kind) {
            case Types::Primitive::AbstractInt:
            case Types::Primitive::I32:
                m_stringBuilder.append("int");
                break;
            case Types::Primitive::U32:
                m_stringBuilder.append("unsigned");
                break;
            case Types::Primitive::AbstractFloat:
            case Types::Primitive::F32:
                m_stringBuilder.append("float");
                break;
            case Types::Primitive::Void:
            case Types::Primitive::Bool:
            case Types::Primitive::Sampler:
                m_stringBuilder.append(*type);
                break;
            }
        },
        [&](const Vector& vector) {
            m_stringBuilder.append("vec<");
            visit(vector.element);
            m_stringBuilder.append(", ", vector.size, ">");
        },
        [&](const Matrix& matrix) {
            m_stringBuilder.append("matrix<");
            visit(matrix.element);
            m_stringBuilder.append(", ", matrix.columns, ", ", matrix.rows, ">");
        },
        [&](const Array& array) {
            ASSERT(array.element);
            if (!array.size.has_value()) {
                visit(array.element);
                m_suffix = { "[1]"_s };
                return;
            }

            m_stringBuilder.append("array<");
            visit(array.element);
            m_stringBuilder.append(", ", *array.size, ">");
        },
        [&](const Struct& structure) {
            m_stringBuilder.append(structure.structure.name());
        },
        [&](const Function&) {
            // FIXME: implement this
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Bottom&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Texture& texture) {
            const char* type;
            const char* mode = "sample";
            switch (texture.kind) {
            case Types::Texture::Kind::Texture1d:
                type = "texture1d";
                break;
            case Types::Texture::Kind::Texture2d:
                type = "texture2d";
                break;
            case Types::Texture::Kind::Texture2dArray:
                type = "texture2d_array";
                break;
            case Types::Texture::Kind::Texture3d:
                type = "texture3d";
                break;
            case Types::Texture::Kind::TextureCube:
                type = "texturecube";
                break;
            case Types::Texture::Kind::TextureCubeArray:
                type = "texturecube_array";
                break;
            case Types::Texture::Kind::TextureMultisampled2d:
                type = "texture2d_ms";
                break;

            case Types::Texture::Kind::TextureStorage1d:
                type = "texture1d";
                mode = "write";
                break;
            case Types::Texture::Kind::TextureStorage2d:
                type = "texture2d";
                mode = "write";
                break;
            case Types::Texture::Kind::TextureStorage2dArray:
                type = "texture2d_aray";
                mode = "write";
                break;
            case Types::Texture::Kind::TextureStorage3d:
                type = "texture3d";
                mode = "write";
                break;
            }
            m_stringBuilder.append(type, "<");
            visit(texture.element);
            m_stringBuilder.append(", access::", mode, ">");
        });
}

void FunctionDefinitionWriter::visit(AST::Parameter& parameter)
{
    visit(parameter.typeName());
    m_stringBuilder.append(" ", parameter.name());
    for (auto& attribute : parameter.attributes()) {
        m_stringBuilder.append(" ");
        checkErrorAndVisit(attribute);
    }
}

void FunctionDefinitionWriter::visitArgumentBufferParameter(AST::Parameter& parameter)
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

static void visitArguments(FunctionDefinitionWriter* writer, AST::CallExpression& call, unsigned startOffset = 0)
{
    bool first = true;
    writer->stringBuilder().append("(");
    for (unsigned i = startOffset; i < call.arguments().size(); ++i) {
        if (!first)
            writer->stringBuilder().append(", ");
        writer->visit(call.arguments()[i]);
        first = false;
    }
    writer->stringBuilder().append(")");
};

void FunctionDefinitionWriter::visit(AST::CallExpression& call)
{
    if (is<AST::ArrayTypeName>(call.target())) {
        m_stringBuilder.append("{\n");
        {
            IndentationScope scope(m_indent);
            for (auto& argument : call.arguments()) {
                m_stringBuilder.append(m_indent);
                visit(argument);
                m_stringBuilder.append(",\n");
            }
        }
        m_stringBuilder.append(m_indent, "}");
        return;
    }

    if (is<AST::NamedTypeName>(call.target())) {
        static constexpr std::pair<ComparableASCIILiteral, void(*)(FunctionDefinitionWriter*, AST::CallExpression&)> builtinMappings[] {
            { "textureSample", [](FunctionDefinitionWriter* writer, AST::CallExpression& call) {
                ASSERT(call.arguments().size() > 1);
                writer->visit(call.arguments()[0]);
                writer->stringBuilder().append(".sample");
                visitArguments(writer, call, 1);
            } },
        };
        static constexpr SortedArrayMap builtins { builtinMappings };
        const auto& targetName = downcast<AST::NamedTypeName>(call.target()).name().id();
        if (auto mappedBuiltin = builtins.get(targetName)) {
            mappedBuiltin(this, call);
            return;
        }

        if (AST::ParameterizedTypeName::stringViewToKind(targetName).has_value())
            visit(call.inferredType());
        else
            m_stringBuilder.append(targetName);
        visitArguments(this, call);
        return;
    }

    visit(call.inferredType());
    visitArguments(this, call);
}

void FunctionDefinitionWriter::visit(AST::UnaryExpression& unary)
{
    switch (unary.operation()) {
    case AST::UnaryOperation::Complement:
        m_stringBuilder.append("~");
        break;
    case AST::UnaryOperation::Negate:
        m_stringBuilder.append("-");
        break;

    case AST::UnaryOperation::AddressOf:
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
    m_stringBuilder.append("(");
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
        m_stringBuilder.append(" / ");
        break;
    case AST::BinaryOperation::Modulo:
        m_stringBuilder.append(" % ");
        break;
    case AST::BinaryOperation::And:
        m_stringBuilder.append(" & ");
        break;
    case AST::BinaryOperation::Or:
        m_stringBuilder.append(" | ");
        break;
    case AST::BinaryOperation::Xor:
        m_stringBuilder.append(" ^ ");
        break;

    case AST::BinaryOperation::LeftShift:
    case AST::BinaryOperation::RightShift:
        // FIXME: Implement these
        RELEASE_ASSERT_NOT_REACHED();
        break;

    case AST::BinaryOperation::Equal:
        m_stringBuilder.append(" == ");
        break;
    case AST::BinaryOperation::NotEqual:
        m_stringBuilder.append(" != ");
        break;
    case AST::BinaryOperation::GreaterThan:
        m_stringBuilder.append(" > ");
        break;
    case AST::BinaryOperation::GreaterEqual:
        m_stringBuilder.append(" >= ");
        break;
    case AST::BinaryOperation::LessThan:
        m_stringBuilder.append(" < ");
        break;
    case AST::BinaryOperation::LessEqual:
        m_stringBuilder.append(" <= ");
        break;

    case AST::BinaryOperation::ShortCircuitAnd:
    case AST::BinaryOperation::ShortCircuitOr:
        // FIXME: Implement these
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    visit(binary.rightExpression());
    m_stringBuilder.append(")");
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

void FunctionDefinitionWriter::visit(AST::BoolLiteral& literal)
{
    m_stringBuilder.append(literal.value() ? "true" : "false");
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
    visit(assignment.lhs());
    m_stringBuilder.append(" = ");
    visit(assignment.rhs());
    m_stringBuilder.append(";\n");
}

void FunctionDefinitionWriter::visit(AST::CompoundStatement& statement)
{
    m_stringBuilder.append(m_indent, "{\n");
    {
        IndentationScope scope(m_indent);
        for (auto& statement : statement.statements()) {
            m_stringBuilder.append(m_indent);
            checkErrorAndVisit(statement);
        }
    }
    m_stringBuilder.append(m_indent, "}\n");
}

void FunctionDefinitionWriter::visit(AST::IfStatement& statement)
{
    m_stringBuilder.append("if (");
    visit(statement.test());
    m_stringBuilder.append(")\n");
    visit(statement.trueBody());
    if (statement.maybeFalseBody()) {
        m_stringBuilder.append(m_indent, "else ");
        visit(*statement.maybeFalseBody());
    }
}

void FunctionDefinitionWriter::visit(AST::ReturnStatement& statement)
{
    m_stringBuilder.append("return");
    if (statement.maybeExpression()) {
        m_stringBuilder.append(" ");
        visit(*statement.maybeExpression());
    }
    m_stringBuilder.append(";\n");
}

void FunctionDefinitionWriter::visit(AST::ForStatement& statement)
{
    m_stringBuilder.append("for (");
    if (auto* initializer = statement.maybeInitializer())
        visit(*initializer);
    m_stringBuilder.append(";");
    if (auto* test = statement.maybeTest())
        visit(*test);
    m_stringBuilder.append(";");
    if (auto* update = statement.maybeUpdate())
        visit(*update);
    m_stringBuilder.append(")");
    visit(statement.body());
}

void emitMetalFunctions(StringBuilder& stringBuilder, CallGraph& callGraph)
{
    FunctionDefinitionWriter functionDefinitionWriter(callGraph, stringBuilder);
    functionDefinitionWriter.write();
}

} // namespace Metal
} // namespace WGSL
