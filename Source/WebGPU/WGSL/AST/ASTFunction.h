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
#include "ASTParameterValue.h"
#include "ASTTypeName.h"
#include "CompilationMessage.h"

#include <wtf/UniqueRefVector.h>

namespace WGSL::AST {

class Function final : public Declaration {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using List = UniqueRefVector<Function>;

    Function(SourceSpan span, Identifier&& name, ParameterValue::List&& parameters, TypeName::Ptr&& returnType, CompoundStatement&& body, Attribute::List&& attributes, Attribute::List&& returnAttributes)
        : Declaration(span)
        , m_name(WTFMove(name))
        , m_parameters(WTFMove(parameters))
        , m_attributes(WTFMove(attributes))
        , m_returnAttributes(WTFMove(returnAttributes))
        , m_returnType(WTFMove(returnType))
        , m_body(WTFMove(body))
    { }

    NodeKind kind() const override;
    Identifier& name() { return m_name; }
    ParameterValue::List& parameters() { return m_parameters; }
    Attribute::List& attributes() { return m_attributes; }
    Attribute::List& returnAttributes() { return m_returnAttributes; }
    TypeName* maybeReturnType() { return m_returnType.get(); }
    CompoundStatement& body() { return m_body; }

private:
    Identifier m_name;
    ParameterValue::List m_parameters;
    Attribute::List m_attributes;
    Attribute::List m_returnAttributes;
    TypeName::Ptr m_returnType;
    CompoundStatement m_body;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(Function)
