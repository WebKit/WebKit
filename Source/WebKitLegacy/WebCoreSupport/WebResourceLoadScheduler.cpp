/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004-2008, 2015 Apple Inc. All rights reserved.
    Copyright (C) 2010 Google Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 */

#include "WebResourceLoadScheduler.h"

#include "PingHandle.h"
#include <WebCore/CachedResource.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/NetscapePlugInStreamLoader.h>
#include <WebCore/NetworkStateNotifier.h>
#include <WebCore/PlatformStrategies.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SubresourceLoader.h>
#include <wtf/MainThread.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>

#if PLATFORM(IOS_FAMILY)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

// Match the parallel connection count used by the networking layer.
static unsigned maxRequestsInFlightPerHost;
#if !PLATFORM(IOS_FAMILY)
static const unsigned maxRequestsInFlightForNonHTTPProtocols = 20;
#else
// Limiting this seems to regress performance in some local cases so let's just make it large.
static const unsigned maxRequestsInFlightForNonHTTPProtocols = 10000;
#endif

using namespace WebCore;

WebResourceLoadScheduler& webResourceLoadScheduler()
{
    return static_cast<WebResourceLoadScheduler&>(*platformStrategies()->loaderStrategy());
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebResourceLoadScheduler);

auto WebResourceLoadScheduler::hostForURL(const URL& url, CreateHostPolicy createHostPolicy) -> CheckedPtr<HostInformation>
{
    if (!url.protocolIsInHTTPFamily())
        return m_nonHTTPProtocolHost.ptr();

    m_hosts.checkConsistency();
    String hostName = url.host().toString();
    CheckedPtr host = m_hosts.get(hostName);
    if (!host && createHostPolicy == CreateIfNotFound) {
        auto newHost = makeUnique<HostInformation>(hostName, maxRequestsInFlightPerHost);
        host = newHost.get();
        m_hosts.add(hostName, WTFMove(newHost));
    }
    return host;
}

WebResourceLoadScheduler::WebResourceLoadScheduler()
    : m_nonHTTPProtocolHost(makeUniqueRef<HostInformation>(String(), maxRequestsInFlightForNonHTTPProtocols))
    , m_requestTimer(*this, &WebResourceLoadScheduler::requestTimerFired)
    , m_suspendPendingRequestsCount(0)
    , m_isSerialLoadingEnabled(false)
{
    maxRequestsInFlightPerHost = initializeMaximumHTTPConnectionCountPerHost();
}

WebResourceLoadScheduler::~WebResourceLoadScheduler()
{
}

void WebResourceLoadScheduler::loadResource(LocalFrame& frame, CachedResource& resource, ResourceRequest&& request, const ResourceLoaderOptions& options, CompletionHandler<void(RefPtr<WebCore::SubresourceLoader>&&)>&& completionHandler)
{
    SubresourceLoader::create(frame, resource, WTFMove(request), options, [this, completionHandler = WTFMove(completionHandler)] (RefPtr<WebCore::SubresourceLoader>&& loader) mutable {
        if (loader)
            scheduleLoad(loader.get());
#if PLATFORM(IOS_FAMILY)
        // Since we defer loader initialization until scheduling on iOS, the frame
        // load delegate that would be called in SubresourceLoader::create() on
        // other ports might be called in scheduleLoad() instead. Our contract to
        // callers of this method is that a null loader is returned if the load was
        // cancelled by a frame load delegate.
        if (!loader || loader->reachedTerminalState())
            return completionHandler(nullptr);
#endif
        completionHandler(WTFMove(loader));
    });
}

void WebResourceLoadScheduler::loadResourceSynchronously(FrameLoader& frameLoader, ResourceLoaderIdentifier, const ResourceRequest& request, ClientCredentialPolicy, const FetchOptions& options, const HTTPHeaderMap&, ResourceError& error, ResourceResponse& response, Vector<uint8_t>& data)
{
    auto* document = frameLoader.frame().document();
    auto* sourceOrigin = document ? &document->securityOrigin() : nullptr;
    ResourceHandle::loadResourceSynchronously(frameLoader.networkingContext(), request, options.credentials == FetchOptions::Credentials::Omit ? StoredCredentialsPolicy::DoNotUse : StoredCredentialsPolicy::Use, sourceOrigin, error, response, data);
}

void WebResourceLoadScheduler::pageLoadCompleted(Page&)
{
}

void WebResourceLoadScheduler::browsingContextRemoved(LocalFrame&)
{
}

void WebResourceLoadScheduler::schedulePluginStreamLoad(LocalFrame& frame, NetscapePlugInStreamLoaderClient& client, ResourceRequest&& request, CompletionHandler<void(RefPtr<WebCore::NetscapePlugInStreamLoader>&&)>&& completionHandler)
{
    NetscapePlugInStreamLoader::create(frame, client, WTFMove(request), [this, completionHandler = WTFMove(completionHandler)] (RefPtr<WebCore::NetscapePlugInStreamLoader>&& loader) mutable {
        if (loader)
            scheduleLoad(loader.get());
        completionHandler(WTFMove(loader));
    });
}

void WebResourceLoadScheduler::scheduleLoad(ResourceLoader* resourceLoader)
{
    ASSERT(resourceLoader);

#if PLATFORM(IOS_FAMILY)
    // If there's a web archive resource for this URL, we don't need to schedule the load since it will never touch the network.
    if (!isSuspendingPendingRequests() && resourceLoader->documentLoader()->archiveResourceForURL(resourceLoader->iOSOriginalRequest().url())) {
        resourceLoader->startLoading();
        return;
    }
#else
    if (resourceLoader->documentLoader()->archiveResourceForURL(resourceLoader->request().url())) {
        resourceLoader->start();
        return;
    }
#endif

#if PLATFORM(IOS_FAMILY)
    CheckedRef host = hostForURL(resourceLoader->iOSOriginalRequest().url(), CreateIfNotFound).releaseNonNull();
#else
    CheckedRef host = hostForURL(resourceLoader->url(), CreateIfNotFound).releaseNonNull();
#endif

    ResourceLoadPriority priority = resourceLoader->request().priority();

    bool hadRequests = host->hasRequests();
    host->schedule(resourceLoader, priority);

#if PLATFORM(COCOA)
    if (ResourceRequest::resourcePrioritiesEnabled() && !isSuspendingPendingRequests()) {
        // Serve all requests at once to keep the pipeline full at the network layer.
        // FIXME: Does this code do anything useful, given that we also set maxRequestsInFlightPerHost to effectively unlimited on these platforms?
        servePendingRequests(WTFMove(host), ResourceLoadPriority::VeryLow);
        return;
    }
#endif

#if PLATFORM(IOS_FAMILY)
    if ((priority > ResourceLoadPriority::Low || !resourceLoader->iOSOriginalRequest().url().protocolIsInHTTPFamily() || (priority == ResourceLoadPriority::Low && !hadRequests)) && !isSuspendingPendingRequests()) {
        // Try to request important resources immediately.
        servePendingRequests(WTFMove(host), priority);
        return;
    }
#else
    if (priority > ResourceLoadPriority::Low || !resourceLoader->url().protocolIsInHTTPFamily() || (priority == ResourceLoadPriority::Low && !hadRequests)) {
        // Try to request important resources immediately.
        servePendingRequests(WTFMove(host), priority);
        return;
    }
#endif

    // Handle asynchronously so early low priority requests don't
    // get scheduled before later high priority ones.
    scheduleServePendingRequests();
}

void WebResourceLoadScheduler::remove(ResourceLoader* resourceLoader)
{
    ASSERT(resourceLoader);

    CheckedPtr host = hostForURL(resourceLoader->url());
    if (host)
        host->remove(resourceLoader);
#if PLATFORM(IOS_FAMILY)
    // ResourceLoader::url() doesn't start returning the correct value until the load starts. If we get canceled before that, we need to look for originalRequest url instead.
    // FIXME: ResourceLoader::url() should be made to return a sensible value at all times.
    if (!resourceLoader->iOSOriginalRequest().isNull()) {
        CheckedPtr originalHost = hostForURL(resourceLoader->iOSOriginalRequest().url());
        if (originalHost && originalHost != host)
            originalHost->remove(resourceLoader);
    }
#endif
    scheduleServePendingRequests();
}

void WebResourceLoadScheduler::isResourceLoadFinished(CachedResource& resource, CompletionHandler<void(bool)>&& callback)
{
    if (!resource.loader()) {
        callback(true);
        return;
    }
    bool didFinish = !hostForURL(resource.loader()->url());
    callback(didFinish);
}

void WebResourceLoadScheduler::setDefersLoading(ResourceLoader& loader, bool defers)
{
    if (!defers && !loader.deferredRequest().isNull()) {
        loader.setRequest(loader.takeDeferredRequest());
        loader.start();
    }
}

void WebResourceLoadScheduler::crossOriginRedirectReceived(ResourceLoader* resourceLoader, const URL& redirectURL)
{
    CheckedPtr oldHost = hostForURL(resourceLoader->url());
    ASSERT(oldHost);
    if (!oldHost)
        return;

    CheckedPtr newHost = hostForURL(redirectURL, CreateIfNotFound);

    if (oldHost->name() == newHost->name())
        return;

    newHost->addLoadInProgress(resourceLoader);
    oldHost->remove(resourceLoader);
}

void WebResourceLoadScheduler::servePendingRequests(ResourceLoadPriority minimumPriority)
{
    if (isSuspendingPendingRequests())
        return;

    m_requestTimer.stop();
    
    servePendingRequests(m_nonHTTPProtocolHost.get(), minimumPriority);

    auto hosts = WTF::map(m_hosts.values(), [](auto&& host) {
        return WeakPtr { host.get() };
    });
    for (auto&& host : WTFMove(hosts)) {
        if (!host)
            continue;
        if (host->hasRequests())
            servePendingRequests(*host, minimumPriority);
        else
            m_hosts.take(host->name());
    }
}

void WebResourceLoadScheduler::servePendingRequests(CheckedRef<HostInformation>&& host, ResourceLoadPriority minimumPriority)
{
    auto priority = ResourceLoadPriority::Highest;
    while (true) {
        auto& requestsPending = host->requestsPending(priority);
        while (!requestsPending.isEmpty()) {
            RefPtr<ResourceLoader> resourceLoader = requestsPending.first();

            // For named hosts - which are only http(s) hosts - we should always enforce the connection limit.
            // For non-named hosts - everything but http(s) - we should only enforce the limit if the document isn't done parsing 
            // and we don't know all stylesheets yet.
            Document* document = resourceLoader->frameLoader() ? resourceLoader->frameLoader()->frame().document() : 0;
            bool shouldLimitRequests = !host->name().isNull() || (document && (document->parsing() || !document->haveStylesheetsLoaded()));
            if (shouldLimitRequests && host->limitRequests(priority))
                return;

            requestsPending.removeFirst();
            host->addLoadInProgress(resourceLoader.get());
#if PLATFORM(IOS_FAMILY)
            if (!WTF::IOSApplication::isWebProcess()) {
                resourceLoader->startLoading();
                return;
            }
#endif
            resourceLoader->start();
        }
        if (priority == minimumPriority)
            return;
        --priority;
    }
}

void WebResourceLoadScheduler::suspendPendingRequests()
{
    ++m_suspendPendingRequestsCount;
}

void WebResourceLoadScheduler::resumePendingRequests()
{
    ASSERT(m_suspendPendingRequestsCount);
    --m_suspendPendingRequestsCount;
    if (m_suspendPendingRequestsCount)
        return;
    if (!m_hosts.isEmpty() || m_nonHTTPProtocolHost->hasRequests())
        scheduleServePendingRequests();
}
    
void WebResourceLoadScheduler::scheduleServePendingRequests()
{
    if (!m_requestTimer.isActive())
        m_requestTimer.startOneShot(0_s);
}

void WebResourceLoadScheduler::requestTimerFired()
{
    servePendingRequests();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(WebResourceLoadSchedulerHostInformation, WebResourceLoadScheduler::HostInformation);

WebResourceLoadScheduler::HostInformation::HostInformation(const String& name, unsigned maxRequestsInFlight)
    : m_name(name)
    , m_maxRequestsInFlight(maxRequestsInFlight)
{
}

WebResourceLoadScheduler::HostInformation::~HostInformation()
{
    ASSERT(!hasRequests());
}

unsigned WebResourceLoadScheduler::HostInformation::priorityToIndex(ResourceLoadPriority priority)
{
    switch (priority) {
    case ResourceLoadPriority::VeryLow:
        return 0;
    case ResourceLoadPriority::Low:
        return 1;
    case ResourceLoadPriority::Medium:
        return 2;
    case ResourceLoadPriority::High:
        return 3;
    case ResourceLoadPriority::VeryHigh:
        return 4;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

void WebResourceLoadScheduler::HostInformation::schedule(ResourceLoader* resourceLoader, ResourceLoadPriority priority)
{
    m_requestsPending[priorityToIndex(priority)].append(resourceLoader);
}
    
void WebResourceLoadScheduler::HostInformation::addLoadInProgress(ResourceLoader* resourceLoader)
{
    m_requestsLoading.add(resourceLoader);
}
    
void WebResourceLoadScheduler::HostInformation::remove(ResourceLoader* resourceLoader)
{
    if (m_requestsLoading.remove(resourceLoader))
        return;
    
    for (auto& requestQueue : m_requestsPending) {
        for (auto it = requestQueue.begin(), end = requestQueue.end(); it != end; ++it) {
            if (*it == resourceLoader) {
                requestQueue.remove(it);
                return;
            }
        }
    }
}

bool WebResourceLoadScheduler::HostInformation::hasRequests() const
{
    if (!m_requestsLoading.isEmpty())
        return true;
    for (auto& requestQueue : m_requestsPending) {
        if (!requestQueue.isEmpty())
            return true;
    }
    return false;
}

bool WebResourceLoadScheduler::HostInformation::limitRequests(ResourceLoadPriority priority) const 
{
    if (priority == ResourceLoadPriority::VeryLow && !m_requestsLoading.isEmpty())
        return true;
    return m_requestsLoading.size() >= (webResourceLoadScheduler().isSerialLoadingEnabled() ? 1 : m_maxRequestsInFlight);
}

void WebResourceLoadScheduler::startPingLoad(LocalFrame& frame, ResourceRequest& request, const HTTPHeaderMap&, const FetchOptions& options, ContentSecurityPolicyImposition, PingLoadCompletionHandler&& completionHandler)
{
    // PingHandle manages its own lifetime, deleting itself when its purpose has been fulfilled.
    new PingHandle(frame.loader().networkingContext(), request, options.credentials != FetchOptions::Credentials::Omit, options.redirect == FetchOptions::Redirect::Follow, WTFMove(completionHandler));
}

bool WebResourceLoadScheduler::isOnLine() const
{
    return NetworkStateNotifier::singleton().onLine();
}

void WebResourceLoadScheduler::addOnlineStateChangeListener(WTF::Function<void(bool)>&& listener)
{
    NetworkStateNotifier::singleton().addListener(WTFMove(listener));
}

void WebResourceLoadScheduler::preconnectTo(FrameLoader&, const URL&, StoredCredentialsPolicy, ShouldPreconnectAsFirstParty, PreconnectCompletionHandler&&)
{
}

