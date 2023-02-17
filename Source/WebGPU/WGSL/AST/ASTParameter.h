/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "ASTIdentifier.h"
#include "ASTTypeName.h"

namespace WGSL::AST {

enum class ParameterRole : uint8_t {
    UserDefined,
    StageIn,
    BindGroup,
};

class Parameter final : public Node {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using List = UniqueRefVector<Parameter>;

    Parameter(SourceSpan span, Identifier&& name, TypeName::Ref&& typeName, Attribute::List&& attributes, ParameterRole role)
        : Node(span)
        , m_role(role)
        , m_name(WTFMove(name))
        , m_typeName(WTFMove(typeName))
        , m_attributes(WTFMove(attributes))
    { }

    NodeKind kind() const override;
    Identifier& name() { return m_name; }
    TypeName& typeName() { return m_typeName.get(); }
    Attribute::List& attributes() { return m_attributes; }
    ParameterRole role() { return m_role; }

private:
    ParameterRole m_role;
    Identifier m_name;
    TypeName::Ref m_typeName;
    Attribute::List m_attributes;
};

} // namespace WGSL::AST

SPECIALIZE_TYPE_TRAITS_WGSL_AST(Parameter)
