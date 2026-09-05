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

#pragma once

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include "WasmDebugServerUtilities.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Wasm {

// Breakpoints are keyed by the bytecode they patch, not by a virtual address. That bytecode
// belongs to the module and every instance of it executes the same buffer, so a breakpoint set
// through one instance necessarily stops all of them; the stop is reported against whichever
// instance ran into it. Keying on the patch keeps that one-to-one: LLDB resolving a source
// breakpoint into several instances of one module lands on a single breakpoint here, and taking
// it away through any of them restores the bytecode — which is what lets LLDB step over it.
class JS_EXPORT_PRIVATE BreakpointManager {
    WTF_MAKE_TZONE_ALLOCATED(BreakpointManager);

public:
    BreakpointManager() = default;
    ~BreakpointManager();

    bool hasBreakpoints();
    bool hasOneTimeBreakpoints();

    Breakpoint* findBreakpoint(const uint8_t* pc);
    // No-op when a breakpoint already patches this pc; the existing one wins.
    void setBreakpoint(uint8_t* pc, Breakpoint::Type);
    // False when no breakpoint patched this pc.
    bool removeBreakpoint(const uint8_t* pc);
    void clearAllOneTimeBreakpoints();
    void clearAllBreakpoints();

private:
    void removeBreakpointImpl(uint8_t* pc) WTF_REQUIRES_LOCK(m_lock);

    mutable Lock m_lock;
    UncheckedKeyHashMap<uint8_t*, Breakpoint> m_breakpoints WTF_GUARDED_BY_LOCK(m_lock);
    UncheckedKeyHashSet<uint8_t*> m_oneTimeBreakpoints WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
