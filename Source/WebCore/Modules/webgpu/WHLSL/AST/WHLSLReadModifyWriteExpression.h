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
#include "WHLSLVariableDeclaration.h"
#include "WHLSLVariableReference.h"
#include <memory>
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

/*
 *  1. Evaluate m_lValue
 *  2. Assign the result to m_oldValue
 *  3. Evaluate m_newValueExpression
 *  4. Assign the result to m_newValue
 *  5. Assign the result to m_lValue
 *  6. Evaluate m_resultExpression
 *  7. Return the result
 */
class ReadModifyWriteExpression : public Expression {
public:
    static UniqueRef<ReadModifyWriteExpression> create(Lexer::Token&& origin, UniqueRef<Expression> lValue)
    {
        return makeUniqueRef<ReadModifyWriteExpression>(WTFMove(origin), WTFMove(lValue));
    }

    virtual ~ReadModifyWriteExpression() = default;

    ReadModifyWriteExpression(const ReadModifyWriteExpression&) = delete;
    ReadModifyWriteExpression(ReadModifyWriteExpression&&) = default;

    void setNewValueExpression(UniqueRef<Expression>&& newValueExpression)
    {
        m_newValueExpression = WTFMove(newValueExpression);
    }

    void setResultExpression(UniqueRef<Expression>&& resultExpression)
    {
        m_resultExpression = WTFMove(resultExpression);
    }

    UniqueRef<VariableReference> oldVariableReference()
    {
        // The only reason we don't get use-after-frees is the fact that every instance of ReadModifyWriteExpression is allocated on the heap.
        return makeUniqueRef<VariableReference>(VariableReference::wrap(m_oldValue));
    }

    UniqueRef<VariableReference> newVariableReference()
    {
        // The only reason we don't get use-after-frees is the fact that every instance of ReadModifyWriteExpression is allocated on the heap.
        return makeUniqueRef<VariableReference>(VariableReference::wrap(m_newValue));
    }

    bool isReadModifyWriteExpression() const override { return true; }

    Expression& lValue() { return m_lValue; }
    VariableDeclaration& oldValue() { return m_oldValue; }
    VariableDeclaration& newValue() { return m_newValue; }
    Expression* newValueExpression() { return m_newValueExpression ? &*m_newValueExpression : nullptr; }
    Expression* resultExpression() { return m_resultExpression ? &*m_resultExpression : nullptr; }

private:
    template<class U, class... Args> friend UniqueRef<U> WTF::makeUniqueRef(Args&&...);

    ReadModifyWriteExpression(Lexer::Token&& origin, UniqueRef<Expression> lValue)
        : Expression(Lexer::Token(origin))
        , m_lValue(WTFMove(lValue))
        , m_oldValue(Lexer::Token(origin), Qualifiers(), WTF::nullopt, String(), WTF::nullopt, WTF::nullopt)
        , m_newValue(WTFMove(origin), Qualifiers(), WTF::nullopt, String(), WTF::nullopt, WTF::nullopt)
    {
    }

    UniqueRef<Expression> m_lValue;
    VariableDeclaration m_oldValue;
    VariableDeclaration m_newValue;
    Optional<UniqueRef<Expression>> m_newValueExpression;
    Optional<UniqueRef<Expression>> m_resultExpression;
};

} // namespace AST

}

}

SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(ReadModifyWriteExpression, isReadModifyWriteExpression())

#endif
