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
#include "ASTTypeDecl.h"
#include "CompilationMessage.h"

namespace WGSL::AST {

class StructMember final : public ASTNode {
    WTF_MAKE_FAST_ALLOCATED;

public:
    using List = UniqueRefVector<StructMember>;

    StructMember(SourceSpan span, StringView name, UniqueRef<TypeDecl>&& type, Attribute::List&& attributes)
        : ASTNode(span)
        , m_name(name)
        , m_attributes(WTFMove(attributes))
        , m_type(WTFMove(type))
    {
    }

    const StringView& name() const { return m_name; }
    TypeDecl& type() { return m_type; }
    Attribute::List& attributes() { return m_attributes; }

private:
    StringView m_name;
    Attribute::List m_attributes;
    UniqueRef<TypeDecl> m_type;
};

class StructDecl final : public Decl {
    WTF_MAKE_FAST_ALLOCATED;

public:
    using List = UniqueRefVector<StructDecl>;

    StructDecl(SourceSpan sourceSpan, StringView name, StructMember::List&& members, Attribute::List&& attributes)
        : Decl(sourceSpan)
        , m_name(name)
        , m_attributes(WTFMove(attributes))
        , m_members(WTFMove(members))
    {
    }

    Kind kind() const override { return Kind::Struct; }
    const StringView& name() const { return m_name; }
    Attribute::List& attributes() { return m_attributes; }
    StructMember::List& members() { return m_members; }

private:
    StringView m_name;
    Attribute::List m_attributes;
    StructMember::List m_members;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_GLOBAL_DECL(StructDecl, isStruct())
