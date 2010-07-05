/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if ENABLE(INSPECTOR) && ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "ApplicationCache.h"
#include "ApplicationCacheResource.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "InspectorController.h"
#include "InspectorFrontend.h"
#include "Page.h"
#include "ResourceResponse.h"
#include "ScriptArray.h"
#include "ScriptObject.h"

namespace WebCore {

InspectorApplicationCacheAgent::InspectorApplicationCacheAgent(InspectorController* inspectorController, InspectorFrontend* frontend)
    : m_inspectorController(inspectorController)
    , m_frontend(frontend)
{
}

void InspectorApplicationCacheAgent::didReceiveManifestResponse(unsigned long identifier, const ResourceResponse& response)
{
    m_inspectorController->didReceiveResponse(identifier, response);
}

void InspectorApplicationCacheAgent::updateApplicationCacheStatus(ApplicationCacheHost::Status status)
{
    m_frontend->updateApplicationCacheStatus(status);
}

void InspectorApplicationCacheAgent::updateNetworkState(bool isNowOnline)
{
    m_frontend->updateNetworkState(isNowOnline);
}

void InspectorApplicationCacheAgent::fillResourceList(ApplicationCache* cache, ResourceInfoList* resources)
{
    ASSERT(cache && cache->isComplete());
    ApplicationCache::ResourceMap::const_iterator end = cache->end();
    for (ApplicationCache::ResourceMap::const_iterator it = cache->begin(); it != end; ++it) {
        RefPtr<ApplicationCacheResource> resource = it->second;
        unsigned type = resource->type();
        bool isMaster   = type & ApplicationCacheResource::Master;
        bool isManifest = type & ApplicationCacheResource::Manifest;
        bool isExplicit = type & ApplicationCacheResource::Explicit;
        bool isForeign  = type & ApplicationCacheResource::Foreign;
        bool isFallback = type & ApplicationCacheResource::Fallback;
        resources->append(InspectorApplicationCacheAgent::ResourceInfo(resource->url(), isMaster, isManifest, isFallback, isForeign, isExplicit, resource->estimatedSizeInStorage()));
    }
}

void InspectorApplicationCacheAgent::getApplicationCaches(long callId)
{
    DocumentLoader* documentLoader = m_inspectorController->inspectedPage()->mainFrame()->loader()->documentLoader();
    if (!documentLoader) {
        m_frontend->didGetApplicationCaches(callId, ScriptValue::undefined());
        return;
    }

    ApplicationCacheHost* host = documentLoader->applicationCacheHost();
    ApplicationCache* cache = host->applicationCacheForInspector();
    if (!cache || !cache->isComplete()) {
        m_frontend->didGetApplicationCaches(callId, ScriptValue::undefined());
        return;        
    }

    // FIXME: Add "Creation Time" and "Update Time" to Application Caches.
    ApplicationCacheInfo info(cache->manifestResource()->url(), String(), String(), cache->estimatedSizeInStorage());
    ResourceInfoList resources;
    fillResourceList(cache, &resources);

    m_frontend->didGetApplicationCaches(callId, buildObjectForApplicationCache(resources, info));
}

ScriptObject InspectorApplicationCacheAgent::buildObjectForApplicationCache(const ResourceInfoList& applicationCacheResources, const ApplicationCacheInfo& applicationCacheInfo)
{
    ScriptObject value = m_frontend->newScriptObject();
    value.set("size", applicationCacheInfo.m_size);
    value.set("manifest", applicationCacheInfo.m_manifest.string());
    value.set("lastPathComponent", applicationCacheInfo.m_manifest.lastPathComponent());
    value.set("creationTime", applicationCacheInfo.m_creationTime);
    value.set("updateTime", applicationCacheInfo.m_updateTime);
    value.set("resources", buildArrayForApplicationCacheResources(applicationCacheResources));
    return value;
}

ScriptArray InspectorApplicationCacheAgent::buildArrayForApplicationCacheResources(const ResourceInfoList& applicationCacheResources)
{
    ScriptArray resources = m_frontend->newScriptArray();

    ResourceInfoList::const_iterator end = applicationCacheResources.end();
    ResourceInfoList::const_iterator it = applicationCacheResources.begin();
    for (int i = 0; it != end; ++it, i++)
        resources.set(i, buildObjectForApplicationCacheResource(*it));

    return resources;
}

ScriptObject InspectorApplicationCacheAgent::buildObjectForApplicationCacheResource(const ResourceInfo& resourceInfo)
{
    ScriptObject value = m_frontend->newScriptObject();
    value.set("name", resourceInfo.m_resource.string());
    value.set("size", resourceInfo.m_size);

    String types;
    if (resourceInfo.m_isMaster)
        types.append("Master ");

    if (resourceInfo.m_isManifest)
        types.append("Manifest ");

    if (resourceInfo.m_isFallback)
        types.append("Fallback ");

    if (resourceInfo.m_isForeign)
        types.append("Foreign ");

    if (resourceInfo.m_isExplicit)
        types.append("Explicit ");

    value.set("type", types);
    return value;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(OFFLINE_WEB_APPLICATIONS)
