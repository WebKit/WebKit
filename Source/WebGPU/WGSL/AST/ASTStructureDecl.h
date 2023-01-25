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

class StructMember final : public Node {
    WTF_MAKE_FAST_ALLOCATED;

public:
    using List = UniqueRefVector<StructMember>;

    StructMember(SourceSpan span, const String& name, Ref<TypeDecl>&& type, Attribute::List&& attributes)
        : Node(span)
        , m_name(name)
        , m_attributes(WTFMove(attributes))
        , m_type(WTFMove(type))
    {
    }

    Kind kind() const override;
    const String& name() const { return m_name; }
    TypeDecl& type() { return m_type; }
    Attribute::List& attributes() { return m_attributes; }

private:
    String m_name;
    Attribute::List m_attributes;
    Ref<TypeDecl> m_type;
};

enum class StructRole : uint8_t {
    UserDefined,
    VertexInput,
    FragmentInput,
    ComputeInput,
    VertexOutput,
};

class StructDecl final : public Decl {
    WTF_MAKE_FAST_ALLOCATED;

public:
    using List = UniqueRefVector<StructDecl>;

    StructDecl(SourceSpan sourceSpan, const String& name, StructMember::List&& members, Attribute::List&& attributes, StructRole role)
        : Decl(sourceSpan)
        , m_role(role)
        , m_name(name)
        , m_attributes(WTFMove(attributes))
        , m_members(WTFMove(members))
    {
    }

    Kind kind() const override;
    StructRole role() const { return m_role; }
    const String& name() const { return m_name; }
    Attribute::List& attributes() { return m_attributes; }
    StructMember::List& members() { return m_members; }

    void setRole(StructRole role) { m_role = role; }

private:
    StructRole m_role;
    String m_name;
    Attribute::List m_attributes;
    StructMember::List m_members;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(StructMember)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(StructDecl)
