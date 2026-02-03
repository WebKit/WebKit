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

#if ENABLE(WEBASSEMBLY)

#include "JSWebAssemblyModule.h"
#include "Test.h"
#include "WebAssemblyCompileOptions.h"

#include <wtf/Variant.h>

namespace JSC {

DEFINE_TEST(WebAssemblyCompileOptions_tryCreate,
    R"(
        const inputs = {
            empty : { },
            builtinsOnly : { builtins: ['js-string', 'set2'] },
            stringsOnly : { importedStringConstants: 'foo' },
            builtinsAndStrings : {
                builtins: ['js-string', 'set2'],
                importedStringConstants: 'foo'
            }
        };
        $probe(inputs);
    )",
    {
        JSObject* inputs = arguments[0].getObject();
        {
            auto fromNull = WebAssemblyCompileOptions::tryCreate(globalObject, nullptr);
            CHECK(!fromNull);
        }
        {
            JSObject* optionsObject = GET_PROPERTY(inputs, "empty"_s).getObject();
            CHECK_NO_EXCEPTION;
            auto options = WebAssemblyCompileOptions::tryCreate(globalObject, optionsObject);
            CHECK(options.has_value());
            CHECK(!options->importedStringConstants());
            CHECK(options->qualifiedBuiltinSetNames().isEmpty());
        }
        {
            JSObject* optionsObject = GET_PROPERTY(inputs, "builtinsOnly"_s).getObject();
            CHECK_NO_EXCEPTION;
            auto options = WebAssemblyCompileOptions::tryCreate(globalObject, optionsObject);
            CHECK(options.has_value());
            CHECK(!options->importedStringConstants());
            auto builtinSets = options->qualifiedBuiltinSetNames();
            CHECK_EQ(builtinSets.size(), 2UL);
            CHECK_EQ(builtinSets[0], "wasm:js-string"_s);
            CHECK_EQ(builtinSets[1], "wasm:set2"_s);
        }
        {
            JSObject* optionsObject = GET_PROPERTY(inputs, "stringsOnly"_s).getObject();
            CHECK_NO_EXCEPTION;
            auto options = WebAssemblyCompileOptions::tryCreate(globalObject, optionsObject);
            CHECK(options.has_value());
            CHECK(options->qualifiedBuiltinSetNames().isEmpty());
            auto stringConstants = options->importedStringConstants();
            CHECK(stringConstants.has_value());
            CHECK_EQ(*stringConstants, "foo"_s);
        }
        {
            JSObject* optionsObject = GET_PROPERTY(inputs, "builtinsAndStrings"_s).getObject();
            CHECK_NO_EXCEPTION;
            auto options = WebAssemblyCompileOptions::tryCreate(globalObject, optionsObject);
            CHECK(options.has_value());
            auto builtinSets = options->qualifiedBuiltinSetNames();
            CHECK_EQ(builtinSets.size(), 2UL);
            CHECK_EQ(builtinSets[0], "wasm:js-string"_s);
            CHECK_EQ(builtinSets[1], "wasm:set2"_s);
            auto stringConstants = options->importedStringConstants();
            CHECK(stringConstants.has_value());
            CHECK_EQ(*stringConstants, "foo"_s);
        }
    }
)

DEFINE_TEST(WebAssemblyCompileOptions_validate,
    R"(
        load('./unittestScripts/wasm-module-builder.js');

        const builder = new WasmModuleBuilder();
        const testType = builder.addType({ params: [kWasmExternRef], results: [kWasmI32] });
        builder.addImport('wasm:js-string', 'test', testType);
        builder.addImportedGlobal('ints', 'a', kWasmI32, true);
        const module = builder.toModule();

        const badBuilder = new WasmModuleBuilder();
        const badTestType = badBuilder.addType({ params: [kWasmExternRef, kWasmI32], results: [kWasmI32] });
        badBuilder.addImport('wasm:js-string', 'test', badTestType);
        const badModule = badBuilder.toModule();

        $probe(module, { }, true);
        // Should fail because of duplicate builtin set names:
        $probe(module, { builtins: ['foo', 'bar', 'foo'] }, false);
        $probe(module, { builtins: ['js-string'] }, true);
        // Should fail because js-string:test import type doesn't match the builtin type:
        $probe(badModule, { builtins: ['js-string'] }, false);
        $probe(module, { importedStringConstants: 'foo' }, true);
        // Should fail because the module has an import from 'ints' which is an i32:
        $probe(module, { importedStringConstants: 'ints' }, false);
    )",
    // We are testing the validation of Wasm::Module against compile options. To obtain a
    // Wasm::Module, we build above a complete JSWebAssemblyModule using WasmModuleBuilder and
    // in the probe extract the Wasm::Module from it.
    {
        JSWebAssemblyModule* jsModule = jsCast<JSWebAssemblyModule*>(arguments[0].getObject());
        JSObject* optionsObject = arguments[1].getObject();
        bool expectSuccess = arguments[2].toBoolean(globalObject);
        CHECK_NO_EXCEPTION;
        CHECK(jsModule && optionsObject);

        Ref<Wasm::Module> module = jsModule->module();
        auto options = WebAssemblyCompileOptions::tryCreate(globalObject, optionsObject);
        CHECK(!!options);
        bool success = !options->validateBuiltinsAndImportedStrings(module);
        CHECK(success == expectSuccess);
    }
)

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
