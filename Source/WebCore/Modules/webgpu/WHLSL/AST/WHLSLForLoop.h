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
#include "WHLSLVariableDeclarationsStatement.h"
#include <memory>
#include <wtf/UniqueRef.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class ForLoop : public Statement {
public:
    ForLoop(Lexer::Token&& origin, Variant<VariableDeclarationsStatement, UniqueRef<Expression>>&& initialization, Optional<UniqueRef<Expression>>&& condition, Optional<UniqueRef<Expression>>&& increment, UniqueRef<Statement>&& body)
        : Statement(WTFMove(origin))
        , m_initialization(WTFMove(initialization))
        , m_condition(WTFMove(condition))
        , m_increment(WTFMove(increment))
        , m_body(WTFMove(body))
    {
    }

    virtual ~ForLoop()
    {
    }

    ForLoop(const ForLoop&) = delete;
    ForLoop(ForLoop&&) = default;

    bool isForLoop() const override { return true; }

    Variant<VariableDeclarationsStatement, UniqueRef<Expression>>& initialization() { return m_initialization; }
    Expression* condition() { return m_condition ? &static_cast<Expression&>(*m_condition) : nullptr; }
    Expression* increment() { return m_increment ? &static_cast<Expression&>(*m_increment) : nullptr; }
    Statement& body() { return static_cast<Statement&>(m_body); }

private:
    Variant<VariableDeclarationsStatement, UniqueRef<Expression>> m_initialization;
    Optional<UniqueRef<Expression>> m_condition;
    Optional<UniqueRef<Expression>> m_increment;
    UniqueRef<Statement> m_body;
};

} // namespace AST

}

}

SPECIALIZE_TYPE_TRAITS_WHLSL_STATEMENT(ForLoop, isForLoop())

#endif
