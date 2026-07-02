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

#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <WebCore/InspectorWebAgentBase.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/UncachedLoadType.h>
#include <wtf/AbstractCanMakeCheckedPtr.h>
#include <wtf/Forward.h>

namespace WebCore {
class CachedResource;
class Document;
class DocumentLoader;
class DocumentThreadableLoader;
class NetworkLoadMetrics;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class SharedBuffer;
}

namespace Inspector {

// NetworkAgentInstrumentation is the abstract interface for network instrumentation hooks.
// Both InspectorNetworkAgent (WebCore) and NetworkAgentProxy (WebKit) implement this
// interface. InstrumentingAgents has a separate slot for the proxy so that both the
// in-process agent and the cross-process proxy can coexist.
//
// Named "Instrumentation" (not "AgentBase") to signal this is a hook contract rather than
// a shared-state base class -- subclasses should not share member variables through this.
// This design is intentional: ProxyingNetworkAgent in the UIProcess proxies protocol
// commands via IPC and doesn't share any state with InspectorNetworkAgent in the WebProcess.
//
// It inherits InspectorWebAgentBase so subclasses can register with AgentRegistry,
// and AbstractCanMakeCheckedPtr so InstrumentingAgents can hold CheckedPtr to it.
// Concrete subclasses must use CanMakeCheckedPtr and WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR.
class NetworkAgentInstrumentation : public WebCore::InspectorAgentBase, public AbstractCanMakeCheckedPtr {
public:
    ~NetworkAgentInstrumentation() override = default;

    virtual CommandResult<void> enable() = 0;
    virtual CommandResult<void> disable() = 0;

    virtual void willSendRequest(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, WebCore::ResourceRequest&, const WebCore::ResourceResponse& redirectResponse, const WebCore::CachedResource*, WebCore::ResourceLoader*) = 0;
    virtual void willSendRequestOfType(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, WebCore::ResourceRequest&, Inspector::UncachedLoadType) = 0;
    virtual void didReceiveResponse(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, const WebCore::ResourceResponse&, WebCore::ResourceLoader*) = 0;
    virtual void didReceiveData(WebCore::ResourceLoaderIdentifier, const WebCore::SharedBuffer*, int expectedDataLength, int encodedDataLength) = 0;
    virtual void didFinishLoading(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, const WebCore::NetworkLoadMetrics&, WebCore::ResourceLoader*) = 0;
    virtual void didFailLoading(WebCore::ResourceLoaderIdentifier, WebCore::DocumentLoader*, const WebCore::ResourceError&) = 0;
    virtual void didReceiveScriptResponse(WebCore::ResourceLoaderIdentifier) = 0;
    virtual void setInitialScriptContent(WebCore::ResourceLoaderIdentifier, const String& sourceString) = 0;
    virtual void didLoadResourceFromMemoryCache(WebCore::DocumentLoader*, WebCore::CachedResource&) = 0;
    virtual void didReceiveThreadableLoaderResponse(WebCore::ResourceLoaderIdentifier, WebCore::DocumentThreadableLoader&) = 0;
    virtual void willDestroyCachedResource(WebCore::CachedResource&) = 0;
    virtual void mainFrameNavigated(WebCore::DocumentLoader&) = 0;

protected:
    NetworkAgentInstrumentation(WebCore::WebAgentContext& context)
        : InspectorAgentBase("Network"_s, context)
    {
    }
};

} // namespace Inspector
