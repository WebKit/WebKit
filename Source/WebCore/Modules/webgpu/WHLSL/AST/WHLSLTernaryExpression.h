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
#include "WHLSLLexer.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class TernaryExpression : public Expression {
public:
    TernaryExpression(Lexer::Token&& origin, UniqueRef<Expression>&& predicate, UniqueRef<Expression>&& bodyExpression, UniqueRef<Expression>&& elseExpression)
        : Expression(WTFMove(origin))
        , m_predicate(WTFMove(predicate))
        , m_bodyExpression(WTFMove(bodyExpression))
        , m_elseExpression(WTFMove(elseExpression))
    {
    }

    virtual ~TernaryExpression() = default;

    TernaryExpression(const TernaryExpression&) = delete;
    TernaryExpression(TernaryExpression&&) = default;

    bool isTernaryExpression() const override { return true; }

    Expression& predicate() { return m_predicate; }
    Expression& bodyExpression() { return m_bodyExpression; }
    Expression& elseExpression() { return m_elseExpression; }

private:
    UniqueRef<Expression> m_predicate;
    UniqueRef<Expression> m_bodyExpression;
    UniqueRef<Expression> m_elseExpression;
};

} // namespace AST

}

}

SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(TernaryExpression, isTernaryExpression())

#endif
