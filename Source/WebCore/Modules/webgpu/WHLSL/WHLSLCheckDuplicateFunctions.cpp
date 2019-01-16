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
#include "WHLSLCheckDuplicateFunctions.h"

#if ENABLE(WEBGPU)

#include "WHLSLArrayReferenceType.h"
#include "WHLSLArrayType.h"
#include "WHLSLInferTypes.h"
#include "WHLSLTypeReference.h"

namespace WebCore {

namespace WHLSL {

bool checkDuplicateFunctions(const Program& program)
{
    Vector<std::reference_wrapper<const AST::FunctionDeclaration>> functions;
    for (auto& functionDefinition : program.functionDefinitions())
        functions.append(functionDefinition);
    for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations())
        functions.append(nativeFunctionDeclaration);

    std::sort(functions.begin(), functions.end(), [](const AST::FunctionDeclaration& a, const AST::FunctionDeclaration& b) -> bool {
        if (a.name().length() < b.name().length())
            return true;
        if (a.name().length() > b.name().length())
            return false;
        for (unsigned i = 0; i < a.name().length(); ++i) {
            if (a.name()[i] < b.name()[i])
                return true;
            if (a.name()[i] > b.name()[i])
                return false;
        }
        return false;
    });
    for (size_t i = 0; i < functions.size(); ++i) {
        for (size_t j = i + 1; j < functions.size(); ++i) {
            if (functions[i].get().name() != functions[j].get().name())
                break;
            if (is<AST::NativeFunctionDeclaration>(functions[i].get()) && is<AST::NativeFunctionDeclaration>(functions[j].get()))
                continue;
            if (functions[i].get().parameters().size() != functions[j].get().parameters().size())
                continue;
            if (functions[i].get().isCast() && !matches(functions[i].get().type(), functions[j].get().type()))
                continue;
            bool same = true;
            for (size_t k = 0; k < functions[i].get().parameters().size(); ++k) {
                if (!matches(*functions[i].get().parameters()[k].type(), *functions[j].get().parameters()[k].type())) {
                    same = false;
                    break;
                }
            }
            if (same)
                return false;
        }
        
        if (functions[i].get().name() == "operator&[]" && functions[i].get().parameters().size() == 2
            && is<AST::ArrayReferenceType>(static_cast<const AST::UnnamedType&>(*functions[i].get().parameters()[0].type()))) {
            auto& type = static_cast<const AST::UnnamedType&>(*functions[i].get().parameters()[1].type());
            if (is<AST::TypeReference>(type)) {
                if (auto* resolvedType = downcast<AST::TypeReference>(type).resolvedType()) {
                    if (is<AST::NativeTypeDeclaration>(*resolvedType)) {
                        auto& nativeTypeDeclaration = downcast<AST::NativeTypeDeclaration>(*resolvedType);
                        if (nativeTypeDeclaration.name() == "uint")
                            return false;
                    }
                }
            }
        } else if (functions[i].get().name() == "operator.length" && functions[i].get().parameters().size() == 1
            && (is<AST::ArrayReferenceType>(static_cast<const AST::UnnamedType&>(*functions[i].get().parameters()[0].type()))
            || is<AST::ArrayType>(static_cast<const AST::UnnamedType&>(*functions[i].get().parameters()[0].type()))))
            return false;
        else if (functions[i].get().name() == "operator=="
            && functions[i].get().parameters().size() == 2
            && is<AST::ReferenceType>(static_cast<const AST::UnnamedType&>(*functions[i].get().parameters()[0].type()))
            && is<AST::ReferenceType>(static_cast<const AST::UnnamedType&>(*functions[i].get().parameters()[1].type()))
            && matches(*functions[i].get().parameters()[0].type(), *functions[i].get().parameters()[1].type()))
            return false;
    }
    return true;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
