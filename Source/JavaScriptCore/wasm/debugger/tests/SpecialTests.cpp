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
#include <wtf/text/MakeString.h>

using namespace JSC;
using namespace JSC::Wasm;

namespace WasmDebugInfoTest {

static bool testI32ConstOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x2a, // i32.const 42 - offset 0
        0x1a, // drop - offset 2
        0x0b // end - offset 3
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, { { 0, { 2 } } });
}

static bool testI64ConstOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x42, 0x2a, // i64.const 42 - offset 0
        0x1a, // drop - offset 2
        0x0b // end - offset 3
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, { { 0, { 2 } } });
}

static bool testF32ConstOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x43, 0x00, 0x00, 0x80, 0x3f, // f32.const 1.0 - offset 0
        0x1a, // drop - offset 5
        0x0b // end - offset 6
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, { { 0, { 5 } } });
}

static bool testF64ConstOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, // f64.const 1.0 - offset 0
        0x1a, // drop - offset 9
        0x0b // end - offset 10
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, { { 0, { 9 } } });
}

static bool testGetLocalOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x2a, // i32.const 42 - offset 0
        0x21, 0x00, // local.set 0 - offset 2
        0x20, 0x00, // local.get 0 - offset 4
        0x1a, // drop - offset 6
        0x0b // end - offset 7
    };

    SourceModule module = createWasmModuleWithLocals(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 6 } }
    });
}

static bool testSetLocalOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x2a, // i32.const 42 - offset 0
        0x21, 0x00, // local.set 0 - offset 2
        0x0b // end - offset 4
    };

    SourceModule module = createWasmModuleWithLocals(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } }
    });
}

static bool testTeeLocalOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x2a, // i32.const 42 - offset 0
        0x22, 0x00, // local.tee 0 - offset 2
        0x1a, // drop - offset 4
        0x0b // end - offset 5
    };

    SourceModule module = createWasmModuleWithLocals(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } }
    });
}

static bool testGetGlobalOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x23, 0x00, // global.get 0 - offset 0
        0x1a, // drop - offset 2
        0x0b // end - offset 3
    };

    SourceModule module = createWasmModuleWithGlobals(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, { { 0, { 2 } } });
}

static bool testSetGlobalOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x2a, // i32.const 42 - offset 0
        0x24, 0x00, // global.set 0 - offset 2
        0x0b // end - offset 4
    };

    SourceModule module = createWasmModuleWithGlobals(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } }
    });
}

static bool testTableGetOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x00, // i32.const 0 - offset 0
        0x25, 0x00, // table.get 0 - offset 2
        0x1a, // drop - offset 4
        0x0b // end - offset 5
    };

    SourceModule module = createWasmModuleWithTable(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } }
    });
}

static bool testTableSetOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x00, // i32.const 0 (index) - offset 0
        0xd0, 0x70, // ref.null func - offset 2
        0x26, 0x00, // table.set 0 - offset 4
        0x0b // end - offset 6
    };

    SourceModule module = createWasmModuleWithTable(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 6 } }
    });
}

static bool testCurrentMemoryOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x3f, 0x00, // memory.size - offset 0
        0x1a, // drop - offset 2
        0x0b // end - offset 3
    };

    SourceModule module = createWasmModuleWithMemory(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, { { 0, { 2 } } });
}

static bool testGrowMemoryOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x01, // i32.const 1 - offset 0
        0x40, 0x00, // memory.grow - offset 2
        0x1a, // drop - offset 4
        0x0b // end - offset 5
    };

    SourceModule module = createWasmModuleWithMemory(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } }
    });
}

static bool testRefNullOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0xd0, 0x70, // ref.null func - offset 0
        0x1a, // drop - offset 2
        0x0b // end - offset 3
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, { { 0, { 2 } } });
}

static bool testRefIsNullOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0xd0, 0x70, // ref.null func - offset 0
        0xd1, // ref.is_null - offset 2
        0x1a, // drop - offset 3
        0x0b // end - offset 4
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 3 } },
    });
}

static bool testRefFuncOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0xd2, 0x00, // ref.func 0 - offset 0
        0x0b // end - offset 2
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({}, { toLEB128(TypeKind::Funcref) })
        .withFunctionBody(functionBody)
        .build();

    return module.parseAndVerifyDebugInfo(opcode, { { 0, { 2 } } });
}

static bool testRefEqOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0xd0, 0x6d, // ref.null eq - offset 0
        0xd0, 0x6d, // ref.null eq - offset 2
        0xd3, // ref.eq - offset 4
        0x0b // end - offset 5
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({}, { toLEB128(TypeKind::I32) })
        .withFunctionBody(functionBody)
        .build();

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 5 } },
    });
}

static bool testRefAsNonNullOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x20, 0x00, // local.get 0 - offset 0
        0xd4, // ref.as_non_null - offset 2
        0x0b // end - offset 3
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({ toLEB128(TypeKind::Funcref) }, { toLEB128(TypeKind::Funcref) })
        .withFunctionBody(functionBody)
        .build();

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 3 } },
    });
}

// Note: Call instructions have two debugging behaviors:
// 1. Step-over: Maps call instruction -> next instruction (tested here via offsetToNextInstructions)
// 2. Step-into: Handled at runtime in ExecutionHandler::step() when target is IPInt mode
// This test verifies the step-over case where we map the call to the next instruction.
static bool testCallOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x2a, // i32.const 42 - offset 0
        0x10, 0x00, // call 0 (recursive call) - offset 2
        0x1a, // drop - offset 4
        0x0b // end - offset 5
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
    });
}

static bool testCallIndirectOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x2a, // i32.const 42 (function argument) - offset 0
        0x41, 0x00, // i32.const 0 (table index) - offset 2
        0x11, 0x00, 0x00, // call_indirect type=0 table=0 - offset 4
        0x0b // end - offset 7 (function returns the i32 result)
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({ toLEB128(TypeKind::I32) }, { toLEB128(TypeKind::I32) })
        .withTable()
        .withFunctionBody(functionBody)
        .build();

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 7 } },
    });
}

static bool testCallRefOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0xd2, 0x00, // ref.func 0 - offset 0
        0x14, 0x00, // call_ref type=0 - offset 2
        0x0b // end - offset 4
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({}, { toLEB128(TypeKind::I32) })
        .withFunctionBody(functionBody)
        .build();

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
    });
}

static bool testTailCallOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x2a, // i32.const 42 - offset 0
        0x12, 0x00, // tail_call 0 (recursive tail call) - offset 2
        0x0b // end - offset 4
    };

    SourceModule module = createWasmModuleWithBytecode(functionBody);

    // Tail calls return to caller's caller, not to the next instruction
    // Step-over is handled at runtime by setting breakpoint at caller
    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } }
    });
}

static bool testTailCallIndirectOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x41, 0x2a, // i32.const 42 (function argument) - offset 0
        0x41, 0x00, // i32.const 0 (table index) - offset 2
        0x13, 0x00, 0x00, // tail_call_indirect type=0 table=0 - offset 4
        0x0b // end - offset 7
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({ toLEB128(TypeKind::I32) }, { toLEB128(TypeKind::I32) })
        .withTable()
        .withFunctionBody(functionBody)
        .build();

    // Tail calls return to caller's caller, not to the next instruction
    // Step-over is handled at runtime by setting breakpoint at caller
    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } }
    });
}

static bool testTailCallRefOpcode(OpType opcode)
{
    Vector<uint8_t> functionBody = {
        0xd2, 0x00, // ref.func 0 - offset 0
        0x15, 0x00, // tail_call_ref type=0 - offset 2
        0x0b // end - offset 4
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({}, { toLEB128(TypeKind::I32) })
        .withFunctionBody(functionBody)
        .build();

    // Tail calls return to caller's caller, not to the next instruction
    // Step-over is handled at runtime by setting breakpoint at caller
    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } }
    });
}

void testAllSpecialOps()
{
    dataLogLn("=== Testing All Special Ops Coverage ===");
    dataLogLn("Total special opcodes in WasmOps.h: ", TOTAL_SPECIAL_OPS);

    int opsTested = 0;
    int opsSucceeded = 0;

#define TEST_SPECIAL_OP(name, id, b3, inc)                        \
    do {                                                          \
        opsTested++;                                              \
        testsRun++;                                               \
        if (test##name##Opcode(OpType::name)) {                   \
            opsSucceeded++;                                       \
            testsPassed++;                                        \
        } else {                                                  \
            testsFailed++;                                        \
            dataLogLn("FAILED: ", #name, " special opcode test"); \
        }                                                         \
    } while (0);

    FOR_EACH_WASM_SPECIAL_OP(TEST_SPECIAL_OP)

#undef TEST_SPECIAL_OP

    TEST_ASSERT(opsTested == TOTAL_SPECIAL_OPS,
        makeString("Tested all "_s, String::number(TOTAL_SPECIAL_OPS), " special ops"_s).utf8().data());
    TEST_ASSERT(opsSucceeded == TOTAL_SPECIAL_OPS,
        makeString("All "_s, String::number(TOTAL_SPECIAL_OPS), " special ops completed"_s).utf8().data());

    dataLogLn("  Successfully tested: ", opsSucceeded, " / ", opsTested, " special ops");
    dataLogLn("All special ops coverage testing completed");
}

} // namespace WasmDebugInfoTest

#endif // ENABLE(WEBASSEMBLY)
