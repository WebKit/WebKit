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

class Statement : public ASTNode {
    WTF_MAKE_FAST_ALLOCATED;

public:
    enum class Kind : uint8_t {
        Compound,
        Return,
        Assignment,
        Variable,
    };

    using List = UniqueRefVector<Statement>;

    Statement(SourceSpan span)
        : ASTNode(span)
    {
    }

    virtual ~Statement() { }

    virtual Kind kind() const = 0;
    bool isCompound() const { return kind() == Kind::Compound; }
    bool isReturn() const { return kind() == Kind::Return; }
    bool isAssignment() const { return kind() == Kind::Assignment; }
    bool isVariable() const { return kind() == Kind::Variable; }
};

} // namespace WGSL::AST

#define SPECIALIZE_TYPE_TRAITS_WGSL_STATEMENT(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::AST::ToValueTypeName) \
    static bool isType(const WGSL::AST::Statement& statement) { return statement.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
