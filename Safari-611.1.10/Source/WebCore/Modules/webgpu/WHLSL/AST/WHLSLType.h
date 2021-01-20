/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLDefaultDelete.h"
#include <wtf/FastMalloc.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class Type {
    WTF_MAKE_FAST_ALLOCATED;

protected:
    ~Type() = default;

public:
    enum class Kind : uint8_t {
        // UnnamedTypes
        TypeReference,
        Pointer,
        ArrayReference,
        Array,
        // NamedTypes
        TypeDefinition,
        StructureDefinition,
        EnumerationDefinition,
        NativeTypeDeclaration,
        // ResolvableTypes
        FloatLiteral,
        IntegerLiteral,
        UnsignedIntegerLiteral,
    };

    Type(Kind kind)
        : m_kind(kind)
    { }
    static void destroy(Type&);
    static void destruct(Type&);

    explicit Type(const Type&) = delete;
    Type(Type&&) = default;

    Type& operator=(const Type&) = delete;
    Type& operator=(Type&&) = default;

    Kind kind() const { return m_kind; }

    bool isUnnamedType() const { return kind() >= Kind::TypeReference && kind() <= Kind::Array; }
    bool isNamedType() const { return kind() >= Kind::TypeDefinition && kind() <= Kind::NativeTypeDeclaration; }
    bool isResolvableType() const { return kind() >= Kind::FloatLiteral && kind() <= Kind::UnsignedIntegerLiteral; }

    bool isTypeReference() const { return kind() == Kind::TypeReference; }
    bool isPointerType() const { return kind() == Kind::Pointer; }
    bool isArrayReferenceType() const { return kind() == Kind::ArrayReference; }
    bool isArrayType() const { return kind() == Kind::Array; }
    bool isReferenceType() const { return isPointerType() || isArrayReferenceType(); }

    bool isTypeDefinition() const { return kind() == Kind::TypeDefinition; }
    bool isStructureDefinition() const { return kind() == Kind::StructureDefinition; }
    bool isEnumerationDefinition() const { return kind() == Kind::EnumerationDefinition; }
    bool isNativeTypeDeclaration() const { return kind() == Kind::NativeTypeDeclaration; }

    bool isFloatLiteralType() const { return kind() == Kind::FloatLiteral; }
    bool isIntegerLiteralType() const { return kind() == Kind::IntegerLiteral; }
    bool isUnsignedIntegerLiteralType() const { return kind() == Kind::UnsignedIntegerLiteral; }

    Type& unifyNode();
    const Type& unifyNode() const
    {
        return const_cast<Type*>(this)->unifyNode();
    }

private:
    Kind m_kind;
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(Type)

#define SPECIALIZE_TYPE_TRAITS_WHLSL_TYPE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WHLSL::AST::ToValueTypeName) \
    static bool isType(const WebCore::WHLSL::AST::Type& type) { return type.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WHLSL_COMPILER)
