/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2015-2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BackendResourceDataStore.h"
#include <WebCore/NetworkAgentInstrumentation.h>
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace WebCore {
class InstrumentingAgents;
class CachedResource;
class Document;
class DocumentLoader;
class DocumentThreadableLoader;
class LocalFrame;
class NetworkLoadMetrics;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {

class WebPage;

// FrameNetworkAgentProxy registers on a frame's InstrumentingAgents (via
// FrameInspectorController) so network instrumentation hooks resolve per-frame.
// Each instance forwards network events to ProxyingNetworkAgent in the
// UIProcess via IPC.
class FrameNetworkAgentProxy : public Inspector::NetworkAgentInstrumentation, public CanMakeCheckedPtr<FrameNetworkAgentProxy> {
    WTF_MAKE_TZONE_ALLOCATED(FrameNetworkAgentProxy);
    WTF_MAKE_NONCOPYABLE(FrameNetworkAgentProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FrameNetworkAgentProxy);
public:
    FrameNetworkAgentProxy(WebCore::WebAgentContext&, WebPage&, BackendResourceDataStore&);
    ~FrameNetworkAgentProxy() override;

    // AbstractCanMakeCheckedPtr overrides
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    // InspectorAgentBase (via NetworkAgentInstrumentation)
    void didCreateFrontendAndBackend() final;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) final;

    // NetworkAgentInstrumentation
    Inspector::CommandResult<void> enable() final;
    Inspector::CommandResult<void> disable() final;

    void willSendRequest(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse, const WebCore::CachedResource*, WebCore::ResourceLoader*) final;
    void willSendRequestOfType(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, WebCore::ResourceRequest&, Inspector::UncachedLoadType) final;
    void didReceiveResponse(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, const WebCore::ResourceResponse&, WebCore::ResourceLoader*) final;
    void didReceiveData(WebCore::ResourceLoaderIdentifier, const WebCore::SharedBuffer*, int expectedDataLength, int encodedDataLength) final;
    void didFinishLoading(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, const WebCore::NetworkLoadMetrics&, WebCore::ResourceLoader*) final;
    void didFailLoading(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, const WebCore::ResourceError&) final;
    void didLoadResourceFromMemoryCache(WebCore::DocumentLoader*, WebCore::CachedResource&) final;
    void didReceiveThreadableLoaderResponse(WebCore::ResourceLoaderIdentifier, WebCore::DocumentThreadableLoader&) final;
    void didReceiveScriptResponse(WebCore::ResourceLoaderIdentifier) final;
    void willDestroyCachedResource(WebCore::CachedResource&) final;

    void mainFrameNavigated(WebCore::DocumentLoader&) final;
    void setInitialScriptContent(WebCore::ResourceLoaderIdentifier, const String& sourceString) final;

private:
    WeakRef<WebPage> m_page;
    CheckedRef<BackendResourceDataStore> const m_resourcesData;

    bool m_enabled { false };
};

} // namespace WebKit
