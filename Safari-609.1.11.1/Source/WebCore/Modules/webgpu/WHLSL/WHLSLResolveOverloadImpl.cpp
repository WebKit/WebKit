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
#include "WHLSLResolveOverloadImpl.h"

#if ENABLE(WEBGPU)

#include "WHLSLFunctionDeclaration.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLInferTypes.h"
#include <limits>

namespace WebCore {

namespace WHLSL {

static unsigned conversionCost(AST::FunctionDeclaration& candidate, const Vector<std::reference_wrapper<ResolvingType>>& argumentTypes)
{
    unsigned conversionCost = 0;
    for (size_t i = 0; i < candidate.parameters().size(); ++i) {
        conversionCost += argumentTypes[i].get().visit(WTF::makeVisitor([&](Ref<AST::UnnamedType>&) -> unsigned {
            return 0;
        }, [&](RefPtr<ResolvableTypeReference>& resolvableTypeReference) -> unsigned {
            return resolvableTypeReference->resolvableType().conversionCost(*candidate.parameters()[i]->type());
        }));
    }
    // The return type can never be a literal type, so its conversion cost is always 0.
    return conversionCost;
}

template <typename TypeKind>
static AST::FunctionDeclaration* resolveFunctionOverloadImpl(Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>& possibleFunctions, Vector<std::reference_wrapper<ResolvingType>>& argumentTypes, TypeKind* castReturnType, AST::NameSpace nameSpace)
{
    Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1> candidates;
    for (auto& possibleFunction : possibleFunctions) {
        if (possibleFunction.get().entryPointType())
            continue;
        if (nameSpace == AST::NameSpace::StandardLibrary) {
            if (possibleFunction.get().nameSpace() != AST::NameSpace::StandardLibrary)
                continue;
        } else {
            if (possibleFunction.get().nameSpace() != AST::NameSpace::StandardLibrary && possibleFunction.get().nameSpace() != nameSpace)
                continue;
        }
        if (inferTypesForCall(possibleFunction.get(), argumentTypes, castReturnType))
            candidates.append(possibleFunction.get());
    }

    unsigned minimumConversionCost = std::numeric_limits<unsigned>::max();
    for (auto& candidate : candidates)
        minimumConversionCost = std::min(minimumConversionCost, conversionCost(candidate.get(), argumentTypes));

    Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1> minimumCostCandidates;
    for (auto& candidate : candidates) {
        if (conversionCost(candidate.get(), argumentTypes) == minimumConversionCost)
            minimumCostCandidates.append(candidate);
    }

    if (minimumCostCandidates.size() == 1)
        return &minimumCostCandidates[0].get();
    return nullptr;
}

AST::FunctionDeclaration* resolveFunctionOverload(Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>& possibleFunctions, Vector<std::reference_wrapper<ResolvingType>>& argumentTypes, AST::NameSpace nameSpace)
{
    return resolveFunctionOverloadImpl(possibleFunctions, argumentTypes, static_cast<AST::NamedType*>(nullptr), nameSpace);
}

AST::FunctionDeclaration* resolveFunctionOverload(Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>& possibleFunctions, Vector<std::reference_wrapper<ResolvingType>>& argumentTypes, AST::NamedType* castReturnType, AST::NameSpace nameSpace)
{
    return resolveFunctionOverloadImpl(possibleFunctions, argumentTypes, castReturnType, nameSpace);
}

AST::FunctionDeclaration* resolveFunctionOverload(Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>& possibleFunctions, Vector<std::reference_wrapper<ResolvingType>>& argumentTypes, AST::UnnamedType* castReturnType, AST::NameSpace nameSpace)
{
    return resolveFunctionOverloadImpl(possibleFunctions, argumentTypes, castReturnType, nameSpace);
}

AST::NamedType* resolveTypeOverloadImpl(Vector<std::reference_wrapper<AST::NamedType>, 1>& possibleTypes, AST::TypeArguments& typeArguments)
{
    AST::NamedType* result = nullptr;
    for (auto& possibleType : possibleTypes) {
        if (inferTypesForTypeArguments(possibleType, typeArguments)) {
            if (result)
                return nullptr;
            result = &static_cast<AST::NamedType&>(possibleType);
        }
    }

    return result;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
