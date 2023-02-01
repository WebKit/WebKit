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
#include "ASTIdentifier.h"
#include "ASTTypeName.h"
#include "ASTValue.h"

namespace WGSL::AST {

class ConstantValue : public Value {
    WTF_MAKE_FAST_ALLOCATED;
public:

    ConstantValue(SourceSpan span, Identifier&& name, TypeName::Ptr typeName, Expression::Ref&& initializer)
        : Value(span)
        , m_name(WTFMove(name))
        , m_typeName(WTFMove(typeName))
        , m_initializer(WTFMove(initializer))
    { }

    NodeKind kind() const override;
    Identifier& name() { return m_name; }
    TypeName* maybeTypeName() { return m_typeName.get(); }
    Expression& initializer() { return m_initializer.get(); }

private:
    Identifier m_name;
    TypeName::Ptr m_typeName;
    Expression::Ref m_initializer;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(ConstantValue)
