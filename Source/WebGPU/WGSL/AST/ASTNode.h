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

#include "SourceSpan.h"

#include <wtf/FastMalloc.h>
#include <wtf/TypeCasts.h>

namespace WGSL::AST {

class Node {
    WTF_MAKE_FAST_ALLOCATED;

public:
    enum class Kind : uint8_t {
        // Attribute
        BindingAttribute,
        BuiltinAttribute,
        GroupAttribute,
        LocationAttribute,
        StageAttribute,

        // Decl
        FunctionDecl,
        StructDecl,
        VariableDecl,

        GlobalDirective,

        // Expression
        AbstractFloatLiteral,
        AbstractIntLiteral,
        ArrayAccess,
        BoolLiteral,
        CallableExpression,
        Float32Literal,
        IdentifierExpression,
        Int32Literal,
        StructureAccess,
        Uint32Literal,
        UnaryExpression,

        ShaderModule,

        // Statement
        AssignmentStatement,
        CompoundStatement,
        ReturnStatement,
        VariableStatement,

        // TypeDecl
        ArrayType,
        NamedType,
        ParameterizedType,

        Parameter,
        StructMember,
        VariableQualifier,
    };

    Node(SourceSpan span)
        : m_span(span)
    {
    }
    virtual ~Node() = default;

    virtual Kind kind() const = 0;

    const SourceSpan& span() const { return m_span; }

private:
    SourceSpan m_span;
};

} // namespace WGSL::AST

#define SPECIALIZE_TYPE_TRAITS_WGSL_AST(NodeKind) \
inline WGSL::AST::Node::Kind WGSL::AST::NodeKind::kind() const { return Kind::NodeKind; } \
SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::AST::NodeKind) \
static bool isType(const WGSL::AST::Node& node) { return node.kind() == WGSL::AST::Node::Kind::NodeKind; } \
SPECIALIZE_TYPE_TRAITS_END()
