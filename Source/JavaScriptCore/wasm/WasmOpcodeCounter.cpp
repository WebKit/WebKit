/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "WasmOpcodeCounter.h"

#if ENABLE(WEBASSEMBLY) && ENABLE(B3_JIT)
#include "WasmTypeDefinition.h"
#include <wtf/Atomics.h>
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include <notify.h>
#include <unistd.h>
#endif

namespace JSC {
namespace Wasm {

WasmOpcodeCounter& WasmOpcodeCounter::singleton()
{
    static LazyNeverDestroyed<WasmOpcodeCounter> counter;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        counter.construct();
    });
    return counter;
}


template<typename OpcodeType, typename OpcodeTypeDump, typename IsRegisteredOpcodeFunctor>
void WasmOpcodeCounter::dump(Atomic<uint64_t>* counter, NumberOfRegisteredOpcodes numberOfRegisteredOpcode, CounterSize counterSize, const IsRegisteredOpcodeFunctor& isRegisteredOpcodeFunctor, const char* prefix, const char* suffix)
{
    struct Pair {
        OpcodeType opcode;
        uint64_t count;
    };

    Vector<Pair> vector;
    uint64_t usedOpcode = 0;
    for (size_t i = 0; i < counterSize; i++) {
        if (!isRegisteredOpcodeFunctor((OpcodeType)i))
            continue;

        uint64_t count = counter[i].loadFullyFenced();
        if (count)
            usedOpcode++;
        vector.append({ (OpcodeType)i, count });
    }

    std::sort(vector.begin(), vector.end(), [](Pair& a, Pair& b) {
        return b.count < a.count;
    });

    int pid = 0;
#if PLATFORM(COCOA)
    pid = getpid();
#endif
    float coverage = usedOpcode * 1.0 / numberOfRegisteredOpcode * 100;
    dataLogF("%s<%d> %s use coverage %.2f%%.\n", prefix, pid, suffix, coverage);
    for (Pair& pair : vector)
        dataLogLn(prefix, "<", pid, ">    ", OpcodeTypeDump(pair.opcode), ": ", pair.count);
}

void WasmOpcodeCounter::dump()
{
    dump<ExtSIMDOpType, ExtSIMDOpTypeDump>(m_extendedSIMDOpcodeCounter, m_extendedSIMDOpcodeInfo.first, m_extendedSIMDOpcodeInfo.second, isRegisteredWasmExtendedSIMDOpcode, "<WASM.EXT.SIMD.OP.STAT>", "wasm extended SIMD opcode");

    dump<ExtAtomicOpType, ExtAtomicOpTypeDump>(m_extendedAtomicOpcodeCounter, m_extendedAtomicOpcodeInfo.first, m_extendedAtomicOpcodeInfo.second, isRegisteredExtenedAtomicOpcode, "<WASM.EXT.ATOMIC.OP.STAT>", "wasm extended atomic opcode");

    dump<ExtGCOpType, ExtGCOpTypeDump>(m_GCOpcodeCounter, m_GCOpcodeInfo.first, m_GCOpcodeInfo.second, isRegisteredGCOpcode, "<WASM.GC.OP.STAT>", "wasm GC opcode");

    dump<OpType, OpTypeDump>(m_baseOpcodeCounter, m_baseOpcodeInfo.first, m_baseOpcodeInfo.second, isRegisteredBaseOpcode, "<WASM.BASE.OP.STAT>", "wasm base opcode");
}

void WasmOpcodeCounter::registerDispatch()
{
#if PLATFORM(COCOA)
    static std::once_flag registerFlag;
    std::call_once(registerFlag, [&]() {
        int pid = getpid();
        const char* key = "com.apple.WebKit.wasm.op.stat";
        dataLogF("<WASM.OP.STAT><%d> Registering callback for wasm opcode statistics.\n", pid);
        dataLogF("<WASM.OP.STAT><%d> Use `notifyutil -v -p %s` to dump statistics.\n", pid, key);

        int token;
        notify_register_dispatch(key, &token, dispatch_get_main_queue(), ^(int) {
            WasmOpcodeCounter::singleton().dump();
        });
    });
#endif
}

void WasmOpcodeCounter::increment(ExtSIMDOpType op)
{
    registerDispatch();
    m_extendedSIMDOpcodeCounter[(uint8_t)op].exchangeAdd(1);
}

void WasmOpcodeCounter::increment(ExtAtomicOpType op)
{
    registerDispatch();
    m_extendedAtomicOpcodeCounter[(uint8_t)op].exchangeAdd(1);
}

void WasmOpcodeCounter::increment(ExtGCOpType op)
{
    registerDispatch();
    m_GCOpcodeCounter[(uint8_t)op].exchangeAdd(1);
}

void WasmOpcodeCounter::increment(OpType op)
{
    registerDispatch();
    m_baseOpcodeCounter[(uint8_t)op].exchangeAdd(1);
}

} // namespace JSC
} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY) && && ENABLE(B3_JIT)
