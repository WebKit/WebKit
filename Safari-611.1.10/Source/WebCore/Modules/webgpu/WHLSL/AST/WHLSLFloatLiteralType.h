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

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLResolvableType.h"
#include <wtf/FastMalloc.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class TypeReference;

class FloatLiteralType final : public ResolvableType {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FloatLiteralType(CodeLocation, float value);

    ~FloatLiteralType() = default;

    FloatLiteralType(const FloatLiteralType&) = delete;
    FloatLiteralType(FloatLiteralType&&) = default;

    FloatLiteralType& operator=(const FloatLiteralType&) = delete;
    FloatLiteralType& operator=(FloatLiteralType&&) = default;

    float value() const { return m_value; }

    TypeReference& preferredType() { return m_preferredType; }

    bool canResolve(const Type&) const;
    unsigned conversionCost(const UnnamedType&) const;

    FloatLiteralType clone() const;

private:
    float m_value;
    // This is a unique_ptr to resolve a circular dependency between
    // ConstantExpression -> LiteralType -> TypeReference -> TypeArguments -> ConstantExpression
    Ref<TypeReference> m_preferredType;
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(FloatLiteralType)

SPECIALIZE_TYPE_TRAITS_WHLSL_TYPE(FloatLiteralType, isFloatLiteralType())

#endif // ENABLE(WHLSL_COMPILER)
