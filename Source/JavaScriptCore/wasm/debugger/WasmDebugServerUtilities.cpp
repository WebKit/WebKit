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
#include "WasmDebugServerUtilities.h"

#if ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "CallFrame.h"
#include "JSWebAssemblyInstance.h"
#include "NativeCallee.h"
#include "WasmCallee.h"
#include "WasmVirtualAddress.h"
#include <cstring>
#include <span>
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>

namespace JSC {
namespace Wasm {

// Splits a string using a sequence of delimiters with exact matching.
// Returns empty vector if any delimiter is missing.
//
// Examples:
//   splitWithDelimiters("Z0,400000000000018b,1", ",,") -> ["Z0", "400000000000018b", "1"]
//   splitWithDelimiters("qWasmLocal:0:5", "::") -> ["qWasmLocal", "0", "5"]
//   splitWithDelimiters("invalid", ",,") -> [] (missing delimiters)
Vector<StringView> splitWithDelimiters(StringView packet, StringView delimiters)
{
    Vector<StringView> result;

    if (packet.isEmpty() || delimiters.isEmpty())
        return result;

    StringView current = packet;

    // Split on each delimiter in sequence - must find ALL delimiters for exact matching
    for (size_t i = 0; i < delimiters.length(); ++i) {
        char delimiter = delimiters[i];
        size_t pos = current.find(delimiter);
        if (pos == notFound)
            return Vector<StringView>();

        result.append(current.substring(0, pos));
        current = current.substring(pos + 1);
    }

    result.append(current);
    return result;
}

bool getWasmReturnPC(CallFrame* currentFrame, uint8_t*& returnPC, VirtualAddress& virtualReturnPC)
{
    CallFrame* callerFrame = currentFrame->callerFrame();

    if (!callerFrame->callee().isNativeCallee())
        return false;

    RefPtr caller = callerFrame->callee().asNativeCallee();
    if (caller->category() != NativeCallee::Category::Wasm)
        return false;

    RefPtr wasmCaller = uncheckedDowncast<const Wasm::Callee>(caller.get());
    if (wasmCaller->compilationMode() != Wasm::CompilationMode::IPIntMode)
        return false;

    // Read the WebAssembly return PC from IPInt's saved PC location (cfr-8)
    // This contains the WebAssembly bytecode address where execution should continue in the caller
    uint8_t* pcLocation = reinterpret_cast<uint8_t*>(currentFrame) - 8;
    returnPC = WTF::unalignedLoad<uint8_t*>(pcLocation);

    JSWebAssemblyInstance* callerInstance = callerFrame->wasmInstance();
    RefPtr ipintCaller = uncheckedDowncast<const Wasm::IPIntCallee>(wasmCaller.get());
    virtualReturnPC = VirtualAddress::toVirtual(callerInstance, ipintCaller->functionIndex(), returnPC);
    return true;
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
