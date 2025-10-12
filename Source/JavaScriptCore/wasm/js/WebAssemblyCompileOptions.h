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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "IteratorOperations.h"
#include "WasmModule.h"
#include <wtf/text/WTFString.h>

namespace JSC {

// Captures the information extracted from the optional compilation options argument
// added by the js-string builtins proposal to `WebAssembly.Module` constructor
// and a number of other APIs.
//
// As an instance is constructed, builtin set names listed in the `builtins` attribute (if
// present) are qualified: "foo" becomes "wasm:foo".
class WebAssemblyCompileOptions {
public:
    // Create an instance if `optionsObject` is not a nullptr, or return a `nullopt`.
    static std::optional<WebAssemblyCompileOptions> tryCreate(JSGlobalObject*, JSObject* optionsObject);

    const std::optional<String>& importedStringConstants() const { return m_importedStringConstants; }
    const Vector<String>& qualifiedBuiltinSetNames() const { return m_qualifiedBuiltinSetNames; }

    // Validate the options in the context of the given module as specified in
    // https://webassembly.github.io/js-string-builtins/js-api/#validate-builtins-and-imported-string-for-a-webassembly-module.
    // Return an empty optional on success, or a String with the error message.
    std::optional<String> validateBuiltinsAndImportedStrings(const Wasm::Module&) const;

private:
    bool validateBuiltinSetNames() const;
    bool validateImportForBuiltinSetNames(const Wasm::Import&, const String& importModuleName, const Wasm::ModuleInformation&) const;

    std::optional<String> m_importedStringConstants;
    Vector<String> m_qualifiedBuiltinSetNames;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
