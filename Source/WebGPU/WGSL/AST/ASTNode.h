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
#include <wtf/UniqueRef.h>
#include <wtf/UniqueRefVector.h>

namespace WGSL::AST {

enum class NodeKind : uint8_t {
    Unknown,

    // Attribute
    AlignAttribute,
    BindingAttribute,
    BuiltinAttribute,
    ConstAttribute,
    GroupAttribute,
    IdAttribute,
    InterpolateAttribute,
    InvariantAttribute,
    LocationAttribute,
    SizeAttribute,
    StageAttribute,
    WorkgroupSizeAttribute,

    Directive,

    // Expression
    BinaryExpression,
    BitcastExpression,
    CallExpression,
    FieldAccessExpression,
    IdentifierExpression,
    IdentityExpression,
    IndexAccessExpression,
    PointerDereferenceExpression,
    UnaryExpression,

    Function,
    Parameter,

    Identifier,

    // Literal
    AbstractFloatLiteral,
    AbstractIntegerLiteral,
    BoolLiteral,
    Float32Literal,
    Signed32Literal,
    Unsigned32Literal,

    ShaderModule,

    // Statement
    AssignmentStatement,
    BreakStatement,
    CallStatement,
    CompoundAssignmentStatement,
    CompoundStatement,
    ContinueStatement,
    DecrementIncrementStatement,
    DiscardStatement,
    ForStatement,
    IfStatement,
    LoopStatement,
    PhonyAssignmentStatement,
    ReturnStatement,
    StaticAssertStatement,
    SwitchStatement,
    VariableStatement,
    WhileStatement,

    Structure,
    StructureMember,

    TypeAlias,

    // TypeName
    ArrayTypeName,
    NamedTypeName,
    ParameterizedTypeName,
    ReferenceTypeName,
    StructTypeName,

    Variable,

    VariableQualifier
};

class Node {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Ref = UniqueRef<Node>;
    using List = UniqueRefVector<Node, 2>;

    Node(SourceSpan span)
        : m_span(span)
    { }
    virtual ~Node() = default;

    virtual NodeKind kind() const { return NodeKind::Unknown; };

    const SourceSpan& span() const { return m_span; }

private:
    SourceSpan m_span;
};

} // namespace WGSL::AST

#define SPECIALIZE_TYPE_TRAITS_WGSL_AST(Kind) \
inline WGSL::AST::NodeKind WGSL::AST::Kind::kind() const { return WGSL::AST::NodeKind::Kind; } \
SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::AST::Kind)                           \
static bool isType(const WGSL::AST::Node& node) { return node.kind() == WGSL::AST::NodeKind::Kind; } \
SPECIALIZE_TYPE_TRAITS_END()
