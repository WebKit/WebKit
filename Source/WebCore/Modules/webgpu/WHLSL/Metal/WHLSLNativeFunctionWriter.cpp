/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WHLSLNativeFunctionWriter.h"

#if ENABLE(WEBGPU)

#include "NotImplemented.h"
#include "WHLSLAddressSpace.h"
#include "WHLSLArrayType.h"
#include "WHLSLEnumerationDefinition.h"
#include "WHLSLInferTypes.h"
#include "WHLSLIntrinsics.h"
#include "WHLSLNamedType.h"
#include "WHLSLNativeFunctionDeclaration.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLPointerType.h"
#include "WHLSLStructureDefinition.h"
#include "WHLSLTypeDefinition.h"
#include "WHLSLTypeNamer.h"
#include "WHLSLUnnamedType.h"
#include "WHLSLVariableDeclaration.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

namespace WHLSL {

namespace Metal {

static String mapFunctionName(String& functionName)
{
    if (functionName == "ddx")
        return "dfdx"_str;
    if (functionName == "ddy")
        return "dfdy"_str;
    if (functionName == "asint")
        return "as_type<int32_t>"_str;
    if (functionName == "asuint")
        return "as_type<uint32_t>"_str;
    if (functionName == "asfloat")
        return "as_type<float>"_str;
    return functionName;
}

static String atomicName(String input)
{
    if (input == "Add")
        return "fetch_add"_str;
    if (input == "And")
        return "fetch_and"_str;
    if (input == "Exchange")
        return "exchange"_str;
    if (input == "Max")
        return "fetch_max"_str;
    if (input == "Min")
        return "fetch_min"_str;
    if (input == "Or")
        return "fetch_or"_str;
    ASSERT(input == "Xor");
        return "fetch_xor"_str;
}

static int vectorLength(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    int vectorLength = 1;
    if (!nativeTypeDeclaration.typeArguments().isEmpty()) {
        ASSERT(nativeTypeDeclaration.typeArguments().size() == 2);
        ASSERT(WTF::holds_alternative<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[1]));
        vectorLength = WTF::get<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[1]).integerLiteral().value();
    }
    return vectorLength;
}

static AST::NamedType& vectorInnerType(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    if (nativeTypeDeclaration.typeArguments().isEmpty())
        return nativeTypeDeclaration;

    ASSERT(nativeTypeDeclaration.typeArguments().size() == 2);
    ASSERT(WTF::holds_alternative<Ref<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]));
    return WTF::get<Ref<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0])->resolvedType();
}

static const char* vectorSuffix(int vectorLength)
{
    switch (vectorLength) {
    case 1:
        return "";
    case 2:
        return "2";
    case 3:
        return "3";
    default:
        ASSERT(vectorLength == 4);
        return "4";
    }
}

void inlineNativeFunction(StringBuilder& stringBuilder, AST::NativeFunctionDeclaration& nativeFunctionDeclaration, MangledVariableName returnName, const Vector<MangledVariableName>& args, Intrinsics& intrinsics, TypeNamer& typeNamer, std::function<MangledVariableName()>&& generateNextVariableName, Indentation<4> indent)
{
    auto asMatrixType = [&] (AST::UnnamedType& unnamedType) -> AST::NativeTypeDeclaration* {
        auto& realType = unnamedType.unifyNode();
        if (!realType.isNativeTypeDeclaration())
            return nullptr;

        auto& maybeMatrixType = downcast<AST::NativeTypeDeclaration>(realType);
        if (maybeMatrixType.isMatrix())
            return &maybeMatrixType;

        return nullptr;
    };

    if (nativeFunctionDeclaration.isCast()) {
        auto& returnType = nativeFunctionDeclaration.type();
        auto metalReturnTypeName = typeNamer.mangledNameForType(returnType);

        if (!nativeFunctionDeclaration.parameters().size()) {
            stringBuilder.append(indent, returnName, " = { };\n");
            return;
        }

        if (nativeFunctionDeclaration.parameters().size() == 1) {
            auto& parameterType = *nativeFunctionDeclaration.parameters()[0]->type();
            auto metalParameterTypeName = typeNamer.mangledNameForType(parameterType);

            auto isEnumerationDefinition = [] (auto& type) {
                return is<AST::NamedType>(type) && is<AST::EnumerationDefinition>(downcast<AST::NamedType>(type));
            };
            auto& unifiedReturnType = returnType.unifyNode();
            if (isEnumerationDefinition(unifiedReturnType) && !isEnumerationDefinition(parameterType.unifyNode())) {
                auto variableName = generateNextVariableName();
                stringBuilder.append(indent, metalParameterTypeName, ' ', variableName, " = ", args[0], ";\n");
                auto& enumerationDefinition = downcast<AST::EnumerationDefinition>(downcast<AST::NamedType>(unifiedReturnType));
                stringBuilder.append(indent, "switch (", variableName, ") {\n");
                {
                    IndentationScope switchScope(indent);
                    bool hasZeroCase = false;
                    for (auto& member : enumerationDefinition.enumerationMembers()) {
                        hasZeroCase |= !member.get().value();
                        stringBuilder.append(
                            indent, "case ", member.get().value(), ":\n",
                            indent, "    break;\n");
                    }
                    ASSERT_UNUSED(hasZeroCase, hasZeroCase);
                    stringBuilder.append(
                        indent, "default:\n",
                        indent, "    ", variableName, " = 0;\n",
                        indent, "    break;\n",
                        indent, "}\n");
                }
                stringBuilder.append(indent, returnName, " = static_cast<", metalReturnTypeName, ">(", variableName, ");\n");
            } else
                stringBuilder.append(indent, returnName, " = static_cast<", metalReturnTypeName, ">(", args[0], ");\n");

            return;
        }

        if (auto* matrixType = asMatrixType(returnType)) {
            unsigned numRows = matrixType->numberOfMatrixRows();
            unsigned numColumns = matrixType->numberOfMatrixColumns();
            RELEASE_ASSERT(nativeFunctionDeclaration.parameters().size() == numRows || nativeFunctionDeclaration.parameters().size() == numRows * numColumns);

            auto variableName = generateNextVariableName();

            stringBuilder.append(indent, metalReturnTypeName, ' ', variableName, ";\n");

            // We need to abide by the memory layout we use for matrices here.
            if (nativeFunctionDeclaration.parameters().size() == numRows) {
                // operator matrixMxN (vectorN, ..., vectorN)
                for (unsigned i = 0; i < numRows; ++i) {
                    for (unsigned j = 0; j < numColumns; ++j)
                        stringBuilder.append(indent, variableName, "[", j * numRows + i, "] = ", args[i], "[", j, "];\n");
                }
            } else {
                // operator matrixMxN (scalar, ..., scalar)
                unsigned index = 0;
                for (unsigned i = 0; i < numRows; ++i) {
                    for (unsigned j = 0; j < numColumns; ++j) {
                        stringBuilder.append(indent, variableName, '[', j * numRows + i, "] = ", args[index], ";\n");
                        ++index;
                    }
                }
            }

            stringBuilder.append(indent, returnName, " = ", variableName, ";\n");
            return;
        }

        stringBuilder.append(indent, returnName, " = ", metalReturnTypeName, "(");
        for (unsigned i = 0; i < nativeFunctionDeclaration.parameters().size(); ++i) {
            if (i > 0)
                stringBuilder.append(", ");
            stringBuilder.append(args[i]);
        }
        stringBuilder.append(");\n");
        return;
    }

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198077 Authors can make a struct field named "length" too. Autogenerated getters for those shouldn't take this codepath.
    if (nativeFunctionDeclaration.name() == "operator.length") {
        ASSERT_UNUSED(intrinsics, matches(nativeFunctionDeclaration.type(), intrinsics.uintType()));
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto& parameterType = nativeFunctionDeclaration.parameters()[0]->type()->unifyNode();
        auto& unnamedParameterType = downcast<AST::UnnamedType>(parameterType);
        if (is<AST::ArrayType>(unnamedParameterType)) {
            auto& arrayParameterType = downcast<AST::ArrayType>(unnamedParameterType);
            stringBuilder.append(
                indent, returnName, " = ", arrayParameterType.numElements(), ";\n");
            return;
        }

        ASSERT(is<AST::ArrayReferenceType>(unnamedParameterType));
        stringBuilder.append(
            indent, returnName, " = ", args[0], ".length;\n");
        return;
    }

    if (nativeFunctionDeclaration.name().startsWith("operator."_str)) {
        auto appendMangledFieldName = [&] (const String& fieldName) {
            auto& unifyNode = nativeFunctionDeclaration.parameters()[0]->type()->unifyNode();
            auto& namedType = downcast<AST::NamedType>(unifyNode);
            if (is<AST::StructureDefinition>(namedType)) {
                auto& structureDefinition = downcast<AST::StructureDefinition>(namedType);
                auto* structureElement = structureDefinition.find(fieldName);
                ASSERT(structureElement);
                stringBuilder.append(typeNamer.mangledNameForStructureElement(*structureElement));
                return;
            }
            ASSERT(is<AST::NativeTypeDeclaration>(namedType));
            stringBuilder.append(fieldName);
        };

        if (nativeFunctionDeclaration.name().endsWith("=")) {
            ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
            auto fieldName = nativeFunctionDeclaration.name().substring("operator."_str.length());
            fieldName = fieldName.substring(0, fieldName.length() - 1);

            stringBuilder.append(
                indent, returnName, " = ", args[0], ";\n",
                indent, returnName, '.');
            appendMangledFieldName(fieldName);
            stringBuilder.append(" = ", args[1], ";\n");

            return;
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto fieldName = nativeFunctionDeclaration.name().substring("operator."_str.length());
        stringBuilder.append(
            indent, returnName, " = ", args[0], '.');
        appendMangledFieldName(fieldName);
        stringBuilder.append(";\n");
        return;
    }

    if (nativeFunctionDeclaration.name().startsWith("operator&."_str)) {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto fieldName = nativeFunctionDeclaration.name().substring("operator&."_str.length());

        stringBuilder.append(
            indent, returnName, " = ", args[0], " ? &(", args[0], "->");

        auto& unnamedType = *nativeFunctionDeclaration.parameters()[0]->type();
        auto& unifyNode = downcast<AST::PointerType>(unnamedType).elementType().unifyNode();
        auto& namedType = downcast<AST::NamedType>(unifyNode);
        if (is<AST::StructureDefinition>(namedType)) {
            auto& structureDefinition = downcast<AST::StructureDefinition>(namedType);
            auto* structureElement = structureDefinition.find(fieldName);
            ASSERT(structureElement);
            stringBuilder.append(typeNamer.mangledNameForStructureElement(*structureElement));
        } else
            stringBuilder.append(fieldName);

        stringBuilder.append(") : nullptr;\n");

        return;
    }

    if (nativeFunctionDeclaration.name() == "operator&[]") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        ASSERT(is<AST::ArrayReferenceType>(*nativeFunctionDeclaration.parameters()[0]->type()));

        stringBuilder.append(
            indent, returnName, " = (", args[1], " < ", args[0], ".length) ? ", " &(", args[0], ".pointer[", args[1], "]) : nullptr;\n");
            
        return;
    }

    auto vectorSize = [&] () -> unsigned {
        auto& typeReference = downcast<AST::TypeReference>(*nativeFunctionDeclaration.parameters()[0]->type());
        auto& vectorType = downcast<AST::NativeTypeDeclaration>(downcast<AST::TypeReference>(downcast<AST::TypeDefinition>(typeReference.resolvedType()).type()).resolvedType());
        ASSERT(vectorType.name() == "vector");
        ASSERT(vectorType.typeArguments().size() == 2);
        return WTF::get<AST::ConstantExpression>(vectorType.typeArguments()[1]).integerLiteral().value();
    };

    auto getMatrixType = [&] () -> AST::NativeTypeDeclaration& {
        auto& typeReference = downcast<AST::TypeReference>(*nativeFunctionDeclaration.parameters()[0]->type());
        auto& result = downcast<AST::NativeTypeDeclaration>(downcast<AST::TypeReference>(downcast<AST::TypeDefinition>(typeReference.resolvedType()).type()).resolvedType());
        ASSERT(result.isMatrix());
        return result;
    };

    if (nativeFunctionDeclaration.name() == "operator[]") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto& typeReference = downcast<AST::TypeReference>(*nativeFunctionDeclaration.parameters()[0]->type());
        size_t numTypeArguments = downcast<AST::NativeTypeDeclaration>(downcast<AST::TypeReference>(downcast<AST::TypeDefinition>(typeReference.resolvedType()).type()).resolvedType()).typeArguments().size();
        if (numTypeArguments == 3) {
            auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());

            unsigned numberOfRows = getMatrixType().numberOfMatrixRows();
            unsigned numberOfColumns = getMatrixType().numberOfMatrixColumns();
            
            stringBuilder.append(indent, "do {\n");
            {
                IndentationScope scope(indent);

                stringBuilder.append(
                    indent, metalReturnName, " result;\n",
                    indent, "if (", args[1], " >= ", numberOfRows, ") {\n",
                    indent, "    ", returnName, " = ", metalReturnName, "(0);\n",
                    indent, "    break;\n",
                    indent, "}\n",
                    indent, "result[0] = ", args[0], '[', args[1], "];\n",
                    indent, "result[1] = ", args[0], '[', args[1], " + ", numberOfRows, "];\n");

                if (numberOfColumns >= 3)
                    stringBuilder.append(indent, "result[2] = ", args[0], '[', args[1], " + ", numberOfRows * 2, "];\n");
                if (numberOfColumns >= 4)
                    stringBuilder.append(indent, "result[3] = ", args[0], '[', args[1], " + ", numberOfRows * 3, "];\n");
    
                stringBuilder.append(indent, returnName, " = result;\n");
            }
            stringBuilder.append("} while (0);\n");
        } else {
            RELEASE_ASSERT(numTypeArguments == 2);
            unsigned numElements = vectorSize();

            auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());

            stringBuilder.append(indent, "do {\n");
            {
                IndentationScope scope(indent);
                stringBuilder.append(
                    indent, metalReturnName, " result;\n",
                    indent, "if (", args[1], " >= ", numElements, ") {\n",
                    indent, "    ", returnName, " = ", metalReturnName, "(0);\n",
                    indent, "    break;\n",
                    indent, "}\n",
                    indent, "result = ", args[0], "[", args[1], "];\n",
                    indent, returnName, " = result;\n");
            }
            stringBuilder.append(indent, "} while (0);\n");
        }

        return;
    }

    if (nativeFunctionDeclaration.name() == "operator[]=") {
        auto& typeReference = downcast<AST::TypeReference>(*nativeFunctionDeclaration.parameters()[0]->type());
        size_t numTypeArguments = downcast<AST::NativeTypeDeclaration>(downcast<AST::TypeReference>(downcast<AST::TypeDefinition>(typeReference.resolvedType()).type()).resolvedType()).typeArguments().size();
        if (numTypeArguments == 3) {
            ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
            auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
            auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());

            unsigned numberOfRows = getMatrixType().numberOfMatrixRows();
            unsigned numberOfColumns = getMatrixType().numberOfMatrixColumns();

            stringBuilder.append(indent, "do {\n");
            {
                IndentationScope scope(indent);

                stringBuilder.append(
                    indent, metalReturnName, " m = ", args[0], ";\n",
                    indent, metalParameter2Name, " i = ", args[1], ";\n",
                    indent, "if (i >= ", numberOfRows, ") {\n",
                    indent, "    ", returnName, " = m;\n",
                    indent, "    break;\n",
                    indent, "}\n",
                    indent, "m[i] = ", args[2], "[0];\n",
                    indent, "m[i + ", numberOfRows, "] = ", args[2], "[1];\n");
                if (numberOfColumns >= 3)
                    stringBuilder.append(indent, "m[i + ", numberOfRows * 2, "] = ", args[2], "[2];\n");
                if (numberOfColumns >= 4)
                    stringBuilder.append(indent, "m[i + ", numberOfRows * 3, "] = ", args[2], "[3];\n");
                stringBuilder.append(indent, returnName, " = m;\n");
            }
            
            stringBuilder.append(indent, "} while(0);\n");
        } else {
            RELEASE_ASSERT(numTypeArguments == 2);

            ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
            auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
            auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());

            unsigned numElements = vectorSize();

            stringBuilder.append(indent, "do {\n");
            {
                IndentationScope scope(indent);

                stringBuilder.append(
                    indent, metalReturnName, " v = ", args[0], ";\n",
                    indent, metalParameter2Name, " i = ", args[1], ";\n",
                    indent, "if (i >= ", numElements, ") {\n",
                    indent, "    ", returnName, " = v;\n",
                    indent, "    break;\n",
                    indent, "}\n",
                    indent, "v[i] = ", args[2], ";\n",
                    indent, returnName, " = v;\n");
            }
            stringBuilder.append(indent, "} while(0);\n");
        }

        return;
    }

    if (nativeFunctionDeclaration.isOperator()) {
        auto operatorName = nativeFunctionDeclaration.name().substring("operator"_str.length());
        auto metalReturnType = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        if (nativeFunctionDeclaration.parameters().size() == 1) {
            if (auto* matrixType = asMatrixType(nativeFunctionDeclaration.type())) {
                stringBuilder.append(indent, "{\n");
                {
                    IndentationScope scope(indent);
                    stringBuilder.append(
                        indent, metalReturnType, " x = ", args[0], ";\n",
                        indent, "for (size_t i = 0; i < x.size(); ++i)\n",
                        indent, "    x[i] = ", operatorName, "x[i];\n",
                        indent, returnName, " = x;\n");
                }
                stringBuilder.append(indent, "}\n");
            } else {
                stringBuilder.append(indent, "{\n");
                {
                    IndentationScope scope(indent);
                    stringBuilder.append(
                        indent, metalReturnType, " x = ", args[0], ";\n",
                        indent, returnName, " = ", operatorName, "x;\n");
                }
                stringBuilder.append(indent, "}\n");
            }
            return;
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        if (auto* leftMatrix = asMatrixType(*nativeFunctionDeclaration.parameters()[0]->type())) {
            if (auto* rightMatrix = asMatrixType(*nativeFunctionDeclaration.parameters()[1]->type())) {
                // matrix <op> matrix
                stringBuilder.append(indent, "{\n");
                {
                    IndentationScope scope(indent);
                    stringBuilder.append(
                        indent, metalReturnType, " x;\n",
                        indent, "for (size_t i = 0; i < x.size(); ++i)\n",
                        indent, "    x[i] = ", args[0], "[i] ", operatorName, ' ', args[1], "[i];\n",
                        indent, returnName, " = x;\n");
                }
                stringBuilder.append(indent, "}\n");
            } else {
                // matrix <op> scalar
                stringBuilder.append(indent, "{\n");
                {
                    IndentationScope scope(indent);
                    stringBuilder.append(
                        indent, metalReturnType, " x;\n",
                        indent, "for (size_t i = 0; i < x.size(); ++i)\n",
                        indent, "    x[i] = ", args[0], "[i] ", operatorName, ' ', args[1], ";\n",
                        indent, returnName, " = x;\n");
                }
                stringBuilder.append(indent, "}\n");
            }
        } else if (auto* rightMatrix = asMatrixType(*nativeFunctionDeclaration.parameters()[1]->type())) {
            ASSERT(!asMatrixType(*nativeFunctionDeclaration.parameters()[0]->type()));
            // scalar <op> matrix
            stringBuilder.append(indent, "{\n");
            {
                IndentationScope scope(indent);
                stringBuilder.append(
                    indent, metalReturnType, " x;\n",
                    indent, "for (size_t i = 0; i < x.size(); ++i)\n",
                    indent, "    x[i] = ", args[0], ' ', operatorName, ' ', args[1], "[i];\n",
                    indent, returnName, " = x;\n");
            }
            stringBuilder.append(indent, "}\n");
        } else {
            // scalar <op> scalar
            // vector <op> vector
            // vector <op> scalar
            // scalar <op> vector
            stringBuilder.append(
                indent, returnName, " = ", args[0], ' ', operatorName, ' ', args[1], ";\n");
        }

        return;
    }

    if (nativeFunctionDeclaration.name() == "cos"
        || nativeFunctionDeclaration.name() == "sin"
        || nativeFunctionDeclaration.name() == "tan"
        || nativeFunctionDeclaration.name() == "acos"
        || nativeFunctionDeclaration.name() == "asin"
        || nativeFunctionDeclaration.name() == "atan"
        || nativeFunctionDeclaration.name() == "cosh"
        || nativeFunctionDeclaration.name() == "sinh"
        || nativeFunctionDeclaration.name() == "tanh"
        || nativeFunctionDeclaration.name() == "ceil"
        || nativeFunctionDeclaration.name() == "exp"
        || nativeFunctionDeclaration.name() == "floor"
        || nativeFunctionDeclaration.name() == "log"
        || nativeFunctionDeclaration.name() == "round"
        || nativeFunctionDeclaration.name() == "trunc"
        || nativeFunctionDeclaration.name() == "ddx"
        || nativeFunctionDeclaration.name() == "ddy"
        || nativeFunctionDeclaration.name() == "isnormal"
        || nativeFunctionDeclaration.name() == "isfinite"
        || nativeFunctionDeclaration.name() == "isinf"
        || nativeFunctionDeclaration.name() == "isnan"
        || nativeFunctionDeclaration.name() == "asint"
        || nativeFunctionDeclaration.name() == "asuint"
        || nativeFunctionDeclaration.name() == "asfloat"
        || nativeFunctionDeclaration.name() == "length") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        stringBuilder.append(
            indent, returnName, " = ", mapFunctionName(nativeFunctionDeclaration.name()), '(', args[0], ");\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "pow" || nativeFunctionDeclaration.name() == "atan2") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        stringBuilder.append(
            indent, returnName, " = ", nativeFunctionDeclaration.name(), "(", args[0], ", ", args[1], ");\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "clamp") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
        if (asMatrixType(nativeFunctionDeclaration.type())) {
            auto metalReturnType = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
            
            stringBuilder.append(indent, "{\n");
            {
                IndentationScope scope(indent);
                stringBuilder.append(
                    indent, metalReturnType, " x;\n",
                    indent, "for (size_t i = 0; i < x.size(); ++i) \n",
                    indent, "    x[i] = clamp(", args[0], "[i], ", args[1], "[i], ", args[2], "[i]);",
                    indent, returnName, " = x;\n");
            }
            stringBuilder.append(indent, "}\n");
        } else {
            stringBuilder.append(
                indent, returnName, " = clamp(", args[0], ", ", args[1], ", ", args[2], ");\n");
        }
        return;
    }

    if (nativeFunctionDeclaration.name() == "AllMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(
            indent, "threadgroup_barrier(mem_flags::mem_device);\n",
            indent, "threadgroup_barrier(mem_flags::mem_threadgroup);\n",
            indent, "threadgroup_barrier(mem_flags::mem_texture);\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "DeviceMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(
            indent, "threadgroup_barrier(mem_flags::mem_device);\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "GroupMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(
            indent, "threadgroup_barrier(mem_flags::mem_threadgroup);\n");
        return;
    }

    if (nativeFunctionDeclaration.name().startsWith("Interlocked"_str)) {
        if (nativeFunctionDeclaration.name() == "InterlockedCompareExchange") {
            ASSERT(nativeFunctionDeclaration.parameters().size() == 4);
            stringBuilder.append(
                indent, "atomic_compare_exchange_weak_explicit(", args[0], ", &", args[1], ", ", args[2], ", memory_order_relaxed, memory_order_relaxed);\n",
                indent, '*', args[3], " = ", args[1], ";\n");
            return;
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
        auto name = atomicName(nativeFunctionDeclaration.name().substring("Interlocked"_str.length()));
        stringBuilder.append(
            indent, '*', args[2], " = atomic_", name, "_explicit(", args[0], ", ", args[1], ", memory_order_relaxed);\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "Sample") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 3 || nativeFunctionDeclaration.parameters().size() == 4);
        
        auto& textureType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[0]->type()->unifyNode()));
        auto& locationType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[2]->type()->unifyNode()));
        auto locationVectorLength = vectorLength(locationType);
        auto& returnType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.type().unifyNode()));
        auto returnVectorLength = vectorLength(returnType);

        stringBuilder.append(
            indent, returnName, " = ", args[0], ".sample(", args[1], ", ");

        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            stringBuilder.append(args[2], '.', "xyzw"_str.substring(0, locationVectorLength - 1), ", ", args[2], '.', "xyzw"_str.substring(locationVectorLength - 1, 1));
        } else
            stringBuilder.append(args[2]);
        if (nativeFunctionDeclaration.parameters().size() == 4)
            stringBuilder.append(", ", args[3]);
        stringBuilder.append(")");
        if (!textureType.isDepthTexture())
            stringBuilder.append(".", "xyzw"_str.substring(0, returnVectorLength));
        stringBuilder.append(";\n");

        return;
    }

    if (nativeFunctionDeclaration.name() == "Load") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        
        auto& textureType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[0]->type()->unifyNode()));
        auto& locationType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[1]->type()->unifyNode()));
        auto locationVectorLength = vectorLength(locationType);
        auto& returnType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.type().unifyNode()));
        auto returnVectorLength = vectorLength(returnType);

        auto metalReturnName = typeNamer.mangledNameForType(returnType);

        stringBuilder.append(indent, "do {\n");
        {
            IndentationScope scope(indent);

            if (textureType.isTextureArray()) {
                ASSERT(locationVectorLength > 1);
                String dimensions[] = { "width"_str, "height"_str, "depth"_str };
                for (int i = 0; i < locationVectorLength - 1; ++i) {
                    auto suffix = "xyzw"_str.substring(i, 1);
                    stringBuilder.append(
                        indent, "if (", args[1], '.', suffix, " < 0 || static_cast<uint32_t>(", args[1], '.', suffix, ") >= ", args[0], ".get_", dimensions[i], "()) {\n",
                        indent, "    ", returnName, " = ", metalReturnName, "(0);\n",
                        indent, "    break;\n",
                        indent, "}\n");
                }
                auto suffix = "xyzw"_str.substring(locationVectorLength - 1, 1);
                stringBuilder.append(
                    indent, "if (", args[1], '.', suffix, " < 0 || static_cast<uint32_t>(", args[1], '.', suffix, ") >= ", args[0], ".get_array_size()) {\n",
                    indent, "    ", returnName, " = ", metalReturnName, "(0);\n",
                    indent, "    break;\n",
                    indent, "}\n");
            } else {
                if (locationVectorLength == 1) {
                    stringBuilder.append(
                        indent, "if (", args[1], " < 0 || static_cast<uint32_t>(", args[1], ") >= ", args[0], ".get_width()) {\n",
                        indent, "    ", returnName, " = ", metalReturnName, "(0);\n",
                        indent, "    break;\n",
                        indent, "}\n");
                } else {
                    stringBuilder.append(
                        indent, "if (", args[1], ".x < 0 || static_cast<uint32_t>(", args[1], ".x) >= ", args[0], ".get_width()) {\n",
                        indent, "    ", returnName, " = ", metalReturnName, "(0);\n",
                        indent, "    break;\n",
                        indent, "}\n",
                        indent, "if (", args[1], ".y < 0 || static_cast<uint32_t>(", args[1], ".y) >= ", args[0], ".get_height()) {\n",
                        indent, "    ", returnName, " = ", metalReturnName, "(0);\n",
                        indent, "    break;\n",
                        indent, "}\n");
                    if (locationVectorLength >= 3) {
                        stringBuilder.append(
                            indent, "if (", args[1], ".z < 0 || static_cast<uint32_t>(", args[1], ".z) >= ", args[0], ".get_depth()) {\n",
                            indent, "    ", returnName, " = ", metalReturnName, "(0);\n",
                            indent, "    break;\n",
                            indent, "}\n");
                    }
                }
            }
            stringBuilder.append(indent, returnName, " = ", args[0], ".read(");
            if (textureType.isTextureArray()) {
                ASSERT(locationVectorLength > 1);
                stringBuilder.append("uint", vectorSuffix(locationVectorLength - 1), '(', args[1], '.', "xyzw"_str.substring(0, locationVectorLength - 1), "), uint(", args[1], '.', "xyzw"_str.substring(locationVectorLength - 1, 1), ')');
            } else
                stringBuilder.append("uint", vectorSuffix(locationVectorLength), '(', args[1], ')');
            stringBuilder.append(')');
            if (!textureType.isDepthTexture())
                stringBuilder.append('.', "xyzw"_str.substring(0, returnVectorLength));
            stringBuilder.append(";\n");
        }
        stringBuilder.append(indent, "} while(0);\n");

        return;
    }

    if (nativeFunctionDeclaration.name() == "load") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        stringBuilder.append(
            indent, returnName, " = atomic_load_explicit(", args[0], ", memory_order_relaxed);\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "store") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        stringBuilder.append(
            indent, "atomic_store_explicit(", args[0], ", ", args[1], ", memory_order_relaxed);\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "GetDimensions") {
        auto& textureType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[0]->type()->unifyNode()));

        size_t index = 1;
        bool hasMipLevel = !textureType.isWritableTexture() && textureType.textureDimension() != 1;
        if (hasMipLevel)
            ++index;
        const MangledVariableName& widthName = args[index];
        ++index;
        Optional<MangledVariableName> heightName;
        if (textureType.textureDimension() >= 2) {
            heightName = args[index];
            ++index;
        }
        Optional<MangledVariableName> depthName;
        if (textureType.textureDimension() >= 3) {
            depthName = args[index];
            ++index;
        }
        Optional<MangledVariableName> elementsName;
        if (textureType.isTextureArray()) {
            elementsName = args[index];
            ++index;
        }
        Optional<MangledVariableName> numberOfLevelsName;
        if (!textureType.isWritableTexture() && textureType.textureDimension() != 1) {
            numberOfLevelsName = args[index];
            ++index;
        }
        ASSERT(index == nativeFunctionDeclaration.parameters().size());

        stringBuilder.append(
            indent, "if (", widthName, ")\n",
            indent, "    *", widthName, " = ", args[0], ".get_width(");
        if (hasMipLevel)
            stringBuilder.append(args[1]);
        stringBuilder.append(");\n");

        if (heightName) {
            stringBuilder.append(
                indent, "if (", *heightName, ")\n",
                indent, "    *", *heightName, " = ", args[0], ".get_height(");
            if (hasMipLevel)
                stringBuilder.append(args[1]);
            stringBuilder.append(");\n");
        }
        if (depthName) {
            stringBuilder.append(
                indent, "if (", *depthName, ")\n",
                indent, "    *", *depthName, " = ", args[0], ".get_depth(");
            if (hasMipLevel)
                stringBuilder.append(args[1]);
            stringBuilder.append(");\n");
        }
        if (elementsName) {
            stringBuilder.append(
                indent, "if (", *elementsName, ")\n",
                indent, "    *", *elementsName, " = ", args[0], ".get_array_size();\n");
        }
        if (numberOfLevelsName) {
            stringBuilder.append(
                indent, "if (", *numberOfLevelsName, ")\n",
                indent, "    *", *numberOfLevelsName, " = ", args[0], ".get_num_mip_levels();\n");
        }
        return;
    }

    if (nativeFunctionDeclaration.name() == "SampleBias") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "SampleGrad") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "SampleLevel") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "Gather") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "GatherRed") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "SampleCmp") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "SampleCmpLevelZero") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "Store") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
        
        auto& textureType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[0]->type()->unifyNode()));
        auto& itemType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[1]->type()->unifyNode()));
        auto& itemVectorInnerType = vectorInnerType(itemType);
        auto itemVectorLength = vectorLength(itemType);
        auto& locationType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[2]->type()->unifyNode()));
        auto locationVectorLength = vectorLength(locationType);

        auto metalInnerTypeName = typeNamer.mangledNameForType(itemVectorInnerType);

        stringBuilder.append("do {\n");
        {
            IndentationScope scope(indent);

            if (textureType.isTextureArray()) {
                ASSERT(locationVectorLength > 1);
                String dimensions[] = { "width"_str, "height"_str, "depth"_str };
                for (int i = 0; i < locationVectorLength - 1; ++i) {
                    auto suffix = "xyzw"_str.substring(i, 1);
                    stringBuilder.append(
                        indent, "if (", args[2], ".", suffix, " >= ", args[0], ".get_", dimensions[i], "())\n",
                        indent, "    break;\n");
                }
                auto suffix = "xyzw"_str.substring(locationVectorLength - 1, 1);
                stringBuilder.append(
                    indent, "if (", args[2], '.', suffix, " >= ", args[0], ".get_array_size())\n",
                    indent, "    break;\n");
            } else {
                if (locationVectorLength == 1) {
                    stringBuilder.append(
                        indent, "if (", args[2], " >= ", args[0], ".get_width()) \n",
                        indent, "    break;\n");
                } else {
                    stringBuilder.append(
                        indent, "if (", args[2], ".x >= ", args[0], ".get_width())\n",
                        indent, "    break;\n",
                        indent, "if (", args[2], ".y >= ", args[0], ".get_height())\n",
                        indent, "    break;\n");
                    if (locationVectorLength >= 3) {
                        stringBuilder.append(
                            indent, "if (", args[2], ".z >= ", args[0], ".get_depth())\n",
                            indent, "    break;\n");
                    }
                }
            }
            stringBuilder.append(indent, args[0], ".write(vec<", metalInnerTypeName, ", 4>(", args[1]);
            for (int i = 0; i < 4 - itemVectorLength; ++i)
                stringBuilder.append(", 0");
            stringBuilder.append("), ");
            if (textureType.isTextureArray()) {
                ASSERT(locationVectorLength > 1);
                stringBuilder.append("uint", vectorSuffix(locationVectorLength - 1), '(', args[2], '.', "xyzw"_str.substring(0, locationVectorLength - 1), "), uint(", args[2], ".", "xyzw"_str.substring(locationVectorLength - 1, 1), ')');
            } else
                stringBuilder.append("uint", vectorSuffix(locationVectorLength), '(', args[2], ')');
            stringBuilder.append(");\n");
        }
        stringBuilder.append(indent, "} while(0);\n");

        return;
    }

    if (nativeFunctionDeclaration.name() == "GatherAlpha") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "GatherBlue") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "GatherCmp") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "GatherCmpRed") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "GatherGreen") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    ASSERT_NOT_REACHED();
}

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif
