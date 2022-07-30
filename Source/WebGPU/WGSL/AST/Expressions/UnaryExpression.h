/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "Expression.h"
#include <wtf/text/StringView.h>
#include <wtf/UniqueRef.h>

namespace WGSL::AST {

class UnaryExpression final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Op {
        /* - */ Negation,

        /* ! */ LogicalNot,

        /* ~ */ BitwiseNot,

        // https://gpuweb.github.io/gpuweb/wgsl/#address-of-expr
        /* & */ AddressOf,

        // https://gpuweb.github.io/gpuweb/wgsl/#indirection-expr
        /* * */ Indirection,
    };

    UnaryExpression(SourceSpan span, Op op, UniqueRef<Expression> expression)
        : Expression(span)
        , m_op(op)
        , m_expression(WTFMove(expression))
    {
    }

    Kind kind() const override { return Kind::UnaryExpression; }
    Op op() const { return m_op; }
    const Expression& expression() const { return m_expression; } 

private:
    Op m_op;
    UniqueRef<Expression> m_expression;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_EXPRESSION(UnaryExpression, isUnaryExpression())

