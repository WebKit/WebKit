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
#include "Page.h"
#include "ResourceResponse.h"
#include <inspector/InspectorFrontendDispatchers.h>
#include <inspector/InspectorValues.h>
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
    DocumentLoader* documentLoader = frame->loader().documentLoader();
    if (!documentLoader)
        return;

    ApplicationCacheHost* host = documentLoader->applicationCacheHost();
    ApplicationCacheHost::Status status = host->status();
    ApplicationCacheHost::CacheInfo info = host->applicationCacheInfo();

    String manifestURL = info.m_manifest.string();
    m_frontendDispatcher->applicationCacheStatusUpdated(m_pageAgent->frameId(frame), manifestURL, static_cast<int>(status));
}

void InspectorApplicationCacheAgent::networkStateChanged()
{
    bool isNowOnline = networkStateNotifier().onLine();
    m_frontendDispatcher->networkStateUpdated(isNowOnline);
}

void InspectorApplicationCacheAgent::getFramesWithManifests(ErrorString&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::ApplicationCache::FrameWithManifest>>& result)
{
    result = Inspector::Protocol::Array<Inspector::Protocol::ApplicationCache::FrameWithManifest>::create();

    for (Frame* frame = &m_pageAgent->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        DocumentLoader* documentLoader = frame->loader().documentLoader();
        if (!documentLoader)
            continue;

        ApplicationCacheHost* host = documentLoader->applicationCacheHost();
        ApplicationCacheHost::CacheInfo info = host->applicationCacheInfo();
        String manifestURL = info.m_manifest.string();
        if (!manifestURL.isEmpty()) {
            Ref<Inspector::Protocol::ApplicationCache::FrameWithManifest> value = Inspector::Protocol::ApplicationCache::FrameWithManifest::create()
                .setFrameId(m_pageAgent->frameId(frame))
                .setManifestURL(manifestURL)
                .setStatus(static_cast<int>(host->status()))
                .release();
            result->addItem(WTF::move(value));
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

    ApplicationCacheHost::CacheInfo info = documentLoader->applicationCacheHost()->applicationCacheInfo();
    *manifestURL = info.m_manifest.string();
}

void InspectorApplicationCacheAgent::getApplicationCacheForFrame(ErrorString& errorString, const String& frameId, RefPtr<Inspector::Protocol::ApplicationCache::ApplicationCache>& applicationCache)
{
    DocumentLoader* documentLoader = assertFrameWithDocumentLoader(errorString, frameId);
    if (!documentLoader)
        return;

    ApplicationCacheHost* host = documentLoader->applicationCacheHost();
    ApplicationCacheHost::CacheInfo info = host->applicationCacheInfo();

    ApplicationCacheHost::ResourceInfoList resources;
    host->fillResourceList(&resources);

    applicationCache = buildObjectForApplicationCache(resources, info);
}

Ref<Inspector::Protocol::ApplicationCache::ApplicationCache> InspectorApplicationCacheAgent::buildObjectForApplicationCache(const ApplicationCacheHost::ResourceInfoList& applicationCacheResources, const ApplicationCacheHost::CacheInfo& applicationCacheInfo)
{
    return Inspector::Protocol::ApplicationCache::ApplicationCache::create()
        .setManifestURL(applicationCacheInfo.m_manifest.string())
        .setSize(applicationCacheInfo.m_size)
        .setCreationTime(applicationCacheInfo.m_creationTime)
        .setUpdateTime(applicationCacheInfo.m_updateTime)
        .setResources(buildArrayForApplicationCacheResources(applicationCacheResources))
        .release();
}

Ref<Inspector::Protocol::Array<Inspector::Protocol::ApplicationCache::ApplicationCacheResource>> InspectorApplicationCacheAgent::buildArrayForApplicationCacheResources(const ApplicationCacheHost::ResourceInfoList& applicationCacheResources)
{
    auto resources = Inspector::Protocol::Array<Inspector::Protocol::ApplicationCache::ApplicationCacheResource>::create();

    for (const auto& resourceInfo : applicationCacheResources)
        resources->addItem(buildObjectForApplicationCacheResource(resourceInfo));

    return resources;
}

Ref<Inspector::Protocol::ApplicationCache::ApplicationCacheResource> InspectorApplicationCacheAgent::buildObjectForApplicationCacheResource(const ApplicationCacheHost::ResourceInfo& resourceInfo)
{
    StringBuilder types;

    if (resourceInfo.m_isMaster)
        types.appendLiteral("Master ");

    if (resourceInfo.m_isManifest)
        types.appendLiteral("Manifest ");

    if (resourceInfo.m_isFallback)
        types.appendLiteral("Fallback ");

    if (resourceInfo.m_isForeign)
        types.appendLiteral("Foreign ");

    if (resourceInfo.m_isExplicit)
        types.appendLiteral("Explicit ");

    return Inspector::Protocol::ApplicationCache::ApplicationCacheResource::create()
        .setUrl(resourceInfo.m_resource.string())
        .setSize(static_cast<int>(resourceInfo.m_size))
        .setType(types.toString())
        .release();
}

} // namespace WebCore
