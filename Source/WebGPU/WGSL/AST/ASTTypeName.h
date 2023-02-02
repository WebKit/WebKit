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

#include "ASTExpression.h"
#include "ASTIdentifier.h"

namespace WGSL {
class ResolveTypeReferences;
} // namespace WGSL

namespace WGSL::AST {
class Structure;

class TypeName : public Node, public RefCounted<TypeName> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using Ref = WTF::Ref<TypeName>;
    using Ptr = RefPtr<TypeName>;

    TypeName(SourceSpan span)
        : Node(span)
    { }
};

class ArrayTypeName : public TypeName {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ArrayTypeName(SourceSpan span, TypeName::Ptr&& elementType, Expression::Ptr&& elementCount)
        : TypeName(span)
        , m_elementType(WTFMove(elementType))
        , m_elementCount(WTFMove(elementCount))
    { }

    NodeKind kind() const override;

    TypeName* maybeElementType() const { return m_elementType.get(); }
    Expression* maybeElementCount() const { return m_elementCount.get(); }

private:
    TypeName::Ptr m_elementType;
    Expression::Ptr m_elementCount;
};

class NamedTypeName : public TypeName {
    WTF_MAKE_FAST_ALLOCATED;
    friend class ::WGSL::ResolveTypeReferences;
public:
    NamedTypeName(SourceSpan span, Identifier&& name)
        : TypeName(span)
        , m_name(WTFMove(name))
    { }

    NodeKind kind() const override;
    Identifier& name() { return m_name; }
    TypeName* maybeResolvedReference() const { return m_resolvedReference.get(); }

private:
    void resolveTypeReference(TypeName::Ref&& typeName)
    {
        m_resolvedReference = WTFMove(typeName);
    }

    Identifier m_name;
    TypeName::Ptr m_resolvedReference;
};

class ParameterizedTypeName : public TypeName {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Base {
        Vec2,
        Vec3,
        Vec4,
        Mat2x2,
        Mat2x3,
        Mat2x4,
        Mat3x2,
        Mat3x3,
        Mat3x4,
        Mat4x2,
        Mat4x3,
        Mat4x4
    };

    ParameterizedTypeName(SourceSpan span, Base base, TypeName::Ref&& elementType)
        : TypeName(span)
        , m_base(base)
        , m_elementType(WTFMove(elementType))
    { }

    static std::optional<Base> stringViewToKind(const StringView& view)
    {
        if (view == "vec2"_s)
            return Base::Vec2;
        if (view == "vec3"_s)
            return Base::Vec3;
        if (view == "vec4"_s)
            return Base::Vec4;
        if (view == "mat2x2"_s)
            return Base::Mat2x2;
        if (view == "mat2x3"_s)
            return Base::Mat2x3;
        if (view == "mat2x4"_s)
            return Base::Mat2x4;
        if (view == "mat3x2"_s)
            return Base::Mat3x2;
        if (view == "mat3x3"_s)
            return Base::Mat3x3;
        if (view == "mat3x4"_s)
            return Base::Mat3x4;
        if (view == "mat4x2"_s)
            return Base::Mat4x2;
        if (view == "mat4x3"_s)
            return Base::Mat4x3;
        if (view == "mat4x4"_s)
            return Base::Mat4x4;
        return std::nullopt;
    }

    NodeKind kind() const override;
    Base base() const { return m_base; }
    TypeName& elementType() { return m_elementType; }

private:
    Base m_base;
    TypeName::Ref m_elementType;
};

class ReferenceTypeName final : public TypeName {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ReferenceTypeName(SourceSpan span, TypeName::Ref&& type)
        : TypeName(span)
        , m_type(WTFMove(type))
    { }

    NodeKind kind() const override;
    TypeName& type() const { return m_type.get(); }

private:
    TypeName::Ref m_type;
};

class StructTypeName final : public TypeName {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StructTypeName(SourceSpan span, Structure& structure)
        : TypeName(span)
        , m_structure(structure)
    { }

    NodeKind kind() const override;
    Structure& structure() const { return m_structure; }

private:
    // FIXME: Should this be Ref<Structure>?
    Structure& m_structure;
};


} // namespace WGSL::AST

#define SPECIALIZE_TYPE_TRAITS_WGSL_TYPE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::AST::ToValueTypeName) \
    static bool isType(const WGSL::AST::TypeDecl& type) { return type.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::AST::TypeName)
static bool isType(const WGSL::AST::Node& node)
{
    switch (node.kind()) {
    case WGSL::AST::NodeKind::ArrayTypeName:
    case WGSL::AST::NodeKind::NamedTypeName:
    case WGSL::AST::NodeKind::ParameterizedTypeName:
    case WGSL::AST::NodeKind::StructTypeName:
    case WGSL::AST::NodeKind::ReferenceTypeName:
        return true;
    default:
        return false;
    }
};
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_WGSL_AST(ArrayTypeName)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(NamedTypeName)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(ParameterizedTypeName)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(StructTypeName)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(ReferenceTypeName)
