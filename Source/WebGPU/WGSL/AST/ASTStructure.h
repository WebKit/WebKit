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
#include "ASTIdentifier.h"
#include "ASTStructureMember.h"

namespace WGSL::AST {

enum class StructureRole : uint8_t {
    UserDefined,
    VertexInput,
    FragmentInput,
    ComputeInput,
    VertexOutput,
    BindGroup,
    UserDefinedResource,
    PackedResource,
};

class Structure final : public Declaration {
    WGSL_AST_BUILDER_NODE(Structure);
public:
    using Ref = std::reference_wrapper<Structure>;
    using List = ReferenceWrapperVector<Structure>;

    NodeKind kind() const override;
    StructureRole role() const { return m_role; }
    StructureRole& role() { return m_role; }
    Identifier& name() { return m_name; }
    Attribute::List& attributes() { return m_attributes; }
    StructureMember::List& members() { return m_members; }
    Structure* original() const { return m_original; }
    Structure* packed() const { return m_packed; }

    void setRole(StructureRole role) { m_role = role; }

private:
    Structure(SourceSpan span, Identifier&& name, StructureMember::List&& members, Attribute::List&& attributes, StructureRole role, Structure* original = nullptr)
        : Declaration(span)
        , m_name(WTFMove(name))
        , m_attributes(WTFMove(attributes))
        , m_members(WTFMove(members))
        , m_role(role)
        , m_original(original)
    {
        if (m_original) {
            ASSERT(m_role == StructureRole::PackedResource);
            m_original->m_packed = this;
        }
    }

    Identifier m_name;
    Attribute::List m_attributes;
    StructureMember::List m_members;
    StructureRole m_role;
    Structure* m_original;
    Structure* m_packed { nullptr };
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(Structure)
