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
#include "WHLSLResourceSemantic.h"

#if ENABLE(WEBGPU)

#include "WHLSLArrayType.h"
#include "WHLSLInferTypes.h"
#include "WHLSLIntrinsics.h"
#include "WHLSLReferenceType.h"
#include "WHLSLTypeReference.h"

namespace WebCore {

namespace WHLSL {

namespace AST {

bool ResourceSemantic::isAcceptableType(const UnnamedType& unnamedType, const Intrinsics& intrinsics) const
{
    switch (m_mode) {
    case Mode::UnorderedAccessView:
        if (is<ReferenceType>(unnamedType)) {
            auto& referenceType = downcast<ReferenceType>(unnamedType);
            return referenceType.addressSpace() == ReferenceType::AddressSpace::Constant || referenceType.addressSpace() == ReferenceType::AddressSpace::Device;
        }
        if (is<ArrayType>(unnamedType))
            return true;
        if (is<TypeReference>(unnamedType)) {
            auto& typeReference = downcast<TypeReference>(unnamedType);
            ASSERT(typeReference.resolvedType());
            if (is<NativeTypeDeclaration>(*typeReference.resolvedType()))
                return downcast<NativeTypeDeclaration>(*typeReference.resolvedType()).isTexture();
        }
        return false;
    case Mode::Texture:
        if (is<ReferenceType>(unnamedType))
            return downcast<ReferenceType>(unnamedType).addressSpace() == ReferenceType::AddressSpace::Constant;
        if (is<ArrayType>(unnamedType))
            return true;
        if (is<TypeReference>(unnamedType)) {
            auto& typeReference = downcast<TypeReference>(unnamedType);
            ASSERT(typeReference.resolvedType());
            if (is<NativeTypeDeclaration>(*typeReference.resolvedType()))
                return downcast<NativeTypeDeclaration>(*typeReference.resolvedType()).isTexture();
        }
        return false;
    case Mode::Buffer:
        if (is<ReferenceType>(unnamedType))
            return downcast<ReferenceType>(unnamedType).addressSpace() == ReferenceType::AddressSpace::Constant;
        return is<ArrayType>(unnamedType);
    case Mode::Sampler:
        return matches(unnamedType, intrinsics.samplerType());
    }
}

bool ResourceSemantic::isAcceptableForShaderItemDirection(ShaderItemDirection direction, const FunctionDefinition&) const
{
    return direction == ShaderItemDirection::Input;
}

} // namespace AST

}

}

#endif
