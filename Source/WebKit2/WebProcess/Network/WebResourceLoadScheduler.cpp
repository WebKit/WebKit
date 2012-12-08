/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "WebResourceLoadScheduler.h"

#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "NetworkResourceLoadParameters.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include "WebResourceLoader.h"
#include <WebCore/DocumentLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/ResourceBuffer.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/Settings.h>
#include <WebCore/SubresourceLoader.h>
#include <wtf/text/CString.h>

#if ENABLE(NETWORK_PROCESS)

using namespace WebCore;

namespace WebKit {

WebResourceLoadScheduler::WebResourceLoadScheduler()
    : m_suspendPendingRequestsCount(0)
{
}

WebResourceLoadScheduler::~WebResourceLoadScheduler()
{
}

PassRefPtr<SubresourceLoader> WebResourceLoadScheduler::scheduleSubresourceLoad(Frame* frame, CachedResource* resource, const ResourceRequest& request, ResourceLoadPriority priority, const ResourceLoaderOptions& options)
{
    RefPtr<SubresourceLoader> loader = SubresourceLoader::create(frame, resource, request, options);
    if (loader)
        scheduleLoad(loader.get(), priority);
    return loader.release();
}

PassRefPtr<NetscapePlugInStreamLoader> WebResourceLoadScheduler::schedulePluginStreamLoad(Frame* frame, NetscapePlugInStreamLoaderClient* client, const ResourceRequest& request)
{
    RefPtr<NetscapePlugInStreamLoader> loader = NetscapePlugInStreamLoader::create(frame, client, request);
    if (loader)
        scheduleLoad(loader.get(), ResourceLoadPriorityLow);
    return loader.release();
}

void WebResourceLoadScheduler::scheduleLoad(ResourceLoader* resourceLoader, ResourceLoadPriority priority)
{
    LOG(NetworkScheduling, "(WebProcess) WebResourceLoadScheduler::scheduleLoad, url '%s' priority %i", resourceLoader->url().string().utf8().data(), priority);

    ASSERT(resourceLoader);
    ASSERT(priority != ResourceLoadPriorityUnresolved);
    priority = ResourceLoadPriorityHighest;

    // If there's a web archive resource for this URL, we don't need to schedule the load since it will never touch the network.
    if (resourceLoader->documentLoader()->archiveResourceForURL(resourceLoader->request().url())) {
        startResourceLoader(resourceLoader);
        return;
    }
    
    ResourceLoadIdentifier identifier;
    
    ResourceRequest request = resourceLoader->request();
    
    // We want the network process involved in scheduling data URL loads but it doesn't need to know the full (often long) URL.
    if (request.url().protocolIsData()) {
        DEFINE_STATIC_LOCAL(KURL, dataURL, (KURL(), "data:"));
        request.setURL(dataURL);
    }
    
    // FIXME (NetworkProcess): When the ResourceLoader asks its FrameLoaderClient about using
    // credential storage it passes along its identifier.
    // But at this point it doesn't have the correct identifier yet.
    // In practice clients we know about don't care about the identifier, but this is another reason
    // we need to make sure ResourceLoaders get correct identifiers right off the bat.
    StoredCredentials allowStoredCredentials = resourceLoader->shouldUseCredentialStorage() ? AllowStoredCredentials : DoNotAllowStoredCredentials;

    NetworkResourceLoadParameters loadParameters(request, priority, resourceLoader->shouldSniffContent() ? SniffContent : DoNotSniffContent, allowStoredCredentials, resourceLoader->frameLoader()->frame()->settings()->privateBrowsingEnabled());
    if (!WebProcess::shared().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::ScheduleResourceLoad(loadParameters), Messages::NetworkConnectionToWebProcess::ScheduleResourceLoad::Reply(identifier), 0)) {
        // FIXME (NetworkProcess): What should we do if this fails?
        ASSERT_NOT_REACHED();
    }
    
    resourceLoader->setIdentifier(identifier);
    m_webResourceLoaders.set(identifier, WebResourceLoader::create(resourceLoader));
    
    notifyDidScheduleResourceRequest(resourceLoader);
}

void WebResourceLoadScheduler::addMainResourceLoad(ResourceLoader* resourceLoader)
{
    LOG(NetworkScheduling, "(WebProcess) WebResourceLoadScheduler::addMainResourceLoad, url '%s'", resourceLoader->url().string().utf8().data());

    ResourceLoadIdentifier identifier;

    if (!WebProcess::shared().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::AddLoadInProgress(resourceLoader->url()), Messages::NetworkConnectionToWebProcess::AddLoadInProgress::Reply(identifier), 0)) {
        // FIXME (NetworkProcess): What should we do if this fails?
        ASSERT_NOT_REACHED();
    }

    resourceLoader->setIdentifier(identifier);
    
    m_coreResourceLoaders.set(identifier, resourceLoader);
}

void WebResourceLoadScheduler::remove(ResourceLoader* resourceLoader)
{
    ASSERT(resourceLoader);
    LOG(NetworkScheduling, "(WebProcess) WebResourceLoadScheduler::remove, url '%s'", resourceLoader->url().string().utf8().data());

    // FIXME (NetworkProcess): It's possible for a resourceLoader to be removed before it ever started,
    // meaning before it even has an identifier.
    // We should make this not be possible.
    // The ResourceLoader code path should always for an identifier to ResourceLoaders.
    
    ResourceLoadIdentifier identifier = resourceLoader->identifier();
    if (!identifier) {
        LOG_ERROR("WebResourceLoadScheduler removing a ResourceLoader that has no identifier.");
        return;
    }
    
    // FIXME (NetworkProcess): We should only tell the NetworkProcess to remove load identifiers for ResourceLoaders that were never started.
    // If a resource load was actually started within the NetworkProcess then the NetworkProcess handles clearing out the identifier.
    WebProcess::shared().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::RemoveLoadIdentifier(identifier), 0);
    
    ASSERT(m_webResourceLoaders.contains(identifier) || m_coreResourceLoaders.contains(identifier));
    m_webResourceLoaders.remove(identifier);
    m_coreResourceLoaders.remove(identifier);
}

void WebResourceLoadScheduler::crossOriginRedirectReceived(ResourceLoader*, const KURL&)
{
    // We handle cross origin redirects entirely within the NetworkProcess.
    // We override this call in the WebProcess to make it a no-op.
}

void WebResourceLoadScheduler::servePendingRequests(ResourceLoadPriority minimumPriority)
{
    LOG(NetworkScheduling, "(WebProcess) WebResourceLoadScheduler::servePendingRequests");
    
    // If this WebProcess has its own request suspension count then we don't even
    // have to bother messaging the NetworkProcess.
    if (m_suspendPendingRequestsCount)
        return;

    WebProcess::shared().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::ServePendingRequests(minimumPriority), 0);
}

void WebResourceLoadScheduler::suspendPendingRequests()
{
    WebProcess::shared().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::SuspendPendingRequests(), Messages::NetworkConnectionToWebProcess::SuspendPendingRequests::Reply(), 0);

    ++m_suspendPendingRequestsCount;
}

void WebResourceLoadScheduler::resumePendingRequests()
{
    WebProcess::shared().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::ResumePendingRequests(), Messages::NetworkConnectionToWebProcess::ResumePendingRequests::Reply(), 0);

    ASSERT(m_suspendPendingRequestsCount);
    --m_suspendPendingRequestsCount;
}

void WebResourceLoadScheduler::setSerialLoadingEnabled(bool enabled)
{
    WebProcess::shared().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::SetSerialLoadingEnabled(enabled), Messages::NetworkConnectionToWebProcess::SetSerialLoadingEnabled::Reply(), 0);
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
