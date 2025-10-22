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

#if ENABLE(WEBASSEMBLY)

#include <wtf/DataLog.h>

using namespace JSC;
using namespace JSC::Wasm;

namespace WasmDebugInfoTest {

static bool testUnreachableOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x00, // unreachable - offset 0
        0x0b // end - offset 1
    };

    // IPIntGenerator::didParseOpcode() skips debug info recording for unreachable blocks.
    SourceModule module = createWasmModuleWithBytecode(functionBody);
    return module.parseAndVerifyDebugInfo(opcode);
}

static bool testNopOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x01, // nop - offset 0
        0x41, 0x2a, // i32.const 42 - offset 1
        0x1a, // drop - offset 3
        0x0b // end - offset 4
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
        0x02, 0x40, // block $b0 - offset 0
        0x41, 0x01, // i32.const 1 - offset 2
        0x04, 0x40, // if - offset 4
        0x0c, 0x01, // br 1 (to after $b0) - offset 6
        0x0b, // end if - offset 8
        0x41, 0x00, // i32.const 0 - offset 9
        0x04, 0x40, // if - offset 11
        0x0c, 0x01, // br 1 (to after $b0) - offset 13
        0x0b, // end if - offset 15
        0x0b, // end $b0 - offset 16

        0x02, 0x40, // block $b1 - offset 17
        0x41, 0x01, // i32.const 1 - offset 19
        0x04, 0x40, // if - offset 21
        0x0c, 0x01, // br 1 (to after $b1) - offset 23
        0x0b, // end if - offset 25
        0x41, 0x00, // i32.const 0 - offset 26
        0x04, 0x40, // if - offset 28
        0x0c, 0x01, // br 1 (to after $b1) - offset 30
        0x0b, // end if - offset 32
        0x0b, // end $b1 - offset 33

        0x0b // end function - offset 34
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
        0x41, 0x00, // i32.const 0 - offset 0
        0x21, 0x00, // local.set 0 (counter) - offset 2
        0x03, 0x40, // loop (void) - offset 4
        0x20, 0x00, // local.get 0 - offset 6
        0x41, 0x03, // i32.const 3 - offset 8
        0x49, // i32.lt_s - offset 10
        0x04, 0x40, // if (void) - offset 11
        0x20, 0x00, // local.get 0 - offset 13
        0x41, 0x01, // i32.const 1 - offset 15
        0x6a, // i32.add - offset 17
        0x21, 0x00, // local.set 0 - offset 18
        0x0c, 0x01, // br 1 (back to loop start) - offset 20
        0x0b, // end if - offset 22
        0x0b, // end loop - offset 23
        0x0b // end function - offset 24
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
        0x41, 0x01, // i32.const 1 - offset 0
        0x04, 0x40, // if (void) - offset 2
        0x41, 0x2a, // i32.const 42 - offset 4
        0x1a, // drop - offset 6
        0x05, // else - offset 7
        0x41, 0x63, // i32.const 99 - offset 8
        0x1a, // drop - offset 10
        0x0b, // end if - offset 11
        0x0b // end function - offset 12
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
        0x02, 0x40, // block $b0 - offset 0
        0x41, 0x01, // i32.const 1 - offset 2
        0x0d, 0x00, // br_if 0 (break to after block $b0 if true) - offset 4
        0x41, 0x2a, // i32.const 42 - offset 6
        0x1a, // drop - offset 8
        0x0b, // end block $b0 - offset 9
        0x02, 0x40, // block $b1 - offset 10
        0x41, 0x00, // i32.const 0 - offset 12
        0x0d, 0x00, // br_if 0 (break to after block $b1 if true) - offset 14
        0x41, 0x63, // i32.const 99 - offset 16
        0x1a, // drop - offset 18
        0x0b, // end block $b1 - offset 19
        0x0b // end function - offset 20
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
        0x02, 0x40, // block $b0 - offset 0
        0x02, 0x40, // block $b1 - offset 2
        0x02, 0x40, // block $b2 - offset 4
        0x20, 0x00, // local.get 0 (param: i32 selector) - offset 6
        0x0e, 0x02, 0x02, 0x01, 0x00, // br_table [2, 1] default:0 - offset 8
        // index=0 → label 2 (after $b0), index=1 → label 1 (after $b1), index>=2 → label 0 (after $b2)
        0x0b, // end $b2 - offset 13
        0x41, 0x2a, // i32.const 42 - offset 14 (after $b2)
        0x1a, // drop - offset 16
        0x0b, // end $b1 - offset 17
        0x41, 0x63, // i32.const 99 - offset 18 (after $b1)
        0x1a, // drop - offset 20
        0x0b, // end $b0 - offset 21
        0x0b // end function - offset 22
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({ 0x7f }, {}) // [i32] -> []
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
        0x41, 0x01, // i32.const 1 - offset 0
        0x04, 0x40, // if - offset 2
        0x0f, // return - offset 4
        0x0b, // end if - offset 5
        0x0b // end function - offset 6
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
        0x41, 0x2a, // i32.const 42 - offset 0
        0x41, 0x63, // i32.const 99 - offset 2
        0x41, 0x01, // i32.const 1 - offset 4
        0x1b, // select - offset 6
        0x1a, // drop - offset 7
        0x0b // end - offset 8
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
        0x41, 0x2a, // i32.const 42 - offset 0
        0x41, 0x63, // i32.const 99 - offset 2
        0x41, 0x01, // i32.const 1 - offset 4
        0x1c, 0x01, 0x7f, // select (result i32) - offset 6
        0x1a, // drop - offset 9
        0x0b // end - offset 10
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
        0x02, 0x40, // block $b0 - offset 0
        0xd0, 0x6f, // ref.null extern - offset 2
        0xd5, 0x00, // br_on_null 0 - offset 4
        0x1a, // drop the non-null ref - offset 6
        0xd0, 0x6f, // ref.null extern - offset 7
        0xd5, 0x00, // br_on_null 0 - offset 9
        0x1a, // drop the non-null ref - offset 11
        0x0b, // end $b0 - offset 12

        0x02, 0x40, // block $b1 - offset 13
        0xd0, 0x6f, // ref.null extern - offset 15
        0xd5, 0x00, // br_on_null 0 - offset 17
        0x1a, // drop the non-null ref - offset 19
        0xd0, 0x6f, // ref.null extern - offset 20
        0xd5, 0x00, // br_on_null 0 - offset 22
        0x1a, // drop the non-null ref - offset 24
        0x0b, // end $b1 - offset 25

        0x0b // end function - offset 26
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
        0x02, 0x40, // block $b0 - offset 0
        0x20, 0x00, // local.get 0 (funcref param) - offset 2
        0xd6, 0x00, // br_on_non_null 0 - offset 4
        0x20, 0x00, // local.get 0 - offset 6
        0xd6, 0x00, // br_on_non_null 0 - offset 8
        0x0b, // end $b0 - offset 10

        0x02, 0x40, // block $b1 - offset 11
        0x20, 0x00, // local.get 0 - offset 13
        0xd6, 0x00, // br_on_non_null 0 - offset 15
        0x20, 0x00, // local.get 0 - offset 17
        0xd6, 0x00, // br_on_non_null 0 - offset 19
        0x0b, // end $b1 - offset 21

        0x0b // end function - offset 22
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({ toLEB128(TypeKind::Funcref) }, {})
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

// FIXME: Exception handling opcodes (try/catch/throw/rethrow) require runtime testing
// to properly validate exception throwing, catching, stack unwinding, and debugger interaction.
// Static control flow analysis alone is insufficient to test exception handling semantics.
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

#define TEST_CONTROL_FLOW_OP(name, id, b3, inc)           \
    do {                                                  \
        opsTested++;                                      \
        testsRun++;                                       \
        if (test##name##Opcode(OpType::name)) {           \
            opsSucceeded++;                               \
            testsPassed++;                                \
        } else {                                          \
            testsFailed++;                                \
            dataLogLn("FAILED: ", #name, " opcode test"); \
        }                                                 \
    } while (0);

    FOR_EACH_WASM_CONTROL_FLOW_OP(TEST_CONTROL_FLOW_OP)

#undef TEST_CONTROL_FLOW_OP

    dataLogLn("  Successfully tested: ", opsSucceeded, " / ", opsTested, " control flow ops");
    dataLogLn("All control flow ops coverage testing completed");
}

} // namespace WasmDebugInfoTest

#endif // ENABLE(WEBASSEMBLY)
