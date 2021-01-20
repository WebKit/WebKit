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

#if ENABLE(WHLSL_COMPILER)

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

Expected<void, Error> NameContext::add(AST::TypeDefinition& typeDefinition)
{
    if (auto existing = topLevelExists(typeDefinition.name()))
        return makeUnexpected(Error("Duplicate name in program", *existing));
    typeDefinition.setNameSpace(m_currentNameSpace);
    auto index = static_cast<unsigned>(m_currentNameSpace);
    auto result = m_types[index].add(typeDefinition.name(), Vector<std::reference_wrapper<AST::NamedType>, 1>());
    ASSERT(result.isNewEntry);
    result.iterator->value.append(typeDefinition);
    return { };
}

Expected<void, Error> NameContext::add(AST::StructureDefinition& structureDefinition)
{
    if (auto existing = topLevelExists(structureDefinition.name()))
        return makeUnexpected(Error("Duplicate name in program.", *existing));
    structureDefinition.setNameSpace(m_currentNameSpace);
    auto index = static_cast<unsigned>(m_currentNameSpace);
    auto result = m_types[index].add(structureDefinition.name(), Vector<std::reference_wrapper<AST::NamedType>, 1>());
    ASSERT(result.isNewEntry);
    result.iterator->value.append(structureDefinition);
    return { };
}

Expected<void, Error> NameContext::add(AST::EnumerationDefinition& enumerationDefinition)
{
    if (auto existing = topLevelExists(enumerationDefinition.name()))
        return makeUnexpected(Error("Duplicate name in program.", *existing));
    enumerationDefinition.setNameSpace(m_currentNameSpace);
    auto index = static_cast<unsigned>(m_currentNameSpace);
    auto result = m_types[index].add(enumerationDefinition.name(), Vector<std::reference_wrapper<AST::NamedType>, 1>());
    ASSERT(result.isNewEntry);
    result.iterator->value.append(enumerationDefinition);
    return { };
}

Expected<void, Error> NameContext::add(AST::FunctionDefinition& functionDefinition)
{
    auto index = static_cast<unsigned>(m_currentNameSpace);
    if (auto* type = searchTypes(functionDefinition.name()))
        return makeUnexpected(Error("Duplicate name in program.", type->codeLocation()));
    functionDefinition.setNameSpace(m_currentNameSpace);
    auto result = m_functions[index].add(functionDefinition.name(), Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>());
    result.iterator->value.append(functionDefinition);
    return { };
}

Expected<void, Error> NameContext::add(AST::NativeFunctionDeclaration& nativeFunctionDeclaration)
{
    auto index = static_cast<unsigned>(m_currentNameSpace);
    if (auto* type = searchTypes(nativeFunctionDeclaration.name()))
        return makeUnexpected(Error("Duplicate name in program.", type->codeLocation()));
    nativeFunctionDeclaration.setNameSpace(m_currentNameSpace);
    auto result = m_functions[index].add(nativeFunctionDeclaration.name(), Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>());
    result.iterator->value.append(nativeFunctionDeclaration);
    return { };
}

Expected<void, Error> NameContext::add(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    auto index = static_cast<unsigned>(m_currentNameSpace);
    if (auto* function = searchFunctions(nativeTypeDeclaration.name()))
        return makeUnexpected(Error("Duplicate name in program.", function->codeLocation()));
    nativeTypeDeclaration.setNameSpace(m_currentNameSpace);
    auto result = m_types[index].add(nativeTypeDeclaration.name(), Vector<std::reference_wrapper<AST::NamedType>, 1>());
    result.iterator->value.append(nativeTypeDeclaration);
    return { };
}

Expected<void, Error> NameContext::add(AST::VariableDeclaration& variableDeclaration)
{
    if (variableDeclaration.name().isNull())
        return { };
    if (auto* declaration = localExists(variableDeclaration.name()))
        return makeUnexpected(Error("Duplicate name in program.", declaration->codeLocation()));
    auto result = m_variables.add(String(variableDeclaration.name()), &variableDeclaration);
    ASSERT_UNUSED(result, result.isNewEntry);
    return { };
}

Vector<std::reference_wrapper<AST::NamedType>, 1> NameContext::getTypes(const String& name, AST::NameSpace fromNamespace)
{
    // Named types can only be declared in the global name context.
    if (m_parent)
        return m_parent->getTypes(name, fromNamespace);

    Vector<std::reference_wrapper<AST::NamedType>, 1> result;

    unsigned index = static_cast<unsigned>(fromNamespace);
    auto iterator = m_types[index].find(name);
    if (iterator != m_types[index].end()) {
        for (auto type : iterator->value)
            result.append(type);
    }

    if (fromNamespace != AST::NameSpace::StandardLibrary) {
        index = static_cast<unsigned>(AST::NameSpace::StandardLibrary);
        iterator = m_types[index].find(name);
        if (iterator != m_types[index].end()) {
            for (auto type : iterator->value)
                result.append(type);
        }
    }

    return result;
}

Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1> NameContext::getFunctions(const String& name, AST::NameSpace fromNamespace)
{
    // Functions can only be declared in the global name context.
    if (m_parent)
        return m_parent->getFunctions(name, fromNamespace);

    Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1> result;

    unsigned index = static_cast<unsigned>(fromNamespace);
    auto iterator = m_functions[index].find(name);
    if (iterator != m_functions[index].end()) {
        for (auto type : iterator->value)
            result.append(type);
    }

    if (fromNamespace != AST::NameSpace::StandardLibrary) {
        index = static_cast<unsigned>(AST::NameSpace::StandardLibrary);
        iterator = m_functions[index].find(name);
        if (iterator != m_functions[index].end()) {
            for (auto type : iterator->value)
                result.append(type);
        }
    }

    return result;
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

AST::NamedType* NameContext::searchTypes(String& name) const
{
    ASSERT(!m_parent);
    if (m_currentNameSpace == AST::NameSpace::StandardLibrary) {
        for (auto& types : m_types) {
            auto iter = types.find(name);
            if (iter != types.end())
                return &iter->value[0].get();
        }
        return nullptr;
    }

    auto index = static_cast<unsigned>(m_currentNameSpace);
    auto iter = m_types[index].find(name);
    if (iter != m_types[index].end())
        return &iter->value[0].get();

    index = static_cast<unsigned>(AST::NameSpace::StandardLibrary);
    iter = m_types[index].find(name);
    if (iter != m_types[index].end())
        return &iter->value[0].get();

    return nullptr;
}

AST::FunctionDeclaration* NameContext::searchFunctions(String& name) const
{
    ASSERT(!m_parent);
    if (m_currentNameSpace == AST::NameSpace::StandardLibrary) {
        for (auto& functions : m_functions) {
            auto iter = functions.find(name);
            if (iter != functions.end())
                return &iter->value[0].get();
        }
        return nullptr;
    }

    auto index = static_cast<unsigned>(m_currentNameSpace);
    auto iter = m_functions[index].find(name);
    if (iter != m_functions[index].end())
        return &iter->value[0].get();

    index = static_cast<unsigned>(AST::NameSpace::StandardLibrary);
    iter = m_functions[index].find(name);
    if (iter != m_functions[index].end())
        return &iter->value[0].get();

    return nullptr;
}

Optional<CodeLocation> NameContext::topLevelExists(String& name) const
{
    if (auto* type = searchTypes(name))
        return type->codeLocation();
    if (auto* function = searchFunctions(name))
        return function->codeLocation();
    return WTF::nullopt;
}

AST::VariableDeclaration* NameContext::localExists(String& name) const
{
    ASSERT(m_parent);
    auto iter = m_variables.find(name);
    if (iter != m_variables.end())
        return iter->value;
    return nullptr;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WHLSL_COMPILER)
