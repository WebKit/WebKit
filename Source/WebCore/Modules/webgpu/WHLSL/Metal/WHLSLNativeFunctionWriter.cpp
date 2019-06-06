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

String writeNativeFunction(AST::NativeFunctionDeclaration& nativeFunctionDeclaration, String& outputFunctionName, Intrinsics& intrinsics, TypeNamer& typeNamer)
{
    StringBuilder stringBuilder;
    if (nativeFunctionDeclaration.isCast()) {
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        if (!nativeFunctionDeclaration.parameters().size()) {
            stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, "() {\n"));
            stringBuilder.append(makeString("    ", metalReturnName, " x;\n"));
            // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195771 Zero-fill
            stringBuilder.append("    return x;\n");
            stringBuilder.append("}\n");
            return stringBuilder.toString();
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto metalParameterName = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto& parameterType = nativeFunctionDeclaration.parameters()[0]->type()->unifyNode();
        if (is<AST::NamedType>(parameterType)) {
            auto& parameterNamedType = downcast<AST::NamedType>(parameterType);
            if (is<AST::NativeTypeDeclaration>(parameterNamedType)) {
                auto& parameterNativeTypeDeclaration = downcast<AST::NativeTypeDeclaration>(parameterNamedType);
                if (parameterNativeTypeDeclaration.isAtomic()) {
                    stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, " x) {\n"));
                    stringBuilder.append("    return atomic_load_explicit(&x, memory_order_relaxed);\n");
                    stringBuilder.append("}\n");
                    return stringBuilder.toString();
                }
            }
        }

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
        ASSERT(is<AST::UnnamedType>(parameterType));
        auto& unnamedParameterType = downcast<AST::UnnamedType>(parameterType);
        if (is<AST::ArrayType>(unnamedParameterType)) {
            auto& arrayParameterType = downcast<AST::ArrayType>(unnamedParameterType);
            stringBuilder.append(makeString("uint ", outputFunctionName, '(', metalParameterName, " v) {\n"));
            stringBuilder.append(makeString("    return ", arrayParameterType.numElements(), "u;\n"));
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

    if (nativeFunctionDeclaration.name() == "operator[]") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto metalParameter1Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, " m, ", metalParameter2Name, " i) {\n"));
        stringBuilder.append(makeString("    return m[i];\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "operator&[]") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto metalParameter1Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[0]->type());
        auto metalParameter2Name = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        auto metalReturnName = typeNamer.mangledNameForType(nativeFunctionDeclaration.type());
        auto fieldName = nativeFunctionDeclaration.name().substring("operator&[]."_str.length());
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, " v, ", metalParameter2Name, " n) {\n"));
        stringBuilder.append(makeString("    return &(v.pointer[n]);\n"));
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
        stringBuilder.append(makeString("    m[i] = v;\n"));
        stringBuilder.append(makeString("    return m;\n"));
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

    if (nativeFunctionDeclaration.name() == "f16tof32" || nativeFunctionDeclaration.name() == "f32tof16") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
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
            ASSERT(is<AST::PointerType>(*nativeFunctionDeclaration.parameters()[0]->type()));
            auto& firstArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[0]->type());
            auto firstArgumentAddressSpace = firstArgumentPointer.addressSpace();
            auto firstArgumentPointee = typeNamer.mangledNameForType(firstArgumentPointer.elementType());
            auto secondArgument = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
            auto thirdArgument = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[2]->type());
            ASSERT(is<AST::PointerType>(*nativeFunctionDeclaration.parameters()[3]->type()));
            auto& fourthArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[3]->type());
            auto fourthArgumentAddressSpace = fourthArgumentPointer.addressSpace();
            auto fourthArgumentPointee = typeNamer.mangledNameForType(fourthArgumentPointer.elementType());
            stringBuilder.append(makeString("void ", outputFunctionName, '(', toString(firstArgumentAddressSpace), ' ', firstArgumentPointee, "* object, ", secondArgument, " compare, ", thirdArgument, " desired, ", toString(fourthArgumentAddressSpace), ' ', fourthArgumentPointee, "* out) {\n"));
            stringBuilder.append("    atomic_compare_exchange_weak_explicit(object, &compare, desired, memory_order_relaxed);\n");
            stringBuilder.append("    *out = compare;\n");
            stringBuilder.append("}\n");
            return stringBuilder.toString();
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
        ASSERT(is<AST::PointerType>(*nativeFunctionDeclaration.parameters()[0]->type()));
        auto& firstArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[0]->type());
        auto firstArgumentAddressSpace = firstArgumentPointer.addressSpace();
        auto firstArgumentPointee = typeNamer.mangledNameForType(firstArgumentPointer.elementType());
        auto secondArgument = typeNamer.mangledNameForType(*nativeFunctionDeclaration.parameters()[1]->type());
        ASSERT(is<AST::PointerType>(*nativeFunctionDeclaration.parameters()[2]->type()));
        auto& thirdArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[2]->type());
        auto thirdArgumentAddressSpace = thirdArgumentPointer.addressSpace();
        auto thirdArgumentPointee = typeNamer.mangledNameForType(thirdArgumentPointer.elementType());
        auto name = atomicName(nativeFunctionDeclaration.name().substring("Interlocked"_str.length()));
        stringBuilder.append(makeString("void ", outputFunctionName, '(', toString(firstArgumentAddressSpace), ' ', firstArgumentPointee, "* object, ", secondArgument, " operand, ", toString(thirdArgumentAddressSpace), ' ', thirdArgumentPointee, "* out) {\n"));
        stringBuilder.append(makeString("    *out = atomic_fetch_", name, "_explicit(object, operand, memory_order_relaxed);\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "Sample") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "Load") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
    }

    if (nativeFunctionDeclaration.name() == "GetDimensions") {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
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
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195813 Implement this
        notImplemented();
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
