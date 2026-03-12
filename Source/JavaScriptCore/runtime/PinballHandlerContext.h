/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/FPRInfo.h>
#include <JavaScriptCore/GPRInfo.h>
#include <JavaScriptCore/JSPIContext.h>

namespace JSC {

class EvacuatedStackSlice;
class Exception;
class JSCallee;
class JSFunctionWithFields;
class JSGlobalObject;
class Register;

// Allocated on the stack by assembly entry points of fulfill and reject handlers of a suspension promise.
// Holds all state shared by assembly and C++ code implementing the fulfillment or rejection.

struct PinballHandlerContext final {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    static constexpr size_t NumberOfWasmArgumentRegisters = GPRInfo::numberOfArgumentRegisters + FPRInfo::numberOfArgumentRegisters;

#if ASSERT_ENABLED
    size_t magic;
#endif
    JSGlobalObject* globalObject;
    VM* vm;
    JSFunctionWithFields* handler;
    EvacuatedStackSlice* slice;
    size_t sliceByteSize;
    JSPIContext jspiContext;
    // Callee saves to restore before entering the evacuated code (points into the PinballCompletion held by the handler).
    CPURegister* evacuatedCalleeSaves;
    // Callee saves captured on entry into the handler.
    CPURegister handlerCalleeSaves[NUMBER_OF_CALLEE_SAVES_REGISTERS];
    // A spill buffer for Wasm argument registers to carry their state between slices.
    // The first element is also used to store the argument to pass into the top WasmToJS frame
    // and the return value returned by the bottom JSToWasm frame.
    CPURegister arguments[NumberOfWasmArgumentRegisters];
    // The following fields are only used for handling rejections.
    JSCallee* zombieFrameCallee;
    Exception* exception;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
