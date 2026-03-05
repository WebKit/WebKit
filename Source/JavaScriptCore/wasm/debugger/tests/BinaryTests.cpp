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

#include <wtf/DataLog.h>
#include <wtf/text/MakeString.h>

using namespace JSC;
using namespace JSC::Wasm;

namespace WasmDebugInfoTest {

static bool testI32BinaryOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x01, // [0] i32.const 1
        0x41, 0x02, // [2] i32.const 2
        static_cast<uint8_t>(opcode), // [4] binary op
        0x1a, // [5] drop
        0x0b // [6] end
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 5 } },
    });
}

static bool testI64BinaryOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x42, 0x01, // [0] i64.const 1
        0x42, 0x02, // [2] i64.const 2
        static_cast<uint8_t>(opcode), // [4] binary op
        0x1a, // [5] drop
        0x0b // [6] end
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 5 } },
    });
}

static bool testF32BinaryOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x43, 0x00, 0x00, 0x80, 0x3f, // [0] f32.const 1.0
        0x43, 0x00, 0x00, 0x00, 0x40, // [5] f32.const 2.0
        static_cast<uint8_t>(opcode), // [10] binary op
        0x1a, // [11] drop
        0x0b // [12] end
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 5 } },
        { 5, { 10 } },
        { 10, { 11 } },
    });
}

static bool testF64BinaryOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, // [0] f64.const 1.0
        0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, // [9] f64.const 2.0
        static_cast<uint8_t>(opcode), // [18] binary op
        0x1a, // [19] drop
        0x0b // [20] end
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 9 } },
        { 9, { 18 } },
        { 18, { 19 } },
    });
}

void testAllBinaryOps()
{
    dataLogLn("=== Testing All Binary Ops Coverage ===");
    dataLogLn("Total binary opcodes in WasmOps.h: ", TOTAL_BINARY_OPS);

    int opsTested = 0;
    int opsSucceeded = 0;

#define TEST_BINARY_OP(name, id, b3, inc, leftType, rightType, resultType) \
    do { \
        opsTested++; \
        testsRun++; \
        bool success = false; \
        constexpr auto leftTypeStr = #leftType##_s; \
        if (leftTypeStr == "I32"_s) { \
            success = testI32BinaryOpcode(static_cast<OpType>(id)); \
        } else if (leftTypeStr == "I64"_s) { \
            success = testI64BinaryOpcode(static_cast<OpType>(id)); \
        } else if (leftTypeStr == "F32"_s) { \
            success = testF32BinaryOpcode(static_cast<OpType>(id)); \
        } else if (leftTypeStr == "F64"_s) { \
            success = testF64BinaryOpcode(static_cast<OpType>(id)); \
        } \
        if (success) { \
            opsSucceeded++; \
            testsPassed++; \
        } else { \
            testsFailed++; \
            dataLogLn("FAILED: ", #name, " binary opcode test"); \
        } \
    } while (0);

    FOR_EACH_WASM_BINARY_OP(TEST_BINARY_OP)

#undef TEST_BINARY_OP

    TEST_ASSERT(opsTested == TOTAL_BINARY_OPS, makeString("Tested all "_s, String::number(TOTAL_BINARY_OPS), " binary ops"_s).utf8().data());
    TEST_ASSERT(opsSucceeded == TOTAL_BINARY_OPS, makeString("All "_s, String::number(TOTAL_BINARY_OPS), " binary ops passed strict validation"_s).utf8().data());

    dataLogLn("  Successfully tested with strict mapping validation: ", opsSucceeded, " / ", opsTested, " binary ops");
    dataLogLn("All binary ops coverage testing completed");
}

} // namespace WasmDebugInfoTest

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
