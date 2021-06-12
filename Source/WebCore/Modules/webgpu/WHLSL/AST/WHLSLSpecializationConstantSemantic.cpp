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
#include "WHLSLSpecializationConstantSemantic.h"

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLTypeReference.h"

namespace WebCore {

namespace WHLSL {

namespace AST {

bool SpecializationConstantSemantic::isAcceptableType(const UnnamedType& unnamedType, const Intrinsics&) const
{
    if (!is<TypeReference>(unnamedType))
        return false;
    auto& typeReference = downcast<TypeReference>(unnamedType);
    if (!is<NativeTypeDeclaration>(typeReference.resolvedType()))
        return false;
    return downcast<NativeTypeDeclaration>(typeReference.resolvedType()).isNumber();
}

bool SpecializationConstantSemantic::isAcceptableForShaderItemDirection(ShaderItemDirection direction, const std::optional<EntryPointType>&) const
{
    return direction == ShaderItemDirection::Input;
}

} // namespace AST

}

}

#endif // ENABLE(WHLSL_COMPILER)
