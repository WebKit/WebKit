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
#include "WHLSLStageInOutSemantic.h"

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLArrayType.h"
#include "WHLSLEnumerationDefinition.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLTypeReference.h"

namespace WebCore {

namespace WHLSL {

namespace AST {

bool StageInOutSemantic::isAcceptableType(const UnnamedType& unnamedType, const Intrinsics&) const
{
    if (is<ArrayType>(unnamedType))
        return true;
    if (!is<TypeReference>(unnamedType))
        return false;
    auto& typeReference = downcast<TypeReference>(unnamedType);
    auto& resolvedType = typeReference.resolvedType();
    if (is<EnumerationDefinition>(resolvedType))
        return true;
    if (!is<NativeTypeDeclaration>(resolvedType))
        return false;
    auto& nativeTypeDeclaration = downcast<NativeTypeDeclaration>(typeReference.resolvedType());
    return nativeTypeDeclaration.isNumber()
        || nativeTypeDeclaration.isVector()
        || nativeTypeDeclaration.isMatrix();
}

bool StageInOutSemantic::isAcceptableForShaderItemDirection(ShaderItemDirection direction, const std::optional<EntryPointType>& entryPointType) const
{
    switch (*entryPointType) {
    case EntryPointType::Vertex:
        return true;
    case EntryPointType::Fragment:
        return direction == ShaderItemDirection::Input;
    case EntryPointType::Compute:
        return false;
    }
}

} // namespace AST

}

}

#endif // ENABLE(WHLSL_COMPILER)
