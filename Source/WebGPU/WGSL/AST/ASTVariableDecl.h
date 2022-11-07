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

#include "ASTAttribute.h"
#include "ASTDecl.h"
#include "ASTExpression.h"
#include "ASTTypeDecl.h"
#include "ASTVariableQualifier.h"
#include "CompilationMessage.h"

#include <wtf/text/WTFString.h>

namespace WGSL::AST {

class VariableDecl final : public Decl {
    WTF_MAKE_FAST_ALLOCATED;

public:
    using List = UniqueRefVector<VariableDecl>;

    VariableDecl(SourceSpan span, StringView name, std::unique_ptr<VariableQualifier>&& qualifier, std::unique_ptr<TypeDecl>&& type, std::unique_ptr<Expression>&& initializer, Attribute::List&& attributes)
        : Decl(span)
        , m_name(name)
        , m_attributes(WTFMove(attributes))
        , m_qualifier(WTFMove(qualifier))
        , m_type(WTFMove(type))
        , m_initializer(WTFMove(initializer))
    {
        ASSERT(m_type || m_initializer);
    }

    Kind kind() const override;
    const StringView& name() const { return m_name; }
    Attribute::List& attributes() { return m_attributes; }
    VariableQualifier* maybeQualifier() { return m_qualifier.get(); }
    TypeDecl* maybeTypeDecl() { return m_type.get(); }
    Expression* maybeInitializer() { return m_initializer.get(); }

private:
    StringView m_name;
    Attribute::List m_attributes;
    // Each of the following may be null
    // But at least one of type and initializer must be non-null
    std::unique_ptr<VariableQualifier> m_qualifier;
    std::unique_ptr<TypeDecl> m_type;
    std::unique_ptr<Expression> m_initializer;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(VariableDecl)
