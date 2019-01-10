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
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class VariableReference : public Expression {
public:
    VariableReference(Lexer::Token&& origin, String&& name)
        : Expression(WTFMove(origin))
        , m_name(WTFMove(name))
    {
    }

    virtual ~VariableReference() = default;

    VariableReference(const VariableReference&) = delete;
    VariableReference(VariableReference&&) = default;

    static VariableReference wrap(VariableDeclaration& variableDeclaration)
    {
        VariableReference result(Lexer::Token(variableDeclaration.origin()));
        result.m_variable = &variableDeclaration;
        return result;
    }

    bool isVariableReference() const override { return true; }

    String& name() { return m_name; }

    VariableDeclaration* variable() { return m_variable; }

    void setVariable(VariableDeclaration& variableDeclaration)
    {
        m_variable = &variableDeclaration;
    }

private:
    VariableReference(Lexer::Token&& origin)
        : Expression(WTFMove(origin))
    {
    }

    String m_name;
    VariableDeclaration* m_variable;
};

} // namespace AST

}

}

SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(VariableReference, isVariableReference())

#endif
