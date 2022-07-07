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

#include "ASTNode.h"
#include "Expression.h"
#include <wtf/TypeCasts.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/StringView.h>

namespace WGSL::AST {

class TypeDecl : public ASTNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Kind : uint8_t {
        Named,
        Array,
        Vec,
    };

    TypeDecl(SourceSpan span, Kind kind)
        : ASTNode(span)
        , m_kind(kind)
    {
    }

    virtual ~TypeDecl() { }

    Kind kind() const { return m_kind; }

    bool isNamed() const { return m_kind == Kind::Named; }
    bool isArray() const { return m_kind == Kind::Array; }
    bool isVec() const { return m_kind == Kind::Vec; }

private:
    Kind m_kind;
};

// NamedType represents types that take no parameters
// (like u32, i32, f32, f16, bool), and identifier types that
// refers to a type alias or struct name.
class NamedType final : public TypeDecl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    NamedType(SourceSpan span, StringView&& name)
        : TypeDecl(span, TypeDecl::Kind::Named)
        , m_name(WTFMove(name))
    {
    }

    const StringView& name() const { return m_name; }

private:
    StringView m_name;
};

// VecType represents a vec2/vec3/vec4 vector of elements.
// https://gpuweb.github.io/gpuweb/wgsl/#vector-types
class VecType : public TypeDecl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    VecType(SourceSpan span, UniqueRef<TypeDecl> elementType, uint8_t size)
        : TypeDecl(span, TypeDecl::Kind::Vec)
        , m_elementType(WTFMove(elementType))
        , m_size(size)
    {
    }

    const TypeDecl& elementType() const { return m_elementType; }
    uint8_t size() const { return m_size; }

private:
    UniqueRef<TypeDecl> m_elementType;
    uint8_t m_size;
};

// ArrayType represents an array of elements. An array can be fixed-sized if the
// size is specified as an expression, or runtime-sized otherwise.
// https://gpuweb.github.io/gpuweb/wgsl/#array-types
class ArrayType: public TypeDecl {
    WTF_MAKE_FAST_ALLOCATED;
public:
    // Constructor for runtime-sized array without size expression.
    ArrayType(SourceSpan span, UniqueRef<TypeDecl> elementType)
        : TypeDecl(span, TypeDecl::Kind::Array)
        , m_elementType(WTFMove(elementType))
        , m_sizeExpression()
    {
    }

    // Constructor for fixed-sized array with size expression.
    ArrayType(SourceSpan span, UniqueRef<TypeDecl> elementType, UniqueRef<Expression> sizeExpression)
        : TypeDecl(span, TypeDecl::Kind::Array)
        , m_elementType(WTFMove(elementType))
        , m_sizeExpression(sizeExpression.moveToUniquePtr())
    {
    }

    const TypeDecl& elementType() const { return m_elementType; }
    const Expression* maybeSizeExpression() const { return m_sizeExpression.get(); }

private:
    UniqueRef<TypeDecl> m_elementType;
    std::unique_ptr<Expression> m_sizeExpression;
};

} // namespace WGSL::AST

#define SPECIALIZE_TYPE_TRAITS_WGSL_TYPE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WGSL::AST::ToValueTypeName) \
    static bool isType(const WGSL::AST::TypeDecl& type) { return type.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_WGSL_TYPE(NamedType, isNamed())
SPECIALIZE_TYPE_TRAITS_WGSL_TYPE(ArrayType, isArray())
SPECIALIZE_TYPE_TRAITS_WGSL_TYPE(VecType, isVec())
