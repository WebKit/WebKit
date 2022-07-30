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

#include "Attribute.h"
#include "CompilationMessage.h"
#include "Expression.h"
#include "GlobalDecl.h"
#include "TypeDecl.h"
#include "VariableQualifier.h"
#include <wtf/text/WTFString.h>

namespace WGSL::AST {

class VariableDecl final : public ASTNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Kind {
        Var,
        Let,
        Const
    };
    
    VariableDecl(SourceSpan span, Kind kind, StringView name, std::optional<VariableQualifier>&& qualifier, std::unique_ptr<TypeDecl>&& type, std::unique_ptr<Expression>&& initializer, Attributes&& attributes)
        : ASTNode(span)
        , m_kind(kind)
        , m_name(name)
        , m_attributes(WTFMove(attributes))
        , m_qualifier(WTFMove(qualifier))
        , m_type(WTFMove(type))
        , m_initializer(WTFMove(initializer))
    {
    }

    Kind kind() const { return m_kind; }
    const StringView& name() const { return m_name; }
    const Attributes& attributes() const { return m_attributes; }
    const std::optional<VariableQualifier>& maybeQualifier() const { return m_qualifier; }
    const TypeDecl* maybeTypeDecl() const { return m_type.get(); }
    const Expression* maybeInitializer() const { return m_initializer.get(); }

private:
    Kind m_kind;
    StringView m_name;
    Attributes m_attributes;
    // Each of the following may be null
    // But at least one of type and initializer must be non-null
    std::optional<VariableQualifier> m_qualifier;
    std::unique_ptr<TypeDecl> m_type;
    std::unique_ptr<Expression> m_initializer;
};

} // namespace WGSL::AST
