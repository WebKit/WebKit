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

#include "ASTBuilder.h"
#include "ASTExpression.h"
#include "ASTIdentifier.h"

namespace WGSL {
class ConstantRewriter;
class EntryPointRewriter;
class RewriteGlobalVariables;
class TypeChecker;
struct Type;

namespace AST {
class Structure;

class TypeName : public Node {
    friend TypeChecker;
    friend ConstantRewriter;
    friend EntryPointRewriter;
    friend RewriteGlobalVariables;

public:
    using Ref = std::reference_wrapper<TypeName>;
    using Ptr = TypeName*;

    const Type* resolvedType() const { return m_resolvedType; }

protected:
    TypeName(SourceSpan span)
        : Node(span)
    { }

private:
    const Type* m_resolvedType { nullptr };
};

class ArrayTypeName : public TypeName {
    WGSL_AST_BUILDER_NODE(ArrayTypeName);

public:
    NodeKind kind() const override;

    TypeName* maybeElementType() const { return m_elementType; }
    Expression* maybeElementCount() const { return m_elementCount; }

private:
    ArrayTypeName(SourceSpan span, TypeName::Ptr elementType, Expression::Ptr elementCount)
        : TypeName(span)
        , m_elementType(elementType)
        , m_elementCount(elementCount)
    { }

    TypeName::Ptr m_elementType;
    Expression::Ptr m_elementCount;
};

class NamedTypeName : public TypeName {
    WGSL_AST_BUILDER_NODE(NamedTypeName);

public:
    NodeKind kind() const override;
    Identifier& name() { return m_name; }

private:
    NamedTypeName(SourceSpan span, Identifier&& name)
        : TypeName(span)
        , m_name(WTFMove(name))
    { }

    Identifier m_name;
};

class ParameterizedTypeName : public TypeName {
    WGSL_AST_BUILDER_NODE(ParameterizedTypeName);

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
        Mat4x4,
        Texture1d,
        Texture2d,
        Texture2dArray,
        Texture3d,
        TextureCube,
        TextureCubeArray,
        TextureMultisampled2d,
        TextureStorage1d,
        TextureStorage2d,
        TextureStorage2dArray,
        TextureStorage3d,

        LastEntry = TextureStorage3d,
    };
    static constexpr unsigned NumberOfBaseTypes = static_cast<unsigned>(Base::LastEntry) + 1;

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
        if (view == "texture_1d"_s)
            return Base::Texture1d;
        if (view == "texture_2d"_s)
            return Base::Texture2d;
        if (view == "texture_2d_array"_s)
            return Base::Texture2dArray;
        if (view == "texture_3d"_s)
            return Base::Texture3d;
        if (view == "texture_cube"_s)
            return Base::TextureCube;
        if (view == "texture_cube_array"_s)
            return Base::TextureCubeArray;
        if (view == "texture_multisampled_2d"_s)
            return Base::TextureMultisampled2d;
        if (view == "texture_storage_1d"_s)
            return Base::TextureStorage1d;
        if (view == "texture_storage_2d"_s)
            return Base::TextureStorage2d;
        if (view == "texture_storage_2d_array"_s)
            return Base::TextureStorage2dArray;
        if (view == "texture_storage_3d"_s)
            return Base::TextureStorage3d;
        return std::nullopt;
    }

    static ASCIILiteral baseToString(Base base)
    {
        switch (base) {
        case Base::Vec2:
            return "vec2"_s;
        case Base::Vec3:
            return "vec3"_s;
        case Base::Vec4:
            return "vec4"_s;
        case Base::Mat2x2:
            return "mat2x2"_s;
        case Base::Mat2x3:
            return "mat2x3"_s;
        case Base::Mat2x4:
            return "mat2x4"_s;
        case Base::Mat3x2:
            return "mat3x2"_s;
        case Base::Mat3x3:
            return "mat3x3"_s;
        case Base::Mat3x4:
            return "mat3x4"_s;
        case Base::Mat4x2:
            return "mat4x2"_s;
        case Base::Mat4x3:
            return "mat4x3"_s;
        case Base::Mat4x4:
            return "mat4x4"_s;
        case Base::Texture1d:
            return "texture_1d"_s;
        case Base::Texture2d:
            return "texture_2d"_s;
        case Base::Texture2dArray:
            return "texture_2d_array"_s;
        case Base::Texture3d:
            return "texture_3d"_s;
        case Base::TextureCube:
            return "texture_cube"_s;
        case Base::TextureCubeArray:
            return "texture_cube_array"_s;
        case Base::TextureMultisampled2d:
            return "texture_multisampled_2d"_s;
        case Base::TextureStorage1d:
            return "texture_storage_1d"_s;
        case Base::TextureStorage2d:
            return "texture_storage_2d"_s;
        case Base::TextureStorage2dArray:
            return "texture_storage_2d_array"_s;
        case Base::TextureStorage3d:
            return "texture_storage_3d"_s;
        }
    }

    NodeKind kind() const override;
    Base base() const { return m_base; }
    TypeName& elementType() { return m_elementType; }

private:
    ParameterizedTypeName(SourceSpan span, Base base, TypeName::Ref&& elementType)
        : TypeName(span)
        , m_base(base)
        , m_elementType(WTFMove(elementType))
    { }

    Base m_base;
    TypeName::Ref m_elementType;
};

class ReferenceTypeName final : public TypeName {
    WGSL_AST_BUILDER_NODE(ReferenceTypeName);

public:
    NodeKind kind() const override;
    TypeName& type() const { return m_type.get(); }

private:
    ReferenceTypeName(SourceSpan span, TypeName::Ref&& type)
        : TypeName(span)
        , m_type(WTFMove(type))
    { }

    TypeName::Ref m_type;
};

} // namespace AST
} // namespace WGSL

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
SPECIALIZE_TYPE_TRAITS_WGSL_AST(ReferenceTypeName)
