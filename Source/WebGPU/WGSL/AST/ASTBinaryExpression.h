/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "ASTExpression.h"
#include <wtf/Forward.h>

namespace WGSL::AST {

enum class BinaryOperation : uint8_t {
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,

    And,
    Or,
    Xor,

    LeftShift,
    RightShift,

    Equal,
    NotEqual,
    GreaterThan,
    GreaterEqual,
    LessThan,
    LessEqual,

    ShortCircuitAnd,
    ShortCircuitOr,
};

class BinaryExpression : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Ref = UniqueRef<Expression>;
    using List = UniqueRefVector<Expression>;

    BinaryExpression(SourceSpan span, Expression::Ref&& lhs, Expression::Ref&& rhs, BinaryOperation operation)
        : Expression(span)
        , m_lhs(WTFMove(lhs))
        , m_rhs(WTFMove(rhs))
        , m_operation(operation)
    { }

    NodeKind kind() const override;
    BinaryOperation operation() const { return m_operation; }
    Expression& leftExpression() { return m_lhs.get(); }
    Expression& rightExpression() { return m_rhs.get(); }

private:
    Expression::Ref m_lhs;
    Expression::Ref m_rhs;
    BinaryOperation m_operation;
};

void printInternal(PrintStream&, BinaryOperation);

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(BinaryExpression)
