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

#include "WHLSLExpression.h"
#include "WHLSLIntegerLiteralType.h"
#include <wtf/FastMalloc.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class IntegerLiteral final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    IntegerLiteral(CodeLocation location, int value)
        : Expression(location, Kind::IntegerLiteral)
        , m_type(location, value)
        , m_value(value)
    {
    }

    ~IntegerLiteral() = default;

    IntegerLiteral(const IntegerLiteral&) = delete;
    IntegerLiteral(IntegerLiteral&&) = default;

    IntegerLiteral& operator=(const IntegerLiteral&) = delete;
    IntegerLiteral& operator=(IntegerLiteral&&) = default;

    IntegerLiteralType& type() { return m_type; }
    int value() const { return m_value; }

    IntegerLiteral clone() const
    {
        IntegerLiteral result(codeLocation(), m_value);
        result.m_type = m_type.clone();
        if (auto* resolvedType = m_type.maybeResolvedType())
            result.m_type.resolve(const_cast<AST::UnnamedType&>(*resolvedType));
        copyTypeTo(result);
        return result;
    }

    int64_t valueForSelectedType() const;

private:
    IntegerLiteralType m_type;
    int m_value;
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(IntegerLiteral)

SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(IntegerLiteral, isIntegerLiteral())

#endif // ENABLE(WHLSL_COMPILER)
