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
#include "WHLSLNativeTypeWriter.h"

#if ENABLE(WEBGPU)

#include "WHLSLNamedType.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLTypeReference.h"

namespace WebCore {

namespace WHLSL {

namespace Metal {

String writeNativeType(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    if (nativeTypeDeclaration.name() == "void")
        return "void"_str;
    if (nativeTypeDeclaration.name() == "bool")
        return "bool"_str;
    if (nativeTypeDeclaration.name() == "uchar")
        return "uint8_t"_str;
    if (nativeTypeDeclaration.name() == "ushort")
        return "uint16_t"_str;
    if (nativeTypeDeclaration.name() == "uint")
        return "uint32_t"_str;
    if (nativeTypeDeclaration.name() == "char")
        return "int8_t"_str;
    if (nativeTypeDeclaration.name() == "short")
        return "int16_t"_str;
    if (nativeTypeDeclaration.name() == "int")
        return "int32_t"_str;
    if (nativeTypeDeclaration.name() == "half")
        return "half"_str;
    if (nativeTypeDeclaration.name() == "float")
        return "float"_str;
    if (nativeTypeDeclaration.name() == "atomic_int")
        return "atomic_int"_str;
    if (nativeTypeDeclaration.name() == "atomic_uint")
        return "atomic_uint"_str;
    if (nativeTypeDeclaration.name() == "sampler")
        return "sampler"_str;
    if (nativeTypeDeclaration.name() == "vector") {
        ASSERT(nativeTypeDeclaration.typeArguments().size() == 2);
        ASSERT(WTF::holds_alternative<UniqueRef<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]));
        auto& typeReference = WTF::get<UniqueRef<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]);
        auto& unifyNode = typeReference->unifyNode();
        ASSERT(is<AST::NamedType>(unifyNode));
        auto& namedType = downcast<AST::NamedType>(unifyNode);
        ASSERT(is<AST::NativeTypeDeclaration>(namedType));
        auto& parameterType = downcast<AST::NativeTypeDeclaration>(namedType);
        auto prefix = ([&]() -> String {
            if (parameterType.name() == "bool")
                return "bool";
            if (parameterType.name() == "uchar")
                return "uchar";
            if (parameterType.name() == "ushort")
                return "ushort";
            if (parameterType.name() == "uint")
                return "uint";
            if (parameterType.name() == "char")
                return "char";
            if (parameterType.name() == "short")
                return "short";
            if (parameterType.name() == "int")
                return "int";
            if (parameterType.name() == "half")
                return "half";
            ASSERT(parameterType.name() == "float");
            return "float";
        })();
        ASSERT(WTF::holds_alternative<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[1]));
        auto& constantExpression = WTF::get<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[0]);
        auto& integerLiteral = constantExpression.integerLiteral();
        auto suffix = ([&]() -> String {
            switch (integerLiteral.value()) {
            case 2:
                return "2"_str;
            case 3:
                return "3"_str;
            default:
                ASSERT(integerLiteral.value() == 4);
                return "4"_str;
            }
        })();
        return makeString(prefix, suffix);
    }
    if (nativeTypeDeclaration.name() == "matrix") {
        ASSERT(nativeTypeDeclaration.typeArguments().size() == 3);
        ASSERT(WTF::holds_alternative<UniqueRef<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]));
        auto& typeReference = WTF::get<UniqueRef<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]);
        auto& unifyNode = typeReference->unifyNode();
        ASSERT(is<AST::NamedType>(unifyNode));
        auto& namedType = downcast<AST::NamedType>(unifyNode);
        ASSERT(is<AST::NativeTypeDeclaration>(namedType));
        auto& parameterType = downcast<AST::NativeTypeDeclaration>(namedType);
        auto prefix = ([&]() -> String {
            if (parameterType.name() == "half")
                return "half";
            ASSERT(parameterType.name() == "float");
            return "float";
        })();
        ASSERT(WTF::holds_alternative<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[1]));
        auto& constantExpression1 = WTF::get<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[0]);
        auto& integerLiteral1 = constantExpression1.integerLiteral();
        auto middle = ([&]() -> String {
            switch (integerLiteral1.value()) {
            case 2:
                return "2"_str;
            case 3:
                return "3"_str;
            default:
                ASSERT(integerLiteral1.value() == 4);
                return "4"_str;
            }
        })();
        ASSERT(WTF::holds_alternative<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[2]));
        auto& constantExpression2 = WTF::get<AST::ConstantExpression>(nativeTypeDeclaration.typeArguments()[0]);
        auto& integerLiteral2 = constantExpression2.integerLiteral();
        auto suffix = ([&]() -> String {
            switch (integerLiteral2.value()) {
            case 2:
                return "2"_str;
            case 3:
                return "3"_str;
            default:
                ASSERT(integerLiteral2.value() == 4);
                return "4"_str;
            }
        })();
        return makeString(prefix, middle, 'x', suffix);
    }
    ASSERT(nativeTypeDeclaration.typeArguments().size() == 1);
    ASSERT(WTF::holds_alternative<UniqueRef<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]));
    auto& typeReference = WTF::get<UniqueRef<AST::TypeReference>>(nativeTypeDeclaration.typeArguments()[0]);
    auto prefix = ([&]() -> String {
        if (nativeTypeDeclaration.name() == "Texture1D")
            return "texture1d"_str;
        if (nativeTypeDeclaration.name() == "RWTexture1D")
            return "texture1d"_str;
        if (nativeTypeDeclaration.name() == "Texture1DArray")
            return "texture1d_array"_str;
        if (nativeTypeDeclaration.name() == "RWTexture1DArray")
            return "texture1d_array"_str;
        if (nativeTypeDeclaration.name() == "Texture2D")
            return "texture2d"_str;
        if (nativeTypeDeclaration.name() == "RWTexture2D")
            return "texture2d"_str;
        if (nativeTypeDeclaration.name() == "Texture2DArray")
            return "texture2d_array"_str;
        if (nativeTypeDeclaration.name() == "RWTexture2DArray")
            return "texture2d_array"_str;
        if (nativeTypeDeclaration.name() == "Texture3D")
            return "texture3d"_str;
        if (nativeTypeDeclaration.name() == "RWTexture3D")
            return "texture3d"_str;
        if (nativeTypeDeclaration.name() == "TextureCube")
            return "texturecube"_str;
        if (nativeTypeDeclaration.name() == "TextureDepth2D")
            return "depth2d"_str;
        if (nativeTypeDeclaration.name() == "RWTextureDepth2D")
            return "depth2d"_str;
        if (nativeTypeDeclaration.name() == "TextureDepth2DArray")
            return "depth2d_array"_str;
        if (nativeTypeDeclaration.name() == "RWTextureDepth2DArray")
            return "depth2d_array"_str;
        ASSERT(nativeTypeDeclaration.name() == "TextureDepthCube");
        return "depthcube"_str;
    })();
    auto innerType = ([&]() -> String {
        if (typeReference->name() == "ushort")
            return "ushort"_str;
        if (typeReference->name() == "ushort2")
            return "ushort"_str;
        if (typeReference->name() == "ushort3")
            return "ushort"_str;
        if (typeReference->name() == "ushort4")
            return "ushort"_str;
        if (typeReference->name() == "uint")
            return "uint"_str;
        if (typeReference->name() == "uint2")
            return "uint"_str;
        if (typeReference->name() == "uint3")
            return "uint"_str;
        if (typeReference->name() == "uint4")
            return "uint"_str;
        if (typeReference->name() == "short")
            return "short"_str;
        if (typeReference->name() == "short2")
            return "short"_str;
        if (typeReference->name() == "short3")
            return "short"_str;
        if (typeReference->name() == "short4")
            return "short"_str;
        if (typeReference->name() == "int")
            return "int"_str;
        if (typeReference->name() == "int2")
            return "int"_str;
        if (typeReference->name() == "int3")
            return "int"_str;
        if (typeReference->name() == "int4")
            return "int"_str;
        if (typeReference->name() == "half")
            return "half"_str;
        if (typeReference->name() == "half2")
            return "half"_str;
        if (typeReference->name() == "half3")
            return "half"_str;
        if (typeReference->name() == "half4")
            return "half"_str;
        if (typeReference->name() == "float")
            return "float"_str;
        if (typeReference->name() == "float2")
            return "float"_str;
        if (typeReference->name() == "float3")
            return "float"_str;
        ASSERT(typeReference->name() == "float4");
        return "float"_str;
    })();
    // FIXME: Specify the second template argument to Metal texture types.
    return makeString(prefix, '<', innerType, '>');
}

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif
