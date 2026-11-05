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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#pragma once

#include <WebCore/NetworkInterception.h>
#include <WebCore/ResourceLoaderIdentifier.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class FragmentedSharedBuffer;
class ResourceLoader;
}

namespace WebKit {

class WebPage;

// The per-process end of network interception for a page. The UI process pushes the page's
// interception configuration down to every WebContent process hosting one of its frames, which
// stores it on its WebCore::Page; the sink is where loads in this process consult it.
//
// The sink also holds the loads of this process that are paused for an intercept pattern, and
// applies the resolution that the pattern's owner sends back through the UI process
// (Messages::WebPage::ResolveNetworkInterception). Under Site Isolation, Web Inspector's request
// and response interception pause here: FrameNetworkAgentProxy hands the continuation of the load
// to the sink and reports the pause to ProxyingNetworkAgent, which resolves it through
// WebPageProxy::resolveNetworkInterception.
class NetworkInterceptionSink final {
    WTF_MAKE_TZONE_ALLOCATED(NetworkInterceptionSink);
    WTF_MAKE_NONCOPYABLE(NetworkInterceptionSink);
public:
    explicit NetworkInterceptionSink(WebPage&);
    ~NetworkInterceptionSink();

    const Inspector::NetworkInterception& configuration() const;

    // Whether a pattern of the owner matches the URL at the phase. Like the single-process Web
    // Inspector, the fragment identifier is not part of the match and an empty URL never matches.
    bool matches(const URL&, Inspector::NetworkInterceptionPhase, Inspector::NetworkInterceptOwner) const;

    // Web Inspector's paused loads. The continuations are the ones the loader hands to Web
    // Inspector (WebLoaderStrategy::scheduleLoad, WebResourceLoader::didReceiveResponse).
    using InspectorRequestContinuation = Function<void(const WebCore::ResourceRequest&)>;
    using InspectorResponseContinuation = CompletionHandler<void(const WebCore::ResourceResponse&, RefPtr<WebCore::FragmentedSharedBuffer>)>;
    // Return false if the load is already paused, in which case the caller resumes it.
    bool pauseRequestForInspector(WebCore::ResourceLoader&, InspectorRequestContinuation&&);
    bool pauseResponseForInspector(WebCore::ResourceLoaderIdentifier, const WebCore::ResourceResponse&, InspectorResponseContinuation&&);
    // Web Inspector's Network domain went away; let every load it paused go on unchanged.
    void continueInspectorPauses();

    // Applies the owner's resolution of a load paused at the phase. A load that is no longer paused,
    // or that was paused for another owner, is ignored.
    void resolveInterception(WebCore::ResourceLoaderIdentifier, Inspector::NetworkInterceptOwner, Inspector::NetworkInterceptionPhase, Inspector::NetworkInterceptionResolution&&);

private:
    void resolveInspectorRequest(WebCore::ResourceLoaderIdentifier, Inspector::NetworkInterceptionResolution&&);
    void resolveInspectorResponse(WebCore::ResourceLoaderIdentifier, Inspector::NetworkInterceptionResolution&&);

    struct InspectorPendingRequest {
        WTF_MAKE_STRUCT_TZONE_ALLOCATED(InspectorPendingRequest);

        Ref<WebCore::ResourceLoader> loader;
        InspectorRequestContinuation continuation;
    };

    struct InspectorPendingResponse {
        WTF_MAKE_STRUCT_TZONE_ALLOCATED(InspectorPendingResponse);

        WebCore::ResourceResponse originalResponse;
        InspectorResponseContinuation continuation;
    };

    WeakRef<WebPage> m_page;
    HashMap<WebCore::ResourceLoaderIdentifier, std::unique_ptr<InspectorPendingRequest>> m_inspectorPendingRequests;
    HashMap<WebCore::ResourceLoaderIdentifier, std::unique_ptr<InspectorPendingResponse>> m_inspectorPendingResponses;
};

} // namespace WebKit
