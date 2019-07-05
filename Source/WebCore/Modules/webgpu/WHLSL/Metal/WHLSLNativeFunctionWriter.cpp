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
    ASSERT(WTF::holds_alternative<UniqueRef<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]));
    return WTF::get<UniqueRef<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0])->resolvedType();
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

String writeNativeFunction(AST::NativeFunctionDeclaration& nativeFunctionDeclaration, String& outputFunctionName, Intrinsics& intrinsics, TypeNamer& typeNamer, const char* memsetZeroFunctionName)
{
    StringBuilder stringBuilder;
    if (nativeFunctionDeclaration.isCast()) {
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        if (!nativeFunctionDeclaration.parameters().size()) {
            stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, "() {\n"));
            stringBuilder.append(makeString("    ", metalReturnName, " x;\n"));
            stringBuilder.append(makeString("    ", memsetZeroFunctionName, "(x);\n"));
            stringBuilder.append("    return x;\n");
            stringBuilder.append("}\n");
            return stringBuilder.toString();
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto metalParameterName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, " x) {\n"));
        stringBuilder.append(makeString("    return static_cast<", metalReturnName, ">(x);\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "operator.value") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto metalParameterName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, " x) {\n"));
        stringBuilder.append(makeString("    return static_cast<", metalReturnName, ">(x);\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198077 Authors can make a struct field named "length" too. Autogenerated getters for those shouldn't take this codepath.
    if (nativeFunctionDeclaration.name() == "operator.length") {
        ASSERT_UNUSED(intrinsics, matches(nativeFunctionDeclaration.type(), intrinsics.uintType()));
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto metalParameterName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto& parameterType = nativeFunctionDeclaration.parameters()[0]->type()->unifyNode();
        auto& unnamedParameterType = downcast<AST::UnnamedType>(parameterType);
        if (is<AST::ArrayType>(unnamedParameterType)) {
            auto& arrayParameterType = downcast<AST::ArrayType>(unnamedParameterType);
            stringBuilder.append(makeString("uint ", outputFunctionName, '(', metalParameterName, ") {\n"));
            stringBuilder.append(makeString("    return ", arrayParameterType.numElements(), ";\n"));
            stringBuilder.append("}\n");
            return stringBuilder.toString();
        }

        ASSERT(is<AST::ArrayReferenceType>(unnamedParameterType));
        stringBuilder.append(makeString("uint ", outputFunctionName, '(', metalParameterName, " v) {\n"));
        stringBuilder.append(makeString("    return v.length;\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name().startsWith("operator."_str)) {
        auto mangledFieldName = [&](const String& fieldName) -> String {
            auto& unifyNode = nativeFunctionDeclaration.parameters()[0]->type()->unifyNode();
            auto& namedType = downcast<AST::NamedType>(unifyNode);
            if (is<AST::StructureDefinition>(namedType)) {
                auto& structureDefinition = downcast<AST::StructureDefinition>(namedType);
                auto* structureElement = structureDefinition.find(fieldName);
                ASSERT(structureElement);
                return typeNamer.mangledNameForStructureElement(*structureElement);
            }
            ASSERT(is<AST::NativeTypeDeclaration>(namedType));
            return fieldName;
        };

        if (nativeFunctionDeclaration.name().endsWith("=")) {
            ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
            auto metalParameter1Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
            auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
            auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
            auto fieldName = nativeFunctionDeclaration.name().substring("operator."_str.length());
            fieldName = fieldName.substring(0, fieldName.length() - 1);
            auto metalFieldName = mangledFieldName(fieldName);
            stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, " v, ", metalParameter2Name, " n) {\n"));
            stringBuilder.append(makeString("    v.", metalFieldName, " = n;\n"));
            stringBuilder.append(makeString("    return v;\n"));
            stringBuilder.append("}\n");
            return stringBuilder.toString();
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto fieldName = nativeFunctionDeclaration.name().substring("operator."_str.length());
        auto metalParameterName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        auto metalFieldName = mangledFieldName(fieldName);
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, " v) {\n"));
        stringBuilder.append(makeString("    return v.", metalFieldName, ";\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name().startsWith("operator&."_str)) {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto metalParameterName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        auto fieldName = nativeFunctionDeclaration.name().substring("operator&."_str.length());

        String metalFieldName;
        auto& unnamedType = *nativeFunctionDeclaration.parameters()[0]->type();
        auto& unifyNode = downcast<AST::PointerType>(unnamedType).elementType().unifyNode();
        auto& namedType = downcast<AST::NamedType>(unifyNode);
        if (is<AST::StructureDefinition>(namedType)) {
            auto& structureDefinition = downcast<AST::StructureDefinition>(namedType);
            auto* structureElement = structureDefinition.find(fieldName);
            ASSERT(structureElement);
            metalFieldName = typeNamer.mangledNameForStructureElement(*structureElement);
        } else
            metalFieldName = fieldName;

        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, " v) {\n"));
        stringBuilder.append(makeString("    return &(v->", metalFieldName, ");\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "operator&[]") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto metalParameter1Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, " v, ", metalParameter2Name, " n) {\n"));
        ASSERT(is<AST::ArrayReferenceType>(*nativeFunctionDeclaration.parameters()[0]->type()));
        stringBuilder.append("    if (n < v.length) return &(v.pointer[n]);\n");
        stringBuilder.append("    return nullptr;\n");
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    auto numberOfMatrixRows = [&] {
        auto& typeReference = downcast<AST::TypeReference>(*nativeFunctionDeclaration.parameters()[0]->type());
        auto& matrixType = downcast<AST::NativeTypeDeclaration>(downcast<AST::TypeReference>(downcast<AST::TypeDefinition>(typeReference.resolvedType()).type()).resolvedType());
        ASSERT(matrixType.name() == "matrix");
        ASSERT(matrixType.typeArguments().size() == 3);
        return String::number(WTF::get<AST::ConstantExpression>(matrixType.typeArguments()[1]).integerLiteral().value());
    };

    if (nativeFunctionDeclaration.name() == "operator[]") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto metalParameter1Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, " m, ", metalParameter2Name, " i) {\n"));
        stringBuilder.append(makeString("    if (i < ", numberOfMatrixRows(), ") return m[i];\n"));
        stringBuilder.append(makeString("    return ", metalReturnName, "(0);\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "operator[]=") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
        auto metalParameter1Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        auto metalParameter3Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[2]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, " m, ", metalParameter2Name, " i, ", metalParameter3Name, " v) {\n"));
        stringBuilder.append(makeString("    if (i < ", numberOfMatrixRows(), ") m[i] = v;\n"));
        stringBuilder.append("    return m;\n");
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.isOperator()) {
        if (nativeFunctionDeclaration.parameters().size() == 1) {
            auto operatorName = nativeFunctionDeclaration.name().substring("operator"_str.length());
            auto metalParameterName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
            auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
            stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, " x) {\n"));
            stringBuilder.append(makeString("    return ", operatorName, "x;\n"));
            stringBuilder.append("}\n");
            return stringBuilder.toString();
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto operatorName = nativeFunctionDeclaration.name().substring("operator"_str.length());
        auto metalParameter1Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, " x, ", metalParameter2Name, " y) {\n"));
        stringBuilder.append(makeString("    return x ", operatorName, " y;\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
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
        auto metalParameterName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, " x) {\n"));
        stringBuilder.append(makeString("    return ", mapFunctionName(nativeFunctionDeclaration.name()), "(x);\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "pow" || nativeFunctionDeclaration.name() == "atan2") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto metalParameter1Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, " x, ", metalParameter2Name, " y) {\n"));
        stringBuilder.append(makeString("    return ", nativeFunctionDeclaration.name(), "(x, y);\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "AllMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(makeString("void ", outputFunctionName, "() {\n"));
        stringBuilder.append("    threadgroup_barrier(mem_flags::mem_device);\n");
        stringBuilder.append("    threadgroup_barrier(mem_flags::mem_threadgroup);\n");
        stringBuilder.append("    threadgroup_barrier(mem_flags::mem_texture);\n");
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "DeviceMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(makeString("void ", outputFunctionName, "() {\n"));
        stringBuilder.append("    threadgroup_barrier(mem_flags::mem_device);\n");
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "GroupMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(makeString("void ", outputFunctionName, "() {\n"));
        stringBuilder.append("    threadgroup_barrier(mem_flags::mem_threadgroup);\n");
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name().startsWith("Interlocked"_str)) {
        if (nativeFunctionDeclaration.name() == "InterlockedCompareExchange") {
            ASSERT(nativeFunctionDeclaration.parameters().size() == 4);
            auto& firstArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[0]->type());
            auto firstArgumentAddressSpace = firstArgumentPointer.addressSpace();
            auto firstArgumentPointee = typeNamer.mangledNameForType(firstArgumentPointer.elementType());
            auto secondArgument = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
            auto thirdArgument = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[2]->type());
            auto& fourthArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[3]->type());
            auto fourthArgumentAddressSpace = fourthArgumentPointer.addressSpace();
            auto fourthArgumentPointee = typeNamer.mangledNameForType(fourthArgumentPointer.elementType());
            stringBuilder.append(makeString("void ", outputFunctionName, '(', toString(firstArgumentAddressSpace), ' ', firstArgumentPointee, "* object, ", secondArgument, " compare, ", thirdArgument, " desired, ", toString(fourthArgumentAddressSpace), ' ', fourthArgumentPointee, "* out) {\n"));
            stringBuilder.append("    atomic_compare_exchange_weak_explicit(object, &compare, desired, memory_order_relaxed, memory_order_relaxed);\n");
            stringBuilder.append("    *out = compare;\n");
            stringBuilder.append("}\n");
            return stringBuilder.toString();
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
        auto& firstArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[0]->type());
        auto firstArgumentAddressSpace = firstArgumentPointer.addressSpace();
        auto firstArgumentPointee = typeNamer.mangledNameForType(firstArgumentPointer.elementType());
        auto secondArgument = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        auto& thirdArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[2]->type());
        auto thirdArgumentAddressSpace = thirdArgumentPointer.addressSpace();
        auto thirdArgumentPointee = typeNamer.mangledNameForType(thirdArgumentPointer.elementType());
        auto name = atomicName(nativeFunctionDeclaration.name().substring("Interlocked"_str.length()));
        stringBuilder.append(makeString("void ", outputFunctionName, '(', toString(firstArgumentAddressSpace), ' ', firstArgumentPointee, "* object, ", secondArgument, " operand, ", toString(thirdArgumentAddressSpace), ' ', thirdArgumentPointee, "* out) {\n"));
        stringBuilder.append(makeString("    *out = atomic_", name, "_explicit(object, operand, memory_order_relaxed);\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "Sample") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 3 || nativeFunctionDeclaration.parameters().size() == 4);
        
        auto& textureType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[0]->type()->unifyNode()));
        auto& locationType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[2]->type()->unifyNode()));
        auto locationVectorLength = vectorLength(locationType);
        auto& returnType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.type().unifyNode()));
        auto returnVectorLength = vectorLength(returnType);

        auto metalParameter1Name = typeNamer.mangledNameForType(textureType);
        auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        auto metalParameter3Name = typeNamer.mangledNameForType(locationType);
        String metalParameter4Name;
        if (nativeFunctionDeclaration.parameters().size() == 4)
            metalParameter4Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[3]->type());
        auto metalReturnName = typeNamer.mangledNameForType(returnType);
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, " theTexture, ", metalParameter2Name, " theSampler, ", metalParameter3Name, " location"));
        if (!metalParameter4Name.isNull())
            stringBuilder.append(makeString(", ", metalParameter4Name, " offset"));
        stringBuilder.append(") {\n");
        stringBuilder.append("    return theTexture.sample(theSampler, ");
        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            stringBuilder.append(makeString("location.", "xyzw"_str.substring(0, locationVectorLength - 1), ", location.", "xyzw"_str.substring(locationVectorLength - 1, 1)));
        } else
            stringBuilder.append("location");
        if (!metalParameter4Name.isNull())
            stringBuilder.append(", offset");
        stringBuilder.append(")");
        if (!textureType.isDepthTexture())
            stringBuilder.append(makeString(".", "xyzw"_str.substring(0, returnVectorLength)));
        stringBuilder.append(";\n");
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "Load") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        
        auto& textureType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[0]->type()->unifyNode()));
        auto& locationType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[1]->type()->unifyNode()));
        auto locationVectorLength = vectorLength(locationType);
        auto& returnType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.type().unifyNode()));
        auto returnVectorLength = vectorLength(returnType);

        auto metalParameter1Name = typeNamer.mangledNameForType(textureType);
        auto metalParameter2Name = typeNamer.mangledNameForType(locationType);
        auto metalReturnName = typeNamer.mangledNameForType(returnType);
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, " theTexture, ", metalParameter2Name, " location) {\n"));
        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            String dimensions[] = { "width"_str, "height"_str, "depth"_str };
            for (int i = 0; i < locationVectorLength - 1; ++i) {
                auto suffix = "xyzw"_str.substring(i, 1);
                stringBuilder.append(makeString("    if (location.", suffix, " < 0 || static_cast<uint32_t>(location.", suffix, ") >= theTexture.get_", dimensions[i], "()) return ", metalReturnName, "(0);\n"));
            }
            auto suffix = "xyzw"_str.substring(locationVectorLength - 1, 1);
            stringBuilder.append(makeString("    if (location.", suffix, " < 0 || static_cast<uint32_t>(location.", suffix, ") >= theTexture.get_array_size()) return ", metalReturnName, "(0);\n"));
        } else {
            if (locationVectorLength == 1)
                stringBuilder.append(makeString("    if (location < 0 || static_cast<uint32_t>(location) >= theTexture.get_width()) return ", metalReturnName, "(0);\n"));
            else {
                stringBuilder.append(makeString("    if (location.x < 0 || static_cast<uint32_t>(location.x) >= theTexture.get_width()) return ", metalReturnName, "(0);\n"));
                stringBuilder.append(makeString("    if (location.y < 0 || static_cast<uint32_t>(location.y) >= theTexture.get_height()) return ", metalReturnName, "(0);\n"));
                if (locationVectorLength >= 3)
                    stringBuilder.append(makeString("    if (location.z < 0 || static_cast<uint32_t>(location.z) >= theTexture.get_depth()) return ", metalReturnName, "(0);\n"));
            }
        }
        stringBuilder.append("    return theTexture.read(");
        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            stringBuilder.append(makeString("uint", vectorSuffix(locationVectorLength - 1), "(location.", "xyzw"_str.substring(0, locationVectorLength - 1), "), uint(location.", "xyzw"_str.substring(locationVectorLength - 1, 1), ")"));
        } else
            stringBuilder.append(makeString("uint", vectorSuffix(locationVectorLength), "(location)"));
        stringBuilder.append(")");
        if (!textureType.isDepthTexture())
            stringBuilder.append(makeString(".", "xyzw"_str.substring(0, returnVectorLength)));
        stringBuilder.append(";\n");
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "load") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto metalParameterName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, " x) {\n"));
        stringBuilder.append("    return atomic_load_explicit(x, memory_order_relaxed);\n");
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "store") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto metalParameter1Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        stringBuilder.append(makeString("void ", outputFunctionName, '(', metalParameter1Name, " x, ", metalParameter2Name, " y) {\n"));
        stringBuilder.append("    atomic_store_explicit(x, y, memory_order_relaxed);\n");
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "GetDimensions") {
        auto& textureType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[0]->type()->unifyNode()));

        size_t index = 1;
        if (!textureType.isWritableTexture() && textureType.textureDimension() != 1)
            ++index;
        auto widthTypeName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[index]->type());
        ++index;
        String heightTypeName;
        if (textureType.textureDimension() >= 2) {
            heightTypeName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[index]->type());
            ++index;
        }
        String depthTypeName;
        if (textureType.textureDimension() >= 3) {
            depthTypeName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[index]->type());
            ++index;
        }
        String elementsTypeName;
        if (textureType.isTextureArray()) {
            elementsTypeName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[index]->type());
            ++index;
        }
        String numberOfLevelsTypeName;
        if (!textureType.isWritableTexture() && textureType.textureDimension() != 1) {
            numberOfLevelsTypeName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[index]->type());
            ++index;
        }
        ASSERT(index == nativeFunctionDeclaration.parameters().size());

        auto metalParameter1Name = typeNamer.mangledNameForType(textureType);
        stringBuilder.append(makeString("void ", outputFunctionName, '(', metalParameter1Name, " theTexture"));
        if (!textureType.isWritableTexture() && textureType.textureDimension() != 1)
            stringBuilder.append(", uint mipLevel");
        stringBuilder.append(makeString(", ", widthTypeName, " width"));
        if (!heightTypeName.isNull())
            stringBuilder.append(makeString(", ", heightTypeName, " height"));
        if (!depthTypeName.isNull())
            stringBuilder.append(makeString(", ", depthTypeName, " depth"));
        if (!elementsTypeName.isNull())
            stringBuilder.append(makeString(", ", elementsTypeName, " elements"));
        if (!numberOfLevelsTypeName.isNull())
            stringBuilder.append(makeString(", ", numberOfLevelsTypeName, " numberOfLevels"));
        stringBuilder.append(") {\n");
        stringBuilder.append("    if (width)\n");
        stringBuilder.append("        *width = theTexture.get_width(");
        if (!textureType.isWritableTexture() && textureType.textureDimension() != 1)
            stringBuilder.append("mipLevel");
        stringBuilder.append(");\n");
        if (!heightTypeName.isNull()) {
            stringBuilder.append("    if (height)\n");
            stringBuilder.append("        *height = theTexture.get_height(");
            if (!textureType.isWritableTexture() && textureType.textureDimension() != 1)
                stringBuilder.append("mipLevel");
            stringBuilder.append(");\n");
        }
        if (!depthTypeName.isNull()) {
            stringBuilder.append("    if (depth)\n");
            stringBuilder.append("        *depth = theTexture.get_depth(");
            if (!textureType.isWritableTexture() && textureType.textureDimension() != 1)
                stringBuilder.append("mipLevel");
            stringBuilder.append(");\n");
        }
        if (!elementsTypeName.isNull()) {
            stringBuilder.append("    if (elements)\n");
            stringBuilder.append("        *elements = theTexture.get_array_size();\n");
        }
        if (!numberOfLevelsTypeName.isNull()) {
            stringBuilder.append("    if (numberOfLevels)\n");
            stringBuilder.append("        *numberOfLevels = theTexture.get_num_mip_levels();\n");
        }
        stringBuilder.append("}\n");
        return stringBuilder.toString();
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

        auto metalParameter1Name = typeNamer.mangledNameForType(textureType);
        auto metalParameter2Name = typeNamer.mangledNameForType(itemType);
        auto metalParameter3Name = typeNamer.mangledNameForType(locationType);
        auto metalInnerTypeName = typeNamer.mangledNameForType(itemVectorInnerType);
        stringBuilder.append(makeString("void ", outputFunctionName, '(', metalParameter1Name, " theTexture, ", metalParameter2Name, " item, ", metalParameter3Name, " location) {\n"));
        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            String dimensions[] = { "width"_str, "height"_str, "depth"_str };
            for (int i = 0; i < locationVectorLength - 1; ++i) {
                auto suffix = "xyzw"_str.substring(i, 1);
                stringBuilder.append(makeString("    if (location.", suffix, " >= theTexture.get_", dimensions[i], "()) return;\n"));
            }
            auto suffix = "xyzw"_str.substring(locationVectorLength - 1, 1);
            stringBuilder.append(makeString("    if (location.", suffix, " >= theTexture.get_array_size()) return;\n"));
        } else {
            if (locationVectorLength == 1)
                stringBuilder.append(makeString("    if (location >= theTexture.get_width()) return;\n"));
            else {
                stringBuilder.append(makeString("    if (location.x >= theTexture.get_width()) return;\n"));
                stringBuilder.append(makeString("    if (location.y >= theTexture.get_height()) return;\n"));
                if (locationVectorLength >= 3)
                    stringBuilder.append(makeString("    if (location.z >= theTexture.get_depth()) return;\n"));
            }
        }
        stringBuilder.append(makeString("    theTexture.write(vec<", metalInnerTypeName, ", 4>(item"));
        for (int i = 0; i < 4 - itemVectorLength; ++i)
            stringBuilder.append(", 0");
        stringBuilder.append("), ");
        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            stringBuilder.append(makeString("uint", vectorSuffix(locationVectorLength - 1), "(location.", "xyzw"_str.substring(0, locationVectorLength - 1), "), uint(location.", "xyzw"_str.substring(locationVectorLength - 1, 1), ")"));
        } else
            stringBuilder.append(makeString("uint", vectorSuffix(locationVectorLength), "(location)"));
        stringBuilder.append(");\n");
        stringBuilder.append("}\n");
        return stringBuilder.toString();
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
    return String();
}

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif
