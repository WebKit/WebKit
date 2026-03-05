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
#include "WasmBreakpointManager.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "Options.h"
#include <wtf/DataLog.h>
#include <wtf/RawPointer.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {
namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(BreakpointManager);

BreakpointManager::~BreakpointManager()
{
    clearAllBreakpoints();
}

bool BreakpointManager::hasBreakpoints()
{
    Locker locker { m_lock };
    return !m_breakpoints.isEmpty();
}

bool BreakpointManager::hasOneTimeBreakpoints()
{
    Locker locker { m_lock };
    return !m_oneTimeBreakpoints.isEmpty();
}

void BreakpointManager::setBreakpoint(VirtualAddress address, Breakpoint&& breakpoint)
{
    Locker locker { m_lock };
    breakpoint.patchBreakpoint();
    dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] setBreakpoint ", breakpoint, " at moduleAddress:", address);
    if (breakpoint.isOneTimeBreakpoint())
        m_oneTimeBreakpoints.add(address);
    m_breakpoints.set(address, WTF::move(breakpoint));
}

Breakpoint* BreakpointManager::findBreakpoint(VirtualAddress address)
{
    Locker locker { m_lock };
    if (auto it = m_breakpoints.find(address); it != m_breakpoints.end())
        return &it->value;
    return nullptr;
}

bool BreakpointManager::removeBreakpointImpl(VirtualAddress address)
{
    auto it = m_breakpoints.find(address);
    RELEASE_ASSERT(it != m_breakpoints.end());
    dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] Removing breakpoint ", it->value, " at ", address);
    it->value.restorePatch();
    m_breakpoints.remove(it);
    return true;
}

bool BreakpointManager::removeBreakpoint(VirtualAddress address)
{
    Locker locker { m_lock };
    bool removed = removeBreakpointImpl(address);
    RELEASE_ASSERT(removed);
    return true;
}

void BreakpointManager::clearAllOneTimeBreakpoints()
{
    Locker locker { m_lock };
    for (VirtualAddress address : m_oneTimeBreakpoints)
        removeBreakpointImpl(address);
    m_oneTimeBreakpoints.clear();
    dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] Cleared all one-time breakpoints");
}

void BreakpointManager::clearAllBreakpoints()
{
    Locker locker { m_lock };
    for (auto& [_, breakpoint] : m_breakpoints)
        breakpoint.restorePatch();
    m_breakpoints.clear();
    RELEASE_ASSERT(m_oneTimeBreakpoints.isEmpty());
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
