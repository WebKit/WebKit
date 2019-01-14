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

#pragma once

#if ENABLE(WEBGPU)

#include "WHLSLType.h"
#include "WHLSLUnnamedType.h"
#include <memory>

namespace WebCore {

namespace WHLSL {

namespace AST {

class ResolvableType : public Type {
public:
    ResolvableType() = default;

    virtual ~ResolvableType() = default;

    ResolvableType(const ResolvableType&) = delete;
    ResolvableType(ResolvableType&&) = default;

    ResolvableType& operator=(const ResolvableType&) = delete;
    ResolvableType& operator=(ResolvableType&&) = default;

    bool isResolvableType() const override { return true; }
    virtual bool isFloatLiteralType() const { return false; }
    virtual bool isIntegerLiteralType() const { return false; }
    virtual bool isNullLiteralType() const { return false; }
    virtual bool isUnsignedIntegerLiteralType() const { return false; }

    virtual bool canResolve(const Type&) const = 0;
    virtual unsigned conversionCost(const UnnamedType&) const = 0;

    const UnnamedType* resolvedType() const { return m_resolvedType ? &*m_resolvedType : nullptr; }
    UnnamedType* resolvedType() { return m_resolvedType ? &*m_resolvedType : nullptr; }

    void resolve(UniqueRef<UnnamedType>&& type)
    {
        m_resolvedType = WTFMove(type);
    }

private:
    Optional<UniqueRef<UnnamedType>> m_resolvedType;
};

} // namespace AST

}

}

#define SPECIALIZE_TYPE_TRAITS_WHLSL_RESOLVABLE_TYPE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WHLSL::AST::ToValueTypeName) \
    static bool isType(const WebCore::WHLSL::AST::ResolvableType& type) { return type.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_WHLSL_TYPE(ResolvableType, isResolvableType())

#endif
