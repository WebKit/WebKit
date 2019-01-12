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
#include "WHLSLUnsignedIntegerLiteralType.h"

#if ENABLE(WEBGPU)

#include "WHLSLInferTypes.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLTypeArgument.h"
#include "WHLSLTypeReference.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

UnsignedIntegerLiteralType::UnsignedIntegerLiteralType(Lexer::Token&& origin, unsigned value)
    : m_value(value)
    , m_preferredType(makeUniqueRef<TypeReference>(WTFMove(origin), "uint"_str, TypeArguments()))
{
}

UnsignedIntegerLiteralType::~UnsignedIntegerLiteralType() = default;

UnsignedIntegerLiteralType::UnsignedIntegerLiteralType(UnsignedIntegerLiteralType&&) = default;

UnsignedIntegerLiteralType& UnsignedIntegerLiteralType::operator=(UnsignedIntegerLiteralType&&) = default;

bool UnsignedIntegerLiteralType::canResolve(const Type& type) const
{
    if (!is<NamedType>(type))
        return false;
    auto& namedType = downcast<NamedType>(type);
    if (!is<NativeTypeDeclaration>(namedType))
        return false;
    auto& nativeTypeDeclaration = downcast<NativeTypeDeclaration>(namedType);
    if (!nativeTypeDeclaration.isInt())
        return false;
    if (!nativeTypeDeclaration.isSigned())
        return false;
    if (!nativeTypeDeclaration.canRepresentUnsignedInteger()(m_value))
        return false;
    return true;
}

unsigned UnsignedIntegerLiteralType::conversionCost(const UnnamedType& unnamedType) const
{
    if (matches(unnamedType, static_cast<const TypeReference&>(m_preferredType)))
        return 0;
    return 1;
}

} // namespace AST

}

}

#endif
