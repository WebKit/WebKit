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
#include "WasmSimdCounter.h"

#if ENABLE(WEBASSEMBLY) && ENABLE(B3_JIT)

#include "WasmSIMDOpcodes.h"
#include <wtf/Atomics.h>
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

#if PLATFORM(COCOA)
#include <notify.h>
#include <unistd.h>
#endif

namespace JSC {

WasmSimdOperationCounter& WasmSimdOperationCounter::singleton()
{
    static LazyNeverDestroyed<WasmSimdOperationCounter> counter;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        counter.construct();
    });
    return counter;
}

void WasmSimdOperationCounter::dump()
{
    struct Pair {
        SIMDLaneOperation operation;
        uint64_t count;
    };

    Vector<Pair> vector;
    uint32_t used = 0;
    for (uint8_t i = 0; i < m_totalOperationCount; i++) {
        uint64_t count = m_counter[i].loadFullyFenced();
        if (count)
            used++;
        vector.append({ (SIMDLaneOperation)i, count });
    }

    std::sort(vector.begin(), vector.end(), [](Pair& a, Pair& b) {
        return b.count < a.count;
    });

    int pid = getpid();
    float coverage = used * 1.0 / m_totalOperationCount * 100;
    dataLogF("<WASM.SIMD.STAT><%d> Wasm simd operation use coverage %.2f%%.\n", pid, coverage);
    for (Pair& pair : vector)
        dataLogLn("<WASM.SIMD.STAT><", pid, ">     ", SIMDLaneOperationDump(pair.operation), ": ", pair.count);
}

void WasmSimdOperationCounter::registerAndIncrement(SIMDLaneOperation op)
{
#if PLATFORM(COCOA)
    static std::once_flag registerFlag;
    std::call_once(registerFlag, [&]() {
        dataLogF("<WASM.SIMD.STAT><%d> Registering callback for wasm simd operation statistics.\n", getpid());

        // Use `notifyutil -v -p com.apple.WebKit.wasm.simd.op.stat` to dump statistics.
        int token;
        notify_register_dispatch("com.apple.WebKit.wasm.simd.op.stat", &token, dispatch_get_main_queue(), ^(int) {
            WasmSimdOperationCounter::singleton().dump();
        });
    });
#endif

    m_counter[(uint8_t)op].exchangeAdd(1);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY) && && ENABLE(B3_JIT)
