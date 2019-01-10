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

class PropertyAccessExpression : public Expression {
public:
    PropertyAccessExpression(Lexer::Token&& origin, UniqueRef<Expression>&& base)
        : Expression(WTFMove(origin))
        , m_base(WTFMove(base))
    {
    }

    virtual ~PropertyAccessExpression() = default;

    PropertyAccessExpression(const PropertyAccessExpression&) = delete;
    PropertyAccessExpression(PropertyAccessExpression&&) = default;

    bool isPropertyAccessExpression() const override { return true; }

    virtual String getFunctionName() const = 0;
    virtual String setFunctionName() const = 0;
    virtual String andFunctionName() const = 0;

    Vector<std::reference_wrapper<FunctionDeclaration>, 1>& possibleGetOverloads() { return m_possibleGetOverloads; }
    Vector<std::reference_wrapper<FunctionDeclaration>, 1>& possibleSetOverloads() { return m_possibleSetOverloads; }
    Vector<std::reference_wrapper<FunctionDeclaration>, 1>& possibleAndOverloads() { return m_possibleAndOverloads; }

    void setPossibleGetOverloads(const Vector<std::reference_wrapper<FunctionDeclaration>, 1>& overloads)
    {
        m_possibleGetOverloads = overloads;
    }
    void setPossibleSetOverloads(const Vector<std::reference_wrapper<FunctionDeclaration>, 1>& overloads)
    {
        m_possibleSetOverloads = overloads;
    }
    void setPossibleAndOverloads(const Vector<std::reference_wrapper<FunctionDeclaration>, 1>& overloads)
    {
        m_possibleAndOverloads = overloads;
    }

    Expression& base() { return static_cast<Expression&>(m_base); }

private:
    UniqueRef<Expression> m_base;
    Vector<std::reference_wrapper<FunctionDeclaration>, 1> m_possibleGetOverloads;
    Vector<std::reference_wrapper<FunctionDeclaration>, 1> m_possibleSetOverloads;
    Vector<std::reference_wrapper<FunctionDeclaration>, 1> m_possibleAndOverloads;
};

} // namespace AST

}

}

SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(PropertyAccessExpression, isPropertyAccessExpression())

#endif
