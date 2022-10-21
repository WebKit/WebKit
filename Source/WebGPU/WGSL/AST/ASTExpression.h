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

#include "ASTNode.h"

#include <wtf/TypeCasts.h>
#include <wtf/UniqueRefVector.h>

namespace WGSL::AST {

class Expression : public ASTNode {
    WTF_MAKE_FAST_ALLOCATED;

public:
    enum class Kind {
        BoolLiteral,
        Int32Literal,
        Uint32Literal,
        Float32Literal,
        AbstractIntLiteral,
        AbstractFloatLiteral,
        Identifier,
        ArrayAccess,
        StructureAccess,
        CallableExpression,
        UnaryExpression,
    };

    using List = UniqueRefVector<Expression>;

    Expression(SourceSpan span)
        : ASTNode(span)
    {
    }

    virtual ~Expression() { }

    virtual Kind kind() const = 0;
    bool isBoolLiteral() const { return kind() == Kind::BoolLiteral; }
    bool isInt32Literal() const { return kind() == Kind::Int32Literal; }
    bool isUInt32Literal() const { return kind() == Kind::Uint32Literal; }
    bool isFloat32Literal() const { return kind() == Kind::Float32Literal; }
    bool isAbstractIntLiteral() const { return kind() == Kind::AbstractIntLiteral; }
    bool isAbstractFloatLiteral() const { return kind() == Kind::AbstractFloatLiteral; }
    bool isIdentifier() const { return kind() == Kind::Identifier; }
    bool isArrayAccess() const { return kind() == Kind::ArrayAccess; }
    bool isStructureAccess() const { return kind() == Kind::StructureAccess; }
    bool isCallableExpression() const { return kind() == Kind::CallableExpression; }
    bool isUnaryExpression() const { return kind() == Kind::UnaryExpression; }
};

} // namespace WGSL::AST

#define SPECIALIZE_TYPE_TRAITS_WGSL_EXPRESSION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::AST::ToValueTypeName) \
    static bool isType(const WGSL::AST::Expression& expression) { return expression.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
