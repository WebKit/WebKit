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

#if ENABLE(WEBASSEMBLY)

#include "WasmDebugServerUtilities.h"
#include "WasmVirtualAddress.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class CallFrame;
class JSWebAssemblyInstance;
class JSWebAssemblyModule;

namespace Wasm {

class IPIntCallee;
class ModuleManager;

class BreakpointManager {
    WTF_MAKE_TZONE_ALLOCATED(BreakpointManager);

public:
    JS_EXPORT_PRIVATE BreakpointManager() = default;
    JS_EXPORT_PRIVATE ~BreakpointManager();

    bool hasBreakpoints() { return !m_breakpoints.isEmpty(); };

    JS_EXPORT_PRIVATE Breakpoint* findBreakpoint(VirtualAddress);
    JS_EXPORT_PRIVATE void setBreakpoint(VirtualAddress, Breakpoint&&);
    JS_EXPORT_PRIVATE bool removeBreakpoint(VirtualAddress);
    JS_EXPORT_PRIVATE void clearAllTmpBreakpoints();
    JS_EXPORT_PRIVATE void clearAllBreakpoints();

private:
    UncheckedKeyHashMap<VirtualAddress, Breakpoint> m_breakpoints;
    UncheckedKeyHashSet<VirtualAddress> m_tmpBreakpoints;
};

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
