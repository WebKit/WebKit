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
#include "WHLSLNameContext.h"

#if ENABLE(WEBGPU)

#include "WHLSLEnumerationDefinition.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLNativeFunctionDeclaration.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLStructureDefinition.h"
#include "WHLSLTypeDefinition.h"
#include "WHLSLVariableDeclaration.h"

namespace WebCore {

namespace WHLSL {

NameContext::NameContext(NameContext* parent)
    : m_parent(parent)
{
}

bool NameContext::add(AST::TypeDefinition& typeDefinition)
{
    if (exists(typeDefinition.name()))
        return false;
    auto result = m_types.add(typeDefinition.name(), Vector<std::reference_wrapper<AST::NamedType>, 1>());
    if (!result.isNewEntry)
        return false;
    result.iterator->value.append(typeDefinition);
    return true;
}

bool NameContext::add(AST::StructureDefinition& structureDefinition)
{
    if (exists(structureDefinition.name()))
        return false;
    auto result = m_types.add(structureDefinition.name(), Vector<std::reference_wrapper<AST::NamedType>, 1>());
    if (!result.isNewEntry)
        return false;
    result.iterator->value.append(structureDefinition);
    return true;
}

bool NameContext::add(AST::EnumerationDefinition& enumerationDefinition)
{
    if (exists(enumerationDefinition.name()))
        return false;
    auto result = m_types.add(enumerationDefinition.name(), Vector<std::reference_wrapper<AST::NamedType>, 1>());
    if (!result.isNewEntry)
        return false;
    result.iterator->value.append(enumerationDefinition);
    return true;
}

bool NameContext::add(AST::FunctionDefinition& functionDefinition)
{
    if (m_types.find(functionDefinition.name()) != m_types.end()
        || m_variables.find(functionDefinition.name()) != m_variables.end())
        return false;
    auto result = m_functions.add(functionDefinition.name(), Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>());
    result.iterator->value.append(functionDefinition);
    return true;
}

bool NameContext::add(AST::NativeFunctionDeclaration& nativeFunctionDeclaration)
{
    if (m_types.find(nativeFunctionDeclaration.name()) != m_types.end()
        || m_variables.find(nativeFunctionDeclaration.name()) != m_variables.end())
        return false;
    auto result = m_functions.add(nativeFunctionDeclaration.name(), Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>());
    result.iterator->value.append(nativeFunctionDeclaration);
    return true;
}

bool NameContext::add(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    if (m_functions.find(nativeTypeDeclaration.name()) != m_functions.end()
        || m_variables.find(nativeTypeDeclaration.name()) != m_variables.end())
        return false;
    auto result = m_types.add(nativeTypeDeclaration.name(), Vector<std::reference_wrapper<AST::NamedType>, 1>());
    result.iterator->value.append(nativeTypeDeclaration);
    return true;
}

bool NameContext::add(AST::VariableDeclaration& variableDeclaration)
{
    if (exists(variableDeclaration.name()))
        return false;
    auto result = m_variables.add(String(variableDeclaration.name()), &variableDeclaration);
    return result.isNewEntry;
}

Vector<std::reference_wrapper<AST::NamedType>, 1>* NameContext::getTypes(const String& name)
{
    auto iterator = m_types.find(name);
    if (iterator == m_types.end()) {
        if (m_parent)
            return m_parent->getTypes(name);
        return nullptr;
    }
    return &iterator->value;
}

Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>* NameContext::getFunctions(const String& name)
{
    auto iterator = m_functions.find(name);
    if (iterator == m_functions.end()) {
        if (m_parent)
            return m_parent->getFunctions(name);
        return nullptr;
    }
    return &iterator->value;
}

AST::VariableDeclaration* NameContext::getVariable(const String& name)
{
    auto iterator = m_variables.find(name);
    if (iterator == m_variables.end()) {
        if (m_parent)
            return m_parent->getVariable(name);
        return nullptr;
    }
    return iterator->value;
}

bool NameContext::exists(String& name)
{
    return m_types.find(name) != m_types.end()
        || m_functions.find(name) != m_functions.end()
        || m_variables.find(name) != m_variables.end();
}

}

}

#endif
