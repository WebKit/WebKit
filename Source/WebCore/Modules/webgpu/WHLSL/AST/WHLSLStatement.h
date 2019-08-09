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

#include "WHLSLCodeLocation.h"
#include "WHLSLDefaultDelete.h"
#include <wtf/FastMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class Statement {
    WTF_MAKE_FAST_ALLOCATED;

protected:
    ~Statement() = default;

public:
    enum class Kind : uint8_t {
        Block,
        Break,
        Continue,
        DoWhileLoop,
        EffectfulExpression,
        Fallthrough,
        ForLoop,
        If,
        Return,
        StatementList,
        SwitchCase,
        Switch,
        VariableDeclarations,
        WhileLoop,
    };
    Statement(CodeLocation codeLocation, Kind kind)
        : m_codeLocation(codeLocation)
        , m_kind(kind)
    {
    }

    static void destroy(Statement&);
    static void destruct(Statement&);

    Statement(const Statement&) = delete;
    Statement(Statement&&) = default;

    Kind kind() const { return m_kind; }

    bool isBlock() const { return kind() == Kind::Block; }
    bool isBreak() const { return kind() == Kind::Break; }
    bool isContinue() const { return kind() == Kind::Continue; }
    bool isDoWhileLoop() const { return kind() == Kind::DoWhileLoop; }
    bool isEffectfulExpressionStatement() const { return kind() == Kind::EffectfulExpression; }
    bool isFallthrough() const { return kind() == Kind::Fallthrough; }
    bool isForLoop() const { return kind() == Kind::ForLoop; }
    bool isIfStatement() const { return kind() == Kind::If; }
    bool isReturn() const { return kind() == Kind::Return; }
    bool isStatementList() const { return kind() == Kind::StatementList; }
    bool isSwitchCase() const { return kind() == Kind::SwitchCase; }
    bool isSwitchStatement() const { return kind() == Kind::Switch; }
    bool isVariableDeclarationsStatement() const { return kind() == Kind::VariableDeclarations; }
    bool isWhileLoop() const { return kind() == Kind::WhileLoop; }

    CodeLocation codeLocation() const { return m_codeLocation; }
    void updateCodeLocation(CodeLocation location) { m_codeLocation = location; }

private:
    CodeLocation m_codeLocation;
    Kind m_kind;
};

using Statements = Vector<UniqueRef<Statement>>;

}

}

}

DEFINE_DEFAULT_DELETE(Statement)

#define SPECIALIZE_TYPE_TRAITS_WHLSL_STATEMENT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WHLSL::AST::ToValueTypeName) \
    static bool isType(const WebCore::WHLSL::AST::Statement& statement) { return statement.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
