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

#include "WHLSLExpression.h"
#include "WHLSLUnsignedIntegerLiteralType.h"
#include <wtf/FastMalloc.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class UnsignedIntegerLiteral final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    UnsignedIntegerLiteral(CodeLocation location, unsigned value)
        : Expression(location, Kind::UnsignedIntegerLiteral)
        , m_type(location, value)
        , m_value(value)
    {
    }

    ~UnsignedIntegerLiteral() = default;

    UnsignedIntegerLiteral(const UnsignedIntegerLiteral&) = delete;
    UnsignedIntegerLiteral(UnsignedIntegerLiteral&&) = default;

    UnsignedIntegerLiteral& operator=(const UnsignedIntegerLiteral&) = delete;
    UnsignedIntegerLiteral& operator=(UnsignedIntegerLiteral&&) = default;

    UnsignedIntegerLiteralType& type() { return m_type; }
    unsigned value() const { return m_value; }

    UnsignedIntegerLiteral clone() const
    {
        UnsignedIntegerLiteral result(codeLocation(), m_value);
        result.m_type = m_type.clone();
        if (auto* resolvedType = m_type.maybeResolvedType())
            result.m_type.resolve(const_cast<AST::UnnamedType&>(*resolvedType));
        copyTypeTo(result);
        return result;
    }

    int64_t valueForSelectedType() const;

private:
    UnsignedIntegerLiteralType m_type;
    unsigned m_value;
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(UnsignedIntegerLiteral)

SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(UnsignedIntegerLiteral, isUnsignedIntegerLiteral())

#endif
