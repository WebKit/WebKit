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
#include "WHLSLInferTypes.h"

#if ENABLE(WEBGPU)

#include "WHLSLArrayReferenceType.h"
#include "WHLSLArrayType.h"
#include "WHLSLEnumerationDefinition.h"
#include "WHLSLFunctionDeclaration.h"
#include "WHLSLNamedType.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLPointerType.h"
#include "WHLSLResolvableType.h"
#include "WHLSLStructureDefinition.h"
#include "WHLSLTypeDefinition.h"
#include "WHLSLTypeReference.h"

namespace WebCore {

namespace WHLSL {

static bool matches(const AST::Type& unifyThis, const AST::Type& unifyOther)
{
    if (&unifyThis == &unifyOther)
        return true;

    if (is<AST::NamedType>(unifyThis) && is<AST::NamedType>(unifyOther)) {
#if ASSERT_ENABLED
        auto& namedThis = downcast<AST::NamedType>(unifyThis);
        auto& namedOther = downcast<AST::NamedType>(unifyOther);
        ASSERT(!is<AST::TypeDefinition>(namedThis) && !is<AST::TypeDefinition>(namedOther));
#endif
        return false;
    }
    if (is<AST::UnnamedType>(unifyThis) && is<AST::UnnamedType>(unifyOther)) {
        auto& unnamedThis = downcast<AST::UnnamedType>(unifyThis);
        auto& unnamedOther = downcast<AST::UnnamedType>(unifyOther);
        ASSERT(!is<AST::TypeReference>(unnamedThis) && !is<AST::TypeReference>(unnamedOther));
        return unnamedThis == unnamedOther;
    }
    return false;
}

bool matches(const AST::UnnamedType& unnamedType, const AST::UnnamedType& other)
{
    return matches(unnamedType.unifyNode(), other.unifyNode());
}

bool matches(const AST::NamedType& namedType, const AST::NamedType& other)
{
    return matches(namedType.unifyNode(), other.unifyNode());
}

bool matches(const AST::UnnamedType& unnamedType, const AST::NamedType& other)
{
    return matches(unnamedType.unifyNode(), other.unifyNode());
}

static RefPtr<AST::UnnamedType> matchAndCommit(AST::Type& unifyNode, AST::ResolvableType& resolvableType)
{
    ASSERT(!resolvableType.maybeResolvedType());
    if (!resolvableType.canResolve(unifyNode))
        return nullptr;
    if (is<AST::NamedType>(unifyNode)) {
        auto& namedUnifyNode = downcast<AST::NamedType>(unifyNode);
        auto result = AST::TypeReference::wrap(namedUnifyNode.codeLocation(), namedUnifyNode);
        resolvableType.resolve(result.copyRef());
        return { WTFMove(result) };
    }

    Ref<AST::UnnamedType> result = downcast<AST::UnnamedType>(unifyNode);
    resolvableType.resolve(result.copyRef());
    return result;
}

RefPtr<AST::UnnamedType> matchAndCommit(AST::UnnamedType& unnamedType, AST::ResolvableType& resolvableType)
{
    return matchAndCommit(unnamedType.unifyNode(), resolvableType);
}

RefPtr<AST::UnnamedType> matchAndCommit(AST::NamedType& namedType, AST::ResolvableType& resolvableType)
{
    return matchAndCommit(namedType.unifyNode(), resolvableType);
}

RefPtr<AST::UnnamedType> matchAndCommit(AST::ResolvableType& resolvableType1, AST::ResolvableType& resolvableType2)
{
    ASSERT(!resolvableType1.maybeResolvedType());
    ASSERT(!resolvableType2.maybeResolvedType());
    if (is<AST::FloatLiteralType>(resolvableType1) && is<AST::FloatLiteralType>(resolvableType2)) {
        resolvableType1.resolve(downcast<AST::FloatLiteralType>(resolvableType1).preferredType());
        resolvableType2.resolve(downcast<AST::FloatLiteralType>(resolvableType2).preferredType());
        return &downcast<AST::FloatLiteralType>(resolvableType1).preferredType();
    }
    if (is<AST::IntegerLiteralType>(resolvableType1) && is<AST::IntegerLiteralType>(resolvableType2)) {
        resolvableType1.resolve(downcast<AST::IntegerLiteralType>(resolvableType1).preferredType());
        resolvableType2.resolve(downcast<AST::IntegerLiteralType>(resolvableType2).preferredType());
        return &downcast<AST::IntegerLiteralType>(resolvableType1).preferredType();
    }
    if (is<AST::UnsignedIntegerLiteralType>(resolvableType1) && is<AST::UnsignedIntegerLiteralType>(resolvableType2)) {
        resolvableType1.resolve(downcast<AST::UnsignedIntegerLiteralType>(resolvableType1).preferredType());
        resolvableType2.resolve(downcast<AST::UnsignedIntegerLiteralType>(resolvableType2).preferredType());
        return &downcast<AST::UnsignedIntegerLiteralType>(resolvableType1).preferredType();
    }
    return nullptr;
}

RefPtr<AST::UnnamedType> commit(AST::ResolvableType& resolvableType)
{
    ASSERT(!resolvableType.maybeResolvedType());
    if (is<AST::FloatLiteralType>(resolvableType)) {
        auto& floatLiteralType = downcast<AST::FloatLiteralType>(resolvableType);
        resolvableType.resolve(floatLiteralType.preferredType());
        return &floatLiteralType.preferredType();
    }
    if (is<AST::IntegerLiteralType>(resolvableType)) {
        auto& integerLiteralType = downcast<AST::IntegerLiteralType>(resolvableType);
        resolvableType.resolve(integerLiteralType.preferredType());
        return &integerLiteralType.preferredType();
    }
    if (is<AST::UnsignedIntegerLiteralType>(resolvableType)) {
        auto& unsignedIntegerLiteralType = downcast<AST::UnsignedIntegerLiteralType>(resolvableType);
        resolvableType.resolve(unsignedIntegerLiteralType.preferredType());
        return &unsignedIntegerLiteralType.preferredType();
    }
    return nullptr;
}

bool inferTypesForTypeArguments(AST::NamedType& possibleType, AST::TypeArguments& typeArguments)
{
    if (is<AST::TypeDefinition>(possibleType)
        || is<AST::StructureDefinition>(possibleType)
        || is<AST::EnumerationDefinition>(possibleType)) {
        return typeArguments.isEmpty();
    }

    auto& nativeTypeDeclaration = downcast<AST::NativeTypeDeclaration>(possibleType);
    if (nativeTypeDeclaration.typeArguments().size() != typeArguments.size())
        return false;
    for (size_t i = 0; i < nativeTypeDeclaration.typeArguments().size(); ++i) {
        AST::ConstantExpression* typeArgumentExpression = nullptr;
        AST::TypeReference* typeArgumentTypeReference = nullptr;
        AST::ConstantExpression* nativeTypeArgumentExpression = nullptr;
        AST::TypeReference* nativeTypeArgumentTypeReference = nullptr;

        auto assign = [&](AST::TypeArgument& typeArgument, AST::ConstantExpression*& expression, AST::TypeReference*& typeReference) {
            WTF::visit(WTF::makeVisitor([&](AST::ConstantExpression& constantExpression) {
                expression = &constantExpression;
            }, [&](Ref<AST::TypeReference>& theTypeReference) {
                typeReference = theTypeReference.ptr();
            }), typeArgument);
        };

        assign(typeArguments[i], typeArgumentExpression, typeArgumentTypeReference);
        assign(nativeTypeDeclaration.typeArguments()[i], nativeTypeArgumentExpression, nativeTypeArgumentTypeReference);

        if (typeArgumentExpression && nativeTypeArgumentExpression) {
            if (!typeArgumentExpression->matches(*nativeTypeArgumentExpression))
                return false;
        } else if (typeArgumentTypeReference && nativeTypeArgumentTypeReference) {
            if (!matches(*typeArgumentTypeReference, *nativeTypeArgumentTypeReference))
                return false;
        }
    }

    return true;
}

template <typename TypeKind>
ALWAYS_INLINE bool inferTypesForCallImpl(AST::FunctionDeclaration& possibleFunction, Vector<std::reference_wrapper<ResolvingType>>& argumentTypes, const TypeKind* castReturnType)
{
    if (possibleFunction.parameters().size() != argumentTypes.size())
        return false;
    for (size_t i = 0; i < possibleFunction.parameters().size(); ++i) {
        auto success = argumentTypes[i].get().visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>& unnamedType) -> bool {
            return matches(*possibleFunction.parameters()[i]->type(), unnamedType);
        }, [&](RefPtr<ResolvableTypeReference>& resolvableTypeReference) -> bool {
            return resolvableTypeReference->resolvableType().canResolve(possibleFunction.parameters()[i]->type()->unifyNode());
        }));
        if (!success)
            return false;
    }
    if (castReturnType && !matches(possibleFunction.type(), *castReturnType))
        return false;
    return true;
}

bool inferTypesForCall(AST::FunctionDeclaration& possibleFunction, Vector<std::reference_wrapper<ResolvingType>>& argumentTypes, const AST::NamedType* castReturnType)
{
    return inferTypesForCallImpl(possibleFunction, argumentTypes, castReturnType);
}

bool inferTypesForCall(AST::FunctionDeclaration& possibleFunction, Vector<std::reference_wrapper<ResolvingType>>& argumentTypes, const AST::UnnamedType* castReturnType)
{
    return inferTypesForCallImpl(possibleFunction, argumentTypes, castReturnType);
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
