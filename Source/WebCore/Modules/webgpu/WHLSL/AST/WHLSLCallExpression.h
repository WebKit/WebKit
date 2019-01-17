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
#include "WHLSLFunctionDeclaration.h"
#include "WHLSLLexer.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class NamedType;

class CallExpression : public Expression {
public:
    CallExpression(Lexer::Token&& origin, String&& name, Vector<UniqueRef<Expression>>&& arguments)
        : Expression(WTFMove(origin))
        , m_name(WTFMove(name))
        , m_arguments(WTFMove(arguments))
    {
    }

    virtual ~CallExpression() = default;

    CallExpression(const CallExpression&) = delete;
    CallExpression(CallExpression&&) = default;

    bool isCallExpression() const override { return true; }

    Vector<UniqueRef<Expression>>& arguments() { return m_arguments; }

    String& name() { return m_name; }

    void setCastData(NamedType& namedType)
    {
        m_castReturnType = { namedType };
    }

    bool isCast() { return static_cast<bool>(m_castReturnType); }
    Optional<std::reference_wrapper<NamedType>>& castReturnType() { return m_castReturnType; }
    bool hasOverloads() const { return static_cast<bool>(m_overloads); }
    Optional<Vector<std::reference_wrapper<FunctionDeclaration>, 1>>& overloads() { return m_overloads; }
    void setOverloads(const Vector<std::reference_wrapper<FunctionDeclaration>, 1>& overloads)
    {
        assert(!hasOverloads());
        m_overloads = overloads;
    }

    FunctionDeclaration* function() { return m_function; }

    void setFunction(FunctionDeclaration& functionDeclaration)
    {
        assert(!m_function);
        m_function = &functionDeclaration;
    }

private:
    String m_name;
    Vector<UniqueRef<Expression>> m_arguments;
    Optional<Vector<std::reference_wrapper<FunctionDeclaration>, 1>> m_overloads;
    FunctionDeclaration* m_function { nullptr };
    Optional<std::reference_wrapper<NamedType>> m_castReturnType { WTF::nullopt };
};

} // namespace AST

}

}

SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(CallExpression, isCallExpression())

#endif
