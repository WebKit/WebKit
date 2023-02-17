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
#include "ASTDeclaration.h"
#include "ASTExpression.h"
#include "ASTIdentifier.h"
#include "ASTTypeName.h"
#include "ASTVariableQualifier.h"

namespace WGSL::AST {

enum class VariableFlavor : uint8_t {
    Const,
    Let,
    Override,
    Var,
};

class Variable final : public Declaration {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Ref = UniqueRef<Variable>;
    using List = UniqueRefVector<Variable>;

    Variable(SourceSpan span, VariableFlavor flavor, Identifier&& name, TypeName::Ptr&& type, Expression::Ptr&& initializer)
        : Variable(span, flavor, WTFMove(name), { }, WTFMove(type), WTFMove(initializer), { })
    { }

    Variable(SourceSpan span, VariableFlavor flavor, Identifier&& name, VariableQualifier::Ptr&& qualifier, TypeName::Ptr&& type, Expression::Ptr&& initializer, Attribute::List&& attributes)
        : Declaration(span)
        , m_name(WTFMove(name))
        , m_attributes(WTFMove(attributes))
        , m_qualifier(WTFMove(qualifier))
        , m_type(WTFMove(type))
        , m_initializer(WTFMove(initializer))
        , m_flavor(flavor)
    {
        ASSERT(m_type || m_initializer);
    }

    NodeKind kind() const override;
    VariableFlavor flavor() const { return m_flavor; };
    Identifier& name() { return m_name; }
    Attribute::List& attributes() { return m_attributes; }
    VariableQualifier* maybeQualifier() { return m_qualifier.get(); }
    TypeName* maybeTypeName() { return m_type.get(); }
    Expression* maybeInitializer() { return m_initializer.get(); }

private:
    Identifier m_name;
    Attribute::List m_attributes;
    // Each of the following may be null
    // But at least one of type and initializer must be non-null
    VariableQualifier::Ptr m_qualifier;
    TypeName::Ptr m_type;
    Expression::Ptr m_initializer;
    VariableFlavor m_flavor;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(Variable)
