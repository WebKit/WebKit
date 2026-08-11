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

class Exception;
class PinballCompletion;
class CallFrame;
class JSCallee;
class JSFunctionWithFields;
class JSGlobalObject;

// Allocated on the stack by assembly entry points of fulfill and reject handlers of a suspension promise.
// Holds all state shared by assembly and C++ code implementing the fulfillment or rejection.

struct PinballHandlerContext final {
    WTF_FORBID_HEAP_ALLOCATION_ALLOWING_PLACEMENT_NEW;
public:
    PinballHandlerContext(JSGlobalObject*, CallFrame*);

    static constexpr size_t NumberOfWasmArgumentRegisters = GPRInfo::numberOfArgumentRegisters + FPRInfo::numberOfArgumentRegisters;

#if ASSERT_ENABLED
    static constexpr size_t expectedMagic = 0xBA11FEED;
    size_t magic { expectedMagic };
#endif
    JSGlobalObject* globalObject;
    VM* vm;
    JSFunctionWithFields* handler;
    PinballCompletion* pinball;
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
    // Set to non-zero by assembly when stack overflow is detected during slice implantation.
    size_t stackOverflowDetected { 0 };
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
