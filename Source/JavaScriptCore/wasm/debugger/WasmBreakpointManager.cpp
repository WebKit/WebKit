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

bool BreakpointManager::hasOneTimeBreakpoints()
{
    Locker locker { m_lock };
    return !m_oneTimeBreakpoints.isEmpty();
}

Ref<Breakpoint> BreakpointManager::ensurePatched(const ModuleInformation& owner, uint8_t* pc)
{
    RELEASE_ASSERT(pc);
    if (auto it = m_breakpoints.find(pc); it != m_breakpoints.end()) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] Reusing the patch at ", RawPointer(pc));
        return it->value;
    }

    Ref<Breakpoint> breakpoint = Breakpoint::create(owner, pc);
    breakpoint->patchBreakpoint();
    dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] Patched the byte at ", RawPointer(pc));
    return m_breakpoints.set(pc, WTF::move(breakpoint)).iterator->value;
}

void BreakpointManager::releasePatchIfUnused(uint8_t* pc)
{
    auto it = m_breakpoints.find(pc);
    RELEASE_ASSERT(it != m_breakpoints.end());
    if (it->value->siteCount || m_oneTimeBreakpoints.contains(pc))
        return;

    dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] Restoring ", it->value);
    it->value->restorePatch();
    m_breakpoints.remove(it);
}

void BreakpointManager::setStepBreakpoint(const ModuleInformation& owner, uint8_t* pc)
{
    Locker locker { m_lock };
    ensurePatched(owner, pc);
    m_oneTimeBreakpoints.add(pc);
}

RefPtr<Breakpoint> BreakpointManager::breakpointAt(const uint8_t* pc)
{
    Locker locker { m_lock };
    return m_breakpoints.get(const_cast<uint8_t*>(pc));
}

void BreakpointManager::setBreakpointAt(VirtualAddress address, const ModuleInformation& owner, uint8_t* pc)
{
    Locker locker { m_lock };
    // Re-arming an existing site is a no-op. An address names one instance's view of one byte and
    // IDs are never reused, so it always resolves to this pc.
    if (auto it = m_addressToBreakpoint.find(address); it != m_addressToBreakpoint.end()) {
        RELEASE_ASSERT(it->value->pc == pc);
        return;
    }
    Ref<Breakpoint> breakpoint = ensurePatched(owner, pc);
    breakpoint->siteCount++;
    m_addressToBreakpoint.add(address, breakpoint.copyRef());
    dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] Site added at ", address, " -> ", breakpoint.get());
}

bool BreakpointManager::removeSiteImpl(VirtualAddress address)
{
    // Resolved from the address LLDB installed the site through rather than from a live instance:
    // the bytecode outlives the instance that named it.
    auto it = m_addressToBreakpoint.find(address);
    if (it == m_addressToBreakpoint.end())
        return false;

    Ref<Breakpoint> breakpoint = it->value;
    m_addressToBreakpoint.remove(it);
    // Sibling instances hold their own sites on the same byte, so the patch outlives every
    // removal but the last.
    RELEASE_ASSERT(breakpoint->siteCount);
    breakpoint->siteCount--;
    releasePatchIfUnused(breakpoint->pc);
    return true;
}

bool BreakpointManager::removeBreakpointAt(VirtualAddress address)
{
    Locker locker { m_lock };
    return removeSiteImpl(address);
}

void BreakpointManager::removeSitesForInstance(uint32_t instanceId)
{
    Locker locker { m_lock };
    Vector<VirtualAddress> staleSites;
    for (const auto& pair : m_addressToBreakpoint) {
        if (pair.key.instanceId() == instanceId)
            staleSites.append(pair.key);
    }
    for (VirtualAddress address : staleSites) {
        dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] Dropping site ", address, " of collected instance ", instanceId);
        removeSiteImpl(address);
    }
}

OpType BreakpointManager::originalOpcodeAt(const uint8_t* pc)
{
    Locker locker { m_lock };
    auto it = m_breakpoints.find(const_cast<uint8_t*>(pc));
    if (it == m_breakpoints.end())
        return static_cast<OpType>(*pc);
    return it->value->originalBytecode;
}

std::optional<BreakpointManager::TrapAction> BreakpointManager::trapActionFor(const uint8_t* pc, VirtualAddress hitAddress)
{
    Locker locker { m_lock };
    auto it = m_breakpoints.find(const_cast<uint8_t*>(pc));
    if (it == m_breakpoints.end())
        return std::nullopt;

    TrapAction action { it->value->originalBytecode, std::nullopt };
    // A site names one instance; a sibling sharing the patched byte has no breakpoint here. A
    // site wins over a step at the same byte, so a step never masks a user breakpoint's reason.
    if (m_addressToBreakpoint.get(hitAddress) == it->value.ptr())
        action.stopType = Breakpoint::Type::Regular;
    else if (m_oneTimeBreakpoints.contains(const_cast<uint8_t*>(pc)))
        action.stopType = Breakpoint::Type::Step;
    return action;
}

void BreakpointManager::clearAllOneTimeBreakpoints()
{
    Locker locker { m_lock };
    // Cleared first so releasePatchIfUnused can free unreferenced patches.
    auto steppedPCs = std::exchange(m_oneTimeBreakpoints, { });
    for (uint8_t* pc : steppedPCs)
        releasePatchIfUnused(pc);
    dataLogLnIf(Options::verboseWasmDebugger(), "[BreakpointManager] Cleared all one-time breakpoints");
}

void BreakpointManager::clearAllBreakpoints()
{
    Locker locker { m_lock };
    for (auto& [_, breakpoint] : m_breakpoints)
        breakpoint->restorePatch();
    m_breakpoints.clear();
    m_oneTimeBreakpoints.clear();
    m_addressToBreakpoint.clear();
}

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
