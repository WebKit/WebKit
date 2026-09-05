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

void BreakpointManager::setBreakpoint(uint8_t* pc, Breakpoint::Type type)
{
    Locker locker { m_lock };
    if (m_breakpoints.contains(pc)) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] setBreakpoint keeping the existing breakpoint at ", RawPointer(pc));
        return;
    }

    Breakpoint breakpoint(pc, type);
    breakpoint.patchBreakpoint();
    dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] setBreakpoint ", breakpoint);
    if (breakpoint.isOneTimeBreakpoint())
        m_oneTimeBreakpoints.add(pc);
    m_breakpoints.set(pc, WTF::move(breakpoint));
}

Breakpoint* BreakpointManager::findBreakpoint(const uint8_t* pc)
{
    Locker locker { m_lock };
    if (auto it = m_breakpoints.find(const_cast<uint8_t*>(pc)); it != m_breakpoints.end())
        return &it->value;
    return nullptr;
}

void BreakpointManager::removeBreakpointImpl(uint8_t* pc)
{
    auto it = m_breakpoints.find(pc);
    RELEASE_ASSERT(it != m_breakpoints.end());
    dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] Removing breakpoint ", it->value);
    it->value.restorePatch();
    m_breakpoints.remove(it);
}

bool BreakpointManager::removeBreakpoint(const uint8_t* pc)
{
    Locker locker { m_lock };
    uint8_t* patchedPC = const_cast<uint8_t*>(pc);
    if (!m_breakpoints.contains(patchedPC))
        return false;
    m_oneTimeBreakpoints.remove(patchedPC);
    removeBreakpointImpl(patchedPC);
    return true;
}

void BreakpointManager::clearAllOneTimeBreakpoints()
{
    Locker locker { m_lock };
    for (uint8_t* pc : m_oneTimeBreakpoints)
        removeBreakpointImpl(pc);
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
