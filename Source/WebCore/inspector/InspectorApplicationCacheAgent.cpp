/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "InspectorApplicationCacheAgent.h"

#include "ApplicationCacheHost.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "MainFrame.h"
#include "NetworkStateNotifier.h"
#include <wtf/text/StringBuilder.h>

using namespace Inspector;

namespace WebCore {

InspectorApplicationCacheAgent::InspectorApplicationCacheAgent(WebAgentContext& context, InspectorPageAgent* pageAgent)
    : InspectorAgentBase(ASCIILiteral("ApplicationCache"), context)
    , m_frontendDispatcher(std::make_unique<Inspector::ApplicationCacheFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::ApplicationCacheBackendDispatcher::create(context.backendDispatcher, this))
    , m_pageAgent(pageAgent)
{
}

void InspectorApplicationCacheAgent::didCreateFrontendAndBackend(FrontendRouter*, BackendDispatcher*)
{
}

void InspectorApplicationCacheAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    m_instrumentingAgents.setInspectorApplicationCacheAgent(nullptr);
}

void InspectorApplicationCacheAgent::enable(ErrorString&)
{
    m_instrumentingAgents.setInspectorApplicationCacheAgent(this);

    // We need to pass initial navigator.onOnline.
    networkStateChanged();
}

void InspectorApplicationCacheAgent::updateApplicationCacheStatus(Frame* frame)
{
    if (!frame)
        return;
    auto* documentLoader = frame->loader().documentLoader();
    if (!documentLoader)
        return;

    auto& host = documentLoader->applicationCacheHost();
    int status = host.status();
    auto manifestURL = host.applicationCacheInfo().manifest.string();

    m_frontendDispatcher->applicationCacheStatusUpdated(m_pageAgent->frameId(frame), manifestURL, status);
}

void InspectorApplicationCacheAgent::networkStateChanged()
{
    m_frontendDispatcher->networkStateUpdated(NetworkStateNotifier::singleton().onLine());
}

void InspectorApplicationCacheAgent::getFramesWithManifests(ErrorString&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::ApplicationCache::FrameWithManifest>>& result)
{
    result = Inspector::Protocol::Array<Inspector::Protocol::ApplicationCache::FrameWithManifest>::create();

    for (Frame* frame = &m_pageAgent->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* documentLoader = frame->loader().documentLoader();
        if (!documentLoader)
            continue;

        auto& host = documentLoader->applicationCacheHost();
        String manifestURL = host.applicationCacheInfo().manifest.string();
        if (!manifestURL.isEmpty()) {
            result->addItem(Inspector::Protocol::ApplicationCache::FrameWithManifest::create()
                .setFrameId(m_pageAgent->frameId(frame))
                .setManifestURL(manifestURL)
                .setStatus(static_cast<int>(host.status()))
                .release());
        }
    }
}

DocumentLoader* InspectorApplicationCacheAgent::assertFrameWithDocumentLoader(ErrorString& errorString, const String& frameId)
{
    Frame* frame = m_pageAgent->assertFrame(errorString, frameId);
    if (!frame)
        return nullptr;

    return InspectorPageAgent::assertDocumentLoader(errorString, frame);
}

void InspectorApplicationCacheAgent::getManifestForFrame(ErrorString& errorString, const String& frameId, String* manifestURL)
{
    DocumentLoader* documentLoader = assertFrameWithDocumentLoader(errorString, frameId);
    if (!documentLoader)
        return;

    *manifestURL = documentLoader->applicationCacheHost().applicationCacheInfo().manifest.string();
}

void InspectorApplicationCacheAgent::getApplicationCacheForFrame(ErrorString& errorString, const String& frameId, RefPtr<Inspector::Protocol::ApplicationCache::ApplicationCache>& applicationCache)
{
    auto* documentLoader = assertFrameWithDocumentLoader(errorString, frameId);
    if (!documentLoader)
        return;

    auto& host = documentLoader->applicationCacheHost();
    applicationCache = buildObjectForApplicationCache(host.resourceList(), host.applicationCacheInfo());
}

Ref<Inspector::Protocol::ApplicationCache::ApplicationCache> InspectorApplicationCacheAgent::buildObjectForApplicationCache(const Vector<ApplicationCacheHost::ResourceInfo>& applicationCacheResources, const ApplicationCacheHost::CacheInfo& applicationCacheInfo)
{
    return Inspector::Protocol::ApplicationCache::ApplicationCache::create()
        .setManifestURL(applicationCacheInfo.manifest.string())
        .setSize(applicationCacheInfo.size)
        .setCreationTime(applicationCacheInfo.creationTime)
        .setUpdateTime(applicationCacheInfo.updateTime)
        .setResources(buildArrayForApplicationCacheResources(applicationCacheResources))
        .release();
}

Ref<Inspector::Protocol::Array<Inspector::Protocol::ApplicationCache::ApplicationCacheResource>> InspectorApplicationCacheAgent::buildArrayForApplicationCacheResources(const Vector<ApplicationCacheHost::ResourceInfo>& applicationCacheResources)
{
    auto result = Inspector::Protocol::Array<Inspector::Protocol::ApplicationCache::ApplicationCacheResource>::create();
    for (auto& info : applicationCacheResources)
        result->addItem(buildObjectForApplicationCacheResource(info));
    return result;
}

Ref<Inspector::Protocol::ApplicationCache::ApplicationCacheResource> InspectorApplicationCacheAgent::buildObjectForApplicationCacheResource(const ApplicationCacheHost::ResourceInfo& resourceInfo)
{
    StringBuilder types;

    if (resourceInfo.isMaster)
        types.appendLiteral("Master ");

    if (resourceInfo.isManifest)
        types.appendLiteral("Manifest ");

    if (resourceInfo.isFallback)
        types.appendLiteral("Fallback ");

    if (resourceInfo.isForeign)
        types.appendLiteral("Foreign ");

    if (resourceInfo.isExplicit)
        types.appendLiteral("Explicit ");

    return Inspector::Protocol::ApplicationCache::ApplicationCacheResource::create()
        .setUrl(resourceInfo.resource.string())
        .setSize(static_cast<int>(resourceInfo.size))
        .setType(types.toString())
        .release();
}

} // namespace WebCore
