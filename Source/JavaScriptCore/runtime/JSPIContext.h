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

#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/JSPromise.h>
#include <JavaScriptCore/PinballCompletion.h>
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>

namespace JSC {

class VM;

// Tracks the state of an active JSPI (JavaScript Promise Integration) activation. An
// activation begins when either a WebAssembly.promising() wrapper or a PinballCompletion
// fulfillment handler starts executing Wasm code which may potentially trigger a JSPI
// suspension. It ends when that code stops executing by suspending, returning normally,
// or throwing.
//
// JSPIContext is always allocated on the C++ stack. On creation, it is registered with
// the VM as the topJSPIContext. When the JSPI activation ends, the context must be
// deactivated by calling .deactivate(), which deregisters it with the VM.
//
// The 'limitFrame' identifies the call frame of the promising wrapper or fulfillment
// handler that created this context. When a WebAssembly.Suspending wrapper triggers
// suspension, the stack walker uses limitFrame to determine which frames to evacuate:
// everything between the suspending call site and this frame. After evacuation, the
// suspending function "teleports" by returning into the limitFrame, skipping the
// evacuated frames.
//
// The 'completion' field is set by the suspending function if suspension actually occurs.
// It points to the PinballCompletion object that owns the evacuated stack slices and will
// orchestrate their replay when the suspension promise resolves.
struct JSPIContext {
    WTF_FORBID_HEAP_ALLOCATION_ALLOWING_PLACEMENT_NEW;
    WTF_MAKE_NONCOPYABLE(JSPIContext);
    WTF_MAKE_NONMOVABLE(JSPIContext);

public:
    enum class Purpose {
        Promising, // Started in a 'WebAssembly.promising()' wrapper function.
        Completing // Started in a PinballCompletion fulfillment handler.
    };

    JSPIContext(Purpose, VM&, CallFrame*, JSPromise*);
    ~JSPIContext();

    void deactivate(VM&);

    Purpose purpose;
    JSPIContext* previousContext;
    CallFrame* limitFrame;
    PinballCompletion* completion { nullptr };
    JSPromise* resultPromise;
};

} // namespace JSC

#else // !ENABLE(WEBASSEMBLY)

namespace JSC {

struct JSPIContext { };

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
