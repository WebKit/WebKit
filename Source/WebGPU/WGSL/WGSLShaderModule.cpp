/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WGSLShaderModule.h"

#include "AST.h"
#include "ConstantFunctions.h"
#include "WGSL.h"
#include <mutex>
#include <wtf/Borrow.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WGSL {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ShaderModule);

Result<ConstantValue> ShaderModule::ensureOverrideValue(const AST::Expression& expression, const HashMap<String, ConstantValue>& overrideValues) const
{
    auto maybeValue = evaluate(*this, expression, overrideValues);
    if (!maybeValue)
        return makeUnexpected(Error("Failed to evaluate override value"_s, expression.span()));
    return { *maybeValue };
}

std::optional<Error> ShaderModule::validateOverrides(const PrepareResult& prepareResult, HashMap<String, ConstantValue>& overrideValues)
{
    for (auto* variable : borrow(m_overrides)) {
        String name = variable->id().has_value() ? String::number(*variable->id()) : variable->originalName();
        auto userDefinedValue = overrideValues.find(name);
        if (userDefinedValue != overrideValues.end()) {
            auto value = userDefinedValue->value;
            overrideValues.remove(name);
            overrideValues.add(variable->name(), value);
            continue;
        }

        bool isUsed = false;
        for (const auto& [_, entryPoint] : prepareResult.entryPoints) {
            if (entryPoint.specializationConstants.contains(name)) {
                isUsed = true;
                break;
            }
        }

        if (!isUsed)
            continue;

        auto* initializer = variable->maybeInitializer();
        if (!initializer) {
            if (isUsed)
                return { Error(makeString("override "_s, variable->originalName(), " is used in shader but not provided"_s), variable->span()) };
            continue;
        }

        auto maybeValue = ensureOverrideValue(*initializer, overrideValues);
        if (!maybeValue)
            return maybeValue.error();

        overrideValues.add(variable->name(), *maybeValue);
    }

    for (const auto& validator : borrow(m_overrideValidations)) {
        if (auto maybeError = validator(overrideValues))
            return maybeError;
    }

    return std::nullopt;
}

bool ShaderModule::containsOverride(const String& key) const
{
    for (auto* variable : borrow(m_overrides)) {
        String name = variable->id().has_value() ? String::number(*variable->id()) : variable->originalName();
        if (name == key)
            return true;
    }
    return false;
}

static const HashMap<String, OverloadedDeclaration>& sharedOverloadedOperations()
{
    static LazyNeverDestroyed<HashMap<String, OverloadedDeclaration>> s_overloadedOperations;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        HashMap<String, OverloadedDeclaration> m_overloadedOperations;
        // This file contains the overloads generated from `TypeDeclarations.rb`
        #include "TypeOverloads.h" // NOLINT
        auto& builtinTypes = TypeStore::sharedBuiltinTypes();
        for (auto& entry : m_overloadedOperations) {
            (void)entry.key.hash();
            entry.value.overloads = entry.value.buildOverloads(builtinTypes);
        }
        s_overloadedOperations.construct(WTF::move(m_overloadedOperations));
    });
    return s_overloadedOperations.get();
}

const OverloadedDeclaration* ShaderModule::lookupOverload(const String& name) const
{
    const auto& operations = sharedOverloadedOperations();
    auto it = operations.find(name);
    if (it == operations.end())
        return nullptr;
    return &it->value;
}

const Vector<OverloadCandidate>& ShaderModule::overloadCandidates(const OverloadedDeclaration& declaration) const
{
    return declaration.overloads;
}

size_t ShaderModule::memoryFootprint() const
{
    size_t total = sizeof(ShaderModule);
    total += m_astBuilder.memoryFootprint();
    total += m_types.memoryFootprint();
    total += m_declarations.capacity() * sizeof(void*);
    total += m_directives.capacity() * sizeof(void*);
    total += m_overrideValidations.capacity() * sizeof(OverrideValidator);
    total += m_overrides.capacity() * sizeof(AST::Variable*);
    total += m_pipelineOverrideIds.capacity() * sizeof(uint32_t);
    if (m_callGraph)
        total += m_callGraph->entrypoints().capacity() * sizeof(CallGraph::EntryPoint);
    return total;
}

} // namespace WGSL
