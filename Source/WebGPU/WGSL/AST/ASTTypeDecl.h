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

#include <wtf/RefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/text/WTFString.h>

namespace WGSL {
class ResolveTypeReferences;

namespace AST {

class StructDecl;

class TypeDecl : public Node, public RefCounted<TypeDecl> {
    WTF_MAKE_FAST_ALLOCATED;

public:
    TypeDecl(SourceSpan span)
        : Node(span)
    {
    }
};

class ArrayType final : public TypeDecl {
    WTF_MAKE_FAST_ALLOCATED;

public:
    ArrayType(SourceSpan span, RefPtr<TypeDecl>&& elementType, std::unique_ptr<Expression>&& elementCount)
        : TypeDecl(span)
        , m_elementType(WTFMove(elementType))
        , m_elementCount(WTFMove(elementCount))
    {
    }

    Kind kind() const override;
    TypeDecl* maybeElementType() const { return m_elementType.get(); }
    Expression* maybeElementCount() const { return m_elementCount.get(); }

private:
    RefPtr<TypeDecl> m_elementType;
    std::unique_ptr<Expression> m_elementCount;
};

class NamedType final : public TypeDecl {
    WTF_MAKE_FAST_ALLOCATED;

    friend class ::WGSL::ResolveTypeReferences;
public:
    NamedType(SourceSpan span, const String& name)
        : TypeDecl(span)
        , m_name(name)
    {
    }

    Kind kind() const override;
    const String& name() const { return m_name; }
    TypeDecl* maybeResolvedReference() const { return m_resolvedReference.get(); }

private:
    void resolveTypeReference(Ref<TypeDecl>&& typeDecl)
    {
        m_resolvedReference = WTFMove(typeDecl);
    }

    String m_name;
    RefPtr<TypeDecl> m_resolvedReference;
};

class ParameterizedType : public TypeDecl {
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

    ParameterizedType(SourceSpan span, Base base, Ref<TypeDecl>&& elementType)
        : TypeDecl(span)
        , m_base(base)
        , m_elementType(WTFMove(elementType))
    {
    }

    static std::optional<Base> stringToKind(String& view)
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

    Kind kind() const override;
    Base base() const { return m_base; }
    TypeDecl& elementType() { return m_elementType; }

private:
    Base m_base;
    Ref<TypeDecl> m_elementType;
};

class StructType final : public TypeDecl {
    WTF_MAKE_FAST_ALLOCATED;

public:
    StructType(SourceSpan span, StructDecl& structDecl)
        : TypeDecl(span)
        , m_structDecl(structDecl)
    {
    }

    Kind kind() const override;
    StructDecl& structDecl() const { return m_structDecl; }

private:
    StructDecl& m_structDecl;
};

class ReferenceType final : public TypeDecl {
    WTF_MAKE_FAST_ALLOCATED;

public:
    ReferenceType(SourceSpan span, Ref<TypeDecl>&& type)
        : TypeDecl(span)
        , m_type(WTFMove(type))
    {
    }

    Kind kind() const override;
    TypeDecl& type() const { return m_type.get(); }

private:
    Ref<TypeDecl> m_type;
};

} // namespace AST
} // namespace WGSL

#define SPECIALIZE_TYPE_TRAITS_WGSL_TYPE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::AST::ToValueTypeName) \
    static bool isType(const WGSL::AST::TypeDecl& type) { return type.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::AST::TypeDecl)
static bool isType(const WGSL::AST::Node& node)
{
    switch (node.kind()) {
    case WGSL::AST::Node::Kind::ArrayType:
    case WGSL::AST::Node::Kind::NamedType:
    case WGSL::AST::Node::Kind::ParameterizedType:
    case WGSL::AST::Node::Kind::StructType:
    case WGSL::AST::Node::Kind::ReferenceType:
        return true;
    default:
        return false;
    }
}
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_WGSL_AST(ArrayType)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(NamedType)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(ParameterizedType)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(StructType)
SPECIALIZE_TYPE_TRAITS_WGSL_AST(ReferenceType)
