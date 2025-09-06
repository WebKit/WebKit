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
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>

namespace JSC {
namespace Wasm {

// Implementation of non-inline PacketParser functions


// split - Unified packet parsing function for GDB Remote Protocol
//
// This function splits an input string using a sequence of delimiters with exact matching requirements.
// It's designed to handle the structured nature of GDB Remote Protocol packets where delimiters
// appear in specific positions and quantities.
//
// Parameters:
//   input: The packet string to split (e.g., "Z0,400000000000018b,1")
//   delimiters: String containing delimiters in sequence order (e.g., ",," for two commas)
//
// Returns:
//   std::optional<Vector<StringView>>: Vector of string parts if ALL delimiters found, nullopt otherwise
//
// Examples:
//   split("Z0,400000000000018b,1", ",,") -> ["Z0", "400000000000018b", "1"]
//   split("qWasmLocal:0:5", "::") -> ["qWasmLocal", "0", "5"]
//   split("m1000,20", ",") -> ["m1000", "20"]
//   split("invalid", ",,") -> nullopt (missing delimiters)
//
// Design Philosophy:
//   - Exact matching: ALL specified delimiters must be found or function returns nullopt
//   - Sequential processing: Delimiters are found in the order specified in the delimiters string
//   - No partial results: Either complete success or complete failure
//   - Performance: Single pass through input string with minimal allocations
Vector<StringView> packetSplit(StringView packet, StringView delimiters)
{
    Vector<StringView> result;
    
    if (packet.isEmpty() || delimiters.isEmpty())
        return result;
    
    StringView current = packet;
    
    // Split on each delimiter in sequence - must find ALL delimiters for exact matching
    for (size_t i = 0; i < delimiters.length(); ++i) {
        char delimiter = delimiters[i];
        size_t pos = current.find(delimiter);
        
        if (pos == notFound) {
            // If any delimiter is missing, return empty vector
            // This ensures robust parsing for structured GDB protocol packets
            return Vector<StringView>();
        }
        
        // Extract the part before this delimiter
        result.append(current.substring(0, pos));
        
        // Move past the delimiter for next iteration
        current = current.substring(pos + 1);
    }
    
    // Add the final part after all delimiters have been processed
    result.append(current);
    return result;
}

bool getWasmReturnPC(CallFrame* currentFrame, uint8_t*& returnPC, VirtualAddress& virtualReturnPC)
{
    CallFrame* callerFrame = currentFrame->callerFrame();
    auto* caller = callerFrame->callee().asNativeCallee();
    if (caller->category() != NativeCallee::Category::Wasm)
        return false;

    auto* wasmCaller = static_cast<const Wasm::Callee*>(caller);
    if (wasmCaller->compilationMode() != Wasm::CompilationMode::IPIntMode)
        return false;

    // Read the WebAssembly return PC from IPInt's saved PC location (cfr-8)
    // This contains the WebAssembly bytecode address where execution should continue in the caller
    uint8_t* pcLocation = reinterpret_cast<uint8_t*>(currentFrame) - 8;
    memcpy(&returnPC, pcLocation, sizeof(returnPC));

    JSWebAssemblyInstance* callerInstance = callerFrame->wasmInstance();
    auto* ipintCaller = static_cast<const Wasm::IPIntCallee*>(wasmCaller);
    virtualReturnPC = VirtualAddress::toVirtual(callerInstance, ipintCaller->functionIndex(), returnPC);
    return true;
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
