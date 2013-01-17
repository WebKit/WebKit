#include "config.h"
#include "NetworkResourceLoadScheduler.h"

#include "HostRecord.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcessconnectionMessages.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoader.h"
#include "SyncNetworkResourceLoader.h"
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

#if ENABLE(NETWORK_PROCESS)

using namespace WebCore;

namespace WebKit {

static const unsigned maxRequestsInFlightForNonHTTPProtocols = 20;
static unsigned maxRequestsInFlightPerHost;
static ResourceLoadIdentifier s_currentResourceLoadIdentifier;

NetworkResourceLoadScheduler::NetworkResourceLoadScheduler()
    : m_nonHTTPProtocolHost(new HostRecord(String(), maxRequestsInFlightForNonHTTPProtocols))
    , m_requestTimer(this, &NetworkResourceLoadScheduler::requestTimerFired)

{
    maxRequestsInFlightPerHost = platformInitializeMaximumHTTPConnectionCountPerHost();
}

void NetworkResourceLoadScheduler::scheduleServePendingRequests()
{
    if (!m_requestTimer.isActive())
        m_requestTimer.startOneShot(0);
}

void NetworkResourceLoadScheduler::requestTimerFired(WebCore::Timer<NetworkResourceLoadScheduler>*)
{
    servePendingRequests();
}

ResourceLoadIdentifier NetworkResourceLoadScheduler::scheduleResourceLoad(const NetworkResourceLoadParameters& loadParameters, NetworkConnectionToWebProcess* connection)
{
    ResourceLoadPriority priority = loadParameters.priority();
    const ResourceRequest& resourceRequest = loadParameters.request();
    
    ResourceLoadIdentifier identifier = ++s_currentResourceLoadIdentifier;
    RefPtr<NetworkResourceLoader> loader = NetworkResourceLoader::create(loadParameters, identifier, connection);
    
    m_resourceLoaders.add(identifier, loader);

    LOG(NetworkScheduling, "(NetworkProcess) NetworkResourceLoadScheduler::scheduleNetworkResourceRequest resource %llu '%s'", identifier, resourceRequest.url().string().utf8().data());

    HostRecord* host = hostForURL(resourceRequest.url(), CreateIfNotFound);
    bool hadRequests = host->hasRequests();
    host->schedule(loader);
    m_identifiers.add(identifier, host);

    if (priority > ResourceLoadPriorityLow || !resourceRequest.url().protocolIsInHTTPFamily() || (priority == ResourceLoadPriorityLow && !hadRequests)) {
        // Try to request important resources immediately.
        servePendingRequestsForHost(host, priority);
        return identifier;
    }
    
    // Handle asynchronously so early low priority requests don't get scheduled before later high priority ones.
    scheduleServePendingRequests();
    return identifier;
}

void NetworkResourceLoadScheduler::scheduleSyncNetworkResourceLoader(PassRefPtr<SyncNetworkResourceLoader> loader)
{
    // FIXME (NetworkProcess): Sync loaders need to get identifiers in a sane way.
    ResourceLoadIdentifier identifier = ++s_currentResourceLoadIdentifier;
    loader->setIdentifier(identifier);

    const ResourceRequest& resourceRequest = loader->loadParameters().request();

    LOG(NetworkScheduling, "(NetworkProcess) NetworkResourceLoadScheduler::scheduleSyncNetworkResourceLoader synchronous resource '%s'", resourceRequest.url().string().utf8().data());

    HostRecord* host = hostForURL(resourceRequest.url(), CreateIfNotFound);
    bool hadRequests = host->hasRequests();
    host->syncLoadersPending().append(loader);
    m_identifiers.add(identifier, host);
    
    if (!hadRequests)
        servePendingRequestsForHost(host, ResourceLoadPriorityHighest);

    scheduleServePendingRequests();
}

ResourceLoadIdentifier NetworkResourceLoadScheduler::addLoadInProgress(const WebCore::KURL& url)
{
    ResourceLoadIdentifier identifier = ++s_currentResourceLoadIdentifier;

    LOG(NetworkScheduling, "(NetworkProcess) NetworkResourceLoadScheduler::addLoadInProgress resource %llu with url '%s'", identifier, url.string().utf8().data());

    HostRecord* host = hostForURL(url, CreateIfNotFound);
    host->addLoadInProgress(identifier);
    m_identifiers.add(identifier, host);
    
    return identifier;
}

HostRecord* NetworkResourceLoadScheduler::hostForURL(const WebCore::KURL& url, CreateHostPolicy createHostPolicy)
{
    if (!url.protocolIsInHTTPFamily())
        return m_nonHTTPProtocolHost;

    m_hosts.checkConsistency();
    String hostName = url.host();
    HostRecord* host = m_hosts.get(hostName);
    if (!host && createHostPolicy == CreateIfNotFound) {
        host = new HostRecord(hostName, maxRequestsInFlightPerHost);
        m_hosts.add(hostName, host);
    }
    
    return host;
}

void NetworkResourceLoadScheduler::removeLoadIdentifier(ResourceLoadIdentifier identifier)
{
    ASSERT(isMainThread());
    ASSERT(identifier);

    LOG(NetworkScheduling, "(NetworkProcess) NetworkResourceLoadScheduler::removeLoadIdentifier removing load identifier %llu", identifier);

    HostRecord* host = m_identifiers.take(identifier);
    
    // Due to a race condition the WebProcess might have messaged the NetworkProcess to remove this identifier
    // after the NetworkProcess has already removed it internally.
    // In this situation we might not have a HostRecord to clean up.
    if (host)
        host->remove(identifier);
    
    m_resourceLoaders.remove(identifier);

    scheduleServePendingRequests();
}

NetworkResourceLoader* NetworkResourceLoadScheduler::networkResourceLoaderForIdentifier(ResourceLoadIdentifier identifier)
{
    ASSERT(m_resourceLoaders.get(identifier));
    return m_resourceLoaders.get(identifier).get();
}

void NetworkResourceLoadScheduler::receivedRedirect(ResourceLoadIdentifier identifier, const WebCore::KURL& redirectURL)
{
    ASSERT(isMainThread());
    LOG(NetworkScheduling, "(NetworkProcess) NetworkResourceLoadScheduler::receivedRedirect resource %llu redirected to '%s'", identifier, redirectURL.string().utf8().data());

    HostRecord* oldHost = m_identifiers.get(identifier);

    // The load may have been cancelled while the message was in flight from network thread to main thread.
    if (!oldHost)
        return;

    HostRecord* newHost = hostForURL(redirectURL, CreateIfNotFound);
    
    if (oldHost->name() == newHost->name())
        return;
    
    newHost->addLoadInProgress(identifier);
    m_identifiers.set(identifier, newHost);

    oldHost->remove(identifier);    
}

void NetworkResourceLoadScheduler::servePendingRequests(ResourceLoadPriority minimumPriority)
{
    LOG(NetworkScheduling, "(NetworkProcess) NetworkResourceLoadScheduler::servePendingRequests Serving requests for up to %i hosts", m_hosts.size());

    m_requestTimer.stop();
    
    servePendingRequestsForHost(m_nonHTTPProtocolHost, minimumPriority);

    m_hosts.checkConsistency();
    Vector<HostRecord*> hostsToServe;
    copyValuesToVector(m_hosts, hostsToServe);

    size_t size = hostsToServe.size();
    for (size_t i = 0; i < size; ++i) {
        HostRecord* host = hostsToServe[i];
        if (host->hasRequests())
            servePendingRequestsForHost(host, minimumPriority);
        else
            delete m_hosts.take(host->name());
    }
}

void NetworkResourceLoadScheduler::servePendingRequestsForHost(HostRecord* host, ResourceLoadPriority minimumPriority)
{
    LOG(NetworkScheduling, "NetworkResourceLoadScheduler::servePendingRequests Host name='%s'", host->name().utf8().data());

    // We serve synchronous requests before any other requests to improve responsiveness in any
    // WebProcess that is waiting on a synchronous load.
    HostRecord::SyncLoaderQueue& syncLoadersPending = host->syncLoadersPending();
    while (!syncLoadersPending.isEmpty()) {
        RefPtr<SyncNetworkResourceLoader> loader = syncLoadersPending.first();

        // FIXME (NetworkProcess): How do we know this synchronous load isn't associated with a WebProcess
        // we've lost our connection to?
        bool shouldLimitRequests = !host->name().isNull();
        if (shouldLimitRequests && host->limitRequests(ResourceLoadPriorityHighest, false))
            return;

        syncLoadersPending.removeFirst();
        host->addLoadInProgress(loader->identifier());

        loader->start();
    }
    
    for (int priority = ResourceLoadPriorityHighest; priority >= minimumPriority; --priority) {
        HostRecord::LoaderQueue& loadersPending = host->loadersPending(ResourceLoadPriority(priority));

        while (!loadersPending.isEmpty()) {
            RefPtr<NetworkResourceLoader> loader = loadersPending.first();
            
            // This request might be from WebProcess we've lost our connection to.
            // If so we should just skip it.
            if (!loader->connectionToWebProcess()) {
                loadersPending.removeFirst();
                continue;
            }

            // For named hosts - which are only http(s) hosts - we should always enforce the connection limit.
            // For non-named hosts - everything but http(s) - we should only enforce the limit if the document
            // isn't done parsing and we don't know all stylesheets yet.

            // FIXME (NetworkProcess): The above comment about document parsing and stylesheets is a holdover
            // from the WebCore::ResourceLoadScheduler.
            // The behavior described was at one time important for WebCore's single threadedness.
            // It's possible that we don't care about it with the NetworkProcess.
            // We should either decide it's not important and change the above comment, or decide it is
            // still important and somehow account for it.

            bool shouldLimitRequests = !host->name().isNull();
            if (shouldLimitRequests && host->limitRequests(ResourceLoadPriority(priority), loader->connectionToWebProcess()->isSerialLoadingEnabled()))
                return;

            loadersPending.removeFirst();
            host->addLoadInProgress(loader->identifier());

            loader->start();
        }
    }
}

static bool removeScheduledLoadIdentifiersCalled = false;

void NetworkResourceLoadScheduler::removeScheduledLoadIdentifiers(void* context)
{
    ASSERT(isMainThread());
    ASSERT(removeScheduledLoadIdentifiersCalled);

    NetworkResourceLoadScheduler* scheduler = static_cast<NetworkResourceLoadScheduler*>(context);
    scheduler->removeScheduledLoadIdentifiers();
}

void NetworkResourceLoadScheduler::removeScheduledLoadIdentifiers()
{
    Vector<ResourceLoadIdentifier> identifiers;
    {
        MutexLocker locker(m_identifiersToRemoveMutex);
        copyToVector(m_identifiersToRemove, identifiers);
        m_identifiersToRemove.clear();
        removeScheduledLoadIdentifiersCalled = false;
    }
    
    for (size_t i = 0; i < identifiers.size(); ++i)
        removeLoadIdentifier(identifiers[i]);
}

void NetworkResourceLoadScheduler::scheduleRemoveLoadIdentifier(ResourceLoadIdentifier identifier)
{
    MutexLocker locker(m_identifiersToRemoveMutex);
    
    m_identifiersToRemove.add(identifier);
    
    if (!removeScheduledLoadIdentifiersCalled) {
        removeScheduledLoadIdentifiersCalled = true;
        callOnMainThread(NetworkResourceLoadScheduler::removeScheduledLoadIdentifiers, this);
    }
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
