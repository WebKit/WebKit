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

#include "WHLSLAddressSpace.h"
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

static String getNativeName(AST::UnnamedType& unnamedType, TypeNamer& typeNamer)
{
    ASSERT(is<AST::NamedType>(unnamedType.unifyNode()));
    auto& namedType = downcast<AST::NamedType>(unnamedType.unifyNode());
    ASSERT(is<AST::NativeTypeDeclaration>(namedType));
    auto& nativeTypeDeclaration = downcast<AST::NativeTypeDeclaration>(namedType);
    return typeNamer.mangledNameForType(nativeTypeDeclaration);
}

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

static String convertAddressSpace(AST::AddressSpace addressSpace)
{
    switch (addressSpace) {
    case AST::AddressSpace::Constant:
        return "constant"_str;
    case AST::AddressSpace::Device:
        return "device"_str;
    case AST::AddressSpace::Threadgroup:
        return "threadgroup"_str;
    default:
        ASSERT(addressSpace == AST::AddressSpace::Thread);
        return "thread"_str;
    }
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

String writeNativeFunction(AST::NativeFunctionDeclaration& nativeFunctionDeclaration, String& outputFunctionName, TypeNamer& typeNamer)
{
    StringBuilder stringBuilder;
    if (nativeFunctionDeclaration.isCast()) {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto metalParameterName = getNativeName(*nativeFunctionDeclaration.parameters()[0].type(), typeNamer);
        auto metalReturnName = getNativeName(nativeFunctionDeclaration.type(), typeNamer);
        if (metalParameterName != "atomic_int"_str && metalParameterName != "atomic_uint"_str) {
            stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, "x) {\n"));
            stringBuilder.append(makeString("    return static_cast<", metalReturnName, ">(x);\n"));
            stringBuilder.append("}\n");
            return stringBuilder.toString();
        }

        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, "x) {\n"));
        stringBuilder.append("    return atomic_load_explicit(&x, memory_order_relaxed);\n");
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name().startsWith("operator."_str)) {
        if (nativeFunctionDeclaration.name().endsWith("=")) {
            ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
            auto metalParameter1Name = getNativeName(*nativeFunctionDeclaration.parameters()[0].type(), typeNamer);
            auto metalParameter2Name = getNativeName(*nativeFunctionDeclaration.parameters()[1].type(), typeNamer);
            auto metalReturnName = getNativeName(nativeFunctionDeclaration.type(), typeNamer);
            auto fieldName = nativeFunctionDeclaration.name().substring("operator."_str.length());
            fieldName = fieldName.substring(0, fieldName.length() - 1);
            stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, "v, ", metalParameter2Name, " n) {\n"));
            stringBuilder.append(makeString("    v.", fieldName, " = n;\n"));
            stringBuilder.append(makeString("    return v;\n"));
            stringBuilder.append("}\n");
            return stringBuilder.toString();
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        auto metalParameterName = getNativeName(*nativeFunctionDeclaration.parameters()[0].type(), typeNamer);
        auto metalReturnName = getNativeName(nativeFunctionDeclaration.type(), typeNamer);
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, "v) {\n"));
        stringBuilder.append(makeString("    return v.", nativeFunctionDeclaration.name().substring("operator."_str.length()), ";\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();

    }

    if (nativeFunctionDeclaration.name() == "operator[]") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto metalParameter1Name = getNativeName(*nativeFunctionDeclaration.parameters()[0].type(), typeNamer);
        auto metalParameter2Name = getNativeName(*nativeFunctionDeclaration.parameters()[1].type(), typeNamer);
        auto metalReturnName = getNativeName(nativeFunctionDeclaration.type(), typeNamer);
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, "m, ", metalParameter2Name, " i) {\n"));
        stringBuilder.append(makeString("    return m[i];\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "operator[]=") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
        auto metalParameter1Name = getNativeName(*nativeFunctionDeclaration.parameters()[0].type(), typeNamer);
        auto metalParameter2Name = getNativeName(*nativeFunctionDeclaration.parameters()[1].type(), typeNamer);
        auto metalParameter3Name = getNativeName(*nativeFunctionDeclaration.parameters()[2].type(), typeNamer);
        auto metalReturnName = getNativeName(nativeFunctionDeclaration.type(), typeNamer);
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, "m, ", metalParameter2Name, " i, ", metalParameter3Name, " v) {\n"));
        stringBuilder.append(makeString("    m[i] = v;\n"));
        stringBuilder.append(makeString("    return m;\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.isOperator()) {
        if (nativeFunctionDeclaration.parameters().size() == 1) {
            auto operatorName = nativeFunctionDeclaration.name().substring("operator"_str.length());
            auto metalParameterName = getNativeName(*nativeFunctionDeclaration.parameters()[0].type(), typeNamer);
            auto metalReturnName = getNativeName(nativeFunctionDeclaration.type(), typeNamer);
            stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, "x) {\n"));
            stringBuilder.append(makeString("    return ", operatorName, "x;\n"));
            stringBuilder.append("}\n");
            return stringBuilder.toString();
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto operatorName = nativeFunctionDeclaration.name().substring("operator"_str.length());
        auto metalParameter1Name = getNativeName(*nativeFunctionDeclaration.parameters()[0].type(), typeNamer);
        auto metalParameter2Name = getNativeName(*nativeFunctionDeclaration.parameters()[1].type(), typeNamer);
        auto metalReturnName = getNativeName(nativeFunctionDeclaration.type(), typeNamer);
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, "x, ", metalParameter2Name, " y) {\n"));
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
        auto metalParameterName = getNativeName(*nativeFunctionDeclaration.parameters()[0].type(), typeNamer);
        auto metalReturnName = getNativeName(nativeFunctionDeclaration.type(), typeNamer);
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameterName, "x) {\n"));
        stringBuilder.append(makeString("    return ", mapFunctionName(nativeFunctionDeclaration.name()), "(x);\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "pow" || nativeFunctionDeclaration.name() == "atan2") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        auto metalParameter1Name = getNativeName(*nativeFunctionDeclaration.parameters()[0].type(), typeNamer);
        auto metalParameter2Name = getNativeName(*nativeFunctionDeclaration.parameters()[1].type(), typeNamer);
        auto metalReturnName = getNativeName(nativeFunctionDeclaration.type(), typeNamer);
        stringBuilder.append(makeString(metalReturnName, ' ', outputFunctionName, '(', metalParameter1Name, "x, ", metalParameter2Name, " y) {\n"));
        stringBuilder.append(makeString("    return ", nativeFunctionDeclaration.name(), "(x, y);\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "f16tof32" || nativeFunctionDeclaration.name() == "f32tof16") {
        // FIXME: Implement this
        CRASH();
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
            ASSERT(is<AST::PointerType>(*nativeFunctionDeclaration.parameters()[0].type()));
            auto& firstArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[0].type());
            auto firstArgumentAddressSpace = firstArgumentPointer.addressSpace();
            auto firstArgumentPointee = getNativeName(firstArgumentPointer.elementType(), typeNamer);
            auto secondArgument = getNativeName(*nativeFunctionDeclaration.parameters()[1].type(), typeNamer);
            auto thirdArgument = getNativeName(*nativeFunctionDeclaration.parameters()[2].type(), typeNamer);
            ASSERT(is<AST::PointerType>(*nativeFunctionDeclaration.parameters()[3].type()));
            auto& fourthArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[3].type());
            auto fourthArgumentAddressSpace = fourthArgumentPointer.addressSpace();
            auto fourthArgumentPointee = getNativeName(fourthArgumentPointer.elementType(), typeNamer);
            stringBuilder.append(makeString("void ", outputFunctionName, '(', convertAddressSpace(firstArgumentAddressSpace), ' ', firstArgumentPointee, "* object, ", secondArgument, " compare, ", thirdArgument, " desired, ", convertAddressSpace(fourthArgumentAddressSpace), ' ', fourthArgumentPointee, "* out) {\n"));
            stringBuilder.append("    atomic_compare_exchange_weak_explicit(object, &compare, desired, memory_order_relaxed);\n");
            stringBuilder.append("    *out = compare;\n");
            stringBuilder.append("}\n");
            return stringBuilder.toString();
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
        ASSERT(is<AST::PointerType>(*nativeFunctionDeclaration.parameters()[0].type()));
        auto& firstArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[0].type());
        auto firstArgumentAddressSpace = firstArgumentPointer.addressSpace();
        auto firstArgumentPointee = getNativeName(firstArgumentPointer.elementType(), typeNamer);
        auto secondArgument = getNativeName(*nativeFunctionDeclaration.parameters()[1].type(), typeNamer);
        ASSERT(is<AST::PointerType>(*nativeFunctionDeclaration.parameters()[2].type()));
        auto& thirdArgumentPointer = downcast<AST::PointerType>(*nativeFunctionDeclaration.parameters()[2].type());
        auto thirdArgumentAddressSpace = thirdArgumentPointer.addressSpace();
        auto thirdArgumentPointee = getNativeName(thirdArgumentPointer.elementType(), typeNamer);
        auto name = atomicName(nativeFunctionDeclaration.name().substring("Interlocked"_str.length()));
        stringBuilder.append(makeString("void ", outputFunctionName, '(', convertAddressSpace(firstArgumentAddressSpace), ' ', firstArgumentPointee, "* object, ", secondArgument, " operand, ", convertAddressSpace(thirdArgumentAddressSpace), ' ', thirdArgumentPointee, "* out) {\n"));
        stringBuilder.append(makeString("    *out = atomic_fetch_", name, "_explicit(object, operand, memory_order_relaxed);\n"));
        stringBuilder.append("}\n");
        return stringBuilder.toString();
    }

    if (nativeFunctionDeclaration.name() == "Sample") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "Load") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "GetDimensions") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "SampleBias") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "SampleGrad") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "SampleLevel") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "Gather") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "GatherRed") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "SampleCmp") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "SampleCmpLevelZero") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "Store") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "GatherAlpha") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "GatherBlue") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "GatherCmp") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "GatherCmpRed") {
        // FIXME: Implement this.
        CRASH();
    }

    if (nativeFunctionDeclaration.name() == "GatherGreen") {
        // FIXME: Implement this.
        CRASH();
    }

    // FIXME: Add all the functions that the compiler generated.

    ASSERT_NOT_REACHED();
    return String();
}

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif
