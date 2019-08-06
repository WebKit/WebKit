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

Expected<void, Error> synthesizeStructureAccessors(Program& program)
{
    bool isOperator = true;
    for (auto& structureDefinition : program.structureDefinitions()) {
        for (auto& structureElement : structureDefinition->structureElements()) {
            // The ander: operator&.field
            auto createAnder = [&](AST::AddressSpace addressSpace) -> AST::NativeFunctionDeclaration {
                auto argumentType = AST::PointerType::create(structureElement.codeLocation(), addressSpace, AST::TypeReference::wrap(structureElement.codeLocation(), structureDefinition));
                auto variableDeclaration = makeUniqueRef<AST::VariableDeclaration>(structureElement.codeLocation(), AST::Qualifiers(), WTFMove(argumentType), String(), nullptr, nullptr);
                AST::VariableDeclarations parameters;
                parameters.append(WTFMove(variableDeclaration));
                auto returnType = AST::PointerType::create(structureElement.codeLocation(), addressSpace, structureElement.type());
                AST::NativeFunctionDeclaration nativeFunctionDeclaration(AST::FunctionDeclaration(structureElement.codeLocation(), AST::AttributeBlock(), WTF::nullopt, WTFMove(returnType), makeString("operator&.", structureElement.name()), WTFMove(parameters), nullptr, isOperator, ParsingMode::StandardLibrary));
                return nativeFunctionDeclaration;
            };
            if (!program.append(createAnder(AST::AddressSpace::Constant))
                || !program.append(createAnder(AST::AddressSpace::Device))
                || !program.append(createAnder(AST::AddressSpace::Threadgroup))
                || !program.append(createAnder(AST::AddressSpace::Thread)))
                return makeUnexpected(Error("Can not create ander"));
        }
    }
    return { };
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
