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

using namespace JSC;
using namespace JSC::Wasm;

namespace WasmDebugInfoTest {

static bool testUnreachableOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x00, // [0] unreachable
        0x0b // [1] end
    };

    // IPIntGenerator::didParseOpcode() skips debug info recording for unreachable blocks.
    SourceModule module = createWasmModuleWithBytecode(functionBody);
    return module.parseAndVerifyDebugInfo(opcode, { });
}

static bool testNopOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x01, // [0] nop
        0x41, 0x2a, // [1] i32.const 42
        0x1a, // [3] drop
        0x0b // [4] end
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    // nop, drop, and end are not in the mappings because they're handled directly
    // in ExecutionHandler::step() by setting breakpoint at currentPC + 1
    return module.parseAndVerifyDebugInfo(opcode, { { 1, { 3 } } });
}
static bool testDropOpcode(OpType opcode) { return testNopOpcode(opcode); }
static bool testEndOpcode(OpType opcode) { return testNopOpcode(opcode); }

static bool testBlockOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x02, 0x40, // [0] block $b0
        0x41, 0x01, // [2] i32.const 1
        0x04, 0x40, // [4] if
        0x0c, 0x01, // [6] br 1 (to after $b0)
        0x0b, // [8] end if
        0x41, 0x00, // [9] i32.const 0
        0x04, 0x40, // [11] if
        0x0c, 0x01, // [13] br 1 (to after $b0)
        0x0b, // [15] end if
        0x0b, // [16] end $b0

        0x02, 0x40, // [17] block $b1
        0x41, 0x01, // [19] i32.const 1
        0x04, 0x40, // [21] if
        0x0c, 0x01, // [23] br 1 (to after $b1)
        0x0b, // [25] end if
        0x41, 0x00, // [26] i32.const 0
        0x04, 0x40, // [28] if
        0x0c, 0x01, // [30] br 1 (to after $b1)
        0x0b, // [32] end if
        0x0b, // [33] end $b1

        0x0b // [34] end function
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 6, 9 } },
        { 6, { 19 } }, // br 1 jumps past $b0 to first instruction in $b1
        { 9, { 11 } },
        { 11, { 13, 16 } },
        { 13, { 19 } }, // br 1 jumps past $b0 to first instruction in $b1
        { 17, { 19 } },
        { 19, { 21 } },
        { 21, { 23, 26 } },
        { 23, { 34 } }, // br 1 jumps past $b1 to end function
        { 26, { 28 } },
        { 28, { 30, 33 } },
        { 30, { 34 } }, // br 1 jumps past $b1 to end function
    });
}
static bool testBrOpcode(OpType opcode) { return testBlockOpcode(opcode); }

static bool testLoopOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x00, // [0] i32.const 0
        0x21, 0x00, // [2] local.set 0 (counter)
        0x03, 0x40, // [4] loop (void)
        0x20, 0x00, // [6] local.get 0
        0x41, 0x03, // [8] i32.const 3
        0x49, // [10] i32.lt_s
        0x04, 0x40, // [11] if (void)
        0x20, 0x00, // [13] local.get 0
        0x41, 0x01, // [15] i32.const 1
        0x6a, // [17] i32.add
        0x21, 0x00, // [18] local.set 0
        0x0c, 0x01, // [20] br 1 (back to loop start)
        0x0b, // [22] end if
        0x0b, // [23] end loop
        0x0b // [24] end function
    };

    SourceModule module = createWasmModuleWithLocals(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 6 } },
        { 6, { 8 } },
        { 8, { 10 } },
        { 10, { 11 } },
        { 11, { 13, 23 } },
        { 13, { 15 } },
        { 15, { 17 } },
        { 17, { 18 } },
        { 18, { 20 } },
        { 20, { 4 } },
    });
}

static bool testIfOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x01, // [0] i32.const 1
        0x04, 0x40, // [2] if (void)
        0x41, 0x2a, // [4] i32.const 42
        0x1a, // [6] drop
        0x05, // [7] else
        0x41, 0x63, // [8] i32.const 99
        0x1a, // [10] drop
        0x0b, // [11] end if
        0x0b // [12] end function
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4, 8 } },
        { 4, { 6 } },
        { 7, { 12 } },
        { 8, { 10 } },
    });
}
static bool testElseOpcode(OpType opcode) { return testIfOpcode(opcode); }

static bool testBrIfOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x02, 0x40, // [0] block $b0
        0x41, 0x01, // [2] i32.const 1
        0x0d, 0x00, // [4] br_if 0 (break to after block $b0 if true)
        0x41, 0x2a, // [6] i32.const 42
        0x1a, // [8] drop
        0x0b, // [9] end block $b0
        0x02, 0x40, // [10] block $b1
        0x41, 0x00, // [12] i32.const 0
        0x0d, 0x00, // [14] br_if 0 (break to after block $b1 if true)
        0x41, 0x63, // [16] i32.const 99
        0x1a, // [18] drop
        0x0b, // [19] end block $b1
        0x0b // [20] end function
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 6, 12 } }, // br_if: continue (6) or jump past $b0 end to $b1 start (12)
        { 6, { 8 } },
        { 10, { 12 } },
        { 12, { 14 } },
        { 14, { 16, 20 } }, // br_if: continue (16) or jump to function end (20)
        { 16, { 18 } },
    });
}

static bool testBrTableOpcode(OpType opcode)
{
    // Create a br_table with distinct branch targets
    Vector<uint8_t> functionBody = {
        0x02, 0x40, // [0] block $b0
        0x02, 0x40, // [2] block $b1
        0x02, 0x40, // [4] block $b2
        0x20, 0x00, // [6] local.get 0 (param: i32 selector)
        0x0e, 0x02, 0x02, 0x01, 0x00, // [8] br_table [2, 1] default:0
        // index=0 → label 2 (after $b0), index=1 → label 1 (after $b1), index>=2 → label 0 (after $b2)
        0x0b, // [13] end $b2
        0x41, 0x2a, // [14] i32.const 42 (after $b2)
        0x1a, // [16] drop
        0x0b, // [17] end $b1
        0x41, 0x63, // [18] i32.const 99 (after $b1)
        0x1a, // [20] drop
        0x0b, // [21] end $b0
        0x0b // [22] end function
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({ 0x7f }, { }) // [i32] -> []
        .withFunctionBody(functionBody)
        .build();

    return module.parseAndVerifyDebugInfo(opcode, {
        // FIXME: Block coalescing (offsets 0→2→4→6) should ideally result in { 0, { 6 } } only,
        // but exit handlers in resolveExitTarget/coalesceControlFlow use ADD mode instead of UPDATE mode,
        // accumulating all intermediate targets {2, 4, 6}. This doesn't break debugger functionality
        // but could be optimized to use UPDATE mode like resolveEntryTarget does.
        { 0, { 2, 4, 6 } },
        { 6, { 8 } },
        { 8, { 14, 18, 22 } },
        { 14, { 16 } },
        { 18, { 20 } },
    });
}

static bool testReturnOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x01, // [0] i32.const 1
        0x04, 0x40, // [2] if
        0x0f, // [4] return
        0x0b, // [5] end if
        0x0b // [6] end function
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    // This is handled directly in ExecutionHandler::step().
    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4, 6 } },
    });
}

static bool testSelectOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x2a, // [0] i32.const 42
        0x41, 0x63, // [2] i32.const 99
        0x41, 0x01, // [4] i32.const 1
        0x1b, // [6] select
        0x1a, // [7] drop
        0x0b // [8] end
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    // This is handled directly in ExecutionHandler::step().
    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 6 } },
    });
}

static bool testAnnotatedSelectOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x2a, // [0] i32.const 42
        0x41, 0x63, // [2] i32.const 99
        0x41, 0x01, // [4] i32.const 1
        0x1c, 0x01, 0x7f, // [6] select (result i32)
        0x1a, // [9] drop
        0x0b // [10] end
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 6 } },
        { 6, { 9 } },
    });
}

static bool testBrOnNullOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x02, 0x40, // [0] block $b0
        0xd0, 0x6f, // [2] ref.null extern
        0xd5, 0x00, // [4] br_on_null 0
        0x1a, // drop the non-null ref - offset 6
        0xd0, 0x6f, // [7] ref.null extern
        0xd5, 0x00, // [9] br_on_null 0
        0x1a, // drop the non-null ref - offset 11
        0x0b, // [12] end $b0

        0x02, 0x40, // [13] block $b1
        0xd0, 0x6f, // [15] ref.null extern
        0xd5, 0x00, // [17] br_on_null 0
        0x1a, // drop the non-null ref - offset 19
        0xd0, 0x6f, // [20] ref.null extern
        0xd5, 0x00, // [22] br_on_null 0
        0x1a, // drop the non-null ref - offset 24
        0x0b, // [25] end $b1

        0x0b // [26] end function
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 6, 15 } },
        { 7, { 9 } },
        { 9, { 11, 15 } },
        { 13, { 15 } },
        { 15, { 17 } },
        { 17, { 19, 26 } },
        { 20, { 22 } },
        { 22, { 24, 26 } },
    });
}

static bool testBrOnNonNullOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x02, 0x40, // [0] block $b0
        0x20, 0x00, // [2] local.get 0 (funcref param)
        0xd6, 0x00, // [4] br_on_non_null 0
        0x20, 0x00, // [6] local.get 0
        0xd6, 0x00, // [8] br_on_non_null 0
        0x0b, // [10] end $b0

        0x02, 0x40, // [11] block $b1
        0x20, 0x00, // [13] local.get 0
        0xd6, 0x00, // [15] br_on_non_null 0
        0x20, 0x00, // [17] local.get 0
        0xd6, 0x00, // [19] br_on_non_null 0
        0x0b, // [21] end $b1

        0x0b // [22] end function
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({ toLEB128(TypeKind::Funcref) }, { })
        .withFunctionBody(functionBody)
        .build();

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 6, 13 } },
        { 6, { 8 } },
        { 8, { 10, 13 } },
        { 11, { 13 } },
        { 13, { 15 } },
        { 15, { 17, 22 } },
        { 17, { 19 } },
        { 19, { 21, 22 } },
    });
}

// Exception handling opcodes require runtime testing because genericUnwind() dynamically
// computes handler PCs. See runtime tests in JSTests/wasm/debugger/resources/wasm/:
//   throw-catch.js, throw-catch-all.js, rethrow.js, throw-ref.js, delegate.js, try-table.js
static bool testTryOpcode(OpType) { return true; }
static bool testCatchOpcode(OpType) { return true; }
static bool testThrowOpcode(OpType) { return true; }
static bool testRethrowOpcode(OpType) { return true; }
static bool testThrowRefOpcode(OpType) { return true; }
static bool testDelegateOpcode(OpType) { return true; }
static bool testCatchAllOpcode(OpType) { return true; }
static bool testTryTableOpcode(OpType) { return true; }

void testAllControlFlowOps()
{
    dataLogLn("=== Testing All Control Flow Ops Coverage ===");
    dataLogLn("Total control flow opcodes in WasmOps.h: ", TOTAL_CONTROL_OPS);

    int opsTested = 0;
    int opsSucceeded = 0;

#define TEST_CONTROL_FLOW_OP(name, id, b3, inc) \
    do { \
        opsTested++; \
        testsRun++; \
        if (test##name##Opcode(OpType::name)) { \
            opsSucceeded++; \
            testsPassed++; \
        } else { \
            testsFailed++; \
            dataLogLn("FAILED: ", #name, " opcode test"); \
        } \
    } while (0);

    FOR_EACH_WASM_CONTROL_FLOW_OP(TEST_CONTROL_FLOW_OP)

#undef TEST_CONTROL_FLOW_OP

    dataLogLn("  Successfully tested: ", opsSucceeded, " / ", opsTested, " control flow ops");
    dataLogLn("All control flow ops coverage testing completed");
}

} // namespace WasmDebugInfoTest

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
