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
#include "WHLSLSynthesizeStructureAccessors.h"

#if ENABLE(WEBGPU)

#include "WHLSLAddressSpace.h"
#include "WHLSLPointerType.h"
#include "WHLSLProgram.h"
#include "WHLSLReferenceType.h"
#include "WHLSLTypeReference.h"
#include "WHLSLVariableDeclaration.h"

namespace WebCore {

namespace WHLSL {

bool synthesizeStructureAccessors(Program& program)
{
    bool isOperator = true;
    for (auto& structureDefinition : program.structureDefinitions()) {
        for (auto& structureElement : structureDefinition->structureElements()) {
            // The ander: operator&.field
            auto createAnder = [&](AST::AddressSpace addressSpace) -> AST::NativeFunctionDeclaration {
                auto argumentType = makeUniqueRef<AST::PointerType>(Lexer::Token(structureElement.origin()), addressSpace, AST::TypeReference::wrap(Lexer::Token(structureElement.origin()), structureDefinition));
                auto variableDeclaration = makeUniqueRef<AST::VariableDeclaration>(Lexer::Token(structureElement.origin()), AST::Qualifiers(), UniqueRef<AST::UnnamedType>(WTFMove(argumentType)), String(), WTF::nullopt, WTF::nullopt);
                AST::VariableDeclarations parameters;
                parameters.append(WTFMove(variableDeclaration));
                auto returnType = makeUniqueRef<AST::PointerType>(Lexer::Token(structureElement.origin()), addressSpace, structureElement.type().clone());
                AST::NativeFunctionDeclaration nativeFunctionDeclaration(AST::FunctionDeclaration(Lexer::Token(structureElement.origin()), AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), makeString("operator&.", structureElement.name()), WTFMove(parameters), WTF::nullopt, isOperator));
                return nativeFunctionDeclaration;
            };
            if (!program.append(createAnder(AST::AddressSpace::Constant))
                || !program.append(createAnder(AST::AddressSpace::Device))
                || !program.append(createAnder(AST::AddressSpace::Threadgroup))
                || !program.append(createAnder(AST::AddressSpace::Thread)))
                return false;
        }
    }
    return true;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
