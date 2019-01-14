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

void synthesizeStructureAccessors(Program& program)
{
    bool isOperator = true;
    bool isRestricted = false;
    for (auto& structureDefinition : program.structureDefinitions()) {
        for (auto& structureElement : structureDefinition->structureElements()) {
            {
                // The getter: operator.field
                AST::VariableDeclaration variableDeclaration(Lexer::Token(structureElement.origin()), AST::Qualifiers(), { AST::TypeReference::wrap(Lexer::Token(structureElement.origin()), structureDefinition) }, String(), WTF::nullopt, WTF::nullopt);
                AST::VariableDeclarations parameters;
                parameters.append(WTFMove(variableDeclaration));
                AST::NativeFunctionDeclaration nativeFunctionDeclaration(AST::FunctionDeclaration(Lexer::Token(structureElement.origin()), AST::AttributeBlock(), WTF::nullopt, structureElement.type().clone(), String::format("operator.%s", structureElement.name().utf8().data()), WTFMove(parameters), WTF::nullopt, isOperator), isRestricted);
                program.append(WTFMove(nativeFunctionDeclaration));
            }

            {
                // The setter: operator.field=
                AST::VariableDeclaration variableDeclaration1(Lexer::Token(structureElement.origin()), AST::Qualifiers(), { AST::TypeReference::wrap(Lexer::Token(structureElement.origin()), structureDefinition) }, String(), WTF::nullopt, WTF::nullopt);
                AST::VariableDeclaration variableDeclaration2(Lexer::Token(structureElement.origin()), AST::Qualifiers(), { structureElement.type().clone() }, String(), WTF::nullopt, WTF::nullopt);
                AST::VariableDeclarations parameters;
                parameters.append(WTFMove(variableDeclaration1));
                parameters.append(WTFMove(variableDeclaration2));
                AST::NativeFunctionDeclaration nativeFunctionDeclaration(AST::FunctionDeclaration(Lexer::Token(structureElement.origin()), AST::AttributeBlock(), WTF::nullopt, AST::TypeReference::wrap(Lexer::Token(structureElement.origin()), structureDefinition), String::format("operator.%s=", structureElement.name().utf8().data()), WTFMove(parameters), WTF::nullopt, isOperator), isRestricted);
                program.append(WTFMove(nativeFunctionDeclaration));
            }

            // The ander: operator&.field
            auto createAnder = [&](AST::AddressSpace addressSpace) -> AST::NativeFunctionDeclaration {
                auto argumentType = makeUniqueRef<AST::PointerType>(Lexer::Token(structureElement.origin()), addressSpace, AST::TypeReference::wrap(Lexer::Token(structureElement.origin()), structureDefinition));
                AST::VariableDeclaration variableDeclaration(Lexer::Token(structureElement.origin()), AST::Qualifiers(), { WTFMove(argumentType) }, String(), WTF::nullopt, WTF::nullopt);
                AST::VariableDeclarations parameters;
                parameters.append(WTFMove(variableDeclaration));
                auto returnType = makeUniqueRef<AST::PointerType>(Lexer::Token(structureElement.origin()), addressSpace, structureElement.type().clone());
                AST::NativeFunctionDeclaration nativeFunctionDeclaration(AST::FunctionDeclaration(Lexer::Token(structureElement.origin()), AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), String::format("operator&.%s", structureElement.name().utf8().data()), WTFMove(parameters), WTF::nullopt, isOperator), isRestricted);
                return nativeFunctionDeclaration;
            };
            program.append(createAnder(AST::AddressSpace::Constant));
            program.append(createAnder(AST::AddressSpace::Device));
            program.append(createAnder(AST::AddressSpace::Threadgroup));
            program.append(createAnder(AST::AddressSpace::Thread));
        }
    }
}

}

}

#endif
