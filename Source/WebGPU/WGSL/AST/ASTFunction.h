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
#include "ASTCompoundStatement.h"
#include "ASTDeclaration.h"
#include "ASTParameter.h"
#include "ASTTypeName.h"

#include <wtf/UniqueRefVector.h>

namespace WGSL::AST {

class Function final : public Declaration {
    WGSL_AST_BUILDER_NODE(Function);
public:
    using Ref = std::reference_wrapper<Function>;
    using List = ReferenceWrapperVector<Function>;

    NodeKind kind() const override;
    Identifier& name() { return m_name; }
    Parameter::List& parameters() { return m_parameters; }
    Attribute::List& attributes() { return m_attributes; }
    Attribute::List& returnAttributes() { return m_returnAttributes; }
    TypeName* maybeReturnType() { return m_returnType; }
    CompoundStatement& body() { return m_body.get(); }
    const Identifier& name() const { return m_name; }
    const Parameter::List& parameters() const { return m_parameters; }
    const Attribute::List& attributes() const { return m_attributes; }
    const Attribute::List& returnAttributes() const { return m_returnAttributes; }
    const TypeName* maybeReturnType() const { return m_returnType; }
    const CompoundStatement& body() const { return m_body.get(); }

private:
    Function(SourceSpan span, Identifier&& name, Parameter::List&& parameters, TypeName::Ptr returnType, CompoundStatement::Ref&& body, Attribute::List&& attributes, Attribute::List&& returnAttributes)
        : Declaration(span)
        , m_name(WTFMove(name))
        , m_parameters(WTFMove(parameters))
        , m_attributes(WTFMove(attributes))
        , m_returnAttributes(WTFMove(returnAttributes))
        , m_returnType(returnType)
        , m_body(WTFMove(body))
    { }

    Identifier m_name;
    Parameter::List m_parameters;
    Attribute::List m_attributes;
    Attribute::List m_returnAttributes;
    TypeName::Ptr m_returnType;
    CompoundStatement::Ref m_body;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(Function)
