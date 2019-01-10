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

#include "WHLSLLexer.h"
#include "WHLSLValue.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class Statement : public Value {
public:
    Statement(Lexer::Token&& origin)
        : m_origin(WTFMove(origin))
    {
    }

    virtual ~Statement() = default;

    Statement(const Statement&) = delete;
    Statement(Statement&&) = default;

    virtual bool isBlock() const { return false; }
    virtual bool isBreak() const { return false; }
    virtual bool isContinue() const { return false; }
    virtual bool isDoWhileLoop() const { return false; }
    virtual bool isEffectfulExpressionStatement() const { return false; }
    virtual bool isFallthrough() const { return false; }
    virtual bool isForLoop() const { return false; }
    virtual bool isIfStatement() const { return false; }
    virtual bool isReturn() const { return false; }
    virtual bool isSwitchCase() const { return false; }
    virtual bool isSwitchStatement() const { return false; }
    virtual bool isTrap() const { return false; }
    virtual bool isVariableDeclarationsStatement() const { return false; }
    virtual bool isWhileLoop() const { return false; }

private:
    Lexer::Token m_origin;
};

using Statements = Vector<UniqueRef<Statement>>;

} // namespace AST

}

}

#define SPECIALIZE_TYPE_TRAITS_WHLSL_STATEMENT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WHLSL::AST::ToValueTypeName) \
    static bool isType(const WebCore::WHLSL::AST::Statement& statement) { return statement.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
