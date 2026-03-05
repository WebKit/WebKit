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

#pragma once

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include "InitializeThreading.h"
#include "JSLock.h"
#include "VM.h"
#include "WasmIPIntPlan.h"
#include "WasmModuleDebugInfo.h"
#include "WasmModuleInformation.h"
#include "WasmOps.h"
#include "WasmTypeDefinition.h"
#include "WasmWorklist.h"
#include <initializer_list>
#include <utility>
#include <wtf/DataLog.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/HexNumber.h>
#include <wtf/ListDump.h>
#include <wtf/Vector.h>

namespace WasmDebugInfoTest {

using OffsetToNextInstructions = UncheckedKeyHashMap<uint32_t, UncheckedKeyHashSet<uint32_t>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;

class SourceModule {
public:
    Vector<uint8_t> bytes;
    uint32_t functionDataStart; // Offset where function payload starts (after body size, points to locals count)
    uint32_t bytecodeStart; // Offset where actual bytecode starts (after locals declaration)

    template<typename OpcodeType>
    bool parseAndVerifyDebugInfo(OpcodeType expectedOpcode, std::initializer_list<std::pair<uint32_t, std::initializer_list<uint32_t>>> mappings) const;


    static SourceModule create();
    SourceModule& withFunctionType(Vector<uint8_t> params, Vector<uint8_t> results);
    SourceModule& withAdditionalType(Vector<uint8_t> params, Vector<uint8_t> results);
    SourceModule& withLocals(uint32_t count, uint8_t type);
    SourceModule& withGlobals(bool isMutable = true);
    SourceModule& withTable();
    SourceModule& withMemory();
    SourceModule& withFunctionBody(const Vector<uint8_t>& body);
    SourceModule build();

private:
    Vector<uint8_t> m_functionBody;
    Vector<uint8_t> m_params;
    Vector<uint8_t> m_results;
    Vector<Vector<uint8_t>> m_additionalTypes; // Additional types for blocks/etc
    Vector<uint8_t> m_globalSection;
    Vector<uint8_t> m_tableSection;
    Vector<uint8_t> m_memorySection;
    Vector<uint8_t> m_localsDeclaration;
    JSC::VM* m_vm { nullptr };
    bool m_isBuilt { false };
};

extern int testsRun;
extern int testsPassed;
extern int testsFailed;

extern JSC::VM* gTestVM;

static inline uint8_t toLEB128(JSC::Wasm::TypeKind kind) { return static_cast<uint8_t>(kind) & 0x7f; }

#define TEST_ASSERT(condition, message) \
    do { \
        WasmDebugInfoTest::testsRun++; \
        if (condition) { \
            WasmDebugInfoTest::testsPassed++; \
            dataLogLn("PASS: ", message); \
        } else { \
            WasmDebugInfoTest::testsFailed++; \
            dataLogLn("FAIL: ", message, " (", #condition, ")"); \
        } \
    } while (0)

#define COUNT_OP(...) +1
static constexpr int TOTAL_SPECIAL_OPS = 0 FOR_EACH_WASM_SPECIAL_OP(COUNT_OP);
static constexpr int TOTAL_CONTROL_OPS = 0 FOR_EACH_WASM_CONTROL_FLOW_OP(COUNT_OP);
static constexpr int TOTAL_UNARY_OPS = 0 FOR_EACH_WASM_UNARY_OP(COUNT_OP);
static constexpr int TOTAL_BINARY_OPS = 0 FOR_EACH_WASM_BINARY_OP(COUNT_OP);
static constexpr int TOTAL_MEMORY_LOAD_OPS = 0 FOR_EACH_WASM_MEMORY_LOAD_OP(COUNT_OP);
static constexpr int TOTAL_MEMORY_STORE_OPS = 0 FOR_EACH_WASM_MEMORY_STORE_OP(COUNT_OP);
static constexpr int TOTAL_EXTGC_OPS = 0 FOR_EACH_WASM_GC_OP(COUNT_OP);
#undef COUNT_OP

SourceModule createWasmModuleWithBytecode(const Vector<uint8_t>& functionBody);
SourceModule createWasmModuleWithLocals(const Vector<uint8_t>& functionBody);
SourceModule createWasmModuleWithGlobals(const Vector<uint8_t>& functionBody, bool mutableGlobal = true);
SourceModule createWasmModuleWithTable(const Vector<uint8_t>& functionBody);
SourceModule createWasmModuleWithMemory(const Vector<uint8_t>& functionBody);

void testAllControlFlowOps();
void testAllUnaryOps();
void testAllBinaryOps();
void testAllMemoryOps();
void testAllSpecialOps();
void testAllExtGCOps();

bool verifyOpcodeInModule(const SourceModule&, JSC::Wasm::OpType expectedOpcode);

} // namespace WasmDebugInfoTest

int testWasmDebugInfo();

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
