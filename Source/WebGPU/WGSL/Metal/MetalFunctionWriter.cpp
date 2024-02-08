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
#include "ASTInterpolateAttribute.h"
#include "ASTStringDumper.h"
#include "ASTVisitor.h"
#include "CallGraph.h"
#include "Constraints.h"
#include "Types.h"
#include "WGSLShaderModule.h"
#include <numbers>
#include <wtf/HashSet.h>
#include <wtf/SetForScope.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/text/StringBuilder.h>

namespace WGSL {

namespace Metal {

class FunctionDefinitionWriter : public AST::Visitor {
public:
    FunctionDefinitionWriter(const CallGraph& callGraph, StringBuilder& stringBuilder, const HashMap<String, ConstantValue>& constantValues)
        : m_stringBuilder(stringBuilder)
        , m_callGraph(callGraph)
        , m_constantValues(constantValues)
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
    void visit(AST::InterpolateAttribute&) override;

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
    void visit(AST::Float16Literal&) override;
    void visit(AST::IdentifierExpression&) override;
    void visit(AST::IndexAccessExpression&) override;
    void visit(AST::PointerDereferenceExpression&) override;
    void visit(AST::UnaryExpression&) override;
    void visit(AST::Signed32Literal&) override;
    void visit(AST::Unsigned32Literal&) override;

    void visit(AST::Statement&) override;
    void visit(AST::AssignmentStatement&) override;
    void visit(AST::CallStatement&) override;
    void visit(AST::CompoundAssignmentStatement&) override;
    void visit(AST::CompoundStatement&) override;
    void visit(AST::DecrementIncrementStatement&) override;
    void visit(AST::DiscardStatement&) override;
    void visit(AST::IfStatement&) override;
    void visit(AST::PhonyAssignmentStatement&) override;
    void visit(AST::ReturnStatement&) override;
    void visit(AST::ForStatement&) override;
    void visit(AST::LoopStatement&) override;
    void visit(AST::Continuing&) override;
    void visit(AST::WhileStatement&) override;
    void visit(AST::SwitchStatement&) override;
    void visit(AST::BreakStatement&) override;
    void visit(AST::ContinueStatement&) override;

    void visit(AST::Parameter&) override;
    void visitArgumentBufferParameter(AST::Parameter&);

    void visit(const Type*);

    StringBuilder& stringBuilder() { return m_stringBuilder; }
    Indentation<4>& indent() { return m_indent; }

private:
    void emitNecessaryHelpers();
    void visitGlobal(AST::Variable&);
    void serializeVariable(AST::Variable&);
    void generatePackingHelpers(AST::Structure&);
    bool emitPackedVector(const Types::Vector&);
    void serializeConstant(const Type*, ConstantValue);
    void serializeBinaryExpression(AST::Expression&, AST::BinaryOperation, AST::Expression&);
    void visitStatements(AST::Statement::List&);

    StringBuilder& m_stringBuilder;
    const CallGraph& m_callGraph;
    Indentation<4> m_indent { 0 };
    std::optional<AST::StructureRole> m_structRole;
    std::optional<ShaderStage> m_entryPointStage;
    unsigned m_functionConstantIndex { 0 };
    AST::Continuing*m_continuing { nullptr };
    HashSet<AST::Function*> m_visitedFunctions;
    const HashMap<String, ConstantValue>& m_constantValues;
    HashMap<String, ConstantValue> m_overrides;
};

static const char* serializeAddressSpace(AddressSpace addressSpace)
{
    switch (addressSpace) {
    case AddressSpace::Function:
    case AddressSpace::Private:
        return "thread";
    case AddressSpace::Workgroup:
        return "threadgroup";
    case AddressSpace::Uniform:
        return "constant";
    case AddressSpace::Storage:
        return "device";
    case AddressSpace::Handle:
        return nullptr;
    }
}

void FunctionDefinitionWriter::write()
{
    emitNecessaryHelpers();

    for (auto& declaration : m_callGraph.ast().declarations()) {
        if (is<AST::Structure>(declaration))
            visit(downcast<AST::Structure>(declaration));
        else if (is<AST::Variable>(declaration))
            visitGlobal(downcast<AST::Variable>(declaration));
    }

    for (auto& declaration : m_callGraph.ast().declarations()) {
        if (is<AST::Structure>(declaration))
            generatePackingHelpers(downcast<AST::Structure>(declaration));
    }

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
            m_stringBuilder.append(m_indent, "uint get_width(uint lod = 0) const { return FirstPlane.get_width(lod); }\n");
            m_stringBuilder.append(m_indent, "uint get_height(uint lod = 0) const { return FirstPlane.get_height(lod); }\n");
        }
        m_stringBuilder.append("};\n\n");
    }

    if (m_callGraph.ast().usesPackArray()) {
        m_callGraph.ast().clearUsesPackArray();
        m_stringBuilder.append(m_indent, "template<typename T, size_t N>\n");
        m_stringBuilder.append(m_indent, "array<typename T::PackedType, N> __pack(array<T, N> unpacked)\n");
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
        m_stringBuilder.append(m_indent, "array<typename T::UnpackedType, N> __unpack(array<T, N> packed)\n");
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

    if (m_callGraph.ast().usesWorkgroupUniformLoad()) {
        m_stringBuilder.append(m_indent, "template<typename T>\n");
        m_stringBuilder.append(m_indent, "T __workgroup_uniform_load(threadgroup T* const ptr)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "threadgroup_barrier(mem_flags::mem_threadgroup);\n");
            m_stringBuilder.append(m_indent, "auto result = *ptr;\n");
            m_stringBuilder.append(m_indent, "threadgroup_barrier(mem_flags::mem_threadgroup);\n");
            m_stringBuilder.append(m_indent, "return result;\n");
        }
        m_stringBuilder.append(m_indent, "}\n\n");
    }

    if (m_callGraph.ast().usesDivision()) {
        m_stringBuilder.append(m_indent, "template<typename T, typename U, typename V = conditional_t<is_scalar_v<U>, T, U>>\n");
        m_stringBuilder.append(m_indent, "V __wgslDiv(T lhs, U rhs)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto predicate = V(rhs) == V(0);\n");
            m_stringBuilder.append(m_indent, "if constexpr (is_signed_v<U>)\n");
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, "predicate = predicate || (V(lhs) == V(numeric_limits<T>::lowest()) && V(rhs) == V(-1));\n");
            }
            m_stringBuilder.append(m_indent, "return lhs / select(V(rhs), V(1), predicate);\n");
        }
        m_stringBuilder.append(m_indent, "}\n\n");
    }

    if (m_callGraph.ast().usesModulo()) {
        m_stringBuilder.append(m_indent, "template<typename T, typename U, typename V = conditional_t<is_scalar_v<U>, T, U>>\n");
        m_stringBuilder.append(m_indent, "V __wgslMod(T lhs, U rhs)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto predicate = V(rhs) == V(0);\n");
            m_stringBuilder.append(m_indent, "if constexpr (is_signed_v<U>)\n");
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, "predicate = predicate || (V(lhs) == V(numeric_limits<T>::lowest()) && V(rhs) == V(-1));\n");
            }
            m_stringBuilder.append(m_indent, "return select(lhs % V(rhs), V(0), predicate);\n");
        }
        m_stringBuilder.append(m_indent, "}\n\n");
    }


    if (m_callGraph.ast().usesFrexp()) {
        m_stringBuilder.append(m_indent, "template<typename T, typename U>\n");
        m_stringBuilder.append(m_indent, "struct __frexp_result {\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "T fract;\n");
            m_stringBuilder.append(m_indent, "U exp;\n");
        }
        m_stringBuilder.append(m_indent, "};\n\n");

        m_stringBuilder.append(m_indent, "template<typename T, typename U = conditional_t<is_vector_v<T>, vec<int, vec_elements<T>::value ?: 2>, int>>\n");
        m_stringBuilder.append(m_indent, "__frexp_result<T, U> __wgslFrexp(T value)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "__frexp_result<T, U> result;\n");
            m_stringBuilder.append(m_indent, "result.fract = frexp(value, result.exp);\n");
            m_stringBuilder.append(m_indent, "return result;\n");
        }
        m_stringBuilder.append(m_indent, "}\n\n");
    }

    if (m_callGraph.ast().usesModf()) {
        m_stringBuilder.append(m_indent, "template<typename T, typename U>\n");
        m_stringBuilder.append(m_indent, "struct __modf_result {\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "T fract;\n");
            m_stringBuilder.append(m_indent, "U whole;\n");
        }
        m_stringBuilder.append(m_indent, "};\n\n");

        m_stringBuilder.append(m_indent, "template<typename T>\n");
        m_stringBuilder.append(m_indent, "__modf_result<T, T> __wgslModf(T value)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "__modf_result<T, T> result;\n");
            m_stringBuilder.append(m_indent, "result.fract = modf(value, result.whole);\n");
            m_stringBuilder.append(m_indent, "return result;\n");
        }
        m_stringBuilder.append(m_indent, "}\n\n");
    }

    if (m_callGraph.ast().usesAtomicCompareExchange()) {
        m_stringBuilder.append(m_indent, "template<typename T, typename U = bool>\n");
        m_stringBuilder.append(m_indent, "struct __atomic_compare_exchange_result {\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "T old_value;\n");
            m_stringBuilder.append(m_indent, "U exchanged;\n");
        }
        m_stringBuilder.append(m_indent, "};\n\n");

        m_stringBuilder.append(m_indent, "#define __wgslAtomicCompareExchangeWeak(atomic, compare, value) \\\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "({ auto innerCompare = compare; \\\n");
            m_stringBuilder.append(m_indent, "bool exchanged = atomic_compare_exchange_weak_explicit((atomic), &innerCompare, value, memory_order_relaxed, memory_order_relaxed); \\\n");
            m_stringBuilder.append(m_indent, "__atomic_compare_exchange_result<decltype(compare)> { innerCompare, exchanged }; \\\n");
            m_stringBuilder.append(m_indent, "})\n");
        }
    }

    if (m_callGraph.ast().usesDot()) {
        m_stringBuilder.append(m_indent, "template<typename T, unsigned N>\n");
        m_stringBuilder.append(m_indent, "T __wgslDot(vec<T, N> lhs, vec<T, N> rhs)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto result = lhs[0] * rhs[0] + lhs[1] * rhs[1];\n");
            m_stringBuilder.append(m_indent, "if constexpr (N > 2) result += lhs[2] * rhs[2];\n");
            m_stringBuilder.append(m_indent, "if constexpr (N > 3) result += lhs[3] * rhs[3];\n");
            m_stringBuilder.append(m_indent, "return result;\n");
        }
        m_stringBuilder.append(m_indent, "}\n");
    }

    if (m_callGraph.ast().usesDot4I8Packed()) {
        m_stringBuilder.append(m_indent, "int __wgslDot4I8Packed(uint lhs, uint rhs)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto vec1 = as_type<packed_char4>(lhs);");
            m_stringBuilder.append(m_indent, "auto vec2 = as_type<packed_char4>(rhs);");
            m_stringBuilder.append(m_indent, "return vec1[0] * vec2[0] + vec1[1] * vec2[1] + vec1[2] * vec2[2] + vec1[3] * vec2[3];");
        }
        m_stringBuilder.append(m_indent, "}\n");
    }

    if (m_callGraph.ast().usesDot4U8Packed()) {
        m_stringBuilder.append(m_indent, "uint __wgslDot4U8Packed(uint lhs, uint rhs)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto vec1 = as_type<packed_uchar4>(lhs);");
            m_stringBuilder.append(m_indent, "auto vec2 = as_type<packed_uchar4>(rhs);");
            m_stringBuilder.append(m_indent, "return vec1[0] * vec2[0] + vec1[1] * vec2[1] + vec1[2] * vec2[2] + vec1[3] * vec2[3];");
        }
        m_stringBuilder.append(m_indent, "}\n");
    }

    if (m_callGraph.ast().usesFirstLeadingBit()) {
        m_stringBuilder.append(m_indent, "template<typename T>\n");
        m_stringBuilder.append(m_indent, "T __wgslFirstLeadingBit(T e)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "if constexpr (is_signed_v<T>)\n");
            m_stringBuilder.append(m_indent, "    return select(T(31 - select(clz(e), clz(~e), e < T(0))), T(-1), e == T(0) || e == T(-1));\n");
            m_stringBuilder.append(m_indent, "else\n");
            m_stringBuilder.append(m_indent, "    return select(T(31 - clz(e)), T(-1), e == T(0));\n");
        }
        m_stringBuilder.append(m_indent, "}\n");
    }

    if (m_callGraph.ast().usesFirstTrailingBit()) {
        m_stringBuilder.append(m_indent, "template<typename T>\n");
        m_stringBuilder.append(m_indent, "T __wgslFirstTrailingBit(T e)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "return select(ctz(e), T(-1), e == T(0));\n");
        }
        m_stringBuilder.append(m_indent, "}\n");
    }

    if (m_callGraph.ast().usesSign()) {
        m_stringBuilder.append(m_indent, "template<typename T>\n");
        m_stringBuilder.append(m_indent, "T __wgslSign(T e)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "return select(select(T(-1), T(1), e > 0), T(0), e == 0);\n");
        }
        m_stringBuilder.append(m_indent, "}\n");
    }

    if (m_callGraph.ast().usesExtractBits()) {
        m_stringBuilder.append(m_indent, "template<typename T>\n");
        m_stringBuilder.append(m_indent, "T __wgslExtractBits(T e, uint offset, uint count)\n");
        m_stringBuilder.append(m_indent, "{\n");
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto o = min(offset, 32u);\n");
            m_stringBuilder.append(m_indent, "auto c = min(count, 32u - o);\n");
            m_stringBuilder.append(m_indent, "return extract_bits(e, o, c);\n");
        }
        m_stringBuilder.append(m_indent, "}\n");
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
        visit(functionDefinition.maybeReturnType()->inferredType());
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
            auto* type = member.type().inferredType();
            if (isPrimitiveReference(type, Types::Primitive::TextureExternal)) {
                m_stringBuilder.append(m_indent, "texture2d<float> __", name, "_FirstPlane;\n");
                m_stringBuilder.append(m_indent, "texture2d<float> __", name, "_SecondPlane;\n");
                m_stringBuilder.append(m_indent, "float3x2 __", name, "_UVRemapMatrix;\n");
                m_stringBuilder.append(m_indent, "float4x3 __", name, "_ColorSpaceConversionMatrix;\n");
                continue;
            }

            m_stringBuilder.append(m_indent);
            visit(member.type().inferredType());
            m_stringBuilder.append(" ", name);
            for (auto &attribute : member.attributes()) {
                m_stringBuilder.append(" ");
                visit(attribute);
            }
            m_stringBuilder.append(";\n");

            if (shouldPack && member.padding())
                addPadding(member.padding());
        }

        if (structDecl.role() == AST::StructureRole::VertexOutput || structDecl.role() == AST::StructureRole::FragmentOutput) {
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
        } else if (structDecl.role() == AST::StructureRole::FragmentOutputWrapper) {
            ASSERT(structDecl.members().size() == 1);
            auto& member = structDecl.members()[0];

            m_stringBuilder.append("\n");
            m_stringBuilder.append(m_indent, "template<typename T>\n");
            m_stringBuilder.append(m_indent, structDecl.name(), "(T value)\n");
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, ": ", member.name(), "(value)\n");
            }
            m_stringBuilder.append(m_indent, "{ }\n");
        }
    }
    m_stringBuilder.append(m_indent, "};\n\n");
    m_structRole = std::nullopt;
}

void FunctionDefinitionWriter::generatePackingHelpers(AST::Structure& structure)
{
    if (structure.role() != AST::StructureRole::PackedResource || !structure.inferredType()->isConstructible())
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
            if (member.type().inferredType()->packing() == Packing::PackedStruct)
                m_stringBuilder.append(m_indent, "packed.", name, " = __pack(unpacked.", name, ");\n");
            else
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
            if (member.type().inferredType()->packing() == Packing::PackedStruct)
                m_stringBuilder.append(m_indent, "unpacked.", name, " = __unpack(packed.", name, ");\n");
            else
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
    case Types::Primitive::F16:
        m_stringBuilder.append("packed_half", String::number(vector.size));
        break;
    case Types::Primitive::Bool:
    case Types::Primitive::Void:
    case Types::Primitive::Sampler:
    case Types::Primitive::SamplerComparison:
    case Types::Primitive::TextureExternal:
    case Types::Primitive::AccessMode:
    case Types::Primitive::TexelFormat:
    case Types::Primitive::AddressSpace:
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

    String entryName = variable.originalName();
    if (variable.id())
        entryName = String::number(*variable.id());
    auto it = m_constantValues.find(entryName);
    if (it != m_constantValues.end()) {
        m_overrides.add(variable.name(), it->value);
        return;
    }
    auto* initializer = variable.maybeInitializer();
    if (!initializer)
        return;
    if (auto& maybeConstant = initializer->constantValue())
        m_overrides.add(variable.name(), *maybeConstant);
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

    if (auto* qualifier = variable.maybeQualifier()) {
        switch (qualifier->addressSpace()) {
        case AddressSpace::Workgroup:
            m_stringBuilder.append("threadgroup ");
            break;
        case AddressSpace::Function:
        case AddressSpace::Handle:
        case AddressSpace::Private:
        case AddressSpace::Storage:
        case AddressSpace::Uniform:
            break;
        }
    }

    visit(type);
    m_stringBuilder.append(" ", variable.name());

    if (variable.flavor() == AST::VariableFlavor::Override)
        return;

    if (auto* initializer = variable.maybeInitializer()) {
        m_stringBuilder.append(" = ");
        visit(type, *initializer);
    } else
        m_stringBuilder.append(" { }");
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
    if (m_structRole.has_value() && *m_structRole != AST::StructureRole::VertexOutput && *m_structRole != AST::StructureRole::FragmentOutput && *m_structRole != AST::StructureRole::FragmentOutputWrapper)
        return;

    switch (builtin.builtin()) {
    case Builtin::FragDepth:
        m_stringBuilder.append("[[depth(any)]]");
        break;
    case Builtin::FrontFacing:
        m_stringBuilder.append("[[front_facing]]");
        break;
    case Builtin::GlobalInvocationId:
        m_stringBuilder.append("[[thread_position_in_grid]]");
        break;
    case Builtin::InstanceIndex:
        m_stringBuilder.append("[[instance_id]]");
        break;
    case Builtin::LocalInvocationId:
        m_stringBuilder.append("[[thread_position_in_threadgroup]]");
        break;
    case Builtin::LocalInvocationIndex:
        m_stringBuilder.append("[[thread_index_in_threadgroup]]");
        break;
    case Builtin::NumWorkgroups:
        m_stringBuilder.append("[[threadgroups_per_grid]]");
        break;
    case Builtin::Position:
        m_stringBuilder.append("[[position]]");
        break;
    case Builtin::SampleIndex:
        m_stringBuilder.append("[[sample_id]]");
        break;
    case Builtin::SampleMask:
        m_stringBuilder.append("[[sample_mask]]");
        break;
    case Builtin::VertexIndex:
        m_stringBuilder.append("[[vertex_id]]");
        break;
    case Builtin::WorkgroupId:
        m_stringBuilder.append("[[threadgroup_position_in_grid]]");
        break;
    }
}

void FunctionDefinitionWriter::visit(AST::StageAttribute& stage)
{
    m_entryPointStage = { stage.stage() };
    switch (stage.stage()) {
    case ShaderStage::Vertex:
        m_stringBuilder.append("[[vertex]]");
        break;
    case ShaderStage::Fragment:
        m_stringBuilder.append("[[fragment]]");
        break;
    case ShaderStage::Compute:
        m_stringBuilder.append("[[kernel]]");
        break;
    }
}

void FunctionDefinitionWriter::visit(AST::GroupAttribute& group)
{
    unsigned bufferIndex = group.group().constantValue()->integerValue();
    if (m_entryPointStage.has_value() && *m_entryPointStage == ShaderStage::Vertex) {
        ASSERT(m_callGraph.ast().configuration().maxBuffersPlusVertexBuffersForVertexStage > 0);
        auto max = m_callGraph.ast().configuration().maxBuffersPlusVertexBuffersForVertexStage - 1;
        bufferIndex = vertexBufferIndexForBindGroup(bufferIndex, max);
    }
    m_stringBuilder.append("[[buffer(", bufferIndex, ")]]");
}

void FunctionDefinitionWriter::visit(AST::BindingAttribute& binding)
{
    m_stringBuilder.append("[[id(", binding.binding().constantValue()->integerValue(), ")]]");
}

void FunctionDefinitionWriter::visit(AST::LocationAttribute& location)
{
    if (m_structRole.has_value()) {
        auto role = *m_structRole;
        switch (role) {
        case AST::StructureRole::VertexOutput:
        case AST::StructureRole::FragmentInput:
            m_stringBuilder.append("[[user(loc", location.location().constantValue()->integerValue(), ")]]");
            return;
        case AST::StructureRole::BindGroup:
        case AST::StructureRole::UserDefined:
        case AST::StructureRole::ComputeInput:
        case AST::StructureRole::UserDefinedResource:
        case AST::StructureRole::PackedResource:
            return;
        case AST::StructureRole::FragmentOutputWrapper:
            RELEASE_ASSERT_NOT_REACHED();
        case AST::StructureRole::FragmentOutput:
            m_stringBuilder.append("[[color(", location.location().constantValue()->integerValue(), ")]]");
            return;
        case AST::StructureRole::VertexInput:
            m_stringBuilder.append("[[attribute(", location.location().constantValue()->integerValue(), ")]]");
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

static const char* convertToSampleMode(InterpolationType type, InterpolationSampling sampleType)
{
    switch (type) {
    case InterpolationType::Flat:
        return "flat";
    case InterpolationType::Linear:
        switch (sampleType) {
        case InterpolationSampling::Center:
            return "center_no_perspective";
        case InterpolationSampling::Centroid:
            return "centroid_no_perspective";
        case InterpolationSampling::Sample:
            return "sample_no_perspective";
        }
    case InterpolationType::Perspective:
        switch (sampleType) {
        case InterpolationSampling::Center:
            return "center_perspective";
        case InterpolationSampling::Centroid:
            return "centroid_perspective";
        case InterpolationSampling::Sample:
            return "sample_perspective";
        }
    }

    ASSERT_NOT_REACHED();
    return "flat";
}

void FunctionDefinitionWriter::visit(AST::InterpolateAttribute& attribute)
{
    m_stringBuilder.append("[[", convertToSampleMode(attribute.type(), attribute.sampling()), "]]");
}

// Types
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
            case Types::Primitive::F16:
                m_stringBuilder.append("half");
                break;
            case Types::Primitive::Void:
            case Types::Primitive::Bool:
            case Types::Primitive::Sampler:
                m_stringBuilder.append(*type);
                break;
            case Types::Primitive::SamplerComparison:
                m_stringBuilder.append("sampler");
                break;
            case Types::Primitive::TextureExternal:
                m_stringBuilder.append("texture_external");
                break;
            case Types::Primitive::AccessMode:
            case Types::Primitive::TexelFormat:
            case Types::Primitive::AddressSpace:
                RELEASE_ASSERT_NOT_REACHED();
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
            m_stringBuilder.append(", ");
            WTF::switchOn(array.size,
                [&](unsigned size) { m_stringBuilder.append(size); },
                [&](std::monostate) { m_stringBuilder.append(1); },
                [&](AST::Expression* size) {
                    visit(*size);
                });
            m_stringBuilder.append(">");
        },
        [&](const Struct& structure) {
            m_stringBuilder.append(structure.structure.name());
            if (m_structRole.has_value() && *m_structRole == AST::StructureRole::PackedResource && structure.structure.role() == AST::StructureRole::UserDefinedResource)
                m_stringBuilder.append("::PackedType");
        },
        [&](const PrimitiveStruct& structure) {
            m_stringBuilder.append(structure.name, "<");
            bool first = true;
            for (auto& value : structure.values) {
                if (!first)
                    m_stringBuilder.append(", ");
                first = false;
                visit(value);
            }
            m_stringBuilder.append(">");
        },
        [&](const Texture& texture) {
            const char* type;
            const char* access = "sample";
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
                access = "read";
                break;
            }
            m_stringBuilder.append(type, "<");
            visit(texture.element);
            m_stringBuilder.append(", access::", access, ">");
        },
        [&](const TextureStorage& texture) {
            const char* base;
            const char* mode;
            switch (texture.kind) {
            case Types::TextureStorage::Kind::TextureStorage1d:
                base = "texture1d";
                break;
            case Types::TextureStorage::Kind::TextureStorage2d:
                base = "texture2d";
                break;
            case Types::TextureStorage::Kind::TextureStorage2dArray:
                base = "texture2d_array";
                break;
            case Types::TextureStorage::Kind::TextureStorage3d:
                base = "texture3d";
                break;
            }
            switch (texture.access) {
            case AccessMode::Read:
                mode = "read";
                break;
            case AccessMode::Write:
                mode = "write";
                break;
            case AccessMode::ReadWrite:
                mode = "read_write";
                break;
            }
            m_stringBuilder.append(base, "<");
            visit(shaderTypeForTexelFormat(texture.format, m_callGraph.ast().types()));
            m_stringBuilder.append(", access::", mode, ">");
        },
        [&](const TextureDepth& texture) {
            const char* base;
            switch (texture.kind) {
            case TextureDepth::Kind::TextureDepth2d:
                base = "depth2d";
                break;
            case TextureDepth::Kind::TextureDepth2dArray:
                base = "depth2d_array";
                break;
            case TextureDepth::Kind::TextureDepthCube:
                base = "depthcube";
                break;
            case TextureDepth::Kind::TextureDepthCubeArray:
                base = "depthcube_array";
                break;
            case TextureDepth::Kind::TextureDepthMultisampled2d:
                base = "depth2d_ms";
                break;
            }
            m_stringBuilder.append(base, "<float>");
        },
        [&](const Reference& reference) {
            const char* addressSpace = serializeAddressSpace(reference.addressSpace);
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
        [&](const Pointer& pointer) {
            const char* addressSpace = serializeAddressSpace(pointer.addressSpace);
            if (pointer.accessMode == AccessMode::Read)
                m_stringBuilder.append("const ");
            if (addressSpace)
                m_stringBuilder.append(addressSpace, " ");
            visit(pointer.element);
            m_stringBuilder.append("*");
        },
        [&](const Atomic& atomic) {
            if (atomic.element == m_callGraph.ast().types().i32Type())
                m_stringBuilder.append("atomic_int");
            else
                m_stringBuilder.append("atomic_uint");
        },
        [&](const Function&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const TypeConstructor&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Bottom&) {
            RELEASE_ASSERT_NOT_REACHED();
        });
}

void FunctionDefinitionWriter::visit(AST::Parameter& parameter)
{
    visit(parameter.typeName().inferredType());
    m_stringBuilder.append(" ", parameter.name());
    for (auto& attribute : parameter.attributes()) {
        m_stringBuilder.append(" ");
        checkErrorAndVisit(attribute);
    }
}

void FunctionDefinitionWriter::visitArgumentBufferParameter(AST::Parameter& parameter)
{
    m_stringBuilder.append("constant ");
    visit(parameter.typeName().inferredType());
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
    if (auto constantValue = expression.constantValue()) {
        serializeConstant(type, *constantValue);
        return;
    }

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
    writer->stringBuilder().append("(");
    for (unsigned i = startOffset; i < call.arguments().size(); ++i) {
        if (i != startOffset)
            writer->stringBuilder().append(", ");
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(")");
}

static void emitTextureDimensions(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    const auto& get = [&](const char* property) {
        writer->visit(call.arguments()[0]);
        writer->stringBuilder().append(".get_", property, "(");
        if (call.arguments().size() > 1)
            writer->visit(call.arguments()[1]);
        writer->stringBuilder().append(")");
    };

    const auto* vector = std::get_if<Types::Vector>(call.inferredType());
    if (!vector) {
        get("width");
        return;
    }

    auto size = vector->size;
    ASSERT(size >= 2 && size <= 3);
    writer->stringBuilder().append("uint", String::number(size), "(");
    get("width");
    writer->stringBuilder().append(", ");
    get("height");
    if (size > 2) {
        writer->stringBuilder().append(", ");
        get("depth");
    }
    writer->stringBuilder().append(")");
}

static void emitTextureGather(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    ASSERT(call.arguments().size() > 1);
    unsigned offset = 0;
    const char* component = nullptr;
    bool hasOffset = true;
    auto& firstArgument = call.arguments()[0];
    if (std::holds_alternative<Types::Primitive>(*firstArgument.inferredType())) {
        offset = 1;
        switch (firstArgument.constantValue()->integerValue()) {
        case 0:
            component = "x";
            break;
        case 1:
            component = "y";
            break;
        case 2:
            component = "z";
            break;
        case 3:
            component = "w";
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        auto& textureType = std::get<Types::Texture>(*call.arguments()[1].inferredType());
        if (textureType.kind == Types::Texture::Kind::Texture2d || textureType.kind == Types::Texture::Kind::Texture2dArray) {
            auto& lastArgument = call.arguments().last();
            auto* vectorType = std::get_if<Types::Vector>(lastArgument.inferredType());
            if (!vectorType || !satisfies(vectorType->element, Constraints::Integer))
                hasOffset = false;
        }
    }
    writer->visit(call.arguments()[offset]);
    writer->stringBuilder().append(".gather(");
    for (unsigned i = offset + 1; i < call.arguments().size(); ++i) {
        if (i != offset + 1)
            writer->stringBuilder().append(", ");
        writer->visit(call.arguments()[i]);
    }
    if (!hasOffset)
        writer->stringBuilder().append(", int2(0)");
    if (component)
        writer->stringBuilder().append(", component::", component);
    writer->stringBuilder().append(")");
}

static void emitTextureGatherCompare(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    ASSERT(call.arguments().size() > 1);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".gather_compare");
    visitArguments(writer, call, 1);
}

static void emitTextureLoad(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto& texture = call.arguments()[0];
    auto* textureType = texture.inferredType();

    // FIXME: this should become isPrimitiveReference once PR#14299 lands
    auto* primitive = std::get_if<Types::Primitive>(textureType);
    bool isExternalTexture = primitive && primitive->kind == Types::Primitive::TextureExternal;
    if (!isExternalTexture) {
        writer->visit(call.arguments()[0]);
        writer->stringBuilder().append(".read");
        writer->stringBuilder().append("(");
        bool is1d = true;
        const char* cast = "uint";
        if (const auto* vector = std::get_if<Types::Vector>(call.arguments()[1].inferredType())) {
            is1d = false;
            switch (vector->size) {
            case 2:
                cast = "uint2";
                break;
            case 3:
                cast = "uint3";
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
        bool first = true;
        auto argumentCount = call.arguments().size();
        for (unsigned i = 1; i < argumentCount; ++i) {
            if (first) {
                writer->stringBuilder().append(cast, "(");
                writer->visit(call.arguments()[i]);
                writer->stringBuilder().append(")");
            } else if (is1d && i == argumentCount - 1) {
                // From the MSL spec for texture1d::read:
                // > Since mipmaps are not supported for 1D textures, lod must be 0.
                continue;
            } else {
                writer->stringBuilder().append(", ");
                writer->visit(call.arguments()[i]);
            }
            first = false;
        }
        writer->stringBuilder().append(")");
        return;
    }

    auto& coordinates = call.arguments()[1];
    writer->stringBuilder().append("({\n");
    {
        IndentationScope scope(writer->indent());
        {
            writer->stringBuilder().append(writer->indent(), "auto __coords = uint2((");
            writer->visit(texture);
            writer->stringBuilder().append(".UVRemapMatrix * float3(float2(");
            writer->visit(coordinates);
            writer->stringBuilder().append("), 1)).xy);\n");
        }
        {
            writer->stringBuilder().append(writer->indent(), "auto __y = float(");
            writer->visit(texture);
            writer->stringBuilder().append(".FirstPlane.read(__coords).r);\n");
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
}

static void emitTextureSample(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    ASSERT(call.arguments().size() > 1);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".sample");
    visitArguments(writer, call, 1);
}

static void emitTextureSampleCompare(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    ASSERT(call.arguments().size() > 1);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".sample_compare");
    visitArguments(writer, call, 1);
}

static void emitTextureSampleGrad(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{

    ASSERT(call.arguments().size() > 1);
    auto& texture = call.arguments()[0];
    auto& textureType = std::get<Types::Texture>(*texture.inferredType());

    unsigned gradientIndex;
    const char* gradientFunction;
    switch (textureType.kind) {
    case Types::Texture::Kind::Texture1d:
    case Types::Texture::Kind::Texture2d:
    case Types::Texture::Kind::TextureMultisampled2d:
        gradientIndex = 3;
        gradientFunction = "gradient2d";
        break;

    case Types::Texture::Kind::Texture3d:
        gradientIndex = 3;
        gradientFunction = "gradient3d";
        break;

    case Types::Texture::Kind::TextureCube:
        gradientIndex = 3;
        gradientFunction = "gradientcube";
        break;

    case Types::Texture::Kind::Texture2dArray:
        gradientIndex = 4;
        gradientFunction = "gradient2d";
        break;

    case Types::Texture::Kind::TextureCubeArray:
        gradientIndex = 4;
        gradientFunction = "gradientcube";
        break;
    }
    writer->visit(texture);
    writer->stringBuilder().append(".sample(");
    for (unsigned i = 1; i < gradientIndex; ++i) {
        if (i != 1)
            writer->stringBuilder().append(", ");
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(", ", gradientFunction, "(");
    writer->visit(call.arguments()[gradientIndex]);
    writer->stringBuilder().append(", ");
    writer->visit(call.arguments()[gradientIndex + 1]);
    writer->stringBuilder().append(")");
    for (unsigned i = gradientIndex + 2; i < call.arguments().size(); ++i) {
        writer->stringBuilder().append(", ");
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(")");
}

static void emitTextureSampleLevel(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    bool isArray = false;
    auto& texture = call.arguments()[0];
    if (auto* textureType = std::get_if<Types::Texture>(texture.inferredType())) {
        switch (textureType->kind) {
        case Types::Texture::Kind::Texture2dArray:
        case Types::Texture::Kind::TextureCubeArray:
            isArray = true;
            break;
        case Types::Texture::Kind::Texture1d:
        case Types::Texture::Kind::Texture2d:
        case Types::Texture::Kind::Texture3d:
        case Types::Texture::Kind::TextureCube:
        case Types::Texture::Kind::TextureMultisampled2d:
            break;
        }
    } else if (auto* textureStorageType = std::get_if<Types::TextureStorage>(texture.inferredType())) {
        switch (textureStorageType->kind) {
        case Types::TextureStorage::Kind::TextureStorage2dArray:
            isArray = true;
            break;
        case Types::TextureStorage::Kind::TextureStorage1d:
        case Types::TextureStorage::Kind::TextureStorage2d:
        case Types::TextureStorage::Kind::TextureStorage3d:
            break;
        }
    } else {
        auto& textureDepthType = std::get<Types::TextureDepth>(*texture.inferredType());
        switch (textureDepthType.kind) {
        case Types::TextureDepth::Kind::TextureDepth2dArray:
        case Types::TextureDepth::Kind::TextureDepthCubeArray:
            isArray = true;
            break;
        case Types::TextureDepth::Kind::TextureDepth2d:
        case Types::TextureDepth::Kind::TextureDepthCube:
        case Types::TextureDepth::Kind::TextureDepthMultisampled2d:
            break;
        }
    }

    unsigned levelIndex = isArray ? 4 : 3;
    writer->visit(texture);
    writer->stringBuilder().append(".sample(");
    for (unsigned i = 1; i < levelIndex; ++i) {
        if (i != 1)
            writer->stringBuilder().append(",");
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(", level(");
    writer->visit(call.arguments()[levelIndex]);
    writer->stringBuilder().append(")");
    for (unsigned i = levelIndex + 1; i < call.arguments().size(); ++i) {
        writer->stringBuilder().append(",");
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(")");
}

static void emitTextureSampleBaseClampToEdge(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto& texture = call.arguments()[0];
    auto* textureType = std::get_if<Types::Texture>(texture.inferredType());

    if (textureType) {
        // FIXME: this needs to clamp the coordinates
        writer->visit(texture);
        writer->stringBuilder().append(".sample");
        visitArguments(writer, call, 1);
        return;
    }

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
}

static void emitTextureSampleBias(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto& texture = call.arguments()[0];
    auto& textureType = std::get<Types::Texture>(*texture.inferredType());
    bool isArray = false;
    switch (textureType.kind) {
    case Types::Texture::Kind::Texture2dArray:
    case Types::Texture::Kind::TextureCubeArray:
        isArray = true;
        break;
    case Types::Texture::Kind::Texture1d:
    case Types::Texture::Kind::Texture2d:
    case Types::Texture::Kind::Texture3d:
    case Types::Texture::Kind::TextureCube:
    case Types::Texture::Kind::TextureMultisampled2d:
        break;
    }

    unsigned biasIndex = isArray ? 4 : 3;
    writer->visit(texture);
    writer->stringBuilder().append(".sample(");
    for (unsigned i = 1; i < biasIndex; ++i) {
        if (i != 1)
            writer->stringBuilder().append(", ");
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(", bias(");
    writer->visit(call.arguments()[biasIndex]);
    writer->stringBuilder().append(")");
    for (unsigned i = biasIndex + 1; i < call.arguments().size(); ++i) {
        writer->stringBuilder().append(", ");
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(")");
}

static void emitTextureNumLayers(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".get_array_size()");
}

static void emitTextureNumLevels(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".get_num_mip_levels()");
}

static void emitTextureNumSamples(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".get_num_samples()");
}

static void emitTextureStore(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    const char* cast = "uint";
    if (const auto* vector = std::get_if<Types::Vector>(call.arguments()[1].inferredType())) {
        switch (vector->size) {
        case 2:
            cast = "uint2";
            break;
        case 3:
            cast = "uint3";
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    AST::Expression& texture = call.arguments()[0];
    AST::Expression& coords = call.arguments()[1];
    AST::Expression* arrayIndex = nullptr;
    AST::Expression* value = nullptr;
    if (call.arguments().size() == 3)
        value = &call.arguments()[2];
    else {
        arrayIndex = &call.arguments()[2];
        value = &call.arguments()[3];
    }

    writer->visit(texture);
    writer->stringBuilder().append(".write(");
    writer->visit(*value);
    writer->stringBuilder().append(", ", cast, "(");
    writer->visit(coords);
    writer->stringBuilder().append(")");
    if (arrayIndex) {
        writer->stringBuilder().append(", ");
        writer->visit(*arrayIndex);
    }
    writer->stringBuilder().append(")");
}

static void emitStorageBarrier(FunctionDefinitionWriter* writer, AST::CallExpression&)
{
    writer->stringBuilder().append("threadgroup_barrier(mem_flags::mem_device)");
}

static void emitWorkgroupBarrier(FunctionDefinitionWriter* writer, AST::CallExpression&)
{
    writer->stringBuilder().append("threadgroup_barrier(mem_flags::mem_threadgroup)");
}

static void emitWorkgroupUniformLoad(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("__workgroup_uniform_load(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(")");
}

static void atomicFunction(const char* name, FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append(name, "(");
    bool first = true;
    for (auto& argument : call.arguments()) {
        if (!first)
            writer->stringBuilder().append(", ");
        first = false;
        writer->visit(argument);
    }
    writer->stringBuilder().append(", memory_order_relaxed)");
}

static void emitAtomicLoad(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_load_explicit", writer, call);
}

static void emitAtomicStore(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_store_explicit", writer, call);
}

static void emitAtomicAdd(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_add_explicit", writer, call);
}

static void emitAtomicSub(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_sub_explicit", writer, call);
}

static void emitAtomicMax(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_max_explicit", writer, call);
}

static void emitAtomicMin(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_min_explicit", writer, call);
}

static void emitAtomicAnd(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_and_explicit", writer, call);
}

static void emitAtomicOr(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_or_explicit", writer, call);
}

static void emitAtomicXor(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_xor_explicit", writer, call);
}

static void emitAtomicExchange(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_exchange_explicit", writer, call);
}

static void emitArrayLength(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".size()");
}

static void emitDistance(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto* argumentType = call.arguments()[0].inferredType();
    if (std::holds_alternative<Types::Primitive>(*argumentType)) {
        writer->stringBuilder().append("abs(");
        writer->visit(call.arguments()[0]);
        writer->stringBuilder().append(" - ");
        writer->visit(call.arguments()[1]);
        writer->stringBuilder().append(")");
        return;
    }
    writer->visit(call.target());
    visitArguments(writer, call);
}

static void emitLength(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto* argumentType = call.arguments()[0].inferredType();
    if (!holds_alternative<Types::Vector>(*argumentType))
        writer->stringBuilder().append("abs");
    else
        writer->stringBuilder().append("length");
    visitArguments(writer, call);
}

static void emitDegrees(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(" * ", String::number(180 / std::numbers::pi), ")");
}

static void emitDynamicOffset(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto* targetType = call.target().inferredType();
    auto& pointer = std::get<Types::Pointer>(*targetType);
    auto* addressSpace = serializeAddressSpace(pointer.addressSpace);

    writer->stringBuilder().append("(*(");
    writer->visit(targetType);
    writer->stringBuilder().append(")(((", addressSpace, " uint8_t*)&(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(")) + __DynamicOffsets[");
    writer->visit(call.arguments()[1]);
    writer->stringBuilder().append("]))");
}

static void emitBitcast(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<");
    writer->visit(call.target().inferredType());
    writer->stringBuilder().append(">(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(")");
}

static void emitPack2x16Float(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<uint>(half2(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))");
}

static void emitUnpack2x16Float(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("float2(as_type<half2>(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))");
}

static void emitPack4xI8(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<uint>(char4(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))");
}

static void emitPack4xI8Clamp(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<uint>(char4(clamp(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(", -128, 127)))");
}

static void emitUnpack4xI8(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("int4(as_type<char4>(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))");
}

static void emitPack4xU8(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<uint>(uchar4(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))");
}

static void emitPack4xU8Clamp(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<uint>(uchar4(min(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(", 255)))");
}

static void emitQuantizeToF16(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto& argument = call.arguments()[0];
    String suffix = ""_s;
    if (auto* vectorType = std::get_if<Types::Vector>(argument.inferredType()))
        suffix = String::number(vectorType->size);
    writer->stringBuilder().append("float", suffix, "(half", suffix, "(");
    writer->visit(argument);
    writer->stringBuilder().append("))");
}

static void emitRadians(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(" * ", String::number(std::numbers::pi / 180), ")");
}

static void emitUnpack4xU8(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("uint4(as_type<uchar4>(");
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))");
}

void FunctionDefinitionWriter::visit(const Type* type, AST::CallExpression& call)
{
    if (is<AST::ElaboratedTypeExpression>(call.target())) {
        auto& base = downcast<AST::ElaboratedTypeExpression>(call.target()).base();
        if (base == "bitcast"_s) {
            emitBitcast(this, call);
            return;
        }
    }

    auto isArray = is<AST::ArrayTypeExpression>(call.target());
    auto isStruct = !isArray && std::holds_alternative<Types::Struct>(*call.target().inferredType());
    if (isArray || isStruct) {
        visit(type);
        m_stringBuilder.append("(");
        const Type* arrayElementType = nullptr;
        if (isArray)
            arrayElementType = std::get<Types::Array>(*type).element;

        m_stringBuilder.append("{\n");
        {
            IndentationScope scope(m_indent);
            for (auto& argument : call.arguments()) {
                m_stringBuilder.append(m_indent);
                if (isStruct)
                    visit(argument);
                else
                    visit(arrayElementType, argument);
                m_stringBuilder.append(",\n");
            }
        }
        m_stringBuilder.append(m_indent, "})");
        return;
    }

    if (is<AST::IdentifierExpression>(call.target())) {
        static constexpr std::pair<ComparableASCIILiteral, void(*)(FunctionDefinitionWriter*, AST::CallExpression&)> builtinMappings[] {
            { "__dynamicOffset", emitDynamicOffset },
            { "arrayLength", emitArrayLength },
            { "atomicAdd", emitAtomicAdd },
            { "atomicAnd", emitAtomicAnd },
            { "atomicExchange", emitAtomicExchange },
            { "atomicLoad", emitAtomicLoad },
            { "atomicMax", emitAtomicMax },
            { "atomicMin", emitAtomicMin },
            { "atomicOr", emitAtomicOr },
            { "atomicStore", emitAtomicStore },
            { "atomicSub", emitAtomicSub },
            { "atomicXor", emitAtomicXor },
            { "degrees", emitDegrees },
            { "distance", emitDistance },
            { "length", emitLength },
            { "pack2x16float", emitPack2x16Float },
            { "pack4xI8", emitPack4xI8 },
            { "pack4xI8Clamp", emitPack4xI8Clamp },
            { "pack4xU8", emitPack4xU8 },
            { "pack4xU8Clamp", emitPack4xU8Clamp },
            { "quantizeToF16", emitQuantizeToF16 },
            { "radians", emitRadians },
            { "storageBarrier", emitStorageBarrier },
            { "textureDimensions", emitTextureDimensions },
            { "textureGather", emitTextureGather },
            { "textureGatherCompare", emitTextureGatherCompare },
            { "textureLoad", emitTextureLoad },
            { "textureNumLayers", emitTextureNumLayers },
            { "textureNumLevels", emitTextureNumLevels },
            { "textureNumSamples", emitTextureNumSamples },
            { "textureSample", emitTextureSample },
            { "textureSampleBaseClampToEdge", emitTextureSampleBaseClampToEdge },
            { "textureSampleBias", emitTextureSampleBias },
            { "textureSampleCompare", emitTextureSampleCompare },
            { "textureSampleCompareLevel", emitTextureSampleCompare },
            { "textureSampleGrad", emitTextureSampleGrad },
            { "textureSampleLevel", emitTextureSampleLevel },
            { "textureStore", emitTextureStore },
            { "unpack2x16float", emitUnpack2x16Float },
            { "unpack4xI8", emitUnpack4xI8 },
            { "unpack4xU8", emitUnpack4xU8 },
            { "workgroupBarrier", emitWorkgroupBarrier },
            { "workgroupUniformLoad", emitWorkgroupUniformLoad },
        };
        static constexpr SortedArrayMap builtins { builtinMappings };
        const auto& targetName = downcast<AST::IdentifierExpression>(call.target()).identifier().id();
        if (auto mappedBuiltin = builtins.get(targetName)) {
            mappedBuiltin(this, call);
            return;
        }

        static constexpr std::pair<ComparableASCIILiteral, ASCIILiteral> directMappings[] {
            { "atomicCompareExchangeWeak", "__wgslAtomicCompareExchangeWeak"_s },
            { "countLeadingZeros", "clz"_s },
            { "countOneBits", "popcount"_s },
            { "countTrailingZeros", "ctz"_s },
            { "dot", "__wgslDot"_s },
            { "dot4I8Packed", "__wgslDot4I8Packed"_s },
            { "dot4U8Packed", "__wgslDot4U8Packed"_s },
            { "dpdx", "dfdx"_s },
            { "dpdxCoarse", "dfdx"_s },
            { "dpdxFine", "dfdx"_s },
            { "dpdy", "dfdy"_s },
            { "dpdyCoarse", "dfdy"_s },
            { "dpdyFine", "dfdy"_s },
            { "extractBits", "__wgslExtractBits"_s },
            { "faceForward", "faceforward"_s },
            { "firstLeadingBit", "__wgslFirstLeadingBit"_s },
            { "firstTrailingBit", "__wgslFirstTrailingBit"_s },
            { "frexp", "__wgslFrexp"_s },
            { "fwidthCoarse", "fwidth"_s },
            { "fwidthFine", "fwidth"_s },
            { "insertBits", "insert_bits"_s },
            { "inverseSqrt", "rsqrt"_s },
            { "modf", "__wgslModf"_s },
            { "pack2x16snorm", "pack_float_to_snorm2x16"_s },
            { "pack2x16unorm", "pack_float_to_unorm2x16"_s },
            { "pack4x8snorm", "pack_float_to_snorm4x8"_s },
            { "pack4x8unorm", "pack_float_to_unorm4x8"_s },
            { "reverseBits", "reverse_bits"_s },
            { "round", "rint"_s },
            { "sign", "__wgslSign"_s },
            { "unpack2x16snorm", "unpack_snorm2x16_to_float"_s },
            { "unpack2x16unorm", "unpack_unorm2x16_to_float"_s },
            { "unpack4x8snorm", "unpack_snorm4x8_to_float"_s },
            { "unpack4x8unorm", "unpack_unorm4x8_to_float"_s },
        };
        static constexpr SortedArrayMap mappedNames { directMappings };
        if (call.isConstructor()) {
            visit(type);
        } else if (auto mappedName = mappedNames.get(targetName))
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
    m_stringBuilder.append("(");
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
        m_stringBuilder.append("&");
        break;
    case AST::UnaryOperation::Dereference:
        m_stringBuilder.append("*");
        break;
    }
    visit(unary.expression());
    m_stringBuilder.append(")");
}

void FunctionDefinitionWriter::serializeBinaryExpression(AST::Expression& lhs, AST::BinaryOperation operation, AST::Expression& rhs)
{
    bool isDiv = operation == AST::BinaryOperation::Divide;
    bool isMod = !isDiv && operation == AST::BinaryOperation::Modulo;

    if (isDiv || isMod) {
        auto* rightType = rhs.inferredType();
        if (auto* vectorType = std::get_if<Types::Vector>(rightType))
            rightType = vectorType->element;

        const char* helperFunction = nullptr;
        if (satisfies(rightType, Constraints::Integer)) {
            if (isDiv)
                helperFunction = "__wgslDiv";
            else
                helperFunction = "__wgslMod";
        } else if (isMod)
            helperFunction = "fmod";

        if (helperFunction) {
            m_stringBuilder.append(helperFunction, "(");
            visit(lhs);
            m_stringBuilder.append(", ");
            visit(rhs);
            m_stringBuilder.append(")");
            return;
        }
    }

    m_stringBuilder.append("(");
    visit(lhs);
    switch (operation) {
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
        m_stringBuilder.append(" << ");
        break;
    case AST::BinaryOperation::RightShift:
        m_stringBuilder.append(" >> ");
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
    visit(rhs);
    m_stringBuilder.append(")");
}

void FunctionDefinitionWriter::visit(AST::BinaryExpression& binary)
{
    serializeBinaryExpression(binary.leftExpression(), binary.operation(), binary.rightExpression());
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
    auto it = m_overrides.find(identifier.identifier());
    if (UNLIKELY(it != m_overrides.end())) {
        m_stringBuilder.append("(");
        serializeConstant(identifier.inferredType(), it->value);
        m_stringBuilder.append(")");
        return;
    }
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
    m_stringBuilder.append(literal.value());
    auto& primitiveType = std::get<Types::Primitive>(*literal.inferredType());
    if (primitiveType.kind == Types::Primitive::U32)
        m_stringBuilder.append("u");
}

void FunctionDefinitionWriter::visit(AST::Signed32Literal& literal)
{
    m_stringBuilder.append(literal.value());
}

void FunctionDefinitionWriter::visit(AST::Unsigned32Literal& literal)
{
    m_stringBuilder.append(literal.value(), "u");
}

void FunctionDefinitionWriter::visit(AST::AbstractFloatLiteral& literal)
{
    NumberToStringBuffer buffer;
    WTF::numberToStringWithTrailingPoint(literal.value(), buffer);

    m_stringBuilder.append(&buffer[0]);
}

void FunctionDefinitionWriter::visit(AST::Float32Literal& literal)
{
    NumberToStringBuffer buffer;
    WTF::numberToStringWithTrailingPoint(literal.value(), buffer);

    m_stringBuilder.append(&buffer[0]);
}

void FunctionDefinitionWriter::visit(AST::Float16Literal& literal)
{
    NumberToStringBuffer buffer;
    WTF::numberToStringWithTrailingPoint(literal.value(), buffer);

    m_stringBuilder.append(&buffer[0]);
}

void FunctionDefinitionWriter::visit(AST::Statement& statement)
{
    AST::Visitor::visit(statement);
}

void FunctionDefinitionWriter::visit(AST::AssignmentStatement& assignment)
{
    visit(assignment.lhs());
    m_stringBuilder.append(" = ");
    const auto* assignmentType = assignment.lhs().inferredType();
    if (!assignmentType) {
        // In theory this should never happen, but the assignments generated by
        // the EntryPointRewriter do not have inferred types
        visit(assignment.rhs());
        return;
    }

    const auto& reference = std::get<Types::Reference>(*assignmentType);
    visit(reference.element, assignment.rhs());
}

void FunctionDefinitionWriter::visit(AST::CallStatement& statement)
{
    visit(statement.call().inferredType(), statement.call());
}

void FunctionDefinitionWriter::visit(AST::CompoundAssignmentStatement& statement)
{
    visit(statement.leftExpression());
    m_stringBuilder.append(" = ");
    serializeBinaryExpression(statement.leftExpression(), statement.operation(), statement.rightExpression());
}

void FunctionDefinitionWriter::visit(AST::CompoundStatement& statement)
{
    m_stringBuilder.append("{\n");
    {
        IndentationScope scope(m_indent);
        visitStatements(statement.statements());
    }
    m_stringBuilder.append(m_indent, "}");
}

void FunctionDefinitionWriter::visitStatements(AST::Statement::List& statements)
{
    for (auto& statement : statements) {
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

void FunctionDefinitionWriter::visit(AST::DiscardStatement&)
{
    m_stringBuilder.append("discard_fragment()");
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

void FunctionDefinitionWriter::visit(AST::LoopStatement& statement)
{
    m_stringBuilder.append("while (true) {\n");
    {
        auto& continuing = statement.continuing();
        SetForScope continuingScope(m_continuing, continuing.has_value() ? &*continuing : nullptr);

        IndentationScope scope(m_indent);
        visitStatements(statement.body());

        if (continuing.has_value()) {
            m_stringBuilder.append(m_indent);
            visit(*continuing);
        }
    }
    m_stringBuilder.append(m_indent, "}");
}

void FunctionDefinitionWriter::visit(AST::Continuing& continuing)
{
    // Do not emit the same continuing for continue statements within the continuing block
    SetForScope continuingScope(m_continuing, nullptr);

    m_stringBuilder.append("{\n");
    {
        IndentationScope scope(m_indent);
        visitStatements(continuing.body);

        if (auto* breakIf = continuing.breakIf) {
            m_stringBuilder.append(m_indent, "if (");
            visit(*breakIf);
            m_stringBuilder.append(")\n");

            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "break;\n");
        }
    }
    m_stringBuilder.append(m_indent, "}\n");
}

void FunctionDefinitionWriter::visit(AST::WhileStatement& statement)
{
    m_stringBuilder.append("while (");
    visit(statement.test());
    m_stringBuilder.append(") ");
    visit(statement.body());
}

void FunctionDefinitionWriter::visit(AST::SwitchStatement& statement)
{
    const auto& visitClause = [&](AST::SwitchClause& clause, bool isDefault = false) {
        for (auto& selector : clause.selectors) {
            m_stringBuilder.append("\n");
            m_stringBuilder.append(m_indent, "case ");
            visit(selector);
            m_stringBuilder.append(":");
        }
        if (isDefault) {
            m_stringBuilder.append("\n");
            m_stringBuilder.append(m_indent, "default:");
        }
        m_stringBuilder.append(" ");
        visit(clause.body);

        IndentationScope scope(m_indent);
        m_stringBuilder.append("\n", m_indent, "break;");
    };

    m_stringBuilder.append("switch (");
    visit(statement.value());
    m_stringBuilder.append(") {");
    for (auto& clause : statement.clauses())
        visitClause(clause);
    visitClause(statement.defaultClause(), true);
    m_stringBuilder.append("\n", m_indent, "}");
}

void FunctionDefinitionWriter::visit(AST::BreakStatement&)
{
    m_stringBuilder.append("break");
}

void FunctionDefinitionWriter::visit(AST::ContinueStatement&)
{
    if (m_continuing) {
        visit(*m_continuing);
        m_stringBuilder.append(m_indent);
    }
    m_stringBuilder.append("continue");
}

void FunctionDefinitionWriter::serializeConstant(const Type* type, ConstantValue value)
{
    using namespace Types;

    WTF::switchOn(*type,
        [&](const Primitive& primitive) {
            switch (primitive.kind) {
            case Primitive::AbstractInt:
                m_stringBuilder.append(std::get<int64_t>(value));
                break;
            case Primitive::I32:
                m_stringBuilder.append(std::get<int32_t>(value));
                break;
            case Primitive::U32:
                m_stringBuilder.append(std::get<uint32_t>(value), "u");
                break;
            case Primitive::AbstractFloat: {
                NumberToStringBuffer buffer;
                WTF::numberToStringWithTrailingPoint(std::get<double>(value), buffer);
                m_stringBuilder.append(&buffer[0]);
                break;
            }
            case Primitive::F32: {
                NumberToStringBuffer buffer;
                WTF::numberToStringWithTrailingPoint(std::get<float>(value), buffer);
                m_stringBuilder.append(&buffer[0]);
                break;
            }
            case Primitive::F16: {
                NumberToStringBuffer buffer;
                WTF::numberToStringWithTrailingPoint(std::get<half>(value), buffer);
                m_stringBuilder.append(&buffer[0], "h");
                break;
            }
            case Primitive::Bool:
                m_stringBuilder.append(std::get<bool>(value) ? "true" : "false");
                break;
            case Primitive::Void:
            case Primitive::Sampler:
            case Primitive::SamplerComparison:
            case Primitive::TextureExternal:
            case Primitive::AccessMode:
            case Primitive::TexelFormat:
            case Primitive::AddressSpace:
                RELEASE_ASSERT_NOT_REACHED();
            }
        },
        [&](const Reference& reference) {
            return serializeConstant(reference.element, value);
        },
        [&](const Vector& vectorType) {
            auto& vector = std::get<ConstantVector>(value);
            visit(type);
            m_stringBuilder.append("(");
            bool first = true;
            for (auto& element : vector.elements) {
                if (!first)
                    m_stringBuilder.append(", ");
                first = false;
                serializeConstant(vectorType.element, element);
            }
            m_stringBuilder.append(")");
        },
        [&](const Array& arrayType) {
            auto& array = std::get<ConstantArray>(value);
            visit(type);
            m_stringBuilder.append("{");
            bool first = true;
            for (auto& element : array.elements) {
                if (!first)
                    m_stringBuilder.append(", ");
                first = false;
                serializeConstant(arrayType.element, element);
            }
            m_stringBuilder.append("}");
        },
        [&](const Matrix& matrixType) {
            auto& matrix = std::get<ConstantMatrix>(value);
            m_stringBuilder.append("matrix<");
            visit(matrixType.element);
            m_stringBuilder.append(", ", matrixType.columns, ", ", matrixType.rows, ">(");
            bool first = true;
            for (auto& element : matrix.elements) {
                if (!first)
                    m_stringBuilder.append(", ");
                first = false;
                serializeConstant(matrixType.element, element);
            }
            m_stringBuilder.append(")");
        },
        [&](const Struct&) {
            // Not supported yet
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const PrimitiveStruct& primitiveStruct) {
            auto& constantStruct = std::get<ConstantStruct>(value);
            const auto& keys = Types::PrimitiveStruct::keys[primitiveStruct.kind];

            m_stringBuilder.append(primitiveStruct.name, "<");
            bool first = true;
            for (auto& value : primitiveStruct.values) {
                if (!first)
                    m_stringBuilder.append(", ");
                first = false;
                visit(value);
            }
            m_stringBuilder.append("> {");
            first = true;
            for (auto& entry : constantStruct.fields) {
                if (!first)
                    m_stringBuilder.append(", ");
                first = false;
                m_stringBuilder.append(".", entry.key, " = ");
                auto* key = keys.tryGet(entry.key);
                RELEASE_ASSERT(key);
                auto* type = primitiveStruct.values[*key];
                serializeConstant(type, entry.value);
            }
            m_stringBuilder.append("}");
        },
        [&](const Pointer&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Function&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Texture&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const TextureStorage&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const TextureDepth&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Atomic&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const TypeConstructor&) {
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const Bottom&) {
            RELEASE_ASSERT_NOT_REACHED();
        });
}

void emitMetalFunctions(StringBuilder& stringBuilder, const CallGraph& callGraph, const HashMap<String, ConstantValue>& constantValues)
{
    FunctionDefinitionWriter functionDefinitionWriter(callGraph, stringBuilder, constantValues);
    functionDefinitionWriter.write();
}

} // namespace Metal
} // namespace WGSL
