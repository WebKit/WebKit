/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ApplicationCacheHost.h"
#include "InspectorWebAgentBase.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/Noncopyable.h>

namespace Inspector {
class ApplicationCacheFrontendDispatcher;
}

namespace WebCore {

class Frame;
class Page;

typedef String ErrorString;

class InspectorApplicationCacheAgent final : public InspectorAgentBase, public Inspector::ApplicationCacheBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorApplicationCacheAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorApplicationCacheAgent(PageAgentContext&);
    ~InspectorApplicationCacheAgent() override;

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    // ApplicationCacheBackendDispatcherHandler
    void enable(ErrorString&) override;
    void disable(ErrorString&) override;
    void getFramesWithManifests(ErrorString&, RefPtr<JSON::ArrayOf<Inspector::Protocol::ApplicationCache::FrameWithManifest>>& result) override;
    void getManifestForFrame(ErrorString&, const String& frameId, String* manifestURL) override;
    void getApplicationCacheForFrame(ErrorString&, const String& frameId, RefPtr<Inspector::Protocol::ApplicationCache::ApplicationCache>&) override;

    // InspectorInstrumentation
    void updateApplicationCacheStatus(Frame*);
    void networkStateChanged();

private:
    Ref<Inspector::Protocol::ApplicationCache::ApplicationCache> buildObjectForApplicationCache(const Vector<ApplicationCacheHost::ResourceInfo>&, const ApplicationCacheHost::CacheInfo&);
    Ref<JSON::ArrayOf<Inspector::Protocol::ApplicationCache::ApplicationCacheResource>> buildArrayForApplicationCacheResources(const Vector<ApplicationCacheHost::ResourceInfo>&);
    Ref<Inspector::Protocol::ApplicationCache::ApplicationCacheResource> buildObjectForApplicationCacheResource(const ApplicationCacheHost::ResourceInfo&);

    DocumentLoader* assertFrameWithDocumentLoader(ErrorString&, const String& frameId);

    std::unique_ptr<Inspector::ApplicationCacheFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::ApplicationCacheBackendDispatcher> m_backendDispatcher;
    Page& m_inspectedPage;
};

} // namespace WebCore

