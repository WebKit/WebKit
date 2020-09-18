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

#if ENABLE(WHLSL_COMPILER)

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

enum class SampleType {
    Sample,
    SampleLevel,
    SampleBias,
    SampleGrad
};

static Optional<SampleType> sampleType(const String& functionName)
{
    if (functionName == "Sample")
        return SampleType::Sample;
    if (functionName == "SampleLevel")
        return SampleType::SampleLevel;
    if (functionName == "SampleBias")
        return SampleType::SampleBias;
    if (functionName == "SampleGrad")
        return SampleType::SampleGrad;
    return WTF::nullopt;
}

void inlineNativeFunction(StringBuilder& stringBuilder, AST::NativeFunctionDeclaration& nativeFunctionDeclaration, const Vector<MangledVariableName>& args, MangledVariableName resultName, TypeNamer& typeNamer)
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
            stringBuilder.append(metalReturnTypeName, " { }");
            return;
        }

        if (nativeFunctionDeclaration.parameters().size() == 1) {
            auto& parameterType = *nativeFunctionDeclaration.parameters()[0]->type();

            auto isEnumerationDefinition = [] (auto& type) {
                return is<AST::NamedType>(type) && is<AST::EnumerationDefinition>(downcast<AST::NamedType>(type));
            };
            auto& unifiedReturnType = returnType.unifyNode();
            if (isEnumerationDefinition(unifiedReturnType) && !isEnumerationDefinition(parameterType.unifyNode())) {
                auto& enumerationDefinition = downcast<AST::EnumerationDefinition>(downcast<AST::NamedType>(unifiedReturnType));
                stringBuilder.append("static_cast<", metalReturnTypeName, ">((");
                bool loopedOnce = false;
                bool hasZeroCase = false;
                for (auto& member : enumerationDefinition.enumerationMembers()) {
                    if (loopedOnce)
                        stringBuilder.append(" || ");
                    hasZeroCase |= !member.get().value();
                    stringBuilder.append(args[0], " == ", member.get().value());
                    loopedOnce = true;
                }
                ASSERT_UNUSED(hasZeroCase, hasZeroCase);

                stringBuilder.append(") ? ", args[0], " : 0)");
            } else
                stringBuilder.append("static_cast<", metalReturnTypeName, ">(", args[0], ")");

            return;
        }

        if (auto* matrixType = asMatrixType(returnType)) {
            // We're either constructing with all individual elements, or with
            // vectors for each column.

            stringBuilder.append('(');
            if (args.size() == matrixType->numberOfMatrixColumns()) {
                // Constructing with vectors for each column.
                for (size_t i = 0; i < args.size(); ++i) {
                    if (i)
                        stringBuilder.append(", ");
                    stringBuilder.append(resultName, ".columns[", i, "] = ", args[i]);
                }
            } else {
                // Constructing with all elements.
                RELEASE_ASSERT(args.size() == matrixType->numberOfMatrixColumns() * matrixType->numberOfMatrixRows());

                size_t argNumber = 0;
                for (size_t i = 0; i < matrixType->numberOfMatrixColumns(); ++i) {
                    for (size_t j = 0; j < matrixType->numberOfMatrixRows(); ++j) {
                        if (argNumber)
                            stringBuilder.append(", ");
                        stringBuilder.append(resultName, ".columns[", i, "][", j, "] = ", args[argNumber]);
                        ++argNumber;
                    }
                }
            }

            stringBuilder.append(", ", resultName, ')');
            return;
        }

        stringBuilder.append(metalReturnTypeName, '(');
        for (unsigned i = 0; i < nativeFunctionDeclaration.parameters().size(); ++i) {
            if (i)
                stringBuilder.append(", ");
            stringBuilder.append(args[i]);
        }
        stringBuilder.append(')');
        return;
    }

    if (nativeFunctionDeclaration.isOperator()) {
        auto operatorName = nativeFunctionDeclaration.name().substring("operator"_str.length());

        if (nativeFunctionDeclaration.parameters().size() == 1) {
            // This is ok to do since the args to this function are all temps.
            // So things like ++ and -- are ok to do.

            if (auto* matrixType = asMatrixType(nativeFunctionDeclaration.type())) {
                stringBuilder.append('(');
                for (unsigned i = 0; i < matrixType->numberOfMatrixColumns(); ++i) {
                    if (i)
                        stringBuilder.append(", ");
                    stringBuilder.append(resultName, '[', i, "] = ", operatorName, args[0], '[', i, ']');
                }
                stringBuilder.append(", ", resultName, ')');
            } else
                stringBuilder.append(operatorName, args[0]);
            return;
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        if (auto* leftMatrix = asMatrixType(*nativeFunctionDeclaration.parameters()[0]->type())) {
            if (auto* rightMatrix = asMatrixType(*nativeFunctionDeclaration.parameters()[1]->type())) {
                // matrix <op> matrix
                stringBuilder.append('(');
                for (unsigned i = 0; i < leftMatrix->numberOfMatrixColumns(); ++i) {
                    if (i)
                        stringBuilder.append(", ");
                    stringBuilder.append(resultName, '[', i, "] = ", args[0], '[', i, "] ", operatorName, ' ', args[1], '[', i, ']');
                }
                stringBuilder.append(", ", resultName, ')');
            } else {
                // matrix <op> scalar
                stringBuilder.append('(');
                for (unsigned i = 0; i < leftMatrix->numberOfMatrixColumns(); ++i) {
                    if (i)
                        stringBuilder.append(", ");
                    stringBuilder.append(resultName, '[', i, "] = ", args[0], '[', i, "] ", operatorName, ' ', args[1]);
                }
                stringBuilder.append(", ", resultName, ')');
            }
        } else if (auto* rightMatrix = asMatrixType(*nativeFunctionDeclaration.parameters()[1]->type())) {
            ASSERT(!asMatrixType(*nativeFunctionDeclaration.parameters()[0]->type()));
            // scalar <op> matrix
            stringBuilder.append('(');
            for (unsigned i = 0; i < rightMatrix->numberOfMatrixColumns(); ++i) {
                if (i)
                    stringBuilder.append(", ");
                stringBuilder.append(resultName, '[', i, "] = ", args[0], ' ', operatorName, ' ', args[1], '[', i, ']');
            }
            stringBuilder.append(", ", resultName, ')');
        } else {
            // scalar <op> scalar
            // vector <op> vector
            // vector <op> scalar
            // scalar <op> vector
            stringBuilder.append(
                '(', args[0], ' ', operatorName, ' ', args[1], ')');
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
            mapFunctionName(nativeFunctionDeclaration.name()), '(', args[0], ')');
        return;
    }

    if (nativeFunctionDeclaration.name() == "pow" || nativeFunctionDeclaration.name() == "atan2") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        stringBuilder.append(
            nativeFunctionDeclaration.name(), '(', args[0], ", ", args[1], ')');
        return;
    }

    if (nativeFunctionDeclaration.name() == "clamp") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);

        if (auto* matrixType = asMatrixType(nativeFunctionDeclaration.type())) {
            stringBuilder.append('(');
            bool ranOnce = false;
            for (unsigned i = 0; i < matrixType->numberOfMatrixColumns(); ++i) {
                for (unsigned j = 0; j < matrixType->numberOfMatrixRows(); ++j) {
                    if (ranOnce)
                        stringBuilder.append(", ");
                    ranOnce = true;
                    stringBuilder.append(
                        resultName, '[', i, "][", j, "] = clamp(", args[0], '[', i, "][", j, "], ", args[1], '[', i, "][", j, "], ", args[2], '[', i, "][", j, "])");
                }
            }
            stringBuilder.append(", ", resultName, ")");
        } else {
            stringBuilder.append(
                "clamp(", args[0], ", ", args[1], ", ", args[2], ')');
        }
        return;
    }

    if (nativeFunctionDeclaration.name() == "AllMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(
            "(threadgroup_barrier(mem_flags::mem_device), threadgroup_barrier(mem_flags::mem_threadgroup), threadgroup_barrier(mem_flags::mem_texture))");
        return;
    }

    if (nativeFunctionDeclaration.name() == "DeviceMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(
            "threadgroup_barrier(mem_flags::mem_device)");
        return;
    }

    if (nativeFunctionDeclaration.name() == "GroupMemoryBarrierWithGroupSync") {
        ASSERT(!nativeFunctionDeclaration.parameters().size());
        stringBuilder.append(
            "threadgroup_barrier(mem_flags::mem_threadgroup)");
        return;
    }

    if (nativeFunctionDeclaration.name().startsWith("Interlocked"_str)) {
        if (nativeFunctionDeclaration.name() == "InterlockedCompareExchange") {
            ASSERT(nativeFunctionDeclaration.parameters().size() == 4);
            stringBuilder.append(
                "(atomic_compare_exchange_weak_explicit(", args[0], ", &", args[1], ", ", args[2], ", memory_order_relaxed, memory_order_relaxed), ",
                '*', args[3], " = ", args[1], ')');
            return;
        }

        ASSERT(nativeFunctionDeclaration.parameters().size() == 3);
        auto name = atomicName(nativeFunctionDeclaration.name().substring("Interlocked"_str.length()));
        stringBuilder.append(
            '*', args[2], " = atomic_", name, "_explicit(", args[0], ", ", args[1], ", memory_order_relaxed)");
        return;
    }

    if (auto sampleType = WHLSL::Metal::sampleType(nativeFunctionDeclaration.name())) {
        size_t baseArgumentCount = 0;
        switch (*sampleType) {
        case SampleType::Sample:
            baseArgumentCount = 3;
            break;
        case SampleType::SampleLevel:
        case SampleType::SampleBias:
            baseArgumentCount = 4;
            break;
        case SampleType::SampleGrad:
            baseArgumentCount = 5;
            break;
        }
        ASSERT(nativeFunctionDeclaration.parameters().size() == baseArgumentCount || nativeFunctionDeclaration.parameters().size() == baseArgumentCount + 1);
        
        auto& textureType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[0]->type()->unifyNode()));
        auto& locationType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[2]->type()->unifyNode()));
        auto locationVectorLength = vectorLength(locationType);
        auto& returnType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.type().unifyNode()));
        auto returnVectorLength = vectorLength(returnType);

        int argumentIndex = 0;
        stringBuilder.append(args[argumentIndex], ".sample(", args[argumentIndex + 1], ", ");
        argumentIndex += 2;

        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            stringBuilder.append(args[argumentIndex], '.', "xyzw"_str.substring(0, locationVectorLength - 1), ", ", args[argumentIndex], '.', "xyzw"_str.substring(locationVectorLength - 1, 1));
            ++argumentIndex;
        } else
            stringBuilder.append(args[argumentIndex++]);

        switch (*sampleType) {
        case SampleType::Sample:
            break;
        case SampleType::SampleLevel:
            stringBuilder.append(", level(", args[argumentIndex++], ")");
            break;
        case SampleType::SampleBias:
            stringBuilder.append(", bias(", args[argumentIndex++], ")");
            break;
        case SampleType::SampleGrad:
            if (textureType.isCubeTexture())
                stringBuilder.append(", gradientcube(", args[argumentIndex], ", ", args[argumentIndex + 1], ")");
            else
                stringBuilder.append(", gradient2d(", args[argumentIndex], ", ", args[argumentIndex + 1], ")");
            argumentIndex += 2;
            break;
        }

        if (nativeFunctionDeclaration.parameters().size() == baseArgumentCount + 1)
            stringBuilder.append(", ", args[argumentIndex++]);
        stringBuilder.append(")");
        if (!textureType.isDepthTexture())
            stringBuilder.append(".", "xyzw"_str.substring(0, returnVectorLength));

        return;
    }

    if (nativeFunctionDeclaration.name() == "Load") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);

        auto& textureType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[0]->type()->unifyNode()));
        auto& locationType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.parameters()[1]->type()->unifyNode()));
        auto locationVectorLength = vectorLength(locationType);
        auto& returnType = downcast<AST::NativeTypeDeclaration>(downcast<AST::NamedType>(nativeFunctionDeclaration.type().unifyNode()));
        auto returnVectorLength = vectorLength(returnType);
        auto locationTypeName = typeNamer.mangledNameForType(locationType);

        stringBuilder.append('(', args[1], " = ");
        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            stringBuilder.append("clamp(", args[1], ", ", locationTypeName, '(');
            for (int i = 0; i < locationVectorLength; ++i) {
                if (i)
                    stringBuilder.append(", ");
                stringBuilder.append('0');
            }
            stringBuilder.append("), ", locationTypeName, '(');

            String dimensions[] = { "width"_str, "height"_str, "depth"_str };
            for (int i = 0; i < locationVectorLength - 1; ++i) {
                if (i)
                    stringBuilder.append(", ");
                stringBuilder.append(
                    args[0], ".get_", dimensions[i], "() - 1");
            }
            stringBuilder.append(
                args[0], ".get_array_size() - 1))");
        } else {
            if (locationVectorLength == 1)
                stringBuilder.append("clamp(", args[1], ", 0, ", args[0], ".get_width() - 1)");
            else {
                stringBuilder.append("clamp(", args[1], ", ", locationTypeName, "(0, 0");
                if (locationVectorLength >= 3)
                    stringBuilder.append(", 0");
                stringBuilder.append("), ", locationTypeName, '(', args[0], ".get_width() - 1, ", args[0], ".get_height() - 1");
                if (locationVectorLength >= 3)
                    stringBuilder.append(", ", args[0], ".get_depth() - 1");
                stringBuilder.append("))");
            }
        }

        stringBuilder.append(", ", args[0], ".read(");

        if (textureType.isTextureArray()) {
            ASSERT(locationVectorLength > 1);
            stringBuilder.append("uint", vectorSuffix(locationVectorLength - 1), '(', args[1], '.', "xyzw"_str.substring(0, locationVectorLength - 1), "), uint(", args[1], '.', "xyzw"_str.substring(locationVectorLength - 1, 1), ')');
        } else
            stringBuilder.append("uint", vectorSuffix(locationVectorLength), '(', args[1], ')');
        stringBuilder.append(')');
        if (!textureType.isDepthTexture())
            stringBuilder.append('.', "xyzw"_str.substring(0, returnVectorLength));

        stringBuilder.append(')');

        return;
    }

    if (nativeFunctionDeclaration.name() == "load") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 1);
        stringBuilder.append(
            "atomic_load_explicit(", args[0], ", memory_order_relaxed)");
        return;
    }

    if (nativeFunctionDeclaration.name() == "store") {
        ASSERT(nativeFunctionDeclaration.parameters().size() == 2);
        stringBuilder.append(
            "atomic_store_explicit(", args[0], ", ", args[1], ", memory_order_relaxed)");
        return;
    }

    if (nativeFunctionDeclaration.name() == "GetDimensions") {
        stringBuilder.append('(');

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
            '*', widthName, " = ", args[0], ".get_width(");
        if (hasMipLevel)
            stringBuilder.append(args[1]);
        stringBuilder.append(')');

        if (heightName) {
            stringBuilder.append(
                ", *", *heightName, " = ", args[0], ".get_height(");
            if (hasMipLevel)
                stringBuilder.append(args[1]);
            stringBuilder.append(')');
        }
        if (depthName) {
            stringBuilder.append(
                ", *", *depthName, " = ", args[0], ".get_depth(");
            if (hasMipLevel)
                stringBuilder.append(args[1]);
            stringBuilder.append(')');
        }
        if (elementsName) {
            stringBuilder.append(
                ", *", *elementsName, " = ", args[0], ".get_array_size()");
        }
        if (numberOfLevelsName) {
            stringBuilder.append(
                ", *", *numberOfLevelsName, " = ", args[0], ".get_num_mip_levels()");
        }

        stringBuilder.append(')');
        return;
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
}

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WHLSL_COMPILER)
