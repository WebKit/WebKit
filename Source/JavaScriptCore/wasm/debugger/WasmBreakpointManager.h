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
#include "WasmVirtualAddress.h"
#include <optional>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Wasm {

// A bytecode patch persists while either a breakpoint site or single-step references it. Instances
// of a module share one bytecode buffer, so a breakpoint patches that buffer once for all of them.
//
// Sites are scoped to the instance their address names. LLDB gives every instance its own library
// and places a site in each, so a site says "stop this instance here", not "stop this bytecode".
// A sibling instance reaching the same patched byte has no breakpoint there and resumes through
// the patch without reporting a stop.
class JS_EXPORT_PRIVATE BreakpointManager {
    WTF_MAKE_TZONE_ALLOCATED(BreakpointManager);

public:
    BreakpointManager() = default;
    ~BreakpointManager();

    // What the interpreter should do with a trap raised by a patched byte.
    struct TrapAction {
        OpType displacedOpcode { OpType::Unreachable }; // The opcode the patch replaced; resuming dispatches it.
        // Absent when nothing at this PC belongs to the instance that reached it.
        std::optional<DebugStopReason> stopReason;
    };

    bool hasOneTimeBreakpoints();

    // Absent when no breakpoint patched this PC, i.e. the trap is a genuine `unreachable`.
    std::optional<TrapAction> trapActionFor(const uint8_t* pc, VirtualAddress hitAddress);

    OpType originalOpcodeAt(const uint8_t* pc);

    // One-time breakpoints serving a step. Not instance scoped: a step belongs to the debuggee
    // VM, which is the only one running while it is in flight.
    void setStepBreakpoint(const ModuleInformation& owner, uint8_t* pc);
    void clearAllOneTimeBreakpoints();

    // Breakpoint sites installed by LLDB (Z0/z0), scoped to the instance the address names.
    void setBreakpointAt(VirtualAddress, const ModuleInformation& owner, uint8_t* pc);
    bool removeBreakpointAt(VirtualAddress);

    // Drops the sites an instance held. LLDB unloads the library of a collected instance without
    // sending z0 for the sites in it, so nothing else releases the patch they keep alive.
    void removeSitesForInstance(uint32_t instanceId);

    void clearAllBreakpoints();

    RefPtr<Breakpoint> breakpointAt(const uint8_t* pc); // FIXME: Should be used for test only

private:
    Ref<Breakpoint> ensurePatched(const ModuleInformation& owner, uint8_t* pc) WTF_REQUIRES_LOCK(m_lock);
    void releasePatchIfUnused(Ref<Breakpoint>) WTF_REQUIRES_LOCK(m_lock);
    bool removeSiteImpl(VirtualAddress) WTF_REQUIRES_LOCK(m_lock);

    mutable Lock m_lock;
    UncheckedKeyHashMap<uint8_t*, Ref<Breakpoint>> m_breakpoints WTF_GUARDED_BY_LOCK(m_lock);
    Vector<Ref<Breakpoint>> m_oneTimeBreakpoints WTF_GUARDED_BY_LOCK(m_lock);
    UncheckedKeyHashMap<VirtualAddress, Ref<Breakpoint>> m_addressToBreakpoint WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
