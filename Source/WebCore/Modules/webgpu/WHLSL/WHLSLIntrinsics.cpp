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
#include "WHLSLIntrinsics.h"

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLConstantExpression.h"
#include "WHLSLTypeArgument.h"
#include "WHLSLTypeReference.h"
#include <algorithm>
#include <cstring>

namespace WebCore {

namespace WHLSL {

constexpr const char* Intrinsics::m_textureTypeNames[];
constexpr const char* Intrinsics::m_textureInnerTypeNames[];
constexpr const char* Intrinsics::m_depthTextureInnerTypes[];

Intrinsics::Intrinsics()
{
}

void Intrinsics::add(AST::NativeFunctionDeclaration& nativeFunctionDeclaration)
{
    if (nativeFunctionDeclaration.name() == "ddx")
        m_ddx = &nativeFunctionDeclaration;
    else if (nativeFunctionDeclaration.name() == "ddy")
        m_ddy = &nativeFunctionDeclaration;
    else if (nativeFunctionDeclaration.name() == "AllMemoryBarrierWithGroupSync")
        m_allMemoryBarrier = &nativeFunctionDeclaration;
    else if (nativeFunctionDeclaration.name() == "DeviceMemoryBarrierWithGroupSync")
        m_deviceMemoryBarrier = &nativeFunctionDeclaration;
    else if (nativeFunctionDeclaration.name() == "GroupMemoryBarrierWithGroupSync")
        m_groupMemoryBarrier = &nativeFunctionDeclaration;
}

bool Intrinsics::addPrimitive(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    if (nativeTypeDeclaration.typeArguments().size())
        return false;

    if (nativeTypeDeclaration.name() == "void")
        m_voidType = &nativeTypeDeclaration;
    else if (nativeTypeDeclaration.name() == "bool")
        m_boolType = &nativeTypeDeclaration;
    else if (nativeTypeDeclaration.name() == "uint") {
        nativeTypeDeclaration.setIsInt();
        nativeTypeDeclaration.setIsNumber();
        nativeTypeDeclaration.setCanRepresentInteger([](int x) {
            return x >= 0;
        });
        nativeTypeDeclaration.setCanRepresentUnsignedInteger([](unsigned) {
            return true;
        });
        nativeTypeDeclaration.setCanRepresentFloat([](float x) {
            return static_cast<float>(static_cast<uint32_t>(x)) == x;
        });
        nativeTypeDeclaration.setSuccessor([](int64_t x) -> int64_t {
            return static_cast<uint32_t>(x + 1);
        });
        nativeTypeDeclaration.setFormatValueFromInteger([](int x) -> int64_t {
            return static_cast<uint32_t>(x);
        });
        nativeTypeDeclaration.setFormatValueFromUnsignedInteger([](unsigned x) -> int64_t {
            return static_cast<uint32_t>(x);
        });
        nativeTypeDeclaration.setIterateAllValues([](const std::function<bool(int64_t)>& callback) {
            for (int64_t i = 0; i < 0x100000000; ++i) {
                if (callback(i))
                    break;
            }
        });
        m_uintType = &nativeTypeDeclaration;
    } else if (nativeTypeDeclaration.name() == "int") {
        nativeTypeDeclaration.setIsInt();
        nativeTypeDeclaration.setIsNumber();
        nativeTypeDeclaration.setIsSigned();
        nativeTypeDeclaration.setCanRepresentInteger([](int) {
            return true;
        });
        nativeTypeDeclaration.setCanRepresentUnsignedInteger([](unsigned x) {
            return x <= 2147483647;
        });
        nativeTypeDeclaration.setCanRepresentFloat([](float x) {
            return static_cast<float>(static_cast<int32_t>(x)) == x;
        });
        nativeTypeDeclaration.setSuccessor([](int64_t x) -> int64_t {
            return static_cast<int32_t>(x + 1);
        });
        nativeTypeDeclaration.setFormatValueFromInteger([](int x) -> int64_t {
            return static_cast<int32_t>(x);
        });
        nativeTypeDeclaration.setFormatValueFromUnsignedInteger([](unsigned x) -> int64_t {
            return static_cast<int32_t>(x);
        });
        nativeTypeDeclaration.setIterateAllValues([](const std::function<bool(int64_t)>& callback) {
            for (int64_t i = -2147483647; i < 2147483648; ++i) {
                if (callback(i))
                    break;
            }
        });
        m_intType = &nativeTypeDeclaration;
    } else if (nativeTypeDeclaration.name() == "float") {
        nativeTypeDeclaration.setIsNumber();
        nativeTypeDeclaration.setIsFloating();
        nativeTypeDeclaration.setCanRepresentInteger([](int) {
            return true;
        });
        nativeTypeDeclaration.setCanRepresentUnsignedInteger([](unsigned) {
            return true;
        });
        nativeTypeDeclaration.setCanRepresentFloat([](float) {
            return true;
        });
        m_floatType = &nativeTypeDeclaration;
    } else if (nativeTypeDeclaration.name() == "atomic_int") {
        nativeTypeDeclaration.setIsAtomic();
        m_atomicIntType = &nativeTypeDeclaration;
    } else if (nativeTypeDeclaration.name() == "atomic_uint") {
        nativeTypeDeclaration.setIsAtomic();
        m_atomicUintType = &nativeTypeDeclaration;
    } else if (nativeTypeDeclaration.name() == "sampler") {
        m_samplerType = &nativeTypeDeclaration;
        nativeTypeDeclaration.setIsOpaqueType();
    } else
        ASSERT_NOT_REACHED();
    return true;
}

bool Intrinsics::addVector(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    if (nativeTypeDeclaration.name() != "vector")
        return false;

    ASSERT(nativeTypeDeclaration.typeArguments().size() == 2);
    ASSERT(WTF::holds_alternative<Ref<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]));
    ASSERT(WTF::holds_alternative<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[1]));
    auto& innerType = static_cast<AST::TypeReference&>(WTF::get<Ref<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]));
    auto& lengthExpression = WTF::get<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[1]);
    ASSERT(!innerType.typeArguments().size());
    AST::NativeTypeDeclaration** array;
    if (innerType.name() == "bool")
        array = m_vectorBool;
    else if (innerType.name() == "uint")
        array = m_vectorUint;
    else if (innerType.name() == "int")
        array = m_vectorInt;
    else {
        ASSERT(innerType.name() == "float");
        array = m_vectorFloat;
    }
    int length = lengthExpression.integerLiteral().value();
    ASSERT(length >= 2 && length <= 4);
    nativeTypeDeclaration.setIsVector();
    array[length - 2] = &nativeTypeDeclaration;
    return true;
}

bool Intrinsics::addMatrix(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    if (nativeTypeDeclaration.name() != "matrix")
        return false;

    nativeTypeDeclaration.setIsMatrix();

    ASSERT(nativeTypeDeclaration.typeArguments().size() == 3);
    ASSERT(WTF::holds_alternative<Ref<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]));
    ASSERT(WTF::holds_alternative<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[1]));
    ASSERT(WTF::holds_alternative<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[2]));
    return true;
}

bool Intrinsics::addFullTexture(AST::NativeTypeDeclaration& nativeTypeDeclaration, AST::TypeReference& innerType)
{
    auto textureTypeIndex = std::find(m_textureTypeNames, m_textureTypeNames + WTF_ARRAY_LENGTH(m_textureTypeNames), nativeTypeDeclaration.name()) - m_textureTypeNames;
    if (textureTypeIndex == WTF_ARRAY_LENGTH(m_textureTypeNames))
        return false;

    unsigned innerTypeIndex = WTF_ARRAY_LENGTH(m_textureInnerTypeNames);
    unsigned vectorLength = 0;
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(m_textureInnerTypeNames); ++i) {
        if (innerType.name().startsWith(m_textureInnerTypeNames[i])) {
            innerTypeIndex = i;
            if (innerType.name() == m_textureInnerTypeNames[i])
                vectorLength = 1;
            else {
                ASSERT_WITH_SECURITY_IMPLICATION(innerType.name().length() == strlen(m_textureInnerTypeNames[i]) + 1);
                ASSERT_WITH_SECURITY_IMPLICATION(innerType.name()[innerType.name().length() - 1] == '2'
                    || innerType.name()[innerType.name().length() - 1] == '3'
                    || innerType.name()[innerType.name().length() - 1] == '4');
                vectorLength = innerType.name()[innerType.name().length() - 1] - '0';
            }
        }
    }
    RELEASE_ASSERT(vectorLength && vectorLength < 5);
    ASSERT_WITH_SECURITY_IMPLICATION(innerTypeIndex != WTF_ARRAY_LENGTH(m_textureInnerTypeNames));
    nativeTypeDeclaration.setIsTexture();
    nativeTypeDeclaration.setIsOpaqueType();
    if (nativeTypeDeclaration.name() == "Texture1DArray" || nativeTypeDeclaration.name() == "RWTexture1DArray" || nativeTypeDeclaration.name() == "Texture2DArray" || nativeTypeDeclaration.name() == "RWTexture2DArray")
        nativeTypeDeclaration.setIsTextureArray();
    if (nativeTypeDeclaration.name() == "RWTexture1D" || nativeTypeDeclaration.name() == "RWTexture2D" || nativeTypeDeclaration.name() == "RWTexture3D" || nativeTypeDeclaration.name() == "RWTexture1DArray" || nativeTypeDeclaration.name() == "RWTexture2DArray")
        nativeTypeDeclaration.setIsWritableTexture();
    if (nativeTypeDeclaration.name() == "Texture1D" || nativeTypeDeclaration.name() == "RWTexture1D" || nativeTypeDeclaration.name() == "Texture1DArray" || nativeTypeDeclaration.name() == "RWTexture1DArray")
        nativeTypeDeclaration.setTextureDimension(1);
    if (nativeTypeDeclaration.name() == "Texture2D" || nativeTypeDeclaration.name() == "RWTexture2D" || nativeTypeDeclaration.name() == "TextureCube" || nativeTypeDeclaration.name() == "Texture2DArray" || nativeTypeDeclaration.name() == "RWTexture2DArray")
        nativeTypeDeclaration.setTextureDimension(2);
    if (nativeTypeDeclaration.name() == "Texture3D" || nativeTypeDeclaration.name() == "RWTexture3D")
        nativeTypeDeclaration.setTextureDimension(3);
    if (nativeTypeDeclaration.name() == "TextureCube")
        nativeTypeDeclaration.setIsCubeTexture();
    m_fullTextures[textureTypeIndex][innerTypeIndex][vectorLength - 1] = &nativeTypeDeclaration;
    return true;
}

void Intrinsics::addDepthTexture(AST::NativeTypeDeclaration& nativeTypeDeclaration, AST::TypeReference& innerType)
{
    AST::NativeTypeDeclaration** texture = nullptr;
    if (nativeTypeDeclaration.name() == "TextureDepth2D")
        texture = m_textureDepth2D;
    else if (nativeTypeDeclaration.name() == "TextureDepth2DArray")
        texture = m_textureDepth2DArray;
    else {
        ASSERT(nativeTypeDeclaration.name() == "TextureDepthCube");
        texture = m_textureDepthCube;
    }
    auto innerTypeIndex = std::find(m_depthTextureInnerTypes, m_depthTextureInnerTypes + WTF_ARRAY_LENGTH(m_depthTextureInnerTypes), innerType.name()) - m_depthTextureInnerTypes;
    ASSERT(innerTypeIndex != WTF_ARRAY_LENGTH(m_depthTextureInnerTypes));
    nativeTypeDeclaration.setIsTexture();
    nativeTypeDeclaration.setIsOpaqueType();
    if (texture == m_textureDepth2DArray)
        nativeTypeDeclaration.setIsTextureArray();
    if (texture == m_textureDepthCube)
        nativeTypeDeclaration.setIsCubeTexture();
    nativeTypeDeclaration.setTextureDimension(2);
    nativeTypeDeclaration.setIsDepthTexture();
    texture[innerTypeIndex] = &nativeTypeDeclaration;
}

void Intrinsics::addTexture(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    ASSERT(nativeTypeDeclaration.typeArguments().size() == 1);
    ASSERT(WTF::holds_alternative<Ref<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]));
    auto& innerType = static_cast<AST::TypeReference&>(WTF::get<Ref<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]));
    ASSERT(!innerType.typeArguments().size());
    if (addFullTexture(nativeTypeDeclaration, innerType)) {
        m_textureSet.add(&nativeTypeDeclaration);
        return;
    }
    addDepthTexture(nativeTypeDeclaration, innerType);
    m_textureSet.add(&nativeTypeDeclaration);
}

void Intrinsics::add(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    if (addPrimitive(nativeTypeDeclaration))
        return;
    if (addVector(nativeTypeDeclaration))
        return;
    if (addMatrix(nativeTypeDeclaration))
        return;
    addTexture(nativeTypeDeclaration);
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WHLSL_COMPILER)
