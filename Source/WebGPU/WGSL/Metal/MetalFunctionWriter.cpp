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
#include "Constraints.h"
#include "Types.h"
#include "WGSLShaderModule.h"
#include <wtf/HashSet.h>
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
    void visit(AST::WorkgroupSizeAttribute&) override;
    void visit(AST::SizeAttribute&) override;
    void visit(AST::AlignAttribute&) override;

    void visit(AST::Function&) override;
    void visit(AST::Structure&) override;
    void visit(AST::Variable&) override;

    void visit(const Type*, AST::Expression&);
    void visit(const Type*, AST::CallExpression&);

    void visit(AST::BoolLiteral&) override;
    void visit(AST::AbstractFloatLiteral&) override;
    void visit(AST::AbstractIntegerLiteral&) override;
    void visit(AST::BinaryExpression&) override;
    void visit(AST::Expression&) override;
    void visit(AST::FieldAccessExpression&) override;
    void visit(AST::Float32Literal&) override;
    void visit(AST::IdentifierExpression&) override;
    void visit(AST::IndexAccessExpression&) override;
    void visit(AST::PointerDereferenceExpression&) override;
    void visit(AST::UnaryExpression&) override;
    void visit(AST::Signed32Literal&) override;
    void visit(AST::Unsigned32Literal&) override;

    void visit(AST::Statement&) override;
    void visit(AST::AssignmentStatement&) override;
    void visit(AST::CompoundStatement&) override;
    void visit(AST::DecrementIncrementStatement&) override;
    void visit(AST::IfStatement&) override;
    void visit(AST::PhonyAssignmentStatement&) override;
    void visit(AST::ReturnStatement&) override;
    void visit(AST::ForStatement&) override;
    void visit(AST::BreakStatement&) override;
    void visit(AST::ContinueStatement&) override;

    void visit(AST::TypeName&) override;

    void visit(AST::Parameter&) override;
    void visitArgumentBufferParameter(AST::Parameter&);

    StringBuilder& stringBuilder() { return m_stringBuilder; }
    Indentation<4>& indent() { return m_indent; }

private:
    void emitNecessaryHelpers();
    void visit(const Type*);
    void visitGlobal(AST::Variable&);
    void serializeVariable(AST::Variable&);
    void generatePackingHelpers(AST::Structure&);
    bool emitPackedVector(const Types::Vector&);

    StringBuilder& m_stringBuilder;
    CallGraph& m_callGraph;
    Indentation<4> m_indent { 0 };
    std::optional<AST::StructureRole> m_structRole;
    std::optional<AST::StageAttribute::Stage> m_entryPointStage;
    unsigned m_functionConstantIndex { 0 };
    HashSet<AST::Function*> m_visitedFunctions;
};

void FunctionDefinitionWriter::write()
{
    emitNecessaryHelpers();

    for (auto& structure : m_callGraph.ast().structures())
        visit(structure);
    for (auto& structure : m_callGraph.ast().structures())
        generatePackingHelpers(structure);
    for (auto& variable : m_callGraph.ast().variables())
        visitGlobal(variable);
    for (auto& entryPoint : m_callGraph.entrypoints())
        visit(entryPoint.function);
}

void FunctionDefinitionWriter::emitNecessaryHelpers()
{
    if (m_callGraph.ast().usesExternalTextures()) {
        m_callGraph.ast().clearUsesExternalTextures();
        m_stringBuilder.append("struct texture_external {\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "texture2d<float> FirstPlane;\n");
            m_stringBuilder.append(m_indent, "texture2d<float> SecondPlane;\n");
            m_stringBuilder.append(m_indent, "float3x2 UVRemapMatrix;\n");
            m_stringBuilder.append(m_indent, "float4x3 ColorSpaceConversionMatrix;\n");
        }
        m_stringBuilder.append("};\n\n");
    }

    if (m_callGraph.ast().usesPackArray()) {
        m_callGraph.ast().clearUsesPackArray();
        m_stringBuilder.append(m_indent, "template<typename T, size_t N>\n");
        m_stringBuilder.append(m_indent, "array<typename T::PackedType, N> __pack_array(array<T, N> unpacked)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "array<typename T::PackedType, N> packed;\n");
            m_stringBuilder.append(m_indent, "for (size_t i = 0; i < N; ++i)\n");
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, "packed[i] = __pack(unpacked[i]);\n");
            }
            m_stringBuilder.append(m_indent, "return packed;\n");
        }
        m_stringBuilder.append(m_indent, "}\n\n");
    }

    if (m_callGraph.ast().usesUnpackArray()) {
        m_callGraph.ast().clearUsesUnpackArray();
        m_stringBuilder.append(m_indent, "template<typename T, size_t N>\n");
        m_stringBuilder.append(m_indent, "array<typename T::UnpackedType, N> __unpack_array(array<T, N> packed)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "array<typename T::UnpackedType, N> unpacked;\n");
            m_stringBuilder.append(m_indent, "for (size_t i = 0; i < N; ++i)\n");
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, "unpacked[i] = __unpack(packed[i]);\n");
            }
            m_stringBuilder.append(m_indent, "return unpacked;\n");
        }
        m_stringBuilder.append(m_indent, "}\n\n");
    }
}

void FunctionDefinitionWriter::visit(AST::Function& functionDefinition)
{
    if (!m_visitedFunctions.add(&functionDefinition).isNewEntry)
        return;

    for (auto& callee : m_callGraph.callees(functionDefinition))
        visit(*callee.target);

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
    m_stringBuilder.append("\n\n");
}

void FunctionDefinitionWriter::visit(AST::Structure& structDecl)
{
    // FIXME: visit struct attributes
    m_structRole = { structDecl.role() };
    m_stringBuilder.append(m_indent, "struct ", structDecl.name(), " {\n");
    {
        IndentationScope scope(m_indent);
        unsigned alignment = 0;
        unsigned size = 0;
        unsigned paddingID = 0;
        bool shouldPack = structDecl.role() == AST::StructureRole::PackedResource;
        const auto& addPadding = [&](unsigned paddingSize) {
            ASSERT(shouldPack);
            m_stringBuilder.append(m_indent, "uint8_t __padding", ++paddingID, "[", String::number(paddingSize), "]; \n");
        };

        if (structDecl.role() == AST::StructureRole::PackedResource)
            m_stringBuilder.append(m_indent, "using UnpackedType = struct ", structDecl.original()->name(), ";\n\n");
        else if (structDecl.role() == AST::StructureRole::UserDefinedResource)
            m_stringBuilder.append(m_indent, "using PackedType = struct ", structDecl.packed()->name(), ";\n\n");

        for (auto& member : structDecl.members()) {
            auto& name = member.name();
            auto* type = member.type().resolvedType();
            if (isPrimitiveReference(type, Types::Primitive::TextureExternal)) {
                m_stringBuilder.append(m_indent, "texture2d<float> __", name, "_FirstPlane;\n");
                m_stringBuilder.append(m_indent, "texture2d<float> __", name, "_SecondPlane;\n");
                m_stringBuilder.append(m_indent, "float3x2 __", name, "_UVRemapMatrix;\n");
                m_stringBuilder.append(m_indent, "float4x3 __", name, "_ColorSpaceConversionMatrix;\n");
                continue;
            }

            unsigned fieldSize = 0;
            unsigned explicitSize = 0;
            unsigned fieldAlignment = 0;
            unsigned offset = 0;
            if (shouldPack) {
                fieldSize = type->size();
                fieldAlignment = type->alignment();
                explicitSize = fieldSize;
                for (auto &attribute : member.attributes()) {
                    if (is<AST::SizeAttribute>(attribute))
                        explicitSize = *AST::extractInteger(downcast<AST::SizeAttribute>(attribute).size());
                    else if (is<AST::AlignAttribute>(attribute))
                        fieldAlignment = *AST::extractInteger(downcast<AST::AlignAttribute>(attribute).alignment());
                }

                offset = WTF::roundUpToMultipleOf(fieldAlignment, size);
                if (offset != size)
                    addPadding(offset - size);
            }

            m_stringBuilder.append(m_indent);
            visit(member.type());
            m_stringBuilder.append(" ", name);
            for (auto &attribute : member.attributes()) {
                m_stringBuilder.append(" ");
                visit(attribute);
            }
            m_stringBuilder.append(";\n");

            if (shouldPack) {
                if (explicitSize != fieldSize)
                    addPadding(explicitSize - fieldSize);

                alignment = std::max(alignment, fieldAlignment);
                size = offset + explicitSize;
            }
        }

        if (shouldPack) {
            auto finalSize = WTF::roundUpToMultipleOf(alignment, size);
            if (finalSize != size)
                addPadding(finalSize - size);
        }

        if (structDecl.role() == AST::StructureRole::VertexOutput) {
            m_stringBuilder.append("\n");
            m_stringBuilder.append(m_indent, "template<typename T>\n");
            m_stringBuilder.append(m_indent, structDecl.name(), "(const thread T& other)\n");
            {
                IndentationScope scope(m_indent);
                char prefix = ':';
                for (auto& member : structDecl.members()) {
                    auto& name = member.name();
                    m_stringBuilder.append(m_indent, prefix, " ", name, "(other.", name, ")\n");
                    prefix = ',';
                }
            }
            m_stringBuilder.append(m_indent, "{ }\n");
        }
    }
    m_stringBuilder.append(m_indent, "};\n\n");
    m_structRole = std::nullopt;
}

void FunctionDefinitionWriter::generatePackingHelpers(AST::Structure& structure)
{
    if (structure.role() != AST::StructureRole::PackedResource)
        return;

    const String& packedName = structure.name();
    auto unpackedName = structure.original()->name();

    m_stringBuilder.append(m_indent, packedName, " __pack(", unpackedName, " unpacked)\n");
    m_stringBuilder.append(m_indent, "{\n");
    {
        IndentationScope scope(m_indent);
        m_stringBuilder.append(m_indent, packedName, " packed;\n");
        for (auto& member : structure.members()) {
            auto& name = member.name();
            m_stringBuilder.append(m_indent, "packed.", name, " = unpacked.", name, ";\n");
        }
        m_stringBuilder.append(m_indent, "return packed;\n");
    }
    m_stringBuilder.append(m_indent, "}\n\n");

    m_stringBuilder.append(m_indent, unpackedName, " __unpack(", packedName, " packed)\n");
    m_stringBuilder.append(m_indent, "{\n");
    {
        IndentationScope scope(m_indent);
        m_stringBuilder.append(m_indent, unpackedName, " unpacked;\n");
        for (auto& member : structure.members()) {
            auto& name = member.name();
            m_stringBuilder.append(m_indent, "unpacked.", name, " = packed.", name, ";\n");
        }
        m_stringBuilder.append(m_indent, "return unpacked;\n");
    }
    m_stringBuilder.append(m_indent, "}\n\n");
}

bool FunctionDefinitionWriter::emitPackedVector(const Types::Vector& vector)
{
    if (!m_structRole.has_value())
        return false;
    if (*m_structRole != AST::StructureRole::PackedResource)
        return false;
    // The only vectors that need to be packed are the vectors with 3 elements,
    // because their size differs between Metal and WGSL (4 * element size vs
    // 3 * element size, respectively)
    if (vector.size != 3)
        return false;
    auto& primitive = std::get<Types::Primitive>(*vector.element);
    switch (primitive.kind) {
    case Types::Primitive::AbstractInt:
    case Types::Primitive::I32:
        m_stringBuilder.append("packed_int", String::number(vector.size));
        break;
    case Types::Primitive::U32:
        m_stringBuilder.append("packed_uint", String::number(vector.size));
        break;
    case Types::Primitive::AbstractFloat:
    case Types::Primitive::F32:
        m_stringBuilder.append("packed_float", String::number(vector.size));
        break;
    case Types::Primitive::Bool:
    case Types::Primitive::Void:
    case Types::Primitive::Sampler:
    case Types::Primitive::TextureExternal:
        RELEASE_ASSERT_NOT_REACHED();
    }
    return true;
}

void FunctionDefinitionWriter::visit(AST::Variable& variable)
{
    serializeVariable(variable);
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
    if (variable.flavor() == AST::VariableFlavor::Const)
        return;

    const Type* type = variable.storeType();
    if (isPrimitiveReference(type, Types::Primitive::TextureExternal)) {
        ASSERT(variable.maybeInitializer());
        m_stringBuilder.append("texture_external ", variable.name(), " { ");
        visit(*variable.maybeInitializer());
        m_stringBuilder.append("_FirstPlane, ");
        visit(*variable.maybeInitializer());
        m_stringBuilder.append("_SecondPlane, ");
        visit(*variable.maybeInitializer());
        m_stringBuilder.append("_UVRemapMatrix, ");
        visit(*variable.maybeInitializer());
        m_stringBuilder.append("_ColorSpaceConversionMatrix }");
        return;
    }

    visit(type);
    m_stringBuilder.append(" ", variable.name());
    if (variable.maybeInitializer()) {
        m_stringBuilder.append(" = ");
        visit(type, *variable.maybeInitializer());
    }
}

void FunctionDefinitionWriter::visit(AST::Attribute& attribute)
{
    AST::Visitor::visit(attribute);
}

void FunctionDefinitionWriter::visit(AST::BuiltinAttribute& builtin)
{
    // Built-in attributes are only valid for parameters. If a struct member originally
    // had a built-in attribute it must have already been hoisted into a parameter, but
    // we keep the original struct so we can reconstruct it.
    if (m_structRole.has_value() && *m_structRole != AST::StructureRole::VertexOutput)
        return;

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
    unsigned bufferIndex = *AST::extractInteger(group.group());
    if (m_entryPointStage.has_value() && *m_entryPointStage == AST::StageAttribute::Stage::Vertex) {
        auto max = m_callGraph.ast().configuration().maxBuffersPlusVertexBuffersForVertexStage;
        bufferIndex = vertexBufferIndexForBindGroup(bufferIndex, max);
    }
    m_stringBuilder.append("[[buffer(", bufferIndex, ")]]");
}

void FunctionDefinitionWriter::visit(AST::BindingAttribute& binding)
{
    m_stringBuilder.append("[[id(", *AST::extractInteger(binding.binding()), ")]]");
}

void FunctionDefinitionWriter::visit(AST::LocationAttribute& location)
{
    if (m_structRole.has_value()) {
        auto role = *m_structRole;
        switch (role) {
        case AST::StructureRole::VertexOutput:
        case AST::StructureRole::FragmentInput:
            m_stringBuilder.append("[[user(loc", *AST::extractInteger(location.location()), ")]]");
            return;
        case AST::StructureRole::BindGroup:
        case AST::StructureRole::UserDefined:
        case AST::StructureRole::ComputeInput:
        case AST::StructureRole::UserDefinedResource:
        case AST::StructureRole::PackedResource:
            return;
        case AST::StructureRole::VertexInput:
            m_stringBuilder.append("[[attribute(", *AST::extractInteger(location.location()), ")]]");
            break;
        }
    }
}

void FunctionDefinitionWriter::visit(AST::WorkgroupSizeAttribute&)
{
    // This attribute shouldn't generate any code. The workgroup size is passed
    // to the API through the EntryPointInformation.
}

void FunctionDefinitionWriter::visit(AST::SizeAttribute&)
{
    // This attribute shouldn't generate any code. The size is used when serializing
    // structs.
}

void FunctionDefinitionWriter::visit(AST::AlignAttribute&)
{
    // This attribute shouldn't generate any code. The alignment is used when
    // serializing structs.
}

// Types
void FunctionDefinitionWriter::visit(AST::TypeName& type)
{
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
            case Types::Primitive::TextureExternal:
                m_stringBuilder.append("texture_external");
                break;
            }
        },
        [&](const Vector& vector) {
            if (emitPackedVector(vector))
                return;
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
            m_stringBuilder.append("array<");
            visit(array.element);
            m_stringBuilder.append(", ", array.size.value_or(1), ">");
        },
        [&](const Struct& structure) {
            m_stringBuilder.append(structure.structure.name());
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
        },
        [&](const Reference& reference) {
            const char* addressSpace = nullptr;
            switch (reference.addressSpace) {
            case AddressSpace::Function:
                addressSpace = "thread";
                break;
            case AddressSpace::Workgroup:
                addressSpace = "threadgroup";
                break;
            case AddressSpace::Uniform:
                addressSpace = "constant";
                break;
            case AddressSpace::Storage:
                addressSpace = "device";
                break;
            case AddressSpace::Handle:
            case AddressSpace::Private:
                break;
            }
            if (!addressSpace) {
                visit(reference.element);
                return;
            }
            if (reference.accessMode == AccessMode::Read)
                m_stringBuilder.append("const ");
            m_stringBuilder.append(addressSpace, " ");
            visit(reference.element);
            m_stringBuilder.append("&");
        },
        [&](const Function&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Bottom&) {
            RELEASE_ASSERT_NOT_REACHED();
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
    visit(expression.inferredType(), expression);
}

void FunctionDefinitionWriter::visit(const Type* type, AST::Expression& expression)
{
    switch (expression.kind()) {
    case AST::NodeKind::CallExpression:
        visit(type, downcast<AST::CallExpression>(expression));
        break;
    case AST::NodeKind::IdentityExpression:
        visit(type, downcast<AST::IdentityExpression>(expression).expression());
        break;
    default:
        AST::Visitor::visit(expression);
        break;
    }
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
}

void FunctionDefinitionWriter::visit(const Type* type, AST::CallExpression& call)
{
    auto isArray = is<AST::ArrayTypeName>(call.target());
    auto isStruct = !isArray && std::holds_alternative<Types::Struct>(*call.target().resolvedType());
    if (isArray || isStruct) {
        if (isStruct) {
            visit(type);
            m_stringBuilder.append(" ");
        }
        const Type* arrayElementType = nullptr;
        if (isArray)
            arrayElementType = std::get<Types::Array>(*type).element;

        const auto& visitArgument = [&](auto& argument) {
            if (isStruct)
                visit(argument);
            else
                visit(arrayElementType, argument);
        };

        m_stringBuilder.append("{\n");
        {
            IndentationScope scope(m_indent);
            for (auto& argument : call.arguments()) {
                m_stringBuilder.append(m_indent);
                visitArgument(argument);
                m_stringBuilder.append(",\n");
            }
        }
        m_stringBuilder.append(m_indent, "}");
        return;
    }

    if (is<AST::NamedTypeName>(call.target())) {
        static constexpr std::pair<ComparableASCIILiteral, void(*)(FunctionDefinitionWriter*, AST::CallExpression&)> builtinMappings[] {
            { "textureLoad", [](FunctionDefinitionWriter* writer, AST::CallExpression& call) {
                auto& texture = call.arguments()[0];
                auto* textureType = texture.inferredType();

                // FIXME: this should become isPrimitiveReference once PR#14299 lands
                auto* primitive = std::get_if<Types::Primitive>(textureType);
                bool isExternalTexture = primitive && primitive->kind == Types::Primitive::TextureExternal;
                if (!isExternalTexture) {
                    writer->visit(call.arguments()[0]);
                    writer->stringBuilder().append(".read");
                    visitArguments(writer, call, 1);
                    return;
                }

                auto& coordinates = call.arguments()[1];
                writer->stringBuilder().append("({\n");
                {
                    IndentationScope scope(writer->indent());
                    {
                        writer->stringBuilder().append(writer->indent(), "auto __coords = (");
                        writer->visit(texture);
                        writer->stringBuilder().append(".UVRemapMatrix * float3(");
                        writer->visit(coordinates);
                        writer->stringBuilder().append(", 1)).xy;\n");
                    }
                    {
                        writer->stringBuilder().append(writer->indent(), "auto __y = float(");
                        writer->visit(texture);
                        writer->stringBuilder().append(".FirstPlane.read(__cords).r);\n");
                    }
                    {
                        writer->stringBuilder().append(writer->indent(), "auto __cbcr = float2(");
                        writer->visit(texture);
                        writer->stringBuilder().append(".SecondPlane.read(__coords).rg);\n");
                    }
                    writer->stringBuilder().append(writer->indent(), "auto __ycbcr = float3(__y, __cbcr);\n");
                    {
                        writer->stringBuilder().append(writer->indent(), "float4(");
                        writer->visit(texture);
                        writer->stringBuilder().append(".ColorSpaceConversionMatrix * float4(__ycbcr, 1), 1);\n");
                    }
                }
                writer->stringBuilder().append(writer->indent(), "})");
            } },
            { "textureSample", [](FunctionDefinitionWriter* writer, AST::CallExpression& call) {
                ASSERT(call.arguments().size() > 1);
                writer->visit(call.arguments()[0]);
                writer->stringBuilder().append(".sample");
                visitArguments(writer, call, 1);
            } },
            { "textureSampleBaseClampToEdge", [](FunctionDefinitionWriter* writer, AST::CallExpression& call) {
                // FIXME: we need to handle `texture2d<T>` here too, not only `texture_external`
                auto& texture = call.arguments()[0];
                auto& sampler = call.arguments()[1];
                auto& coordinates = call.arguments()[2];
                writer->stringBuilder().append("({\n");
                {
                    IndentationScope scope(writer->indent());
                    {
                        writer->stringBuilder().append(writer->indent(), "auto __coords = (");
                        writer->visit(texture);
                        writer->stringBuilder().append(".UVRemapMatrix * float3(");
                        writer->visit(coordinates);
                        writer->stringBuilder().append(", 1)).xy;\n");
                    }
                    {
                        writer->stringBuilder().append(writer->indent(), "auto __y = float(");
                        writer->visit(texture);
                        writer->stringBuilder().append(".FirstPlane.sample(");
                        writer->visit(sampler);
                        writer->stringBuilder().append(", __coords).r);\n");
                    }
                    {
                        writer->stringBuilder().append(writer->indent(), "auto __cbcr = float2(");
                        writer->visit(texture);
                        writer->stringBuilder().append(".SecondPlane.sample(");
                        writer->visit(sampler);
                        writer->stringBuilder().append(", __coords).rg);\n");
                    }
                    writer->stringBuilder().append(writer->indent(), "auto __ycbcr = float3(__y, __cbcr);\n");
                    {
                        writer->stringBuilder().append(writer->indent(), "float4(");
                        writer->visit(texture);
                        writer->stringBuilder().append(".ColorSpaceConversionMatrix * float4(__ycbcr, 1), 1);\n");
                    }
                }
                writer->stringBuilder().append(writer->indent(), "})");
            } },
        };
        static constexpr SortedArrayMap builtins { builtinMappings };
        const auto& targetName = downcast<AST::NamedTypeName>(call.target()).name().id();
        if (auto mappedBuiltin = builtins.get(targetName)) {
            mappedBuiltin(this, call);
            return;
        }

        static constexpr std::pair<ComparableASCIILiteral, ASCIILiteral> baseTypesMappings[] {
            { "f32", "float"_s },
            { "i32", "int"_s },
            { "u32", "unsigned"_s }
        };
        static constexpr SortedArrayMap baseTypes { baseTypesMappings };

        if (AST::ParameterizedTypeName::stringViewToKind(targetName).has_value())
            visit(type);
        else if (auto mappedName = baseTypes.get(targetName))
            m_stringBuilder.append(mappedName);
        else
            m_stringBuilder.append(targetName);
        visitArguments(this, call);
        return;
    }

    visit(type);
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
    case AST::UnaryOperation::Not:
        m_stringBuilder.append("!");
        break;

    case AST::UnaryOperation::AddressOf:
    case AST::UnaryOperation::Dereference:
        // FIXME: Implement these
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
    visit(unary.expression());
}

void FunctionDefinitionWriter::visit(AST::BinaryExpression& binary)
{
    if (binary.operation() == AST::BinaryOperation::Modulo) {
        auto* leftType = binary.leftExpression().inferredType();
        auto* rightType = binary.rightExpression().inferredType();
        if (satisfies(leftType, Constraints::Float) || satisfies(rightType, Constraints::Float)) {
            m_stringBuilder.append("fmod(");
            visit(binary.leftExpression());
            m_stringBuilder.append(", ");
            visit(binary.rightExpression());
            m_stringBuilder.append(")");
            return;
        }
    }

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
        m_stringBuilder.append(" && ");
        break;
    case AST::BinaryOperation::ShortCircuitOr:
        m_stringBuilder.append(" || ");
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
    auto& primitiveType = std::get<Types::Primitive>(*literal.inferredType());
    if (primitiveType.kind == Types::Primitive::U32)
        m_stringBuilder.append("u");
}

void FunctionDefinitionWriter::visit(AST::Signed32Literal& literal)
{
    // FIXME: this might not serialize all values correctly
    m_stringBuilder.append(literal.value());
}

void FunctionDefinitionWriter::visit(AST::Unsigned32Literal& literal)
{
    // FIXME: this might not serialize all values correctly
    m_stringBuilder.append(literal.value(), "u");
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
}

void FunctionDefinitionWriter::visit(AST::CompoundStatement& statement)
{
    m_stringBuilder.append("{\n");
    {
        IndentationScope scope(m_indent);
        for (auto& statement : statement.statements()) {
            m_stringBuilder.append(m_indent);
            checkErrorAndVisit(statement);
            switch (statement.kind()) {
            case AST::NodeKind::AssignmentStatement:
            case AST::NodeKind::BreakStatement:
            case AST::NodeKind::CallStatement:
            case AST::NodeKind::CompoundAssignmentStatement:
            case AST::NodeKind::ContinueStatement:
            case AST::NodeKind::DecrementIncrementStatement:
            case AST::NodeKind::DiscardStatement:
            case AST::NodeKind::PhonyAssignmentStatement:
            case AST::NodeKind::ReturnStatement:
            case AST::NodeKind::VariableStatement:
                m_stringBuilder.append(';');
                break;
            default:
                break;
            }
            m_stringBuilder.append('\n');
        }
    }
    m_stringBuilder.append(m_indent, "}");
}

void FunctionDefinitionWriter::visit(AST::DecrementIncrementStatement& statement)
{
    visit(statement.expression());
    switch (statement.operation()) {
    case AST::DecrementIncrementStatement::Operation::Increment:
        m_stringBuilder.append("++");
        break;
    case AST::DecrementIncrementStatement::Operation::Decrement:
        m_stringBuilder.append("--");
        break;
    }
}

void FunctionDefinitionWriter::visit(AST::IfStatement& statement)
{
    m_stringBuilder.append("if (");
    visit(statement.test());
    m_stringBuilder.append(") ");
    visit(statement.trueBody());
    if (statement.maybeFalseBody()) {
        m_stringBuilder.append(" else ");
        visit(*statement.maybeFalseBody());
    }
}

void FunctionDefinitionWriter::visit(AST::PhonyAssignmentStatement& statement)
{
    m_stringBuilder.append("(void)(");
    visit(statement.rhs());
    m_stringBuilder.append(")");
}

void FunctionDefinitionWriter::visit(AST::ReturnStatement& statement)
{
    m_stringBuilder.append("return");
    if (statement.maybeExpression()) {
        m_stringBuilder.append(" ");
        visit(*statement.maybeExpression());
    }
}

void FunctionDefinitionWriter::visit(AST::ForStatement& statement)
{
    m_stringBuilder.append("for (");
    if (auto* initializer = statement.maybeInitializer())
        visit(*initializer);
    m_stringBuilder.append(";");
    if (auto* test = statement.maybeTest()) {
        m_stringBuilder.append(" ");
        visit(*test);
    }
    m_stringBuilder.append(";");
    if (auto* update = statement.maybeUpdate()) {
        m_stringBuilder.append(" ");
        visit(*update);
    }
    m_stringBuilder.append(") ");
    visit(statement.body());
}

void FunctionDefinitionWriter::visit(AST::BreakStatement&)
{
    m_stringBuilder.append("break");
}

void FunctionDefinitionWriter::visit(AST::ContinueStatement&)
{
    m_stringBuilder.append("continue");
}

void emitMetalFunctions(StringBuilder& stringBuilder, CallGraph& callGraph)
{
    FunctionDefinitionWriter functionDefinitionWriter(callGraph, stringBuilder);
    functionDefinitionWriter.write();
}

} // namespace Metal
} // namespace WGSL
