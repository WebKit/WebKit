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
#include "TestUtilities.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include <span>
#include <wtf/DataLog.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringView.h>

using namespace JSC;
using namespace JSC::Wasm;

namespace WasmDebugInfoTest {

static bool testMemoryLoadOpcode(OpType opcode, std::span<const char>)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x00, // [0] i32.const 0 (address)
        static_cast<uint8_t>(opcode), // [2] load opcode
        0x00, // [3] alignment (LEB128)
        0x00, // [4] offset (LEB128)
        0x1a, // [5] drop
        0x0b // [6] end
    };

    SourceModule module = createWasmModuleWithMemory(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 5 } } });
}

static bool testMemoryStoreOpcode(OpType opcode, std::span<const char> valueType)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x00, // [0] i32.const 0 (address)
    };

    StringView valueTypeStr(valueType);
    if (valueTypeStr.startsWith("I32"_s)) {
        functionBody.appendVector(Vector<uint8_t> {
            0x41, 0x2a, // [2] i32.const 42
            static_cast<uint8_t>(opcode), // [4] store
            0x00, // [5] alignment
            0x00, // [6] offset
            0x0b // [7] end
        });

        SourceModule module = createWasmModuleWithMemory(functionBody);

        return module.parseAndVerifyDebugInfo(opcode, {
            { 0, { 2 } },
            { 2, { 4 } },
            { 4, { 7 } }
        });
    } else if (valueTypeStr.startsWith("I64"_s)) {
        functionBody.appendVector(Vector<uint8_t> {
            0x42, 0x2a, // [2] i64.const 42
            static_cast<uint8_t>(opcode), // [4] store
            0x00, // [5] alignment
            0x00, // [6] offset
            0x0b // [7] end
        });

        SourceModule module = createWasmModuleWithMemory(functionBody);

        return module.parseAndVerifyDebugInfo(opcode, {
            { 0, { 2 } },
            { 2, { 4 } },
            { 4, { 7 } }
        });
    } else if (valueTypeStr.startsWith("F32"_s)) {
        functionBody.appendVector(Vector<uint8_t> {
            0x43, 0x00, 0x00, 0x80, 0x3f, // [2] f32.const 1.0
            static_cast<uint8_t>(opcode), // [7] store
            0x00, // [8] alignment
            0x00, // [9] offset
            0x0b // [10] end
        });

        SourceModule module = createWasmModuleWithMemory(functionBody);

        return module.parseAndVerifyDebugInfo(opcode, {
            { 0, { 2 } },
            { 2, { 7 } },
            { 7, { 10 } }
        });
    } else { // F64
        functionBody.appendVector(Vector<uint8_t> {
            0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, // [2] f64.const 1.0
            static_cast<uint8_t>(opcode), // [11] store
            0x00, // [12] alignment
            0x00, // [13] offset
            0x0b // [14] end
        });

        SourceModule module = createWasmModuleWithMemory(functionBody);

        return module.parseAndVerifyDebugInfo(opcode, {
            { 0, { 2 } },
            { 2, { 11 } },
            { 11, { 14 } }
        });
    }
}

void testAllMemoryOps()
{
    dataLogLn("=== Testing All Memory Ops Coverage ===");
    dataLogLn("Total memory load opcodes in WasmOps.h: ", TOTAL_MEMORY_LOAD_OPS);
    dataLogLn("Total memory store opcodes in WasmOps.h: ", TOTAL_MEMORY_STORE_OPS);

    int loadOpsTested = 0;
    int loadOpsSucceeded = 0;
    int storeOpsTested = 0;
    int storeOpsSucceeded = 0;

#define TEST_MEMORY_LOAD_OP(name, opcode, b3type, width, type) \
    do { \
        loadOpsTested++; \
        testsRun++; \
        if (testMemoryLoadOpcode(OpType::name, #type)) { \
            loadOpsSucceeded++; \
            testsPassed++; \
        } else { \
            testsFailed++; \
            dataLogLn("FAILED: ", #name, " memory load test"); \
        } \
    } while (0);

    FOR_EACH_WASM_MEMORY_LOAD_OP(TEST_MEMORY_LOAD_OP)

#undef TEST_MEMORY_LOAD_OP

#define TEST_MEMORY_STORE_OP(name, opcode, b3type, width, type) \
    do { \
        storeOpsTested++; \
        testsRun++; \
        if (testMemoryStoreOpcode(OpType::name, #type)) { \
            storeOpsSucceeded++; \
            testsPassed++; \
        } else { \
            testsFailed++; \
            dataLogLn("FAILED: ", #name, " memory store test"); \
        } \
    } while (0);

    FOR_EACH_WASM_MEMORY_STORE_OP(TEST_MEMORY_STORE_OP)

#undef TEST_MEMORY_STORE_OP

    TEST_ASSERT(loadOpsTested == TOTAL_MEMORY_LOAD_OPS, makeString("Tested all "_s, String::number(TOTAL_MEMORY_LOAD_OPS), " memory load ops"_s).utf8().data());
    TEST_ASSERT(loadOpsSucceeded == TOTAL_MEMORY_LOAD_OPS, makeString("All "_s, String::number(TOTAL_MEMORY_LOAD_OPS), " memory load ops passed strict validation"_s).utf8().data());

    TEST_ASSERT(storeOpsTested == TOTAL_MEMORY_STORE_OPS, makeString("Tested all "_s, String::number(TOTAL_MEMORY_STORE_OPS), " memory store ops"_s).utf8().data());
    TEST_ASSERT(storeOpsSucceeded == TOTAL_MEMORY_STORE_OPS, makeString("All "_s, String::number(TOTAL_MEMORY_STORE_OPS), " memory store ops passed strict validation"_s).utf8().data());

    dataLogLn("  Successfully tested with strict mapping validation: ", loadOpsSucceeded, " / ", loadOpsTested, " memory load ops");
    dataLogLn("  Successfully tested with strict mapping validation: ", storeOpsSucceeded, " / ", storeOpsTested, " memory store ops");
    dataLogLn("All memory ops coverage testing completed");
}

} // namespace WasmDebugInfoTest

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
