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
#include "ASTDecl.h"
#include "ASTNode.h"
#include "ASTTypeDecl.h"
#include "CompilationMessage.h"

#include <wtf/UniqueRefVector.h>

namespace WGSL::AST {

class Parameter final : public ASTNode {
    WTF_MAKE_FAST_ALLOCATED;

public:
    using List = UniqueRefVector<Parameter>;

    Parameter(SourceSpan span, StringView name, UniqueRef<TypeDecl>&& type, Attribute::List&& attributes)
        : ASTNode(span)
        , m_name(WTFMove(name))
        , m_type(WTFMove(type))
        , m_attributes(WTFMove(attributes))
    {
    }

    const StringView& name() const { return m_name; }
    TypeDecl& type() { return m_type; }
    Attribute::List& attributes() { return m_attributes; }

private:
    StringView m_name;
    UniqueRef<TypeDecl> m_type;
    Attribute::List m_attributes;
};

class FunctionDecl final : public Decl {
    WTF_MAKE_FAST_ALLOCATED;

public:
    using List = UniqueRefVector<FunctionDecl>;

    FunctionDecl(SourceSpan sourceSpan, StringView name, Parameter::List&& parameters, std::unique_ptr<TypeDecl>&& returnType, CompoundStatement&& body, Attribute::List&& attributes, Attribute::List&& returnAttributes)
        : Decl(sourceSpan)
        , m_name(name)
        , m_parameters(WTFMove(parameters))
        , m_attributes(WTFMove(attributes))
        , m_returnAttributes(WTFMove(returnAttributes))
        , m_returnType(WTFMove(returnType))
        , m_body(WTFMove(body))
    {
    }

    Kind kind() const override { return Kind::Function; }
    const StringView& name() const { return m_name; }
    Parameter::List& parameters() { return m_parameters; }
    Attribute::List& attributes() { return m_attributes; }
    Attribute::List& returnAttributes() { return m_returnAttributes; }
    TypeDecl* maybeReturnType() { return m_returnType.get(); }
    CompoundStatement& body() { return m_body; }

private:
    StringView m_name;
    Parameter::List m_parameters;
    Attribute::List m_attributes;
    Attribute::List m_returnAttributes;
    std::unique_ptr<TypeDecl> m_returnType;
    CompoundStatement m_body;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_GLOBAL_DECL(FunctionDecl, isFunction())
