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
#include "WHLSLVariableDeclaration.h"
#include "WHLSLVariableReference.h"
#include <memory>
#include <wtf/FastMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

/*
 *  1. Evaluate m_leftValue
 *  2. Assign the result to m_oldValue
 *  3. Evaluate m_newValueExpression
 *  4. Assign the result to m_newValue
 *  5. Assign the result to m_leftValue
 *  6. Evaluate m_resultExpression
 *  7. Return the result
 */
class ReadModifyWriteExpression final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ReadModifyWriteExpression(CodeLocation location, UniqueRef<Expression> leftValue)
        : Expression(location, Kind::ReadModifyWrite)
        , m_leftValue(WTFMove(leftValue))
        , m_oldValue(makeUniqueRef<VariableDeclaration>(location, Qualifiers(), nullptr, String(), nullptr, nullptr))
        , m_newValue(makeUniqueRef<VariableDeclaration>(location, Qualifiers(), nullptr, String(), nullptr, nullptr))
    {
    }

    ReadModifyWriteExpression(CodeLocation location, UniqueRef<Expression> leftValue, UniqueRef<VariableDeclaration> oldValue, UniqueRef<VariableDeclaration> newValue)
        : Expression(location, Kind::ReadModifyWrite)
        , m_leftValue(WTFMove(leftValue))
        , m_oldValue(WTFMove(oldValue))
        , m_newValue(WTFMove(newValue))
    {
    }


    ~ReadModifyWriteExpression() = default;

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
        return makeUniqueRef<VariableReference>(VariableReference::wrap(m_oldValue));
    }

    UniqueRef<VariableReference> newVariableReference()
    {
        return makeUniqueRef<VariableReference>(VariableReference::wrap(m_newValue));
    }

    Expression& leftValue() { return m_leftValue; }
    UniqueRef<Expression>& leftValueReference() { return m_leftValue; }
    VariableDeclaration& oldValue() { return m_oldValue; }
    VariableDeclaration& newValue() { return m_newValue; }
    Expression& newValueExpression()
    {
        ASSERT(m_newValueExpression);
        return *m_newValueExpression;
    }
    Expression& resultExpression()
    {
        ASSERT(m_resultExpression);
        return *m_resultExpression;
    }
    UniqueRef<Expression> takeLeftValue() { return WTFMove(m_leftValue); }
    UniqueRef<VariableDeclaration> takeOldValue() { return WTFMove(m_oldValue); }
    UniqueRef<VariableDeclaration> takeNewValue() { return WTFMove(m_newValue); }
    UniqueRef<Expression> takeNewValueExpression()
    {
        auto result = WTFMove(m_newValueExpression.value());
        m_newValueExpression.reset();
        return result;
    }
    UniqueRef<Expression> takeResultExpression()
    {
        auto result = WTFMove(m_resultExpression.value());
        m_resultExpression.reset();
        return result;
    }

private:
    UniqueRef<Expression> m_leftValue;
    UniqueRef<VariableDeclaration> m_oldValue;
    UniqueRef<VariableDeclaration> m_newValue;
    Optional<UniqueRef<Expression>> m_newValueExpression;
    Optional<UniqueRef<Expression>> m_resultExpression;
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(ReadModifyWriteExpression)

SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(ReadModifyWriteExpression, isReadModifyWriteExpression())

#endif
