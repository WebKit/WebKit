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

#include "ASTExpression.h"
#include "ASTTypeName.h"

namespace WGSL::AST {

// A CallExpression expresses a "function" call, which consists of a target to be called,
// and a list of arguments. The target does not necesserily have to be a function identifier,
// but can also be a type, in which the whole call is a type conversion expression. The exact
// kind of expression can only be resolved during semantic analysis.
class CallExpression final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CallExpression(SourceSpan span, TypeName::Ref&& target, Expression::List&& arguments)
        : Expression(span)
        , m_target(WTFMove(target))
        , m_arguments(WTFMove(arguments))
    { }

    NodeKind kind() const override;
    TypeName& target() { return m_target.get(); }
    Expression::List& arguments() { return m_arguments; }

private:
    // If m_target is a NamedType, it could either be a:
    //   * Type that does not accept parameters (bool, i32, u32, ...)
    //   * Identifier that refers to a type alias.
    //   * Identifier that refers to a function.
    TypeName::Ref m_target;
    Expression::List m_arguments;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(CallExpression)
