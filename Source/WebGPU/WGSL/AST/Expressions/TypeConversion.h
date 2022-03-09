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
#include "TypeDecl.h"
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace WGSL::AST {

// May be converted to a function call during validation, as we can't tell the difference during parsing.
class TypeConversion final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TypeConversion(SourceSpan span, UniqueRef<TypeDecl>&& typeDecl, Vector<UniqueRef<Expression>>&& arguments)
        : Expression(span)
        , m_typeDecl(WTFMove(typeDecl))
        , m_arguments(WTFMove(arguments))
    {
    }

    Kind kind() const override { return Kind::TypeConversion; }
    UniqueRef<TypeDecl>& typeDecl() { return m_typeDecl; }
    Vector<UniqueRef<Expression>>& arguments() { return m_arguments; }

private:
    UniqueRef<TypeDecl> m_typeDecl;
    Vector<UniqueRef<Expression>> m_arguments;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_EXPRESSION(TypeConversion, isTypeConversion())
