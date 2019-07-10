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
#include "WHLSLStatement.h"
#include <memory>
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class IfStatement : public Statement {
public:
    IfStatement(Lexer::Token&& origin, UniqueRef<Expression>&& conditional, UniqueRef<Statement>&& body, Optional<UniqueRef<Statement>>&& elseBody)
        : Statement(WTFMove(origin))
        , m_conditional(WTFMove(conditional))
        , m_body(WTFMove(body))
        , m_elseBody(WTFMove(elseBody))
    {
    }

    virtual ~IfStatement() = default;

    IfStatement(const IfStatement&) = delete;
    IfStatement(IfStatement&&) = default;

    bool isIfStatement() const override { return true; }

    Expression& conditional() { return m_conditional; }
    Statement& body() { return m_body; }
    Statement* elseBody() { return m_elseBody ? &*m_elseBody : nullptr; }

private:
    UniqueRef<Expression> m_conditional;
    UniqueRef<Statement> m_body;
    Optional<UniqueRef<Statement>> m_elseBody;
};

} // namespace AST

}

}

SPECIALIZE_TYPE_TRAITS_WHLSL_STATEMENT(IfStatement, isIfStatement())

#endif
