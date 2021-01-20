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
#include "WHLSLStatement.h"
#include "WHLSLVariableDeclarationsStatement.h"
#include <memory>
#include <wtf/FastMalloc.h>
#include <wtf/UniqueRef.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class ForLoop final : public Statement {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ForLoop(CodeLocation location, UniqueRef<Statement>&& initialization, std::unique_ptr<Expression>&& condition, std::unique_ptr<Expression>&& increment, UniqueRef<Statement>&& body)
        : Statement(location, Kind::ForLoop)
        , m_initialization(WTFMove(initialization))
        , m_condition(WTFMove(condition))
        , m_increment(WTFMove(increment))
        , m_body(WTFMove(body))
    {
    }

    ~ForLoop() = default;

    ForLoop(const ForLoop&) = delete;
    ForLoop(ForLoop&&) = default;

    UniqueRef<Statement>& initialization() { return m_initialization; }
    Expression* condition() { return m_condition.get(); }
    Expression* increment() { return m_increment.get(); }
    Statement& body() { return m_body; }

private:
    UniqueRef<Statement> m_initialization;
    std::unique_ptr<Expression> m_condition;
    std::unique_ptr<Expression> m_increment;
    UniqueRef<Statement> m_body;
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(ForLoop)

SPECIALIZE_TYPE_TRAITS_WHLSL_STATEMENT(ForLoop, isForLoop())

#endif // ENABLE(WHLSL_COMPILER)
