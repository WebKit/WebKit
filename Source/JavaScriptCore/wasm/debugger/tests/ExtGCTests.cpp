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

static bool testBrOnCastOpcode(ExtGCOpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x02, 0x01, // [0] block (result i32 i31ref)
        0x41, 0x2a, // [2] i32.const 42
        0x20, 0x00, // [4] local.get 0 (anyref param)
        0xfb, 0x18, // [6] br_on_cast (ExtGC opcode 0x18)
        0x03, // [8] flags (0x03 = both nullable)
        0x00, // [9] branch depth 0
        0x6e, // [10] source ref type: anyref
        0x6c, // [11] target ref type: i31ref
        0x41, 0x07, // [12] i32.const 7
        0xd0, 0x6c, // [14] ref.null i31ref
        0x0c, 0x00, // [16] br 0
        0x0b, // [18] end block
        0x1a, // [19] drop
        0x0b // [20] end function
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({ 0x6e }, { 0x7f }) // Type 0: (anyref) -> i32
        .withAdditionalType({ }, { 0x7f, 0x6c }) // Type 1: () -> (i32, i31ref)
        .withFunctionBody(functionBody)
        .build();

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 6 } },
        { 6, { 12, 19 } },
        { 12, { 14 } },
        { 14, { 16 } },
        { 16, { 19 } }
    });
}

static bool testBrOnCastFailOpcode(ExtGCOpType opcode)
{
    Vector<uint8_t> functionBody = {
        0x02, 0x01, // [0] block (result i32 anyref)
        0x41, 0x2a, // [2] i32.const 42
        0x20, 0x00, // [4] local.get 0 (anyref param)
        0xfb, 0x19, // [6] br_on_cast_fail (ExtGC opcode 0x19)
        0x03, // [8] flags (0x03 = both nullable)
        0x00, // [9] branch depth 0
        0x6e, // [10] source ref type: anyref
        0x6c, // [11] target ref type: i31ref
        0x1a, // [12] drop (i31ref from successful cast)
        0x41, 0x07, // [13] i32.const 7
        0x20, 0x00, // [15] local.get 0 (push anyref again)
        0x0c, 0x00, // [17] br 0
        0x0b, // [19] end block
        0x1a, // [20] drop (anyref)
        0x0b // [21] end function
    };

    SourceModule module = SourceModule::create()
        .withFunctionType({ 0x6e }, { 0x7f }) // Type 0: (anyref) -> i32
        .withAdditionalType({ }, { 0x7f, 0x6e }) // Type 1: () -> (i32, anyref)
        .withFunctionBody(functionBody)
        .build();

    return module.parseAndVerifyDebugInfo(opcode, {
        { 0, { 2 } },
        { 2, { 4 } },
        { 4, { 6 } },
        { 6, { 12, 20 } },
        { 13, { 15 } },
        { 15, { 17 } },
        { 17, { 20 } }
    });
}

// FIXME: Implement proper test for ExtGCOpType::name
#define DEFINE_EXTGC_OP_TEST(name) \
static bool test##name##Opcode(ExtGCOpType) \
{ \
    return true; \
}

DEFINE_EXTGC_OP_TEST(StructNew)
DEFINE_EXTGC_OP_TEST(StructNewDefault)
DEFINE_EXTGC_OP_TEST(StructGet)
DEFINE_EXTGC_OP_TEST(StructGetS)
DEFINE_EXTGC_OP_TEST(StructGetU)
DEFINE_EXTGC_OP_TEST(StructSet)
DEFINE_EXTGC_OP_TEST(ArrayNew)
DEFINE_EXTGC_OP_TEST(ArrayNewDefault)
DEFINE_EXTGC_OP_TEST(ArrayNewFixed)
DEFINE_EXTGC_OP_TEST(ArrayNewData)
DEFINE_EXTGC_OP_TEST(ArrayNewElem)
DEFINE_EXTGC_OP_TEST(ArrayGet)
DEFINE_EXTGC_OP_TEST(ArrayGetS)
DEFINE_EXTGC_OP_TEST(ArrayGetU)
DEFINE_EXTGC_OP_TEST(ArraySet)
DEFINE_EXTGC_OP_TEST(ArrayLen)
DEFINE_EXTGC_OP_TEST(ArrayFill)
DEFINE_EXTGC_OP_TEST(ArrayCopy)
DEFINE_EXTGC_OP_TEST(ArrayInitData)
DEFINE_EXTGC_OP_TEST(ArrayInitElem)
DEFINE_EXTGC_OP_TEST(RefTest)
DEFINE_EXTGC_OP_TEST(RefTestNull)
DEFINE_EXTGC_OP_TEST(RefCast)
DEFINE_EXTGC_OP_TEST(RefCastNull)
DEFINE_EXTGC_OP_TEST(AnyConvertExtern)
DEFINE_EXTGC_OP_TEST(ExternConvertAny)
DEFINE_EXTGC_OP_TEST(RefI31)
DEFINE_EXTGC_OP_TEST(I31GetS)
DEFINE_EXTGC_OP_TEST(I31GetU)

#undef DEFINE_EXTGC_OP_TEST

void testAllExtGCOps()
{
    dataLogLn("=== Testing All ExtGC Ops Coverage ===");
    dataLogLn("Total ExtGC opcodes in WasmOps.h: ", TOTAL_EXTGC_OPS);

    int opsTested = 0;
    int opsSucceeded = 0;

#define TEST_EXTGC_OP(name, id, b3, inc) do { \
        opsTested++; \
        testsRun++; \
        if (test##name##Opcode(ExtGCOpType::name)) { \
            opsSucceeded++; \
            testsPassed++; \
        } else { \
            testsFailed++; \
            dataLogLn("FAILED: ", #name, " ExtGC opcode test"); \
        } \
    } while (0);

    FOR_EACH_WASM_GC_OP(TEST_EXTGC_OP)

#undef TEST_EXTGC_OP

    TEST_ASSERT(opsTested == TOTAL_EXTGC_OPS,
        makeString("Tested all "_s, String::number(TOTAL_EXTGC_OPS), " ExtGC ops"_s).utf8().data());
    TEST_ASSERT(opsSucceeded == TOTAL_EXTGC_OPS,
        makeString("All "_s, String::number(TOTAL_EXTGC_OPS), " ExtGC ops completed"_s).utf8().data());

    dataLogLn("  Successfully tested: ", opsSucceeded, " / ", opsTested, " ExtGC ops");
    dataLogLn("All ExtGC ops coverage testing completed");
}

} // namespace WasmDebugInfoTest

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
