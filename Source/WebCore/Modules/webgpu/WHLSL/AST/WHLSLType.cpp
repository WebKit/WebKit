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

#include "config.h"
#include "WHLSLType.h"

#if ENABLE(WEBGPU)

#include "WHLSLAST.h"

namespace WebCore {

namespace WHLSL {

namespace AST {

void Type::destroy(Type& type)
{
    switch (type.kind()) {
    case Kind::TypeReference:
        delete &downcast<TypeReference>(type);
        break;
    case Kind::Pointer:
        delete &downcast<PointerType>(type);
        break;
    case Kind::ArrayReference:
        delete &downcast<ArrayReferenceType>(type);
        break;
    case Kind::Array:
        delete &downcast<ArrayType>(type);
        break;

    case Kind::TypeDefinition:
        delete &downcast<TypeDefinition>(type);
        break;
    case Kind::StructureDefinition:
        delete &downcast<StructureDefinition>(type);
        break;
    case Kind::EnumerationDefinition:
        delete &downcast<EnumerationDefinition>(type);
        break;
    case Kind::NativeTypeDeclaration:
        delete &downcast<NativeTypeDeclaration>(type);
        break;

    case Kind::FloatLiteral:
        delete &downcast<FloatLiteralType>(type);
        break;
    case Kind::IntegerLiteral:
        delete &downcast<IntegerLiteralType>(type);
        break;
    case Kind::NullLiteral:
        delete &downcast<NullLiteralType>(type);
        break;
    case Kind::UnsignedIntegerLiteral:
        delete &downcast<UnsignedIntegerLiteralType>(type);
        break;
    }
}

void Type::destruct(Type& type)
{
    switch (type.kind()) {
    case Kind::TypeReference:
        downcast<TypeReference>(type).~TypeReference();
        break;
    case Kind::Pointer:
        downcast<PointerType>(type).~PointerType();
        break;
    case Kind::ArrayReference:
        downcast<ArrayReferenceType>(type).~ArrayReferenceType();
        break;
    case Kind::Array:
        downcast<ArrayType>(type).~ArrayType();
        break;

    case Kind::TypeDefinition:
        downcast<TypeDefinition>(type).~TypeDefinition();
        break;
    case Kind::StructureDefinition:
        downcast<StructureDefinition>(type).~StructureDefinition();
        break;
    case Kind::EnumerationDefinition:
        downcast<EnumerationDefinition>(type).~EnumerationDefinition();
        break;
    case Kind::NativeTypeDeclaration:
        downcast<NativeTypeDeclaration>(type).~NativeTypeDeclaration();
        break;

    case Kind::FloatLiteral:
        downcast<FloatLiteralType>(type).~FloatLiteralType();
        break;
    case Kind::IntegerLiteral:
        downcast<IntegerLiteralType>(type).~IntegerLiteralType();
        break;
    case Kind::NullLiteral:
        downcast<NullLiteralType>(type).~NullLiteralType();
        break;
    case Kind::UnsignedIntegerLiteral:
        downcast<UnsignedIntegerLiteralType>(type).~UnsignedIntegerLiteralType();
        break;
    }
}

Type& Type::unifyNode()
{
    switch (kind()) {
    case Kind::TypeReference:
        return downcast<TypeReference>(*this).unifyNodeImpl();
    case Kind::Pointer:
        return downcast<PointerType>(*this).unifyNodeImpl();
    case Kind::ArrayReference:
        return downcast<ArrayReferenceType>(*this).unifyNodeImpl();
    case Kind::Array:
        return downcast<ArrayType>(*this).unifyNodeImpl();

    case Kind::TypeDefinition:
        return downcast<TypeDefinition>(*this).unifyNodeImpl();
    case Kind::StructureDefinition:
        return downcast<StructureDefinition>(*this).unifyNodeImpl();
    case Kind::EnumerationDefinition:
        return downcast<EnumerationDefinition>(*this).unifyNodeImpl();
    case Kind::NativeTypeDeclaration:
        return downcast<NativeTypeDeclaration>(*this).unifyNodeImpl();

    default: 
        RELEASE_ASSERT_NOT_REACHED();
    }
}

bool ResolvableType::canResolve(const Type& type) const
{
    switch (kind()) {
    case Kind::FloatLiteral:
        return downcast<FloatLiteralType>(*this).canResolve(type);
    case Kind::IntegerLiteral:
        return downcast<IntegerLiteralType>(*this).canResolve(type);
    case Kind::NullLiteral:
        return downcast<NullLiteralType>(*this).canResolve(type);
    case Kind::UnsignedIntegerLiteral:
        return downcast<UnsignedIntegerLiteralType>(*this).canResolve(type);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

unsigned ResolvableType::conversionCost(const UnnamedType& type) const
{
    switch (kind()) {
    case Kind::FloatLiteral:
        return downcast<FloatLiteralType>(*this).conversionCost(type);
    case Kind::IntegerLiteral:
        return downcast<IntegerLiteralType>(*this).conversionCost(type);
    case Kind::NullLiteral:
        return downcast<NullLiteralType>(*this).conversionCost(type);
    case Kind::UnsignedIntegerLiteral:
        return downcast<UnsignedIntegerLiteralType>(*this).conversionCost(type);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

String UnnamedType::toString() const
{
    switch (kind()) {
    case Kind::TypeReference:
        return downcast<TypeReference>(*this).toString();
    case Kind::Pointer:
        return downcast<PointerType>(*this).toString();
    case Kind::ArrayReference:
        return downcast<ArrayReferenceType>(*this).toString();
    case Kind::Array:
        return downcast<ArrayType>(*this).toString();
    default: 
        RELEASE_ASSERT_NOT_REACHED();
    }

}

} // namespace AST

} // namespace WHLSL

} // namespace WebCore

#endif
