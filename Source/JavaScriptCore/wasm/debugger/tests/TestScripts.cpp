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
#include "TestScripts.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include <mutex>
#include <span>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace TestScripts {

// WASM Module Structure:
// - Magic + Version: 0x00-0x07
// - Type section (0x08-0x0d): 1 function signature ([] -> [])
// - Function section (0x0e-0x15): 5 functions (all use type 0)
// - Export section (0x16-0x40): Exports "func1" through "func5"
// - Code section (0x41-0x7a): Each function contains:
//   * nop, i32.const N, drop, nop, i32.const M, drop, end
//   (Note: All byte ranges are inclusive on both ends)
static constexpr uint8_t wasmMultiFunctionModuleBytes[] = {
    // [0x00] WASM header
    0x00, 0x61, 0x73, 0x6d, // magic
    0x01, 0x00, 0x00, 0x00, // version

    // [0x08] Type section: (func [] -> [])
    0x01, 0x04, 0x01, 0x60, 0x00, 0x00,

    // [0x0e] Function section: 5 functions (all type 0)
    0x03, 0x06, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00,

    // [0x16] Export section: export 5 functions
    0x07, 0x29, 0x05,
    // Export function 0 as "func1"
    0x05, 0x66, 0x75, 0x6e, 0x63, 0x31, 0x00, 0x00,
    // Export function 1 as "func2"
    0x05, 0x66, 0x75, 0x6e, 0x63, 0x32, 0x00, 0x01,
    // Export function 2 as "func3"
    0x05, 0x66, 0x75, 0x6e, 0x63, 0x33, 0x00, 0x02,
    // Export function 3 as "func4"
    0x05, 0x66, 0x75, 0x6e, 0x63, 0x34, 0x00, 0x03,
    // Export function 4 as "func5"
    0x05, 0x66, 0x75, 0x6e, 0x63, 0x35, 0x00, 0x04,

    // [0x41] Code section
    0x0a, // section id=10
    0x38, // section size=56
    0x05, // 5 functions

    // Function 0: func1 (body size=10)
    0x0a, // [0x44] body size
    0x00, // [0x45] 0 local declarations
    0x01, // [0x46] nop
    0x41, 0x0a, // [0x47] i32.const 10
    0x1a, // [0x49] drop
    0x01, // [0x4a] nop
    0x41, 0x14, // [0x4b] i32.const 20
    0x1a, // [0x4d] drop
    0x0b, // [0x4e] end

    // Function 1: func2 (body size=10)
    0x0a, // [0x4f] body size
    0x00, // [0x50] 0 local declarations
    0x01, // [0x51] nop
    0x41, 0x1e, // [0x52] i32.const 30
    0x1a, // [0x54] drop
    0x01, // [0x55] nop
    0x41, 0x28, // [0x56] i32.const 40
    0x1a, // [0x58] drop
    0x0b, // [0x59] end

    // Function 2: func3 (body size=10)
    0x0a, // [0x5a] body size
    0x00, // [0x5b] 0 local declarations
    0x01, // [0x5c] nop
    0x41, 0x32, // [0x5d] i32.const 50
    0x1a, // [0x5f] drop
    0x01, // [0x60] nop
    0x41, 0x3c, // [0x61] i32.const 60
    0x1a, // [0x63] drop
    0x0b, // [0x64] end

    // Function 3: func4 (body size=10)
    0x0a, // [0x65] body size
    0x00, // [0x66] 0 local declarations
    0x01, // [0x67] nop
    0x41, 0x46, // [0x68] i32.const 70
    0x1a, // [0x6a] drop
    0x01, // [0x6b] nop
    0x41, 0x50, // [0x6c] i32.const 80
    0x1a, // [0x6e] drop
    0x0b, // [0x6f] end

    // Function 4: func5 (body size=10)
    0x0a, // [0x70] body size
    0x00, // [0x71] 0 local declarations
    0x01, // [0x72] nop
    0x41, 0x5a, // [0x73] i32.const 90
    0x1a, // [0x75] drop
    0x01, // [0x76] nop
    0x41, 0x64, // [0x77] i32.const 100
    0x1a, // [0x79] drop
    0x0b // [0x7a] end
};

// Helper to convert bytes to JavaScript array string
static String wasmBytesToJSArray(std::span<const uint8_t> bytes)
{
    StringBuilder result;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i > 0)
            result.append(',');
        result.append(String::number(bytes[i]));
    }
    return result.toString();
}

// Multi-VM test script: creates 5 VMs (main + 4 workers) all running WASM in infinite loops
// Each VM runs a different function from the same module.
String multiVMSameModuleDifferentFunction()
{
    static String* cachedScript = nullptr;
    static std::once_flag once;

    std::call_once(once, [] {
        cachedScript = new String(makeString(R"script(
            // WASM module with 5 different functions
            const wasm = new Uint8Array([)script"_s,
                wasmBytesToJSArray(wasmMultiFunctionModuleBytes),
            R"script(]);

            const module = new WebAssembly.Module(wasm);
            const instance = new WebAssembly.Instance(module, { });

            const NUM_WORKERS = 4;

            // Worker script generator - each worker runs a different function in infinite loop
            function workerScript(workerId, funcName) {
                return `
                    const wasm = new Uint8Array([)script"_s,
                        wasmBytesToJSArray(wasmMultiFunctionModuleBytes),
                        R"script(]);
                    const module = new WebAssembly.Module(wasm);
                    const instance = new WebAssembly.Instance(module, { });

                    // Run specified function in loop until $.shouldExit() returns true
                    while (!$.shouldExit())
                        instance.exports.${funcName}();
                `;
            }

            // Start worker threads via $.agent.start()
            const functions = ['func2', 'func3', 'func4', 'func5'];
            for (let i = 0; i < NUM_WORKERS; i++)
                $.agent.start(workerScript(i + 1, functions[i]));

            // Main thread also runs func1 in loop until $.shouldExit() returns true
            while (!$.shouldExit())
                instance.exports.func1();
        )script"_s));
    });

    return *cachedScript;
}

// Multi-VM test script: creates 5 VMs (main + 4 workers) all running WASM in infinite loops
// All VMs run the same function (func1) from the same module.
String multiVMSameModuleSameFunction()
{
    static String* cachedScript = nullptr;
    static std::once_flag once;

    std::call_once(once, [] {
        cachedScript = new String(makeString(R"script(
            // WASM module with 5 functions, but all VMs will run func1
            const wasm = new Uint8Array([)script"_s,
                wasmBytesToJSArray(wasmMultiFunctionModuleBytes),
            R"script(]);

            const module = new WebAssembly.Module(wasm);
            const instance = new WebAssembly.Instance(module, { });

            const NUM_WORKERS = 4;

            // Worker script generator - all workers run the same function in loop until $.shouldExit()
            function workerScript(workerId) {
                return `
                    const wasm = new Uint8Array([)script"_s,
                        wasmBytesToJSArray(wasmMultiFunctionModuleBytes),
                        R"script(]);
                    const module = new WebAssembly.Module(wasm);
                    const instance = new WebAssembly.Instance(module, { });

                    // All workers run func1 in loop until $.shouldExit() returns true
                    while (!$.shouldExit())
                        instance.exports.func1();
                `;
            }

            // Start worker threads via $.agent.start()
            for (let i = 0; i < NUM_WORKERS; i++)
                $.agent.start(workerScript(i + 1));

            // Main thread also runs func1 in loop until $.shouldExit() returns true
            while (!$.shouldExit())
                instance.exports.func1();
        )script"_s));
    });

    return *cachedScript;
}

// ========== Test Script Registry ==========

static const TestScript allScripts[] = {
    {
        .name = "MultiVMSameModuleDifferentFunction"_s,
        .description = "5 VMs (1 main + 4 workers) running different WASM functions from same module"_s,
        .scriptGenerator = multiVMSameModuleDifferentFunction,
        .expectedVMs = 5,
        .expectedFunctions = 5
    },
    {
        .name = "MultiVMSameModuleSameFunction"_s,
        .description = "5 VMs (1 main + 4 workers) all running same WASM function"_s,
        .scriptGenerator = multiVMSameModuleSameFunction,
        .expectedVMs = 5,
        .expectedFunctions = 5
    },
};

std::span<const TestScript> getTestScripts()
{
    return std::span(allScripts);
}

} // namespace TestScripts

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
