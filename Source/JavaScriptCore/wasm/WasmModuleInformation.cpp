/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WasmModuleInformation.h"
#include "WebAssemblyBuiltin.h"
#include "WebAssemblyCompileOptions.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmNameSection.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)
#include "WasmModuleDebugInfo.h"
#endif

namespace JSC { namespace Wasm {

ModuleInformation::ModuleInformation()
    : nameSection(NameSection::create())
{
#if ENABLE(WEBASSEMBLY_DEBUGGER)
    if (Options::enableWasmDebugger()) [[unlikely]]
        debugInfo = WTF::makeUnique<ModuleDebugInfo>(*this);
#endif
}

ModuleInformation::~ModuleInformation() = default;

// This is called during module creation, so at this point we have fully isolated access
// to this ModuleInformation object.
void ModuleInformation::applyCompileOptions(const WebAssemblyCompileOptions& options)
{
    const auto& constants = options.importedStringConstants();
    if (constants.has_value()) {
        m_importedStringConstants = constants->isolatedCopy();
        // We are making an isolated copy so we are not holding onto a unique string specific to some random thread.
        // The assert below ensures that. Empty strings are special because their isolated copies are all the same canonical atomic empty string.
        ASSERT(!(m_importedStringConstants->impl()->isAtom() || m_importedStringConstants->impl()->isSymbol()) || m_importedStringConstants->impl() == StringImpl::empty());
    }
    const auto& builtinSetNames = options.qualifiedBuiltinSetNames();
    for (const auto& name : builtinSetNames) {
        auto copy = name.isolatedCopy();
        // See the assert notes above.
        ASSERT(!(copy.impl()->isAtom() || copy.impl()->isSymbol()) || copy.impl() == StringImpl::empty());
        m_qualifiedBuiltinSetNames.append(copy);
    }
    populateImportShouldBeHidden();
}

/**
 * Precompute a map indicating which of the imports should not appear in the
 * result of Module.imports() according to
 * https://webassembly.github.io/js-string-builtins/js-api/#dom-module-imports
 */
void ModuleInformation::populateImportShouldBeHidden()
{
    // The following would theoretically be a strict ==, but an inline FixedBitVector reports a larger size than it was created with.
    RELEASE_ASSERT(importShouldBeHidden.size() >= imports.size());
    for (size_t i = 0; i < imports.size(); ++i) {
        const Import& import = imports[i];

        String moduleName = makeString(import.module);
        if (importedStringConstantsEquals(moduleName))
            importShouldBeHidden.testAndSet(i);
        else if (builtinSetsInclude(moduleName)) {
            auto* builtinSet = WebAssemblyBuiltinRegistry::singleton().findByQualifiedName(moduleName);
            if (builtinSet) {
                String fieldName = makeString(import.field);
                if (builtinSet->findBuiltin(fieldName))
                    importShouldBeHidden.testAndSet(i);
            }
        }
    }
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
