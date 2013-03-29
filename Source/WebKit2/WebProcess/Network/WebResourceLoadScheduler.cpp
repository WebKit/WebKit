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
#include "WebErrors.h"
#include "WebFrame.h"
#include "WebFrameLoaderClient.h"
#include "WebPage.h"
#include "WebProcess.h"
#include "WebResourceLoader.h"
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/ReferrerPolicy.h>
#include <WebCore/ResourceBuffer.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/Settings.h>
#include <WebCore/SubresourceLoader.h>
#include <wtf/text/CString.h>

#if ENABLE(NETWORK_PROCESS)

using namespace WebCore;

namespace WebKit {

WebResourceLoadScheduler::WebResourceLoadScheduler()
    : m_internallyFailedLoadTimer(RunLoop::main(), this, &WebResourceLoadScheduler::internallyFailedLoadTimerFired)
    , m_suspendPendingRequestsCount(0)
{
}

WebResourceLoadScheduler::~WebResourceLoadScheduler()
{
}

PassRefPtr<SubresourceLoader> WebResourceLoadScheduler::scheduleSubresourceLoad(Frame* frame, CachedResource* resource, const ResourceRequest& request, ResourceLoadPriority priority, const ResourceLoaderOptions& options)
{
    RefPtr<SubresourceLoader> loader = SubresourceLoader::create(frame, resource, request, options);
    if (loader)
        scheduleLoad(loader.get(), priority, frame->document()->referrerPolicy() == ReferrerPolicyDefault);
    return loader.release();
}

PassRefPtr<NetscapePlugInStreamLoader> WebResourceLoadScheduler::schedulePluginStreamLoad(Frame* frame, NetscapePlugInStreamLoaderClient* client, const ResourceRequest& request)
{
    RefPtr<NetscapePlugInStreamLoader> loader = NetscapePlugInStreamLoader::create(frame, client, request);
    if (loader)
        scheduleLoad(loader.get(), ResourceLoadPriorityLow, frame->document()->referrerPolicy() == ReferrerPolicyDefault);
    return loader.release();
}

void WebResourceLoadScheduler::scheduleLoad(ResourceLoader* resourceLoader, ResourceLoadPriority priority, bool shouldClearReferrerOnHTTPSToHTTPRedirect)
{
    ASSERT(resourceLoader);
    ASSERT(priority != ResourceLoadPriorityUnresolved);
    priority = ResourceLoadPriorityHighest;

    ResourceLoadIdentifier identifier = resourceLoader->identifier();
    ASSERT(identifier);

    // If the DocumentLoader schedules this as an archive resource load,
    // then we should remember the ResourceLoader in our records but not schedule it in the NetworkProcess.
    if (resourceLoader->documentLoader()->scheduleArchiveLoad(resourceLoader, resourceLoader->request())) {
        LOG(NetworkScheduling, "(WebProcess) WebResourceLoadScheduler::scheduleLoad, url '%s' will be handled as an archive resource.", resourceLoader->url().string().utf8().data());
        m_webResourceLoaders.set(identifier, WebResourceLoader::create(resourceLoader));
        return;
    }
    
    LOG(NetworkScheduling, "(WebProcess) WebResourceLoadScheduler::scheduleLoad, url '%s' will be scheduled with the NetworkProcess with priority %i", resourceLoader->url().string().utf8().data(), priority);

    ContentSniffingPolicy contentSniffingPolicy = resourceLoader->shouldSniffContent() ? SniffContent : DoNotSniffContent;
    StoredCredentials allowStoredCredentials = resourceLoader->shouldUseCredentialStorage() ? AllowStoredCredentials : DoNotAllowStoredCredentials;
    bool privateBrowsingEnabled = resourceLoader->frameLoader()->frame()->settings()->privateBrowsingEnabled();

    WebFrame* webFrame = static_cast<WebFrameLoaderClient*>(resourceLoader->frameLoader()->client())->webFrame();
    WebPage* webPage = webFrame->page();

    NetworkResourceLoadParameters loadParameters(identifier, webPage->pageID(), webFrame->frameID(), resourceLoader->request(), priority, contentSniffingPolicy, allowStoredCredentials, privateBrowsingEnabled, shouldClearReferrerOnHTTPSToHTTPRedirect);
    if (!WebProcess::shared().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::ScheduleResourceLoad(loadParameters), 0)) {
        // We probably failed to schedule this load with the NetworkProcess because it had crashed.
        // This load will never succeed so we will schedule it to fail asynchronously.
        scheduleInternallyFailedLoad(resourceLoader);
        return;
    }
    
    m_webResourceLoaders.set(identifier, WebResourceLoader::create(resourceLoader));
    
    notifyDidScheduleResourceRequest(resourceLoader);
}

void WebResourceLoadScheduler::scheduleInternallyFailedLoad(WebCore::ResourceLoader* resourceLoader)
{
    m_internallyFailedResourceLoaders.add(resourceLoader);
    m_internallyFailedLoadTimer.startOneShot(0);
}

void WebResourceLoadScheduler::internallyFailedLoadTimerFired()
{
    Vector<RefPtr<ResourceLoader> > internallyFailedResourceLoaders;
    copyToVector(m_internallyFailedResourceLoaders, internallyFailedResourceLoaders);
    
    for (size_t i = 0; i < internallyFailedResourceLoaders.size(); ++i)
        internallyFailedResourceLoaders[i]->didFail(internalError(internallyFailedResourceLoaders[i]->url()));
}

void WebResourceLoadScheduler::remove(ResourceLoader* resourceLoader)
{
    ASSERT(resourceLoader);
    LOG(NetworkScheduling, "(WebProcess) WebResourceLoadScheduler::remove, url '%s'", resourceLoader->url().string().utf8().data());

    if (m_internallyFailedResourceLoaders.contains(resourceLoader)) {
        m_internallyFailedResourceLoaders.remove(resourceLoader);
        return;
    }
    
    ResourceLoadIdentifier identifier = resourceLoader->identifier();
    if (!identifier) {
        LOG_ERROR("WebResourceLoadScheduler removing a ResourceLoader that has no identifier.");
        return;
    }
    
    // FIXME (NetworkProcess): We should only tell the NetworkProcess to remove load identifiers for ResourceLoaders that were never started.
    // If a resource load was actually started within the NetworkProcess then the NetworkProcess handles clearing out the identifier.
    WebProcess::shared().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::RemoveLoadIdentifier(identifier), 0);
    
    RefPtr<WebResourceLoader> loader = m_webResourceLoaders.take(identifier);
    ASSERT(loader);

    // It's possible that this WebResourceLoader might be just about to message back to the NetworkProcess (e.g. ContinueWillSendRequest)
    // but there's no point in doing so anymore.
    loader->detachFromCoreLoader();
}

void WebResourceLoadScheduler::crossOriginRedirectReceived(ResourceLoader*, const KURL&)
{
    // We handle cross origin redirects entirely within the NetworkProcess.
    // We override this call in the WebProcess to make it a no-op.
}

void WebResourceLoadScheduler::servePendingRequests(ResourceLoadPriority minimumPriority)
{
    LOG(NetworkScheduling, "(WebProcess) WebResourceLoadScheduler::servePendingRequests");
    
    // The NetworkProcess scheduler is good at making sure loads are serviced until there are no more pending requests.
    // If this WebProcess isn't expecting requests to be served then we can ignore messaging the NetworkProcess right now.
    if (m_suspendPendingRequestsCount)
        return;

    WebProcess::shared().networkConnection()->connection()->send(Messages::NetworkConnectionToWebProcess::ServePendingRequests(minimumPriority), 0);
}

void WebResourceLoadScheduler::suspendPendingRequests()
{
    ++m_suspendPendingRequestsCount;
}

void WebResourceLoadScheduler::resumePendingRequests()
{
    ASSERT(m_suspendPendingRequestsCount);
    --m_suspendPendingRequestsCount;
}

void WebResourceLoadScheduler::setSerialLoadingEnabled(bool enabled)
{
    WebProcess::shared().networkConnection()->connection()->sendSync(Messages::NetworkConnectionToWebProcess::SetSerialLoadingEnabled(enabled), Messages::NetworkConnectionToWebProcess::SetSerialLoadingEnabled::Reply(), 0);
}

void WebResourceLoadScheduler::networkProcessCrashed()
{
    HashMap<unsigned long, RefPtr<WebResourceLoader> >::iterator end = m_webResourceLoaders.end();
    for (HashMap<unsigned long, RefPtr<WebResourceLoader> >::iterator i = m_webResourceLoaders.begin(); i != end; ++i)
        scheduleInternallyFailedLoad(i->value.get()->resourceLoader());

    m_webResourceLoaders.clear();
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
