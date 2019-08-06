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

void inlineNativeFunction(StringBuilder& stringBuilder, AST::NativeFunctionDeclaration& nativeFunctionDeclaration, MangledVariableName returnName, const Vector<MangledVariableName>& args, Intrinsics& intrinsics, TypeNamer& typeNamer)
{
    if (nativeFunctionDeclaration.isCast()) {
        auto& returnType = nativeFunctionDeclaration.type();
        auto metalReturnName = typeNamer.mangledNameForType(returnType);
        if (!nativeFunctionDeclaration.parameters().size()) {
            stringBuilder.flexibleAppend(returnName, " = { };\n");
            return;
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto& parameterType = *nativeFunctionDeclaration.parameters()[0]->type();
        auto metalParameterName = typeNamer.mangledNameForType(parameterType);
        stringBuilder.flexibleAppend("{\n", metalParameterName, " x = ", args[0], ";\n");

        {
            auto isEnumerationDefinition = [] (auto& type) {
                return is<AST::NamedType>(type) && is<AST::EnumerationDefinition>(downcast<AST::NamedType>(type));
            };
            auto& unifiedReturnType = returnType.unifyNode();
            if (isEnumerationDefinition(unifiedReturnType) && !isEnumerationDefinition(parameterType.unifyNode())) { 
                auto& enumerationDefinition = downcast<AST::EnumerationDefinition>(downcast<AST::NamedType>(unifiedReturnType));
                stringBuilder.append("    switch (x) {\n");
                bool hasZeroCase = false;
                for (auto& member : enumerationDefinition.enumerationMembers()) {
                    hasZeroCase |= !member.get().value();
                    stringBuilder.flexibleAppend("        case ", member.get().value(), ": break;\n");
                }
                ASSERT_UNUSED(hasZeroCase, hasZeroCase);
                stringBuilder.append("        default: x = 0; break; }\n");
            }
        }

        stringBuilder.flexibleAppend(
            returnName, " = static_cast<", metalReturnName, ">(x);\n}\n");

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
            stringBuilder.flexibleAppend(
                returnName, " = ", arrayParameterType.numElements(), ";\n");
            return;
        }

        ASSERT(is<AST::ArrayReferenceType>(unnamedParameterType));
        stringBuilder.flexibleAppend(
            returnName, " = ", args[0], ".length;\n");
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
                stringBuilder.flexibleAppend(typeNamer.mangledNameForStructureElement(*structureElement));
                return;
            }
            ASSERT(is<AST::NativeTypeDeclaration>(namedType));
            stringBuilder.append(fieldName);
        };

        if (nativeFunctionDeclaration.name().endsWith("=")) {
            ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
            auto fieldName = nativeFunctionDeclaration.name().substring("operator."_str.length());
            fieldName = fieldName.substring(0, fieldName.length() - 1);

            stringBuilder.flexibleAppend(
                returnName, " = ", args[0], ";\n",
                returnName, '.');
            appendMangledFieldName(fieldName);
            stringBuilder.flexibleAppend(" = ", args[1], ";\n");

            return;
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto fieldName = nativeFunctionDeclaration.name().substring("operator."_str.length());
        stringBuilder.flexibleAppend(
            returnName, " = ", args[0], '.');
        appendMangledFieldName(fieldName);
        stringBuilder.append(";\n");
        return;
    }

    if (nativeFunctionDeclaration.name().startsWith("operator&."_str)) {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto fieldName = nativeFunctionDeclaration.name().substring("operator&."_str.length());

        stringBuilder.flexibleAppend(
            returnName, " = &(", args[0], "->");

        auto& unnamedType = *nativeFunctionDeclaration.parameters()[0]->type();
        auto& unifyNode = downcast<AST::PointerType>(unnamedType).elementType().unifyNode();
        auto& namedType = downcast<AST::NamedType>(unifyNode);
        if (is<AST::StructureDefinition>(namedType)) {
            auto& structureDefinition = downcast<AST::StructureDefinition>(namedType);
            auto* structureElement = structureDefinition.find(fieldName);
            ASSERT(structureElement);
            stringBuilder.flexibleAppend(typeNamer.mangledNameForStructureElement(*structureElement));
        } else
            stringBuilder.flexibleAppend(fieldName);

        stringBuilder.append(");\n");

        return;
    }

    if (nativeFunctionDeclaration.name() == "operator&[]") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        ASSERT(is<AST::ArrayReferenceType>(*nativeFunctionDeclaration.parameters()[0]->type()));

        stringBuilder.flexibleAppend(
            returnName, " = (", args[1], " < ", args[0], ".length) ? ", " &(", args[0], ".pointer[", args[1], "]) : nullptr;\n");
            
        return;
    }

    auto matrixDimension = [&] (unsigned typeArgumentIndex) -> unsigned {
        auto& typeReference = downcast<AST::TypeReference>(*nativeFunctionDeclaration.parameters()[0]->type());
        auto& matrixType = downcast<AST::NativeTypeDeclaration>(downcast<AST::TypeReference>(downcast<AST::TypeDefinition>(typeReference.resolvedType()).type()).resolvedType());
        ASSERT(matrixType.name() == "matrix");
        ASSERT(matrixType.typeArguments().size() == 3);
        return WTF::get<AST::ConstantExpression>(matrixType.typeArguments()[typeArgumentIndex]).integerLiteral().value();
    };
    auto numberOfMatrixRows = [&] {
        return matrixDimension(1);
    };
    auto numberOfMatrixColumns = [&] {
        return matrixDimension(2);
    };

    if (nativeFunctionDeclaration.name() == "operator[]") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());

        unsigned numberOfRows = numberOfMatrixRows();
        unsigned numberOfColumns = numberOfMatrixColumns();
        stringBuilder.flexibleAppend("do {\n", metalReturnName, " result;\n");

        stringBuilder.flexibleAppend(
            "    if (", args[1], " >= ", numberOfRows, ") {", returnName, " = ", metalReturnName, "(0); break;}\n",
            "    result[0] = ", args[0], '[', args[1], "];\n",
            "    result[1] = ", args[0], '[', args[1], " + ", numberOfRows, "];\n");

        if (numberOfColumns >= 3)
            stringBuilder.flexibleAppend("    result[2] = ", args[0], '[', args[1], " + ", numberOfRows * 2, "];\n");
        if (numberOfColumns >= 4)
            stringBuilder.flexibleAppend("    result[3] = ", args[0], '[', args[1], " + ", numberOfRows * 3, "];\n");

        stringBuilder.flexibleAppend(
            "    ", returnName, " = result;\n",
            "} while (0);\n");

        return;
    }

    if (nativeFunctionDeclaration.name() == "operator[]=") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
        auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());

        unsigned numberOfRows = numberOfMatrixRows();
        unsigned numberOfColumns = numberOfMatrixColumns();

        stringBuilder.flexibleAppend("do {\n", metalReturnName, " m = ", args[0], ";\n",
            metalParameter2Name, " i = ", args[1], ";\n");

        stringBuilder.flexibleAppend(
            "    if (i >= ", numberOfRows, ") {", returnName, " = m;\nbreak;}\n",
            "    m[i] = ", args[2], "[0];\n",
            "    m[i + ", numberOfRows, "] = ", args[2], "[1];\n");
        if (numberOfColumns >= 3)
            stringBuilder.flexibleAppend("    m[i + ", numberOfRows * 2, "] = ", args[2], "[2];\n");
        if (numberOfColumns >= 4)
            stringBuilder.flexibleAppend("    m[i + ", numberOfRows * 3, "] = ", args[2], "[3];\n");
        stringBuilder.flexibleAppend(
            "    ", returnName, " = m;\n",
            "} while(0);\n");
        return;
    }

    if (nativeFunctionDeclaration.isOperator()) {
        auto operatorName = nativeFunctionDeclaration.name().substring("operator"_str.length());
        if (nativeFunctionDeclaration.parameters().size() == 1) {
            auto metalParameterName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
            stringBuilder.flexibleAppend(
                "{\n", metalParameterName, " x = ", args[0], ";\n", 
                returnName, " = ", operatorName, "x;\n}\n");
            return;
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        stringBuilder.flexibleAppend(
            returnName, " = ", args[0], ' ', operatorName, ' ', args[1], ";\n");
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
        || nativeFunctionDeclaration.name() == "asfloat") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        stringBuilder.flexibleAppend(
            returnName, " = ", mapFunctionName(nativeFunctionDeclaration.name()), '(', args[0], ");\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "pow" || nativeFunctionDeclaration.name() == "atan2") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        stringBuilder.flexibleAppend(
            returnName, " = ", nativeFunctionDeclaration.name(), "(", args[0], ", ", args[1], ");\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "AllMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(
            "threadgroup_barrier(mem_flags::mem_device);\n"
            "threadgroup_barrier(mem_flags::mem_threadgroup);\n"
            "threadgroup_barrier(mem_flags::mem_texture);\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "DeviceMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(
            "threadgroup_barrier(mem_flags::mem_device);\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "GroupMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(
            "threadgroup_barrier(mem_flags::mem_threadgroup);\n");
        return;
    }

    if (nativeFunctionDeclaration.name().startsWith("Interlocked"_str)) {
        if (nativeFunctionDeclaration.name() == "InterlockedCompareExchange") {
            ASSERT(nativeFunctionDeclaration.parameters().size() == 4);
            stringBuilder.flexibleAppend(
                "atomic_compare_exchange_weak_explicit(", args[0], ", &", args[1], ", ", args[2], ", memory_order_relaxed, memory_order_relaxed);\n",
                '*', args[3], " = ", args[1], ";\n");
            return;
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
        auto name = atomicName(nativeFunctionDeclaration.name().substring("Interlocked"_str.length()));
        stringBuilder.flexibleAppend(
            '*', args[2], " = atomic_", name, "_explicit(", args[0], ", ", args[1], ", memory_order_relaxed);\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "Sample") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 3 || nativeFunctionDeclaration.parameters().size() == 4);
        
        auto& textureType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[0]->type()->unifyNode()));
        auto& locationType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[2]->type()->unifyNode()));
        auto locationVectorLength = vectorLength(locationType);
        auto& returnType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.type().unifyNode()));
        auto returnVectorLength = vectorLength(returnType);

        stringBuilder.flexibleAppend(
            returnName, " = ", args[0], ".sample(", args[1], ", ");

        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            stringBuilder.flexibleAppend(args[2], '.', "xyzw"_str.substring(0, locationVectorLength - 1), ", ", args[2], '.', "xyzw"_str.substring(locationVectorLength - 1, 1));
        } else
            stringBuilder.flexibleAppend(args[2]);
        if (nativeFunctionDeclaration.parameters().size() == 4)
            stringBuilder.flexibleAppend(", ", args[3]);
        stringBuilder.append(")");
        if (!textureType.isDepthTexture())
            stringBuilder.flexibleAppend(".", "xyzw"_str.substring(0, returnVectorLength));
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
        stringBuilder.append("do {\n");

        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            String dimensions[] = { "width"_str, "height"_str, "depth"_str };
            for (int i = 0; i < locationVectorLength - 1; ++i) {
                auto suffix = "xyzw"_str.substring(i, 1);
                stringBuilder.flexibleAppend("    if (", args[1], '.', suffix, " < 0 || static_cast<uint32_t>(", args[1], '.', suffix, ") >= ", args[0], ".get_", dimensions[i], "()) {", returnName, " = ", metalReturnName, "(0); break;}\n");
            }
            auto suffix = "xyzw"_str.substring(locationVectorLength - 1, 1);
            stringBuilder.flexibleAppend("    if (", args[1], '.', suffix, " < 0 || static_cast<uint32_t>(", args[1], '.', suffix, ") >= ", args[0], ".get_array_size()) {", returnName, " = ", metalReturnName, "(0); break;}\n");
        } else {
            if (locationVectorLength == 1)
                stringBuilder.flexibleAppend("    if (", args[1], " < 0 || static_cast<uint32_t>(", args[1], ") >= ", args[0], ".get_width()) { ", returnName, " = ", metalReturnName, "(0); break;}\n");
            else {
                stringBuilder.flexibleAppend(
                    "    if (", args[1], ".x < 0 || static_cast<uint32_t>(", args[1], ".x) >= ", args[0], ".get_width()) {", returnName, " = ", metalReturnName, "(0); break;}\n"
                    "    if (", args[1], ".y < 0 || static_cast<uint32_t>(", args[1], ".y) >= ", args[0], ".get_height()) {", returnName, " = ", metalReturnName, "(0); break;}\n");

                if (locationVectorLength >= 3)
                    stringBuilder.flexibleAppend("    if (", args[1], ".z < 0 || static_cast<uint32_t>(", args[1], ".z) >= ", args[0], ".get_depth()) {", returnName, " = ", metalReturnName, "(0); break;}\n");
            }
        }
        stringBuilder.flexibleAppend("    ", returnName, " = ", args[0], ".read(");
        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            stringBuilder.flexibleAppend("uint", vectorSuffix(locationVectorLength - 1), '(', args[1], '.', "xyzw"_str.substring(0, locationVectorLength - 1), "), uint(", args[1], '.', "xyzw"_str.substring(locationVectorLength - 1, 1), ')');
        } else
            stringBuilder.flexibleAppend("uint", vectorSuffix(locationVectorLength), '(', args[1], ')');
        stringBuilder.append(')');
        if (!textureType.isDepthTexture())
            stringBuilder.flexibleAppend('.', "xyzw"_str.substring(0, returnVectorLength));
        stringBuilder.append(
            ";\n"
            "} while(0);\n");

        return;
    }

    if (nativeFunctionDeclaration.name() == "load") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        stringBuilder.flexibleAppend(
            returnName, " = atomic_load_explicit(", args[0], ", memory_order_relaxed);\n");
        return;
    }

    if (nativeFunctionDeclaration.name() == "store") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        stringBuilder.flexibleAppend(
            "atomic_store_explicit(", args[0], ", ", args[1], ", memory_order_relaxed);\n");
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

        stringBuilder.flexibleAppend(
            "if (", widthName, ")\n"
            "    *", widthName, " = ", args[0], ".get_width(");

        if (hasMipLevel)
            stringBuilder.flexibleAppend(args[1]);
        stringBuilder.append(");\n");
        if (heightName) {
            stringBuilder.flexibleAppend(
                "    if (", *heightName, ")\n"
                "        *", *heightName, " = ", args[0], ".get_height(");
            if (hasMipLevel)
                stringBuilder.flexibleAppend(args[1]);
            stringBuilder.append(");\n");
        }
        if (depthName) {
            stringBuilder.flexibleAppend(
                "    if (", *depthName, ")\n"
                "        *", *depthName, " = ", args[0], ".get_depth(");
            if (hasMipLevel)
                stringBuilder.flexibleAppend(args[1]);
            stringBuilder.append(");\n");
        }
        if (elementsName) {
            stringBuilder.flexibleAppend(
                "    if (", *elementsName, ")\n"
                "        *", *elementsName, " = ", args[0], ".get_array_size();\n");
        }
        if (numberOfLevelsName) {
            stringBuilder.flexibleAppend(
                "    if (", *numberOfLevelsName, ")\n"
                "        *", *numberOfLevelsName, " = ", args[0], ".get_num_mip_levels();\n");
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
        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            String dimensions[] = { "width"_str, "height"_str, "depth"_str };
            for (int i = 0; i < locationVectorLength - 1; ++i) {
                auto suffix = "xyzw"_str.substring(i, 1);
                stringBuilder.flexibleAppend("    if (", args[2], ".", suffix, " >= ", args[0], ".get_", dimensions[i], "()) break;\n");
            }
            auto suffix = "xyzw"_str.substring(locationVectorLength - 1, 1);
            stringBuilder.flexibleAppend("    if (", args[2], '.', suffix, " >= ", args[0], ".get_array_size()) break;\n");
        } else {
            if (locationVectorLength == 1)
                stringBuilder.flexibleAppend("    if (", args[2], " >= ", args[0], ".get_width()) break;\n");
            else {
                stringBuilder.flexibleAppend(
                    "    if (", args[2], ".x >= ", args[0], ".get_width()) break;\n"
                    "    if (", args[2], ".y >= ", args[0], ".get_height()) break;\n");
                if (locationVectorLength >= 3)
                    stringBuilder.flexibleAppend("    if (", args[2], ".z >= ", args[0], ".get_depth()) break;\n");
            }
        }
        stringBuilder.flexibleAppend("    ", args[0], ".write(vec<", metalInnerTypeName, ", 4>(", args[1]);
        for (int i = 0; i < 4 - itemVectorLength; ++i)
            stringBuilder.append(", 0");
        stringBuilder.append("), ");
        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            stringBuilder.flexibleAppend("uint", vectorSuffix(locationVectorLength - 1), '(', args[2], '.', "xyzw"_str.substring(0, locationVectorLength - 1), "), uint(", args[2], ".", "xyzw"_str.substring(locationVectorLength - 1, 1), ')');
        } else
            stringBuilder.flexibleAppend("uint", vectorSuffix(locationVectorLength), '(', args[2], ')');
        stringBuilder.append(
            ");\n"
            "} while(0);\n");

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
