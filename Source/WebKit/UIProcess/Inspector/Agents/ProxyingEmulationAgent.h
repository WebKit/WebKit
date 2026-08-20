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

#pragma once

#include "InspectorBackendSyncState.h"
#include "WebPageInspectorAgentBase.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebKit {
class WebProcessProxy;
}

namespace Inspector {

// UIProcess-side owner of the inspected page's emulation overrides under Site Isolation.
//
// The overrides are PAGE-level, not per-frame: this agent holds them in its own
// InspectorBackendSyncState and broadcasts the whole typed snapshot to every WebContent process
// hosting a frame of the page, via the WebInspectorBackend::SetEmulationOverrides IPC. A process
// that joins the page later -- a cross-origin frame spawn or a process swap -- inherits the same
// snapshot from enableInstrumentationForProcess(), so a late-joining process is never left on the
// real platform environment while its siblings are emulated. See webkit.org/b/308897.
//
// Unlike ProxyingPageAgent this agent receives no IPC of its own; it is a pure source of the
// override state, so it needs no MessageReceiver registration.
class ProxyingEmulationAgent final : public RefCounted<ProxyingEmulationAgent>, public WebKit::InspectorAgentBase, public EmulationBackendDispatcherHandler, public CanMakeCheckedPtr<ProxyingEmulationAgent> {
    WTF_MAKE_NONCOPYABLE(ProxyingEmulationAgent);
    WTF_MAKE_TZONE_ALLOCATED(ProxyingEmulationAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ProxyingEmulationAgent);
public:
    ProxyingEmulationAgent(WebKit::WebPageAgentContext&);
    ~ProxyingEmulationAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend() override;
    void willDestroyFrontendAndBackend(DisconnectReason) override;

    bool isEnabled() const { return m_enabled; }
    void enableInstrumentationForProcess(WebKit::WebProcessProxy&, WebCore::PageIdentifier);
    void disableInstrumentationForProcess(WebKit::WebProcessProxy&, WebCore::PageIdentifier);

    // EmulationBackendDispatcherHandler
    CommandResult<void> enable() final;
    CommandResult<void> disable() final;
    CommandResult<void> setEmulatedMedia(const String&) final;

private:
    // Invoke a callback for each WebContent process currently hosting a frame of the inspected page,
    // with the per-process page identifier. Used to broadcast a newly-set override to every existing
    // process; processes that join later instead inherit it via m_syncState at the join seam.
    void forEachInstrumentedProcess(NOESCAPE const Function<void(WebKit::WebProcessProxy&, WebCore::PageIdentifier)>&);

    const Ref<EmulationBackendDispatcher> m_backendDispatcher;
    WeakRef<WebKit::WebPageProxy> m_inspectedPage;

    bool m_enabled { false };
    HashMap<std::pair<WebCore::ProcessIdentifier, WebCore::PageIdentifier>, unsigned> m_instrumentedProcessPageCounts;

    // Keep each tracked WebProcessProxy alive while it is registered here, so the broadcast in
    // setEmulatedMedia() can always resolve a process that has swapped out but still hosts a frame.
    HashMap<WebCore::ProcessIdentifier, Ref<WebKit::WebProcessProxy>> m_pinnedInstrumentedProcesses;

    // Source-side counterpart to the WebProcess EmulationManager sink; sends the typed overrides to
    // late-joining processes at the join seam. See webkit.org/b/308897.
    InspectorBackendSyncState m_syncState;
};

} // namespace Inspector
