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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProxyingEmulationAgent.h"

#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <wtf/Function.h>
#include <wtf/TZoneMallocInlines.h>

namespace Inspector {

using namespace WebKit;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProxyingEmulationAgent);

ProxyingEmulationAgent::ProxyingEmulationAgent(WebPageAgentContext& context)
    : InspectorAgentBase("Emulation"_s, context)
    , m_backendDispatcher(EmulationBackendDispatcher::create(context.backendDispatcher, this))
    , m_inspectedPage(context.inspectedPage)
{
}

ProxyingEmulationAgent::~ProxyingEmulationAgent() = default;

void ProxyingEmulationAgent::forEachInstrumentedProcess(NOESCAPE const Function<void(WebProcessProxy&, PageIdentifier)>& callback)
{
    // Iterate the registration map rather than forEachWebContentProcess(): under Site Isolation a
    // process may have swapped out while still hosting a frame of the page, so it would no longer be
    // enumerated. The pinned refs keep each WebProcessProxy alive for the send.
    for (auto& [key, count] : m_instrumentedProcessPageCounts) {
        auto [processID, pageID] = key;
        auto it = m_pinnedInstrumentedProcesses.find(processID);
        ASSERT(it != m_pinnedInstrumentedProcesses.end());
        if (it == m_pinnedInstrumentedProcesses.end())
            continue;
        callback(it->value.get(), pageID);
    }
}

// MARK: - Frontend lifecycle

void ProxyingEmulationAgent::didCreateFrontendAndBackend()
{
    enable();
}

void ProxyingEmulationAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    disable();
}

// MARK: - Enable / disable IPC flow

void ProxyingEmulationAgent::enableInstrumentationForProcess(WebProcessProxy& webProcess, PageIdentifier pageID)
{
    auto key = std::make_pair(webProcess.coreProcessIdentifier(), pageID);
    auto result = m_instrumentedProcessPageCounts.add(key, 0);
    if (++result.iterator->value > 1)
        return;

    m_pinnedInstrumentedProcesses.ensure(webProcess.coreProcessIdentifier(), [&] {
        return Ref { webProcess };
    });

    // Serialize the active emulation overrides to this newly-joined process so a cross-origin frame
    // spawn or process swap inherits the same config as the existing processes. See webkit.org/b/308897.
    m_syncState.sendTo(webProcess, pageID);
}

void ProxyingEmulationAgent::disableInstrumentationForProcess(WebProcessProxy& webProcess, PageIdentifier pageID)
{
    auto processID = webProcess.coreProcessIdentifier();
    auto key = std::make_pair(processID, pageID);
    auto it = m_instrumentedProcessPageCounts.find(key);
    if (it == m_instrumentedProcessPageCounts.end())
        return;

    if (--it->value > 0)
        return;

    m_instrumentedProcessPageCounts.remove(it);

    // Drop the pin once this process has no remaining page registrations.
    bool processStillHasRegistrations = false;
    for (auto& entry : m_instrumentedProcessPageCounts) {
        if (entry.key.first == processID) {
            processStillHasRegistrations = true;
            break;
        }
    }
    if (!processStillHasRegistrations)
        m_pinnedInstrumentedProcesses.remove(processID);
}

CommandResult<void> ProxyingEmulationAgent::enable()
{
    if (m_enabled)
        return { };

    Ref<WebPageProxy> inspectedPage = m_inspectedPage.get();
    Ref preferences = inspectedPage->preferences();
    if (!preferences->siteIsolationEnabled())
        return { };

    m_enabled = true;

    inspectedPage->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        Ref protectedWebProcess { webProcess };
        enableInstrumentationForProcess(protectedWebProcess, pageID);
    });

    return { };
}

CommandResult<void> ProxyingEmulationAgent::disable()
{
    if (!m_enabled)
        return { };

    m_enabled = false;

    // Clear the stored overrides first, then broadcast the now-empty snapshot so every process
    // hosting a frame of the page reverts to its real platform environment. The WebContent
    // EmulationManager re-applies against the empty snapshot, which is a re-evaluation against real
    // media rather than a replay of a prior override. See webkit.org/b/308898.
    m_syncState.clear();
    forEachInstrumentedProcess([&](WebProcessProxy& process, PageIdentifier pageID) {
        m_syncState.sendTo(process, pageID);
    });

    m_instrumentedProcessPageCounts.clear();
    m_pinnedInstrumentedProcesses.clear();

    return { };
}

// MARK: - Command handlers

CommandResult<void> ProxyingEmulationAgent::setEmulatedMedia(const String& media)
{
    if (!m_enabled)
        return { };

    // Store the override so a late-joining process inherits it at the join seam, then broadcast it
    // to every process already hosting a frame of the inspected page via the same
    // SetEmulationOverrides message the join path sends. The WebContent WebInspectorBackend applies
    // it to its local frames' media queries. See webkit.org/b/308898.
    m_syncState.setEmulatedMedia(media);

    forEachInstrumentedProcess([&](WebProcessProxy& process, PageIdentifier pageID) {
        m_syncState.sendTo(process, pageID);
    });

    return { };
}

} // namespace Inspector
