/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "WebAssemblyCompileOptions.h"

#if ENABLE(WEBASSEMBLY)

#include "IteratorOperations.h"
#include "WasmModuleInformation.h"
#include "WebAssemblyBuiltin.h"

namespace JSC {

std::optional<WebAssemblyCompileOptions> WebAssemblyCompileOptions::tryCreate(JSGlobalObject* globalObject, JSObject *optionsObject)
{
    if (!optionsObject)
        return std::nullopt;

    WebAssemblyCompileOptions options;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Check for the 'importedStringConstants' entry
    JSValue importedStringConstantsValue = optionsObject->get(globalObject, PropertyName(Identifier::fromString(vm, "importedStringConstants"_s)));
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    if (importedStringConstantsValue.isString()) {
        auto importedStringConstants = asString(importedStringConstantsValue)->value(globalObject);
        options.m_importedStringConstants = makeString(StringView(importedStringConstants));
    } else if (!importedStringConstantsValue.isUndefined()) {
        auto error = createTypeError(globalObject, "importedStringConstants option value must be a string"_s);
        throwException(globalObject, scope, error);
        return std::nullopt;
    }

    // Check for the 'builtins' entry, qualifying builtin set names in the process.
    JSValue builtinsValue = optionsObject->get(globalObject, PropertyName(Identifier::fromString(vm, "builtins"_s)));
    RETURN_IF_EXCEPTION(scope, std::nullopt);
    if (builtinsValue.isObject()) {
        bool sawBadEntries = false;
        forEachInIterable(globalObject, builtinsValue, [&] (VM&, JSGlobalObject* globalObject, JSValue nextValue) {
            if (nextValue.isString()) {
                auto contents = asString(nextValue)->value(globalObject);
                String qualifiedName = makeString("wasm:"_s, StringView(contents));
                options.m_qualifiedBuiltinSetNames.append(qualifiedName);
            } else
                sawBadEntries = true;
        });
        RETURN_IF_EXCEPTION(scope, std::nullopt);
        if (sawBadEntries) {
            auto error = createTypeError(globalObject, "builtins list option values must be strings"_s);
            throwException(globalObject, scope, error);
            return std::nullopt;
        }
    }
    return options;
}

static bool namesInclude(const String& expected, const Vector<String>& names)
{
    for (auto& name : names) {
        if (name == expected)
            return true;
    }
    return false;
}

static String makeQualifiedName(const Wasm::Import& import)
{
    return makeString(import.module, ":"_s, import.field);
}

/**
 * See step 2.1 of: https://webassembly.github.io/js-string-builtins/js-api/#validate-builtins-and-imported-string-for-a-webassembly-module
 *
 * Informally: the import should be an immutable global of type externref.
 */
static std::optional<String> validateImportedStringConstant(const Wasm::Import& import, const Wasm::ModuleInformation& moduleInformation)
{
    if (import.kind != Wasm::ExternalKind::Global)
        return makeString("imported string constant "_s, makeQualifiedName(import), " is not a global"_s);
    const Wasm::GlobalInformation& global = moduleInformation.globals[import.kindIndex];
    if (global.mutability != Wasm::Immutable || !Wasm::isExternref(global.type))
        return makeString("imported string constant "_s, makeQualifiedName(import), " is not an immutable external reference"_s);
    return std::nullopt;
}

/**
 * See https://webassembly.github.io/js-string-builtins/js-api/#validate-an-import-for-builtins
 *
 * Informally:
 * Fail the validation if:
 *  - there is a builtin set whose simple name appears in builtinSetNames, and
 *  - the qualified name of the builtin set matches the import module name, and
 *  - the builtin set contains a builtin matching the function name, and
 *  - the builtin type does not match the import type.
 */
bool WebAssemblyCompileOptions::validateImportForBuiltinSetNames(const Wasm::Import& import, const String& importModuleName, const Wasm::ModuleInformation& moduleInfo) const
{
    if (!namesInclude(importModuleName, m_qualifiedBuiltinSetNames))
        return true;
    const WebAssemblyBuiltinSet* builtinSet = WebAssemblyBuiltinRegistry::singleton().findByQualifiedName(importModuleName);
    if (!builtinSet)
        return true;
    String importName = makeString(import.field);
    const WebAssemblyBuiltin* builtin = builtinSet->findBuiltin(importName);
    if (!builtin)
        return true;
    auto& builtinSig = builtin->signature();

    // The spec does not explicitly check if the import is a function because an import type is fully self-contained in `import[2]`.
    // A non-function import would have a non-function type as its `import[2]`, failing the `match_externtype` check in Step 7.
    // In our implementation import type is held externally, so we must check that the import kind is a function before fetching the function type
    // at `kindIndex`. The wrong import kind is equivalent in spec terms to `match_externtype` returning false in Step 7.
    if (import.kind != Wasm::ExternalKind::Function)
        return false;
    Wasm::TypeIndex typeIndex = moduleInfo.importFunctionTypeIndices[import.kindIndex];
    Ref<const Wasm::TypeDefinition> type = Wasm::TypeInformation::get(typeIndex);
    if (!type->is<Wasm::FunctionSignature>())
        return false;
    SUPPRESS_UNCOUNTED_LOCAL auto* importSig = type->as<Wasm::FunctionSignature>();

    return builtinSig.isValid(*importSig);
}

/**
 * See https://webassembly.github.io/js-string-builtins/js-api/#validate-builtins-and-imported-string-for-a-webassembly-module
 *
 * Return nullopt to indicate success or an error message if validation failed.
 */
std::optional<String> WebAssemblyCompileOptions::validateBuiltinsAndImportedStrings(const Wasm::Module& module) const
{
    if (!validateBuiltinSetNames())
        return String::fromLatin1("the list of builtin set names contains duplicates");
    auto moduleInfo = Ref(module.moduleInformation());
    for (const auto& import : moduleInfo.get().imports) {
        String importModuleName = makeString(import.module);
        if (m_importedStringConstants && *m_importedStringConstants == importModuleName) {
            auto errorMessage = validateImportedStringConstant(import, moduleInfo.get());
            if (errorMessage)
                return errorMessage;
        } else {
            if (!validateImportForBuiltinSetNames(import, importModuleName, moduleInfo.get()))
                return makeString("builtin import "_s, makeQualifiedName(import), " has an unexpected signature"_s);
        }
    }
    return std::nullopt;
}

/**
 * See https://webassembly.github.io/js-string-builtins/js-api/#validate-builtin-set-names
 *
 * Informally: the list of builtin set names should not have duplicates.
 */
bool WebAssemblyCompileOptions::validateBuiltinSetNames() const
{
    UncheckedKeyHashSet<String> seen;
    for (const auto& name : m_qualifiedBuiltinSetNames) {
        if (seen.contains(name))
            return false;
        seen.add(name);
    }
    return true;
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
