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
    FunctionDefinitionWriter(ShaderModule& shaderModule, StringBuilder& stringBuilder, PrepareResult& prepareResult, const HashMap<String, ConstantValue>& constantValues)
        : m_stringBuilder(stringBuilder)
        , m_shaderModule(shaderModule)
        , m_prepareResult(prepareResult)
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
    void visit(AST::ConstAssert&) override;

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
    void serializeVariable(AST::Variable&);
    void generatePackingHelpers(AST::Structure&);
    bool emitPackedVector(const Types::Vector&);
    void serializeConstant(const Type*, ConstantValue);
    void serializeBinaryExpression(AST::Expression&, AST::BinaryOperation, AST::Expression&);
    void visitStatements(AST::Statement::List&);
    bool shouldPackType() const;

    StringBuilder& m_stringBuilder;
    ShaderModule& m_shaderModule;
    Indentation<4> m_indent { 0 };
    std::optional<AST::StructureRole> m_structRole;
    std::optional<AST::VariableRole> m_variableRole;
    std::optional<AST::ParameterRole> m_parameterRole;
    std::optional<ShaderStage> m_entryPointStage;
    AST::Function* m_currentFunction { nullptr };
    unsigned m_functionConstantIndex { 0 };
    AST::Continuing*m_continuing { nullptr };
    HashSet<AST::Function*> m_visitedFunctions;
    PrepareResult& m_prepareResult;
    const HashMap<String, ConstantValue>& m_constantValues;
};

static ASCIILiteral serializeAddressSpace(AddressSpace addressSpace)
{
    switch (addressSpace) {
    case AddressSpace::Function:
    case AddressSpace::Private:
        return "thread"_s;
    case AddressSpace::Workgroup:
        return "threadgroup"_s;
    case AddressSpace::Uniform:
        return "constant"_s;
    case AddressSpace::Storage:
        return "device"_s;
    case AddressSpace::Handle:
        return { };
    }
}

void FunctionDefinitionWriter::write()
{
    emitNecessaryHelpers();

    for (auto& declaration : m_shaderModule.declarations()) {
        if (auto* structure = dynamicDowncast<AST::Structure>(declaration))
            visit(*structure);
    }

    for (auto& declaration : m_shaderModule.declarations()) {
        if (auto* structure = dynamicDowncast<AST::Structure>(declaration))
            generatePackingHelpers(*structure);
    }

    for (auto& entryPoint : m_shaderModule.callGraph().entrypoints()) {
        if (m_prepareResult.entryPoints.contains(entryPoint.originalName))
            visit(entryPoint.function);
    }
}

void FunctionDefinitionWriter::emitNecessaryHelpers()
{
    if (m_shaderModule.usesPackedVec3()) {
        m_stringBuilder.append(
            m_indent, "template<typename T>\n"_s,
            m_indent, "struct PackedVec3 {\n"_s
        );
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(
                m_indent, "T x;\n"_s,
                m_indent, "T y;\n"_s,
                m_indent, "T z;\n"_s,
                m_indent, "uint8_t __padding[sizeof(T)];\n"_s,
                m_indent, "\n"_s,
                m_indent, "PackedVec3() { }\n"_s,
                m_indent, "\n"_s,
                m_indent, "PackedVec3(packed_vec<T, 3> v) : x(v.x), y(v.y), z(v.z) { }\n"_s,
                m_indent, "\n"_s,
                m_indent, "operator vec<T, 3>() { return vec<T, 3>(x, y, z); }\n"_s,
                m_indent, "operator packed_vec<T, 3>() { return packed_vec<T, 3>(x, y, z); }\n"_s,
                m_indent, "\n"_s,
                m_indent, "T operator[](int i) const { return i ? i == 2 ? z : y : x; }\n"_s,
                m_indent, "device T& operator[](int i) device { return i ? i == 2 ? z : y : x; }\n"_s,
                m_indent, "constant T& operator[](int i) constant { return i ? i == 2 ? z : y : x; }\n"_s,
                m_indent, "thread T& operator[](int i) thread { return i ? i == 2 ? z : y : x; }\n"_s,
                m_indent, "threadgroup T& operator[](int i) threadgroup { return i ? i == 2 ? z : y : x; }\n"_s
            );
        }
        m_stringBuilder.append(m_indent, "};\n\n"_s);
    }

    if (m_shaderModule.usesExternalTextures()) {
        m_shaderModule.clearUsesExternalTextures();
        m_stringBuilder.append("struct texture_external {\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "texture2d<float> FirstPlane;\n"_s,
                m_indent, "texture2d<float> SecondPlane;\n"_s,
                m_indent, "float3x2 UVRemapMatrix;\n"_s,
                m_indent, "float4x3 ColorSpaceConversionMatrix;\n"_s,
                m_indent, "uint get_width(uint lod = 0) const { return FirstPlane.get_width(lod); }\n"_s,
                m_indent, "uint get_height(uint lod = 0) const { return FirstPlane.get_height(lod); }\n"_s);
        }
        m_stringBuilder.append("};\n\n"_s);
    }

    if (m_shaderModule.usesPackArray()) {
        m_shaderModule.clearUsesPackArray();
        m_stringBuilder.append(m_indent, "template<typename T, size_t N>\n"_s,
            m_indent, "static array<typename T::PackedType, N> __pack(array<T, N> unpacked)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "array<typename T::PackedType, N> packed;\n"_s,
                m_indent, "for (size_t i = 0; i < N; ++i)\n"_s);
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, "packed[i] = __pack(unpacked[i]);\n"_s);
            }
            m_stringBuilder.append(m_indent, "return packed;\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n\n"_s);

        if (m_shaderModule.usesPackedVec3()) {
            m_stringBuilder.append(m_indent, "template<typename T, size_t N>\n"_s,
                m_indent, "static array<PackedVec3<T>, N> __pack(array<vec<T, 3>, N> unpacked)\n"_s,
                m_indent, "{\n"_s);
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, "array<PackedVec3<T>, N> packed;\n"_s,
                    m_indent, "for (size_t i = 0; i < N; ++i)\n"_s);
                {
                    IndentationScope scope(m_indent);
                    m_stringBuilder.append(m_indent, "packed[i] = PackedVec3<T>(unpacked[i]);\n"_s);
                }
                m_stringBuilder.append(m_indent, "return packed;\n"_s);
            }
            m_stringBuilder.append(m_indent, "}\n\n"_s);
        }
    }

    if (m_shaderModule.usesUnpackArray()) {
        m_shaderModule.clearUsesUnpackArray();
        m_stringBuilder.append(m_indent, "template<typename T, size_t N>\n"_s,
            m_indent, "static array<typename T::UnpackedType, N> __unpack(array<T, N> packed)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "array<typename T::UnpackedType, N> unpacked;\n"_s,
                m_indent, "for (size_t i = 0; i < N; ++i)\n"_s);
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, "unpacked[i] = __unpack(packed[i]);\n"_s);
            }
            m_stringBuilder.append(m_indent, "return unpacked;\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n\n"_s);

        if (m_shaderModule.usesPackedVec3()) {
            m_stringBuilder.append(m_indent, "template<typename T, size_t N>\n"_s,
                m_indent, "static array<vec<T, 3>, N> __unpack(array<PackedVec3<T>, N> packed)\n"_s,
                m_indent, "{\n"_s);
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, "array<vec<T, 3>, N> unpacked;\n"_s,
                    m_indent, "for (size_t i = 0; i < N; ++i)\n"_s);
                {
                    IndentationScope scope(m_indent);
                    m_stringBuilder.append(m_indent, "unpacked[i] = vec<T, 3>(packed[i]);\n"_s);
                }
                m_stringBuilder.append(m_indent, "return unpacked;\n"_s);
            }
            m_stringBuilder.append(m_indent, "}\n\n"_s);
        }
    }

    if (m_shaderModule.usesPackVector()) {
        m_shaderModule.clearUsesPackVector();
        m_stringBuilder.append(m_indent, "template<typename T>\n"_s,
            m_indent, "static packed_vec<T, 3> __pack(vec<T, 3> unpacked) { return unpacked; }\n\n"_s);
    }

    if (m_shaderModule.usesUnpackVector()) {
        m_shaderModule.clearUsesUnpackVector();
        m_stringBuilder.append(m_indent, "template<typename T>\n"_s,
            m_indent, "static vec<T, 3> __unpack(packed_vec<T, 3> packed) { return packed; }\n\n"_s);

        if (m_shaderModule.usesPackedVec3()) {
            m_stringBuilder.append(m_indent, "template<typename T>\n"_s,
                m_indent, "static vec<T, 3> __unpack(PackedVec3<T> packed) { return packed; }\n\n"_s);
        }
    }

    if (m_shaderModule.usesWorkgroupUniformLoad()) {
        m_stringBuilder.append(m_indent, "template<typename T>\n"_s,
            m_indent, "static T __workgroup_uniform_load(threadgroup T* const ptr)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "threadgroup_barrier(mem_flags::mem_threadgroup);\n"_s,
                m_indent, "auto result = *ptr;\n"_s,
                m_indent, "threadgroup_barrier(mem_flags::mem_threadgroup);\n"_s,
                m_indent, "return result;\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n\n"_s);
    }

    if (m_shaderModule.usesDivision()) {
        m_stringBuilder.append(m_indent, "template<typename T, typename U, typename V = conditional_t<is_scalar_v<U>, T, U>>\n"_s,
            m_indent, "static V __wgslDiv(T lhs, U rhs)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto predicate = V(rhs) == V(0);\n"_s,
                m_indent, "if constexpr (is_signed_v<U>)\n"_s);
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, "predicate = predicate || (V(lhs) == V(numeric_limits<T>::lowest()) && V(rhs) == V(-1));\n"_s);
            }
            m_stringBuilder.append(m_indent, "return lhs / select(V(rhs), V(1), predicate);\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n\n"_s);
    }

    if (m_shaderModule.usesModulo()) {
        m_stringBuilder.append(m_indent, "template<typename T, typename U, typename V = conditional_t<is_scalar_v<U>, T, U>>\n"_s,
            m_indent, "static V __wgslMod(T lhs, U rhs)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto predicate = V(rhs) == V(0);\n"_s,
                m_indent, "if constexpr (is_signed_v<U>)\n"_s);
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, "predicate = predicate || (V(lhs) == V(numeric_limits<T>::lowest()) && V(rhs) == V(-1));\n"_s);
            }
            m_stringBuilder.append(m_indent, "return select(lhs % V(rhs), V(0), predicate);\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n\n"_s);
    }


    if (m_shaderModule.usesFrexp()) {
        m_stringBuilder.append(m_indent, "template<typename T, typename U>\n"_s,
            m_indent, "struct __frexp_result {\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "T fract;\n"_s,
                m_indent, "U exp;\n"_s);
        }
        m_stringBuilder.append(m_indent, "};\n\n"_s,
            m_indent, "template<typename T, typename U = conditional_t<is_vector_v<T>, vec<int, vec_elements<T>::value ?: 2>, int>>\n"_s,
            m_indent, "static __frexp_result<T, U> __wgslFrexp(T value)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "__frexp_result<T, U> result;\n"_s,
                m_indent, "result.fract = frexp(value, result.exp);\n"_s,
                m_indent, "return result;\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n\n"_s);
    }

    if (m_shaderModule.usesModf()) {
        m_stringBuilder.append(m_indent, "template<typename T, typename U>\n"_s,
            m_indent, "struct __modf_result {\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "T fract;\n"_s,
                m_indent, "U whole;\n"_s);
        }
        m_stringBuilder.append(m_indent, "};\n\n"_s,
            m_indent, "template<typename T>\n"_s,
            m_indent, "static __modf_result<T, T> __wgslModf(T value)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "__modf_result<T, T> result;\n"_s,
                m_indent, "result.fract = modf(value, result.whole);\n"_s,
                m_indent, "return result;\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n\n"_s);
    }

    if (m_shaderModule.usesAtomicCompareExchange()) {
        m_stringBuilder.append(m_indent, "template<typename T, typename U = bool>\n"_s,
            m_indent, "struct __atomic_compare_exchange_result {\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "T old_value;\n"_s,
                m_indent, "U exchanged;\n"_s);
        }
        m_stringBuilder.append(m_indent, "};\n\n"_s,
            m_indent, "#define __wgslAtomicCompareExchangeWeak(atomic, compare, value) \\\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "({ auto innerCompare = compare; \\\n"_s,
                m_indent, "bool exchanged = atomic_compare_exchange_weak_explicit((atomic), &innerCompare, value, memory_order_relaxed, memory_order_relaxed); \\\n"_s,
                m_indent, "__atomic_compare_exchange_result<decltype(compare)> { innerCompare, exchanged }; \\\n"_s,
                m_indent, "})\n"_s);
        }
    }

    if (m_shaderModule.usesDot()) {
        m_stringBuilder.append(m_indent, "template<typename T, unsigned N>\n"_s,
            m_indent, "static T __wgslDot(vec<T, N> lhs, vec<T, N> rhs)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto result = lhs[0] * rhs[0] + lhs[1] * rhs[1];\n"_s,
                m_indent, "if constexpr (N > 2) result += lhs[2] * rhs[2];\n"_s,
                m_indent, "if constexpr (N > 3) result += lhs[3] * rhs[3];\n"_s,
                m_indent, "return result;\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n"_s);
    }

    if (m_shaderModule.usesDot4I8Packed()) {
        m_stringBuilder.append(m_indent, "static int __wgslDot4I8Packed(uint lhs, uint rhs)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto vec1 = as_type<packed_char4>(lhs);"_s,
                m_indent, "auto vec2 = as_type<packed_char4>(rhs);"_s,
                m_indent, "return vec1[0] * vec2[0] + vec1[1] * vec2[1] + vec1[2] * vec2[2] + vec1[3] * vec2[3];"_s);
        }
        m_stringBuilder.append(m_indent, "}\n"_s);
    }

    if (m_shaderModule.usesDot4U8Packed()) {
        m_stringBuilder.append(m_indent, "static uint __wgslDot4U8Packed(uint lhs, uint rhs)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto vec1 = as_type<packed_uchar4>(lhs);"_s,
                m_indent, "auto vec2 = as_type<packed_uchar4>(rhs);"_s,
                m_indent, "return vec1[0] * vec2[0] + vec1[1] * vec2[1] + vec1[2] * vec2[2] + vec1[3] * vec2[3];"_s);
        }
        m_stringBuilder.append(m_indent, "}\n"_s);
    }

    if (m_shaderModule.usesFirstLeadingBit()) {
        m_stringBuilder.append(m_indent, "template<typename T>\n"_s,
            m_indent, "static T __wgslFirstLeadingBit(T e)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "if constexpr (is_signed_v<T>)\n"_s,
                m_indent, "    return select(T(31 - select(clz(e), clz(~e), e < T(0))), T(-1), e == T(0) || e == T(-1));\n"_s,
                m_indent, "else\n"_s,
                m_indent, "    return select(T(31 - clz(e)), T(-1), e == T(0));\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n"_s);
    }

    if (m_shaderModule.usesFirstTrailingBit()) {
        m_stringBuilder.append(m_indent, "template<typename T>\n"_s,
            m_indent, "static T __wgslFirstTrailingBit(T e)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "return select(ctz(e), T(-1), e == T(0));\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n"_s);
    }

    if (m_shaderModule.usesSign()) {
        m_stringBuilder.append(m_indent, "template<typename T>\n"_s,
            m_indent, "static T __wgslSign(T e)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "return select(select(T(-1), T(1), e > 0), T(0), e == 0);\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n"_s);
    }

    if (m_shaderModule.usesExtractBits()) {
        m_stringBuilder.append(m_indent, "template<typename T>\n"_s,
            m_indent, "static T __wgslExtractBits(T e, uint offset, uint count)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "auto o = min(offset, 32u);\n"_s,
                m_indent, "auto c = min(count, 32u - o);\n"_s,
                m_indent, "return extract_bits(e, o, c);\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n"_s);
    }

    if (m_shaderModule.usesMin()) {
        m_stringBuilder.append(m_indent, "static uint __attribute((always_inline)) __wgslMin(uint a, uint b)\n"_s,
            m_indent, "{\n"_s);
        {
            IndentationScope scope(m_indent);
            m_stringBuilder.append("return select(b, a, a < b);\n"_s);
        }
        m_stringBuilder.append(m_indent, "}\n\n"_s);
    }

    m_shaderModule.clearUsesPackedVec3();
}

void FunctionDefinitionWriter::visit(AST::Function& functionDefinition)
{
    if (!m_visitedFunctions.add(&functionDefinition).isNewEntry)
        return;

    for (auto& callee : m_shaderModule.callGraph().callees(functionDefinition))
        visit(*callee.target);

    // FIXME: visit return attributes
    for (auto& attribute : functionDefinition.attributes()) {
        checkErrorAndVisit(attribute);
        m_stringBuilder.append(' ');
    }

    if (functionDefinition.maybeReturnType())
        visit(functionDefinition.maybeReturnType()->inferredType());
    else
        m_stringBuilder.append("void"_s);

    m_stringBuilder.append(' ', functionDefinition.name(), '(');
    bool first = true;
    for (auto& parameter : functionDefinition.parameters()) {
        if (!first)
            m_stringBuilder.append(", "_s);
        switch (parameter.role()) {
        case AST::ParameterRole::UserDefined:
        case AST::ParameterRole::PackedResource:
            checkErrorAndVisit(parameter);
            break;
        case AST::ParameterRole::StageIn:
            checkErrorAndVisit(parameter);
            m_stringBuilder.append(" [[stage_in]]"_s);
            break;
        case AST::ParameterRole::BindGroup:
            visitArgumentBufferParameter(parameter);
            break;
        }
        first = false;
    }

    // Clear the flag set while serializing StageAttribute
    m_entryPointStage = std::nullopt;

    m_currentFunction = &functionDefinition;
    m_stringBuilder.append(")\n"_s);
    checkErrorAndVisit(functionDefinition.body());
    m_stringBuilder.append("\n\n"_s);

    m_currentFunction = nullptr;
}

void FunctionDefinitionWriter::visit(AST::Structure& structDecl)
{
    // FIXME: visit struct attributes
    m_structRole = { structDecl.role() };
    m_stringBuilder.append(m_indent, "struct "_s, structDecl.name(), " {\n"_s);
    {
        IndentationScope scope(m_indent);
        unsigned paddingID = 0;
        bool shouldPack = structDecl.role() == AST::StructureRole::PackedResource;
        const auto& addPadding = [&](unsigned paddingSize) {
            ASSERT(shouldPack);
            m_stringBuilder.append(m_indent, "uint8_t __padding"_s, ++paddingID, '[', String::number(paddingSize), "]; \n"_s);
        };

        if (structDecl.role() == AST::StructureRole::PackedResource)
            m_stringBuilder.append(m_indent, "using UnpackedType = struct "_s, structDecl.original()->name(), ";\n\n"_s);
        else if (structDecl.role() == AST::StructureRole::UserDefinedResource)
            m_stringBuilder.append(m_indent, "using PackedType = struct "_s, structDecl.packed()->name(), ";\n\n"_s);

        for (auto& member : structDecl.members()) {
            auto& name = member.name();
            auto* type = member.type().inferredType();
            if (isPrimitive(type, Types::Primitive::TextureExternal) || isPrimitiveReference(type, Types::Primitive::TextureExternal))  {
                m_stringBuilder.append(m_indent, "texture2d<float> __"_s, name, "_FirstPlane;\n"_s,
                    m_indent, "texture2d<float> __"_s, name, "_SecondPlane;\n"_s,
                    m_indent, "float3x2 __"_s, name, "_UVRemapMatrix;\n"_s,
                    m_indent, "float4x3 __"_s, name, "_ColorSpaceConversionMatrix;\n"_s);
                continue;
            }

            m_stringBuilder.append(m_indent);
            visit(member.type().inferredType());
            m_stringBuilder.append(' ', name);
            for (auto &attribute : member.attributes()) {
                m_stringBuilder.append(' ');
                visit(attribute);
            }
            m_stringBuilder.append(";\n"_s);

            if (shouldPack && member.padding())
                addPadding(member.padding());
        }

        if (structDecl.role() == AST::StructureRole::VertexOutput || structDecl.role() == AST::StructureRole::FragmentOutput) {
            m_stringBuilder.append('\n', m_indent, "template<typename T>\n"_s,
                m_indent, structDecl.name(), "(const thread T& other)\n"_s);
            {
                IndentationScope scope(m_indent);
                char prefix = ':';
                for (auto& member : structDecl.members()) {
                    auto& name = member.name();
                    m_stringBuilder.append(m_indent, prefix, ' ', name, "(other."_s, name, ")\n"_s);
                    prefix = ',';
                }
            }
            m_stringBuilder.append(m_indent, "{ }\n"_s);
        } else if (structDecl.role() == AST::StructureRole::FragmentOutputWrapper) {
            ASSERT(structDecl.members().size() == 1);
            auto& member = structDecl.members()[0];

            m_stringBuilder.append('\n', m_indent, "template<typename T>\n"_s,
                m_indent, structDecl.name(), "(T value)\n"_s);
            {
                IndentationScope scope(m_indent);
                m_stringBuilder.append(m_indent, ": "_s, member.name(), "(value)\n"_s);
            }
            m_stringBuilder.append(m_indent, "{ }\n"_s);
        }
    }
    m_stringBuilder.append(m_indent, "};\n\n"_s);
    m_structRole = std::nullopt;
}

void FunctionDefinitionWriter::generatePackingHelpers(AST::Structure& structure)
{
    if (structure.role() != AST::StructureRole::PackedResource || !structure.inferredType()->isConstructible())
        return;

    const String& packedName = structure.name();
    auto unpackedName = structure.original()->name();

    m_stringBuilder.append(m_indent, "static "_s, packedName, " __pack("_s, unpackedName, " unpacked)\n"_s,
        m_indent, "{\n"_s);
    {
        IndentationScope scope(m_indent);
        m_stringBuilder.append(m_indent, packedName, " packed;\n"_s);
        for (auto& member : structure.members()) {
            auto& name = member.name();
            if (member.type().inferredType()->packing() & (Packing::PStruct | Packing::PArray))
                m_stringBuilder.append(m_indent, "packed."_s, name, " = __pack(unpacked."_s, name, ");\n"_s);
            else
                m_stringBuilder.append(m_indent, "packed."_s, name, " = unpacked."_s, name, ";\n"_s);
        }
        m_stringBuilder.append(m_indent, "return packed;\n"_s);
    }
    m_stringBuilder.append(m_indent, "}\n\n"_s,
        m_indent, "static "_s, unpackedName, " __unpack("_s, packedName, " packed)\n"_s,
        m_indent, "{\n"_s);
    {
        IndentationScope scope(m_indent);
        m_stringBuilder.append(m_indent, unpackedName, " unpacked;\n"_s);
        for (auto& member : structure.members()) {
            auto& name = member.name();
            if (member.type().inferredType()->packing() & (Packing::PStruct | Packing::PArray))
                m_stringBuilder.append(m_indent, "unpacked."_s, name, " = __unpack(packed."_s, name, ");\n"_s);
            else
                m_stringBuilder.append(m_indent, "unpacked."_s, name, " = packed."_s, name, ";\n"_s);
        }
        m_stringBuilder.append(m_indent, "return unpacked;\n"_s);
    }
    m_stringBuilder.append(m_indent, "}\n\n"_s);
}

bool FunctionDefinitionWriter::shouldPackType() const
{
    if (m_structRole.has_value() && (*m_structRole == AST::StructureRole::PackedResource || *m_structRole == AST::StructureRole::BindGroup))
        return true;
    if (m_variableRole.has_value() && *m_variableRole == AST::VariableRole::PackedResource)
        return true;
    if (m_parameterRole.has_value() && (*m_parameterRole == AST::ParameterRole::PackedResource))
        return true;
    return false;
}

bool FunctionDefinitionWriter::emitPackedVector(const Types::Vector& vector)
{
    if (!shouldPackType())
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
        m_stringBuilder.append("packed_int"_s, vector.size);
        break;
    case Types::Primitive::U32:
        m_stringBuilder.append("packed_uint"_s, vector.size);
        break;
    case Types::Primitive::AbstractFloat:
    case Types::Primitive::F32:
        m_stringBuilder.append("packed_float"_s, vector.size);
        break;
    case Types::Primitive::F16:
        m_stringBuilder.append("packed_half"_s, vector.size);
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

void FunctionDefinitionWriter::visit(AST::ConstAssert&)
{
    // const_assert should not generate any code
}

void FunctionDefinitionWriter::serializeVariable(AST::Variable& variable)
{
    if (variable.flavor() == AST::VariableFlavor::Const)
        return;

    auto variableRoleScope = SetForScope(m_variableRole, std::optional<AST::VariableRole> { variable.role() });

    const Type* type = variable.storeType();
    if (isPrimitiveReference(type, Types::Primitive::TextureExternal)) {
        ASSERT(variable.maybeInitializer());
        m_stringBuilder.append("texture_external "_s, variable.name(), " { "_s);
        visit(*variable.maybeInitializer());
        m_stringBuilder.append("_FirstPlane, "_s);
        visit(*variable.maybeInitializer());
        m_stringBuilder.append("_SecondPlane, "_s);
        visit(*variable.maybeInitializer());
        m_stringBuilder.append("_UVRemapMatrix, "_s);
        visit(*variable.maybeInitializer());
        m_stringBuilder.append("_ColorSpaceConversionMatrix }"_s);
        return;
    }

    if (auto* qualifier = variable.maybeQualifier()) {
        switch (qualifier->addressSpace()) {
        case AddressSpace::Workgroup:
            m_stringBuilder.append("threadgroup "_s);
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
    m_stringBuilder.append(' ', variable.name());

    if (variable.flavor() == AST::VariableFlavor::Override)
        return;

    if (auto* initializer = variable.maybeInitializer()) {
        m_stringBuilder.append(" = "_s);
        visit(type, *initializer);
    } else
        m_stringBuilder.append(" { }"_s);
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
        m_stringBuilder.append("[[depth(any)]]"_s);
        break;
    case Builtin::FrontFacing:
        m_stringBuilder.append("[[front_facing]]"_s);
        break;
    case Builtin::GlobalInvocationId:
        m_stringBuilder.append("[[thread_position_in_grid]]"_s);
        break;
    case Builtin::InstanceIndex:
        m_stringBuilder.append("[[instance_id]]"_s);
        break;
    case Builtin::LocalInvocationId:
        m_stringBuilder.append("[[thread_position_in_threadgroup]]"_s);
        break;
    case Builtin::LocalInvocationIndex:
        m_stringBuilder.append("[[thread_index_in_threadgroup]]"_s);
        break;
    case Builtin::NumWorkgroups:
        m_stringBuilder.append("[[threadgroups_per_grid]]"_s);
        break;
    case Builtin::Position:
        m_stringBuilder.append("[[position]]"_s);
        break;
    case Builtin::SampleIndex:
        m_stringBuilder.append("[[sample_id]]"_s);
        break;
    case Builtin::SampleMask:
        m_stringBuilder.append("[[sample_mask]]"_s);
        break;
    case Builtin::VertexIndex:
        m_stringBuilder.append("[[vertex_id]]"_s);
        break;
    case Builtin::WorkgroupId:
        m_stringBuilder.append("[[threadgroup_position_in_grid]]"_s);
        break;
    }
}

void FunctionDefinitionWriter::visit(AST::StageAttribute& stage)
{
    m_entryPointStage = { stage.stage() };
    switch (stage.stage()) {
    case ShaderStage::Vertex:
        m_stringBuilder.append("[[vertex]]"_s);
        break;
    case ShaderStage::Fragment:
        m_stringBuilder.append("[[fragment]]"_s);
        break;
    case ShaderStage::Compute:
        m_stringBuilder.append("[[kernel]]"_s);
        break;
    }
}

void FunctionDefinitionWriter::visit(AST::GroupAttribute& group)
{
    unsigned bufferIndex = group.group().constantValue()->integerValue();
    if (m_entryPointStage.has_value() && *m_entryPointStage == ShaderStage::Vertex) {
        ASSERT(m_shaderModule.configuration().maxBuffersPlusVertexBuffersForVertexStage > 0);
        auto max = m_shaderModule.configuration().maxBuffersPlusVertexBuffersForVertexStage - 1;
        bufferIndex = vertexBufferIndexForBindGroup(bufferIndex, max);
    }
    m_stringBuilder.append("[[buffer("_s, bufferIndex, ")]]"_s);
}

void FunctionDefinitionWriter::visit(AST::BindingAttribute& binding)
{
    m_stringBuilder.append("[[id("_s, binding.binding().constantValue()->integerValue(), ")]]"_s);
}

void FunctionDefinitionWriter::visit(AST::LocationAttribute& location)
{
    if (m_structRole.has_value()) {
        auto role = *m_structRole;
        switch (role) {
        case AST::StructureRole::VertexOutput:
        case AST::StructureRole::FragmentInput:
            m_stringBuilder.append("[[user(loc"_s, location.location().constantValue()->integerValue(), ")]]"_s);
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
            m_stringBuilder.append("[[color("_s, location.location().constantValue()->integerValue(), ")]]"_s);
            return;
        case AST::StructureRole::VertexInput:
            m_stringBuilder.append("[[attribute("_s, location.location().constantValue()->integerValue(), ")]]"_s);
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

static ASCIILiteral convertToSampleMode(InterpolationType type, InterpolationSampling sampleType)
{
    switch (type) {
    case InterpolationType::Flat:
        return "flat"_s;
    case InterpolationType::Linear:
        switch (sampleType) {
        case InterpolationSampling::First:
        case InterpolationSampling::Either:
        case InterpolationSampling::Center:
            return "center_no_perspective"_s;
        case InterpolationSampling::Centroid:
            return "centroid_no_perspective"_s;
        case InterpolationSampling::Sample:
            return "sample_no_perspective"_s;
        }
    case InterpolationType::Perspective:
        switch (sampleType) {
        case InterpolationSampling::First:
        case InterpolationSampling::Either:
        case InterpolationSampling::Center:
            return "center_perspective"_s;
        case InterpolationSampling::Centroid:
            return "centroid_perspective"_s;
        case InterpolationSampling::Sample:
            return "sample_perspective"_s;
        }
    }

    ASSERT_NOT_REACHED();
    return "flat"_s;
}

void FunctionDefinitionWriter::visit(AST::InterpolateAttribute& attribute)
{
    m_stringBuilder.append("[["_s, convertToSampleMode(attribute.type(), attribute.sampling()), "]]"_s);
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
                m_stringBuilder.append("int"_s);
                break;
            case Types::Primitive::U32:
                m_stringBuilder.append("unsigned"_s);
                break;
            case Types::Primitive::AbstractFloat:
            case Types::Primitive::F32:
                m_stringBuilder.append("float"_s);
                break;
            case Types::Primitive::F16:
                m_stringBuilder.append("half"_s);
                break;
            case Types::Primitive::Void:
            case Types::Primitive::Bool:
            case Types::Primitive::Sampler:
                m_stringBuilder.append(*type);
                break;
            case Types::Primitive::SamplerComparison:
                m_stringBuilder.append("sampler"_s);
                break;
            case Types::Primitive::TextureExternal:
                m_stringBuilder.append("texture_external"_s);
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
            m_stringBuilder.append("vec<"_s);
            visit(vector.element);
            m_stringBuilder.append(", "_s, vector.size, '>');
        },
        [&](const Matrix& matrix) {
            m_stringBuilder.append("matrix<"_s);
            visit(matrix.element);
            m_stringBuilder.append(", "_s, matrix.columns, ", "_s, matrix.rows, '>');
        },
        [&](const Array& array) {
            m_stringBuilder.append("array<"_s);
            auto* vector = std::get_if<Types::Vector>(array.element);
            if (vector && vector->size == 3 && shouldPackType()) {
                m_stringBuilder.append("PackedVec3<"_s);
                visit(vector->element);
                m_stringBuilder.append(">"_s);
            } else
                visit(array.element);
            m_stringBuilder.append(", "_s);
            WTF::switchOn(array.size,
                [&](unsigned size) { m_stringBuilder.append(size); },
                [&](std::monostate) { m_stringBuilder.append(1); },
                [&](AST::Expression* size) {
                    visit(*size);
                });
            m_stringBuilder.append('>');
        },
        [&](const Struct& structure) {
            m_stringBuilder.append(structure.structure.name());
            if (shouldPackType() && structure.structure.role() == AST::StructureRole::UserDefinedResource)
                m_stringBuilder.append("::PackedType"_s);
        },
        [&](const PrimitiveStruct& structure) {
            m_stringBuilder.append(structure.name, '<');
            bool first = true;
            for (auto& value : structure.values) {
                if (!first)
                    m_stringBuilder.append(", "_s);
                first = false;
                visit(value);
            }
            m_stringBuilder.append('>');
        },
        [&](const Texture& texture) {
            ASCIILiteral type;
            ASCIILiteral access = "sample"_s;
            switch (texture.kind) {
            case Types::Texture::Kind::Texture1d:
                type = "texture1d"_s;
                break;
            case Types::Texture::Kind::Texture2d:
                type = "texture2d"_s;
                break;
            case Types::Texture::Kind::Texture2dArray:
                type = "texture2d_array"_s;
                break;
            case Types::Texture::Kind::Texture3d:
                type = "texture3d"_s;
                break;
            case Types::Texture::Kind::TextureCube:
                type = "texturecube"_s;
                break;
            case Types::Texture::Kind::TextureCubeArray:
                type = "texturecube_array"_s;
                break;
            case Types::Texture::Kind::TextureMultisampled2d:
                type = "texture2d_ms"_s;
                access = "read"_s;
                break;
            }
            m_stringBuilder.append(type, '<');
            visit(texture.element);
            m_stringBuilder.append(", access::"_s, access, '>');
        },
        [&](const TextureStorage& texture) {
            ASCIILiteral base;
            ASCIILiteral mode;
            switch (texture.kind) {
            case Types::TextureStorage::Kind::TextureStorage1d:
                base = "texture1d"_s;
                break;
            case Types::TextureStorage::Kind::TextureStorage2d:
                base = "texture2d"_s;
                break;
            case Types::TextureStorage::Kind::TextureStorage2dArray:
                base = "texture2d_array"_s;
                break;
            case Types::TextureStorage::Kind::TextureStorage3d:
                base = "texture3d"_s;
                break;
            }
            switch (texture.access) {
            case AccessMode::Read:
                mode = "read"_s;
                break;
            case AccessMode::Write:
                mode = "write"_s;
                break;
            case AccessMode::ReadWrite:
                mode = "read_write"_s;
                break;
            }
            m_stringBuilder.append(base, '<');
            visit(shaderTypeForTexelFormat(texture.format, m_shaderModule.types()));
            m_stringBuilder.append(", access::"_s, mode, '>');
        },
        [&](const TextureDepth& texture) {
            ASCIILiteral base;
            switch (texture.kind) {
            case TextureDepth::Kind::TextureDepth2d:
                base = "depth2d"_s;
                break;
            case TextureDepth::Kind::TextureDepth2dArray:
                base = "depth2d_array"_s;
                break;
            case TextureDepth::Kind::TextureDepthCube:
                base = "depthcube"_s;
                break;
            case TextureDepth::Kind::TextureDepthCubeArray:
                base = "depthcube_array"_s;
                break;
            case TextureDepth::Kind::TextureDepthMultisampled2d:
                base = "depth2d_ms"_s;
                break;
            }
            m_stringBuilder.append(base, "<float>"_s);
        },
        [&](const Reference& reference) {
            auto addressSpace = serializeAddressSpace(reference.addressSpace);
            if (addressSpace.isNull()) {
                visit(reference.element);
                return;
            }
            if (reference.accessMode == AccessMode::Read)
                m_stringBuilder.append("const "_s);
            m_stringBuilder.append(addressSpace, ' ');
            visit(reference.element);
            m_stringBuilder.append('&');
        },
        [&](const Pointer& pointer) {
            auto addressSpace = serializeAddressSpace(pointer.addressSpace);
            if (pointer.accessMode == AccessMode::Read)
                m_stringBuilder.append("const "_s);
            if (addressSpace)
                m_stringBuilder.append(addressSpace, ' ');
            visit(pointer.element);
            m_stringBuilder.append('*');
        },
        [&](const Atomic& atomic) {
            if (atomic.element == m_shaderModule.types().i32Type())
                m_stringBuilder.append("atomic_int"_s);
            else
                m_stringBuilder.append("atomic_uint"_s);
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
    auto parameterRoleScope = SetForScope(m_parameterRole, parameter.role());
    visit(parameter.typeName().inferredType());
    m_stringBuilder.append(' ', parameter.name());
    for (auto& attribute : parameter.attributes()) {
        m_stringBuilder.append(' ');
        checkErrorAndVisit(attribute);
    }
}

void FunctionDefinitionWriter::visitArgumentBufferParameter(AST::Parameter& parameter)
{
    m_stringBuilder.append("constant "_s);
    visit(parameter.typeName().inferredType());
    m_stringBuilder.append("& "_s, parameter.name());
    for (auto& attribute : parameter.attributes()) {
        m_stringBuilder.append(' ');
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

    if (auto* call = dynamicDowncast<AST::CallExpression>(expression))
        visit(type, *call);
    else if (auto* identity = dynamicDowncast<AST::IdentityExpression>(expression))
        visit(type, identity->expression());
    else
        AST::Visitor::visit(expression);
}

static void visitArguments(FunctionDefinitionWriter* writer, AST::CallExpression& call, unsigned startOffset = 0)
{
    writer->stringBuilder().append('(');
    for (unsigned i = startOffset; i < call.arguments().size(); ++i) {
        if (i != startOffset)
            writer->stringBuilder().append(", "_s);
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(')');
}

static void emitTextureDimensions(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    const auto& get = [&](ASCIILiteral property) {
        writer->visit(call.arguments()[0]);
        writer->stringBuilder().append(".get_"_s, property, '(');
        if (call.arguments().size() > 1)
            writer->visit(call.arguments()[1]);
        writer->stringBuilder().append(')');
    };

    const auto* vector = std::get_if<Types::Vector>(call.inferredType());
    if (!vector) {
        get("width"_s);
        return;
    }

    auto size = vector->size;
    ASSERT(size >= 2 && size <= 3);
    writer->stringBuilder().append("uint"_s, String::number(size), '(');
    get("width"_s);
    writer->stringBuilder().append(", "_s);
    get("height"_s);
    if (size > 2) {
        writer->stringBuilder().append(", "_s);
        get("depth"_s);
    }
    writer->stringBuilder().append(')');
}

static void emitTextureGather(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    ASSERT(call.arguments().size() > 1);
    unsigned offset = 0;
    ASCIILiteral component;
    bool hasOffset = true;
    auto& firstArgument = call.arguments()[0];
    if (std::holds_alternative<Types::Primitive>(*firstArgument.inferredType())) {
        offset = 1;
        switch (firstArgument.constantValue()->integerValue()) {
        case 0:
            component = "x"_s;
            break;
        case 1:
            component = "y"_s;
            break;
        case 2:
            component = "z"_s;
            break;
        case 3:
            component = "w"_s;
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
    writer->stringBuilder().append(".gather("_s);
    for (unsigned i = offset + 1; i < call.arguments().size(); ++i) {
        if (i != offset + 1)
            writer->stringBuilder().append(", "_s);
        writer->visit(call.arguments()[i]);
    }
    if (!hasOffset)
        writer->stringBuilder().append(", int2(0)"_s);
    if (!component.isNull())
        writer->stringBuilder().append(", component::"_s, component);
    writer->stringBuilder().append(')');
}

static void emitTextureGatherCompare(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    ASSERT(call.arguments().size() > 1);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".gather_compare"_s);
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
        writer->stringBuilder().append(".read"_s);
        writer->stringBuilder().append('(');
        bool is1d = true;
        auto cast = "uint"_s;
        if (const auto* vector = std::get_if<Types::Vector>(call.arguments()[1].inferredType())) {
            is1d = false;
            switch (vector->size) {
            case 2:
                cast = "uint2"_s;
                break;
            case 3:
                cast = "uint3"_s;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
        bool first = true;
        auto argumentCount = call.arguments().size();
        for (unsigned i = 1; i < argumentCount; ++i) {
            if (first) {
                writer->stringBuilder().append(cast, '(');
                writer->visit(call.arguments()[i]);
                writer->stringBuilder().append(')');
            } else if (is1d && i == argumentCount - 1) {
                // From the MSL spec for texture1d::read:
                // > Since mipmaps are not supported for 1D textures, lod must be 0.
                continue;
            } else {
                writer->stringBuilder().append(", "_s);
                writer->visit(call.arguments()[i]);
            }
            first = false;
        }
        writer->stringBuilder().append(')');
        return;
    }

    auto& coordinates = call.arguments()[1];
    writer->stringBuilder().append("({\n"_s);
    {
        IndentationScope scope(writer->indent());
        {
            writer->stringBuilder().append(writer->indent(), "auto __coords = uint2(("_s);
            writer->visit(texture);
            writer->stringBuilder().append(".UVRemapMatrix * float3(float2("_s);
            writer->visit(coordinates);
            writer->stringBuilder().append("), 1)).xy);\n"_s);
        }
        {
            writer->stringBuilder().append(writer->indent(), "auto __y = float("_s);
            writer->visit(texture);
            writer->stringBuilder().append(".FirstPlane.read(__coords).r);\n"_s);
        }
        {
            writer->stringBuilder().append(writer->indent(), "auto __cbcr = float2("_s);
            writer->visit(texture);
            writer->stringBuilder().append(".SecondPlane.read(__coords).rg);\n"_s);
        }
        writer->stringBuilder().append(writer->indent(), "auto __ycbcr = float3(__y, __cbcr);\n"_s);
        {
            writer->stringBuilder().append(writer->indent(), "float4("_s);
            writer->visit(texture);
            writer->stringBuilder().append(".ColorSpaceConversionMatrix * float4(__ycbcr, 1), 1);\n"_s);
        }
    }
    writer->stringBuilder().append(writer->indent(), "})"_s);
}

static void emitTextureSample(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    ASSERT(call.arguments().size() > 1);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".sample"_s);
    visitArguments(writer, call, 1);
}

static void emitTextureSampleCompare(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    ASSERT(call.arguments().size() > 1);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".sample_compare"_s);
    visitArguments(writer, call, 1);
}

static void emitTextureSampleGrad(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{

    ASSERT(call.arguments().size() > 1);
    auto& texture = call.arguments()[0];
    auto& textureType = std::get<Types::Texture>(*texture.inferredType());

    unsigned gradientIndex;
    ASCIILiteral gradientFunction;
    switch (textureType.kind) {
    case Types::Texture::Kind::Texture1d:
    case Types::Texture::Kind::Texture2d:
    case Types::Texture::Kind::TextureMultisampled2d:
        gradientIndex = 3;
        gradientFunction = "gradient2d"_s;
        break;

    case Types::Texture::Kind::Texture3d:
        gradientIndex = 3;
        gradientFunction = "gradient3d"_s;
        break;

    case Types::Texture::Kind::TextureCube:
        gradientIndex = 3;
        gradientFunction = "gradientcube"_s;
        break;

    case Types::Texture::Kind::Texture2dArray:
        gradientIndex = 4;
        gradientFunction = "gradient2d"_s;
        break;

    case Types::Texture::Kind::TextureCubeArray:
        gradientIndex = 4;
        gradientFunction = "gradientcube"_s;
        break;
    }
    writer->visit(texture);
    writer->stringBuilder().append(".sample("_s);
    for (unsigned i = 1; i < gradientIndex; ++i) {
        if (i != 1)
            writer->stringBuilder().append(", "_s);
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(", "_s, gradientFunction, '(');
    writer->visit(call.arguments()[gradientIndex]);
    writer->stringBuilder().append(", "_s);
    writer->visit(call.arguments()[gradientIndex + 1]);
    writer->stringBuilder().append(')');
    for (unsigned i = gradientIndex + 2; i < call.arguments().size(); ++i) {
        writer->stringBuilder().append(", "_s);
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(')');
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
    writer->stringBuilder().append(".sample("_s);
    for (unsigned i = 1; i < levelIndex; ++i) {
        if (i != 1)
            writer->stringBuilder().append(',');
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(", level("_s);
    writer->visit(call.arguments()[levelIndex]);
    writer->stringBuilder().append(')');
    for (unsigned i = levelIndex + 1; i < call.arguments().size(); ++i) {
        writer->stringBuilder().append(',');
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(')');
}

static void emitTextureSampleBaseClampToEdge(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto& texture = call.arguments()[0];
    auto* textureType = std::get_if<Types::Texture>(texture.inferredType());

    if (textureType) {
        // FIXME: this needs to clamp the coordinates
        writer->visit(texture);
        writer->stringBuilder().append(".sample"_s);
        visitArguments(writer, call, 1);
        return;
    }

    auto& sampler = call.arguments()[1];
    auto& coordinates = call.arguments()[2];
    writer->stringBuilder().append("({\n"_s);
    {
        IndentationScope scope(writer->indent());
        {
            writer->stringBuilder().append(writer->indent(), "auto __coords = ("_s);
            writer->visit(texture);
            writer->stringBuilder().append(".UVRemapMatrix * float3("_s);
            writer->visit(coordinates);
            writer->stringBuilder().append(", 1)).xy;\n"_s);
        }
        {
            writer->stringBuilder().append(writer->indent(), "auto __y = float("_s);
            writer->visit(texture);
            writer->stringBuilder().append(".FirstPlane.sample("_s);
            writer->visit(sampler);
            writer->stringBuilder().append(", __coords).r);\n"_s);
        }
        {
            writer->stringBuilder().append(writer->indent(), "auto __cbcr = float2("_s);
            writer->visit(texture);
            writer->stringBuilder().append(".SecondPlane.sample("_s);
            writer->visit(sampler);
            writer->stringBuilder().append(", __coords).rg);\n"_s);
        }
        writer->stringBuilder().append(writer->indent(), "auto __ycbcr = float3(__y, __cbcr);\n"_s);
        {
            writer->stringBuilder().append(writer->indent(), "float4("_s);
            writer->visit(texture);
            writer->stringBuilder().append(".ColorSpaceConversionMatrix * float4(__ycbcr, 1), 1);\n"_s);
        }
    }
    writer->stringBuilder().append(writer->indent(), "})"_s);
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
    writer->stringBuilder().append(".sample("_s);
    for (unsigned i = 1; i < biasIndex; ++i) {
        if (i != 1)
            writer->stringBuilder().append(", "_s);
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(", bias("_s);
    writer->visit(call.arguments()[biasIndex]);
    writer->stringBuilder().append(')');
    for (unsigned i = biasIndex + 1; i < call.arguments().size(); ++i) {
        writer->stringBuilder().append(", "_s);
        writer->visit(call.arguments()[i]);
    }
    writer->stringBuilder().append(')');
}

static void emitTextureNumLayers(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".get_array_size()"_s);
}

static void emitTextureNumLevels(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".get_num_mip_levels()"_s);
}

static void emitTextureNumSamples(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(".get_num_samples()"_s);
}

static void emitTextureStore(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto cast = "uint"_s;
    if (const auto* vector = std::get_if<Types::Vector>(call.arguments()[1].inferredType())) {
        switch (vector->size) {
        case 2:
            cast = "uint2"_s;
            break;
        case 3:
            cast = "uint3"_s;
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
    writer->stringBuilder().append(".write("_s);
    writer->visit(*value);
    writer->stringBuilder().append(", "_s, cast, '(');
    writer->visit(coords);
    writer->stringBuilder().append(')');
    if (arrayIndex) {
        writer->stringBuilder().append(", "_s);
        writer->visit(*arrayIndex);
    }
    writer->stringBuilder().append(')');

    auto& textureType = std::get<Types::TextureStorage>(*texture.inferredType());
    if (textureType.access == AccessMode::ReadWrite) {
        writer->stringBuilder().append(";\n"_s, writer->indent());
        writer->visit(texture);
        writer->stringBuilder().append(".fence()"_s);
    }
}

static void emitStorageBarrier(FunctionDefinitionWriter* writer, AST::CallExpression&)
{
    writer->stringBuilder().append("threadgroup_barrier(mem_flags::mem_device)"_s);
}

static void emitTextureBarrier(FunctionDefinitionWriter* writer, AST::CallExpression&)
{
    writer->stringBuilder().append("threadgroup_barrier(mem_flags::mem_texture)"_s);
}

static void emitWorkgroupBarrier(FunctionDefinitionWriter* writer, AST::CallExpression&)
{
    writer->stringBuilder().append("threadgroup_barrier(mem_flags::mem_threadgroup)"_s);
}

static void emitWorkgroupUniformLoad(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("__workgroup_uniform_load("_s);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(')');
}

static void atomicFunction(ASCIILiteral name, FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append(name, '(');
    bool first = true;
    for (auto& argument : call.arguments()) {
        if (!first)
            writer->stringBuilder().append(", "_s);
        first = false;
        writer->visit(argument);
    }
    writer->stringBuilder().append(", memory_order_relaxed)"_s);
}

static void emitAtomicLoad(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_load_explicit"_s, writer, call);
}

static void emitAtomicStore(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_store_explicit"_s, writer, call);
}

static void emitAtomicAdd(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_add_explicit"_s, writer, call);
}

static void emitAtomicSub(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_sub_explicit"_s, writer, call);
}

static void emitAtomicMax(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_max_explicit"_s, writer, call);
}

static void emitAtomicMin(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_min_explicit"_s, writer, call);
}

static void emitAtomicAnd(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_and_explicit"_s, writer, call);
}

static void emitAtomicOr(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_or_explicit"_s, writer, call);
}

static void emitAtomicXor(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_fetch_xor_explicit"_s, writer, call);
}

static void emitAtomicExchange(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    atomicFunction("atomic_exchange_explicit"_s, writer, call);
}

[[noreturn]] static void emitArrayLength(FunctionDefinitionWriter*, AST::CallExpression&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

static void emitDistance(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto* argumentType = call.arguments()[0].inferredType();
    if (std::holds_alternative<Types::Primitive>(*argumentType)) {
        writer->stringBuilder().append("abs("_s);
        writer->visit(call.arguments()[0]);
        writer->stringBuilder().append(" - "_s);
        writer->visit(call.arguments()[1]);
        writer->stringBuilder().append(')');
        return;
    }
    writer->visit(call.target());
    visitArguments(writer, call);
}

static void emitLength(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto* argumentType = call.arguments()[0].inferredType();
    if (!holds_alternative<Types::Vector>(*argumentType))
        writer->stringBuilder().append("abs"_s);
    else
        writer->stringBuilder().append("length"_s);
    visitArguments(writer, call);
}

static void emitDegrees(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append('(');
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(" * "_s, String::number(180 / std::numbers::pi), ')');
}

static void emitDynamicOffset(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto* targetType = call.target().inferredType();
    auto& pointer = std::get<Types::Pointer>(*targetType);
    auto addressSpace = serializeAddressSpace(pointer.addressSpace);

    writer->stringBuilder().append("(*("_s);
    writer->visit(targetType);
    writer->stringBuilder().append(")((("_s, addressSpace, " uint8_t*)&("_s);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(")) + __DynamicOffsets["_s);
    writer->visit(call.arguments()[1]);
    writer->stringBuilder().append("]))"_s);
}

static void emitBitcast(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<"_s);
    writer->visit(call.target().inferredType());
    writer->stringBuilder().append(">("_s);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(')');
}

static void emitPack2x16Float(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<uint>(half2("_s);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))"_s);
}

static void emitUnpack2x16Float(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("float2(as_type<half2>("_s);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))"_s);
}

static void emitPack4xI8(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<uint>(char4("_s);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))"_s);
}

static void emitPack4xI8Clamp(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<uint>(char4(clamp("_s);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(", -128, 127)))"_s);
}

static void emitUnpack4xI8(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("int4(as_type<char4>("_s);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))"_s);
}

static void emitPack4xU8(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<uint>(uchar4("_s);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))"_s);
}

static void emitPack4xU8Clamp(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("as_type<uint>(uchar4(min("_s);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(", 255)))"_s);
}

static void emitQuantizeToF16(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    auto& argument = call.arguments()[0];
    String suffix = ""_s;
    if (auto* vectorType = std::get_if<Types::Vector>(argument.inferredType()))
        suffix = String::number(vectorType->size);
    writer->stringBuilder().append("float"_s, suffix, "(half"_s, suffix, '(');
    writer->visit(argument);
    writer->stringBuilder().append("))"_s);
}

static void emitRadians(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append('(');
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append(" * "_s, String::number(std::numbers::pi / 180), ')');
}

static void emitUnpack4xU8(FunctionDefinitionWriter* writer, AST::CallExpression& call)
{
    writer->stringBuilder().append("uint4(as_type<uchar4>("_s);
    writer->visit(call.arguments()[0]);
    writer->stringBuilder().append("))"_s);
}

void FunctionDefinitionWriter::visit(const Type* type, AST::CallExpression& call)
{
    if (auto* target = dynamicDowncast<AST::ElaboratedTypeExpression>(call.target())) {
        if (target->base() == "bitcast"_s) {
            emitBitcast(this, call);
            return;
        }
    }

    auto isArray = is<AST::ArrayTypeExpression>(call.target());
    auto isStruct = !isArray && std::holds_alternative<Types::Struct>(*call.target().inferredType());
    if (call.isConstructor() && (isArray || isStruct)) {
        visit(type);
        m_stringBuilder.append('(');
        const Type* arrayElementType = nullptr;
        if (isArray)
            arrayElementType = std::get<Types::Array>(*type).element;

        m_stringBuilder.append("{\n"_s);
        {
            IndentationScope scope(m_indent);
            for (auto& argument : call.arguments()) {
                m_stringBuilder.append(m_indent);
                if (isStruct)
                    visit(argument);
                else
                    visit(arrayElementType, argument);
                m_stringBuilder.append(",\n"_s);
            }
        }
        m_stringBuilder.append(m_indent, "})"_s);
        return;
    }

    if (auto* target = dynamicDowncast<AST::IdentifierExpression>(call.target())) {
        static constexpr std::pair<ComparableASCIILiteral, void(*)(FunctionDefinitionWriter*, AST::CallExpression&)> builtinMappings[] {
            { "__dynamicOffset"_s, emitDynamicOffset },
            { "arrayLength"_s, emitArrayLength },
            { "atomicAdd"_s, emitAtomicAdd },
            { "atomicAnd"_s, emitAtomicAnd },
            { "atomicExchange"_s, emitAtomicExchange },
            { "atomicLoad"_s, emitAtomicLoad },
            { "atomicMax"_s, emitAtomicMax },
            { "atomicMin"_s, emitAtomicMin },
            { "atomicOr"_s, emitAtomicOr },
            { "atomicStore"_s, emitAtomicStore },
            { "atomicSub"_s, emitAtomicSub },
            { "atomicXor"_s, emitAtomicXor },
            { "degrees"_s, emitDegrees },
            { "distance"_s, emitDistance },
            { "length"_s, emitLength },
            { "pack2x16float"_s, emitPack2x16Float },
            { "pack4xI8"_s, emitPack4xI8 },
            { "pack4xI8Clamp"_s, emitPack4xI8Clamp },
            { "pack4xU8"_s, emitPack4xU8 },
            { "pack4xU8Clamp"_s, emitPack4xU8Clamp },
            { "quantizeToF16"_s, emitQuantizeToF16 },
            { "radians"_s, emitRadians },
            { "storageBarrier"_s, emitStorageBarrier },
            { "textureBarrier"_s, emitTextureBarrier },
            { "textureDimensions"_s, emitTextureDimensions },
            { "textureGather"_s, emitTextureGather },
            { "textureGatherCompare"_s, emitTextureGatherCompare },
            { "textureLoad"_s, emitTextureLoad },
            { "textureNumLayers"_s, emitTextureNumLayers },
            { "textureNumLevels"_s, emitTextureNumLevels },
            { "textureNumSamples"_s, emitTextureNumSamples },
            { "textureSample"_s, emitTextureSample },
            { "textureSampleBaseClampToEdge"_s, emitTextureSampleBaseClampToEdge },
            { "textureSampleBias"_s, emitTextureSampleBias },
            { "textureSampleCompare"_s, emitTextureSampleCompare },
            { "textureSampleCompareLevel"_s, emitTextureSampleCompare },
            { "textureSampleGrad"_s, emitTextureSampleGrad },
            { "textureSampleLevel"_s, emitTextureSampleLevel },
            { "textureStore"_s, emitTextureStore },
            { "unpack2x16float"_s, emitUnpack2x16Float },
            { "unpack4xI8"_s, emitUnpack4xI8 },
            { "unpack4xU8"_s, emitUnpack4xU8 },
            { "workgroupBarrier"_s, emitWorkgroupBarrier },
            { "workgroupUniformLoad"_s, emitWorkgroupUniformLoad },
        };
        static constexpr SortedArrayMap builtins { builtinMappings };
        const auto& targetName = target->identifier().id();
        if (auto mappedBuiltin = builtins.get(targetName)) {
            mappedBuiltin(this, call);
            return;
        }

        static constexpr std::pair<ComparableASCIILiteral, ASCIILiteral> directMappings[] {
            { "atomicCompareExchangeWeak"_s, "__wgslAtomicCompareExchangeWeak"_s },
            { "countLeadingZeros"_s, "clz"_s },
            { "countOneBits"_s, "popcount"_s },
            { "countTrailingZeros"_s, "ctz"_s },
            { "dot"_s, "__wgslDot"_s },
            { "dot4I8Packed"_s, "__wgslDot4I8Packed"_s },
            { "dot4U8Packed"_s, "__wgslDot4U8Packed"_s },
            { "dpdx"_s, "dfdx"_s },
            { "dpdxCoarse"_s, "dfdx"_s },
            { "dpdxFine"_s, "dfdx"_s },
            { "dpdy"_s, "dfdy"_s },
            { "dpdyCoarse"_s, "dfdy"_s },
            { "dpdyFine"_s, "dfdy"_s },
            { "extractBits"_s, "__wgslExtractBits"_s },
            { "faceForward"_s, "faceforward"_s },
            { "firstLeadingBit"_s, "__wgslFirstLeadingBit"_s },
            { "firstTrailingBit"_s, "__wgslFirstTrailingBit"_s },
            { "frexp"_s, "__wgslFrexp"_s },
            { "fwidthCoarse"_s, "fwidth"_s },
            { "fwidthFine"_s, "fwidth"_s },
            { "insertBits"_s, "insert_bits"_s },
            { "inverseSqrt"_s, "rsqrt"_s },
            { "modf"_s, "__wgslModf"_s },
            { "pack2x16snorm"_s, "pack_float_to_snorm2x16"_s },
            { "pack2x16unorm"_s, "pack_float_to_unorm2x16"_s },
            { "pack4x8snorm"_s, "pack_float_to_snorm4x8"_s },
            { "pack4x8unorm"_s, "pack_float_to_unorm4x8"_s },
            { "reverseBits"_s, "reverse_bits"_s },
            { "round"_s, "rint"_s },
            { "sign"_s, "__wgslSign"_s },
            { "unpack2x16snorm"_s, "unpack_snorm2x16_to_float"_s },
            { "unpack2x16unorm"_s, "unpack_unorm2x16_to_float"_s },
            { "unpack4x8snorm"_s, "unpack_snorm4x8_to_float"_s },
            { "unpack4x8unorm"_s, "unpack_unorm4x8_to_float"_s },
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
    m_stringBuilder.append('(');
    switch (unary.operation()) {
    case AST::UnaryOperation::Complement:
        m_stringBuilder.append('~');
        break;
    case AST::UnaryOperation::Negate:
        m_stringBuilder.append('-');
        break;
    case AST::UnaryOperation::Not:
        m_stringBuilder.append('!');
        break;
    case AST::UnaryOperation::AddressOf:
        m_stringBuilder.append('&');
        break;
    case AST::UnaryOperation::Dereference:
        m_stringBuilder.append('*');
        break;
    }
    visit(unary.expression());
    m_stringBuilder.append(')');
}

void FunctionDefinitionWriter::serializeBinaryExpression(AST::Expression& lhs, AST::BinaryOperation operation, AST::Expression& rhs)
{
    bool isDiv = operation == AST::BinaryOperation::Divide;
    bool isMod = !isDiv && operation == AST::BinaryOperation::Modulo;

    if (isDiv || isMod) {
        auto* rightType = rhs.inferredType();
        if (auto* vectorType = std::get_if<Types::Vector>(rightType))
            rightType = vectorType->element;

        ASCIILiteral helperFunction;
        if (satisfies(rightType, Constraints::Integer)) {
            if (isDiv)
                helperFunction = "__wgslDiv"_s;
            else
                helperFunction = "__wgslMod"_s;
        } else if (isMod)
            helperFunction = "fmod"_s;

        if (!helperFunction.isNull()) {
            m_stringBuilder.append(helperFunction, '(');
            visit(lhs);
            m_stringBuilder.append(", "_s);
            visit(rhs);
            m_stringBuilder.append(')');
            return;
        }
    }

    m_stringBuilder.append('(');
    visit(lhs);
    switch (operation) {
    case AST::BinaryOperation::Add:
        m_stringBuilder.append(" + "_s);
        break;
    case AST::BinaryOperation::Subtract:
        m_stringBuilder.append(" - "_s);
        break;
    case AST::BinaryOperation::Multiply:
        m_stringBuilder.append(" * "_s);
        break;
    case AST::BinaryOperation::Divide:
        m_stringBuilder.append(" / "_s);
        break;
    case AST::BinaryOperation::Modulo:
        m_stringBuilder.append(" % "_s);
        break;
    case AST::BinaryOperation::And:
        m_stringBuilder.append(" & "_s);
        break;
    case AST::BinaryOperation::Or:
        m_stringBuilder.append(" | "_s);
        break;
    case AST::BinaryOperation::Xor:
        m_stringBuilder.append(" ^ "_s);
        break;

    case AST::BinaryOperation::LeftShift:
        m_stringBuilder.append(" << "_s);
        break;
    case AST::BinaryOperation::RightShift:
        m_stringBuilder.append(" >> "_s);
        break;

    case AST::BinaryOperation::Equal:
        m_stringBuilder.append(" == "_s);
        break;
    case AST::BinaryOperation::NotEqual:
        m_stringBuilder.append(" != "_s);
        break;
    case AST::BinaryOperation::GreaterThan:
        m_stringBuilder.append(" > "_s);
        break;
    case AST::BinaryOperation::GreaterEqual:
        m_stringBuilder.append(" >= "_s);
        break;
    case AST::BinaryOperation::LessThan:
        m_stringBuilder.append(" < "_s);
        break;
    case AST::BinaryOperation::LessEqual:
        m_stringBuilder.append(" <= "_s);
        break;

    case AST::BinaryOperation::ShortCircuitAnd:
        m_stringBuilder.append(" && "_s);
        break;
    case AST::BinaryOperation::ShortCircuitOr:
        m_stringBuilder.append(" || "_s);
        break;
    }
    visit(rhs);
    m_stringBuilder.append(')');
}

void FunctionDefinitionWriter::visit(AST::BinaryExpression& binary)
{
    serializeBinaryExpression(binary.leftExpression(), binary.operation(), binary.rightExpression());
}

void FunctionDefinitionWriter::visit(AST::PointerDereferenceExpression& pointerDereference)
{
    m_stringBuilder.append("(*"_s);
    visit(pointerDereference.target());
    m_stringBuilder.append(')');
}
void FunctionDefinitionWriter::visit(AST::IndexAccessExpression& access)
{
    bool isPointer = std::holds_alternative<Types::Pointer>(*access.base().inferredType());
    if (isPointer)
        m_stringBuilder.append("(*("_s);
    visit(access.base());
    if (isPointer)
        m_stringBuilder.append("))"_s);
    m_stringBuilder.append('[');
    visit(access.index());
    m_stringBuilder.append(']');
}

void FunctionDefinitionWriter::visit(AST::IdentifierExpression& identifier)
{
    auto it = m_constantValues.find(identifier.identifier());
    if (UNLIKELY(it != m_constantValues.end())) {
        m_stringBuilder.append('(');
        serializeConstant(identifier.inferredType(), it->value);
        m_stringBuilder.append(')');
        return;
    }
    m_stringBuilder.append(identifier.identifier());
}

void FunctionDefinitionWriter::visit(AST::FieldAccessExpression& access)
{
    visit(access.base());
    auto* baseType = access.base().inferredType();
    if (baseType && std::holds_alternative<Types::Pointer>(*baseType))
        m_stringBuilder.append("->"_s);
    else
        m_stringBuilder.append('.');
    m_stringBuilder.append(access.fieldName());
}

void FunctionDefinitionWriter::visit(AST::BoolLiteral& literal)
{
    m_stringBuilder.append(literal.value() ? "true"_s : "false"_s);
}

void FunctionDefinitionWriter::visit(AST::AbstractIntegerLiteral& literal)
{
    m_stringBuilder.append(literal.value());
    auto& primitiveType = std::get<Types::Primitive>(*literal.inferredType());
    if (primitiveType.kind == Types::Primitive::U32)
        m_stringBuilder.append('u');
}

void FunctionDefinitionWriter::visit(AST::Signed32Literal& literal)
{
    m_stringBuilder.append(literal.value());
}

void FunctionDefinitionWriter::visit(AST::Unsigned32Literal& literal)
{
    m_stringBuilder.append(literal.value(), 'u');
}

void FunctionDefinitionWriter::visit(AST::AbstractFloatLiteral& literal)
{
    NumberToStringBuffer buffer;
    m_stringBuilder.append(WTF::numberToStringWithTrailingPoint(literal.value(), buffer));
}

void FunctionDefinitionWriter::visit(AST::Float32Literal& literal)
{
    NumberToStringBuffer buffer;
    m_stringBuilder.append(WTF::numberToStringWithTrailingPoint(literal.value(), buffer));
}

void FunctionDefinitionWriter::visit(AST::Float16Literal& literal)
{
    NumberToStringBuffer buffer;
    m_stringBuilder.append(WTF::numberToStringWithTrailingPoint(literal.value(), buffer));
}

void FunctionDefinitionWriter::visit(AST::Statement& statement)
{
    AST::Visitor::visit(statement);
}

void FunctionDefinitionWriter::visit(AST::AssignmentStatement& assignment)
{
    visit(assignment.lhs());
    m_stringBuilder.append(" = "_s);
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
    bool serialized = false;
    auto* leftExpression = &statement.leftExpression();
    if (auto* identity = dynamicDowncast<AST::IdentityExpression>(*leftExpression))
        leftExpression = &identity->expression();
    if (auto* call = dynamicDowncast<AST::CallExpression>(*leftExpression)) {
        auto& target = call->target();
        if (auto* identifier = dynamicDowncast<AST::IdentifierExpression>(target)) {
            if (identifier->identifier() == "__unpack"_s) {
                serialized = true;
                visit(call->arguments()[0]);
            }
        }
    }
    if (!serialized)
        visit(statement.leftExpression());

    m_stringBuilder.append(" = "_s);
    serializeBinaryExpression(statement.leftExpression(), statement.operation(), statement.rightExpression());
}

void FunctionDefinitionWriter::visit(AST::CompoundStatement& statement)
{
    m_stringBuilder.append("{\n"_s);
    {
        IndentationScope scope(m_indent);
        visitStatements(statement.statements());
    }
    m_stringBuilder.append(m_indent, '}');
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
        m_stringBuilder.append("++"_s);
        break;
    case AST::DecrementIncrementStatement::Operation::Decrement:
        m_stringBuilder.append("--"_s);
        break;
    }
}

void FunctionDefinitionWriter::visit(AST::DiscardStatement&)
{
#if CPU(X86_64)
    m_stringBuilder.append("__asm volatile(\"\"); discard_fragment()"_s);
#else
    m_stringBuilder.append("discard_fragment()"_s);
#endif
}

void FunctionDefinitionWriter::visit(AST::IfStatement& statement)
{
    m_stringBuilder.append("if ("_s);
    visit(statement.test());
    m_stringBuilder.append(") "_s);
    visit(statement.trueBody());
    if (statement.maybeFalseBody()) {
        m_stringBuilder.append(" else "_s);
        visit(*statement.maybeFalseBody());
    }
}

void FunctionDefinitionWriter::visit(AST::PhonyAssignmentStatement& statement)
{
    m_stringBuilder.append("(void)("_s);
    visit(statement.rhs());
    m_stringBuilder.append(')');
}

static std::optional<std::pair<String, String>> fragDepthIdentifierForFunction(AST::Function* function)
{
    if (!function || function->stage() != ShaderStage::Fragment)
        return std::nullopt;

    if (auto expression = function->maybeReturnType()) {
        if (auto* inferredType = expression->inferredType()) {
            auto& type = *inferredType;
            auto* returnStruct = std::get_if<WGSL::Types::Struct>(&type);
            if (!returnStruct)
                return std::nullopt;

            for (auto& member : returnStruct->structure.members()) {
                if (member.builtin() == WGSL::Builtin::FragDepth)
                    return std::make_pair(returnStruct->structure.name(), member.name());
                for (auto& attribute : member.attributes()) {
                    auto* builtinAttribute = dynamicDowncast<AST::BuiltinAttribute>(attribute);
                    if (builtinAttribute && builtinAttribute->builtin() == WGSL::Builtin::FragDepth)
                        return std::make_pair(returnStruct->structure.name(), member.name());
                }
            }
        }
    }

    return std::nullopt;
}

void FunctionDefinitionWriter::visit(AST::ReturnStatement& statement)
{
    auto fragDepthIdentifier = fragDepthIdentifierForFunction(m_currentFunction);
    if (fragDepthIdentifier)
        m_stringBuilder.append(fragDepthIdentifier->first, " __wgslFragmentReturnResult = "_s);
    else
        m_stringBuilder.append("return"_s);
    if (statement.maybeExpression()) {
        m_stringBuilder.append(' ');
        visit(*statement.maybeExpression());
    }

    if (fragDepthIdentifier) {
        m_stringBuilder.append(";\n__wgslFragmentReturnResult."_s, fragDepthIdentifier->second, " = clamp(__wgslFragmentReturnResult."_s, fragDepthIdentifier->second, ", as_type<float>(__DynamicOffsets[0]), as_type<float>(__DynamicOffsets[1]));\n"_s);
        m_stringBuilder.append("return __wgslFragmentReturnResult"_s);
    }
}

void FunctionDefinitionWriter::visit(AST::ForStatement& statement)
{
    m_stringBuilder.append("for ("_s);
    if (auto* initializer = statement.maybeInitializer())
        visit(*initializer);
    m_stringBuilder.append(';');
    if (auto* test = statement.maybeTest()) {
        m_stringBuilder.append(' ');
        visit(*test);
    }
    m_stringBuilder.append(';');
    if (auto* update = statement.maybeUpdate()) {
        m_stringBuilder.append(' ');
        visit(*update);
    }
    m_stringBuilder.append(") "_s);
    visit(statement.body());
}

void FunctionDefinitionWriter::visit(AST::LoopStatement& statement)
{
    m_stringBuilder.append("while (true) {\n"_s);
    {
        if (statement.containsSwitch())
            m_stringBuilder.append("bool __continuing = false;\n"_s, m_indent);
        auto& continuing = statement.continuing();
        SetForScope continuingScope(m_continuing, continuing.has_value() ? &*continuing : nullptr);

        IndentationScope scope(m_indent);
        visitStatements(statement.body());

        if (continuing.has_value()) {
            m_stringBuilder.append(m_indent);
            visit(*continuing);
        }
    }
    m_stringBuilder.append(m_indent, '}');
}

void FunctionDefinitionWriter::visit(AST::Continuing& continuing)
{
    // Do not emit the same continuing for continue statements within the continuing block
    SetForScope continuingScope(m_continuing, nullptr);

    m_stringBuilder.append("{\n"_s);
    {
        IndentationScope scope(m_indent);
        visitStatements(continuing.body);

        if (auto* breakIf = continuing.breakIf) {
            m_stringBuilder.append(m_indent, "if ("_s);
            visit(*breakIf);
            m_stringBuilder.append(")\n"_s);

            IndentationScope scope(m_indent);
            m_stringBuilder.append(m_indent, "break;\n"_s);
        }
    }
    m_stringBuilder.append(m_indent, "}\n"_s);
}

void FunctionDefinitionWriter::visit(AST::WhileStatement& statement)
{
    m_stringBuilder.append("while ("_s);
    visit(statement.test());
    m_stringBuilder.append(") "_s);
    visit(statement.body());
}

void FunctionDefinitionWriter::visit(AST::SwitchStatement& statement)
{
    const auto& visitClause = [&](AST::SwitchClause& clause, bool isDefault = false) {
        for (auto& selector : clause.selectors) {
            m_stringBuilder.append('\n', m_indent, "case "_s);
            visit(selector);
            m_stringBuilder.append(':');
        }
        if (isDefault)
            m_stringBuilder.append('\n', m_indent, "default:"_s);
        m_stringBuilder.append(' ');
        visit(clause.body);

        IndentationScope scope(m_indent);
        m_stringBuilder.append('\n', m_indent, "break;"_s);
    };

    m_stringBuilder.append("switch ("_s);
    visit(statement.value());
    m_stringBuilder.append(") {"_s);
    for (auto& clause : statement.clauses())
        visitClause(clause);
    visitClause(statement.defaultClause(), true);
    m_stringBuilder.append('\n', m_indent, '}');
    if (statement.isInsideLoop()) {
        m_stringBuilder.append('\n', m_indent, "if (__continuing) {"_s);
        {
            auto scope = IndentationScope(m_indent);
            visit(*m_continuing);
        }
        m_stringBuilder.append('\n', m_indent, '}');
    } else if (statement.isNestedInsideLoop()) {
        m_stringBuilder.append('\n', m_indent, "if (__continuing) {"_s);
        {
            auto scope = IndentationScope(m_indent);
            m_stringBuilder.append('\n', m_indent, "break;"_s);
        }
        m_stringBuilder.append('\n', m_indent, '}');
    }
}

void FunctionDefinitionWriter::visit(AST::BreakStatement&)
{
    m_stringBuilder.append("break"_s);
}

void FunctionDefinitionWriter::visit(AST::ContinueStatement& statement)
{
    if (statement.isFromSwitchToContinuing()) {
        m_stringBuilder.append("__continuing = true;\n"_s);
        m_stringBuilder.append(m_indent, "break"_s);
        return;
    }
    if (m_continuing) {
        visit(*m_continuing);
        m_stringBuilder.append(m_indent);
    }
    m_stringBuilder.append("continue"_s);
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
                m_stringBuilder.append(std::get<uint32_t>(value), 'u');
                break;
            case Primitive::AbstractFloat: {
                NumberToStringBuffer buffer;
                m_stringBuilder.append(WTF::numberToStringWithTrailingPoint(std::get<double>(value), buffer));
                break;
            }
            case Primitive::F32: {
                NumberToStringBuffer buffer;
                m_stringBuilder.append(WTF::numberToStringWithTrailingPoint(std::get<float>(value), buffer));
                break;
            }
            case Primitive::F16: {
                NumberToStringBuffer buffer;
                m_stringBuilder.append(WTF::numberToStringWithTrailingPoint(std::get<half>(value), buffer), 'h');
                break;
            }
            case Primitive::Bool:
                m_stringBuilder.append(std::get<bool>(value) ? "true"_s : "false"_s);
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
            m_stringBuilder.append('(');
            bool first = true;
            for (auto& element : vector.elements) {
                if (!first)
                    m_stringBuilder.append(", "_s);
                first = false;
                serializeConstant(vectorType.element, element);
            }
            m_stringBuilder.append(')');
        },
        [&](const Array& arrayType) {
            auto& array = std::get<ConstantArray>(value);
            visit(type);
            m_stringBuilder.append('{');
            bool first = true;
            for (auto& element : array.elements) {
                if (!first)
                    m_stringBuilder.append(", "_s);
                first = false;
                serializeConstant(arrayType.element, element);
            }
            m_stringBuilder.append('}');
        },
        [&](const Matrix& matrixType) {
            auto& matrix = std::get<ConstantMatrix>(value);
            m_stringBuilder.append("matrix<"_s);
            visit(matrixType.element);
            m_stringBuilder.append(", "_s, matrixType.columns, ", "_s, matrixType.rows, ">("_s);
            bool first = true;
            for (auto& element : matrix.elements) {
                if (!first)
                    m_stringBuilder.append(", "_s);
                first = false;
                serializeConstant(matrixType.element, element);
            }
            m_stringBuilder.append(')');
        },
        [&](const Struct& structType) {
            auto& constantStruct = std::get<ConstantStruct>(value);
            m_stringBuilder.append(structType.structure.name(), " { "_s);
            for (auto& member : structType.structure.members()) {
                m_stringBuilder.append('.', member.name(), " = "_s);
                serializeConstant(structType.fields.get(member.originalName()), constantStruct.fields.get(member.originalName()));
                m_stringBuilder.append(", "_s);
            }
            m_stringBuilder.append(" }"_s);
        },
        [&](const PrimitiveStruct& primitiveStruct) {
            auto& constantStruct = std::get<ConstantStruct>(value);
            const auto& keys = Types::PrimitiveStruct::keys[primitiveStruct.kind];

            m_stringBuilder.append(primitiveStruct.name, '<');
            bool first = true;
            for (auto& value : primitiveStruct.values) {
                if (!first)
                    m_stringBuilder.append(", "_s);
                first = false;
                visit(value);
            }
            m_stringBuilder.append("> {"_s);
            first = true;
            for (auto& entry : constantStruct.fields) {
                if (!first)
                    m_stringBuilder.append(", "_s);
                first = false;
                m_stringBuilder.append('.', entry.key, " = "_s);
                auto* key = keys.tryGet(entry.key);
                RELEASE_ASSERT(key);
                auto* type = primitiveStruct.values[*key];
                serializeConstant(type, entry.value);
            }
            m_stringBuilder.append('}');
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

void emitMetalFunctions(StringBuilder& stringBuilder, ShaderModule& shaderModule, PrepareResult& prepareResult, const HashMap<String, ConstantValue>& constantValues)
{
    FunctionDefinitionWriter functionDefinitionWriter(shaderModule, stringBuilder, prepareResult, constantValues);
    functionDefinitionWriter.write();
}

} // namespace Metal
} // namespace WGSL
