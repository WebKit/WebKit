#include "config.h"
#include "NetworkResourceLoadScheduler.h"

#include "HostRecord.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcessconnectionMessages.h"
#include "NetworkRequest.h"
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

ResourceLoadIdentifier NetworkResourceLoadScheduler::scheduleNetworkRequest(const ResourceRequest& request, ResourceLoadPriority priority, NetworkConnectionToWebProcess* connection)
{    
    ResourceLoadIdentifier identifier = ++s_currentResourceLoadIdentifier;

    LOG(Network, "(NetworkProcess) NetworkResourceLoadScheduler::scheduleNetworkRequest resource %llu '%s'", identifier, request.url().string().utf8().data());

    HostRecord* host = hostForURL(request.url(), CreateIfNotFound);
    bool hadRequests = host->hasRequests();
    host->schedule(NetworkRequest::create(request, identifier, connection), priority);
    m_identifiers.add(identifier, host);

    if (priority > ResourceLoadPriorityLow || !request.url().protocolIsInHTTPFamily() || (priority == ResourceLoadPriorityLow && !hadRequests)) {
        // Try to request important resources immediately.
        servePendingRequestsForHost(host, priority);
        return identifier;
    }
    
    // Handle asynchronously so early low priority requests don't get scheduled before later high priority ones.
    scheduleServePendingRequests();
    return identifier;
}

ResourceLoadIdentifier NetworkResourceLoadScheduler::addLoadInProgress(const WebCore::KURL& url)
{
    ResourceLoadIdentifier identifier = ++s_currentResourceLoadIdentifier;

    LOG(Network, "(NetworkProcess) NetworkResourceLoadScheduler::addLoadInProgress resource %llu with url '%s'", identifier, url.string().utf8().data());

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
    ASSERT(identifier);

    LOG(Network, "(NetworkProcess) NetworkResourceLoadScheduler::removeLoadIdentifier removing load identifier %llu", identifier);

    HostRecord* host = m_identifiers.take(identifier);
    ASSERT(host);
    if (host)
        host->remove(identifier);

    scheduleServePendingRequests();
}

void NetworkResourceLoadScheduler::crossOriginRedirectReceived(ResourceLoadIdentifier identifier, const WebCore::KURL& redirectURL)
{
    LOG(Network, "(NetworkProcess) NetworkResourceLoadScheduler::crossOriginRedirectReceived resource %llu redirected to '%s'", identifier, redirectURL.string().utf8().data());

    HostRecord* oldHost = m_identifiers.get(identifier);
    HostRecord* newHost = hostForURL(redirectURL, CreateIfNotFound);
    ASSERT(oldHost);
    
    if (oldHost->name() == newHost->name())
        return;
    
    newHost->addLoadInProgress(identifier);
    m_identifiers.set(identifier, newHost);

    oldHost->remove(identifier);    
}

void NetworkResourceLoadScheduler::servePendingRequests(ResourceLoadPriority minimumPriority)
{
    if (m_suspendPendingRequestsCount)
        return;

    LOG(Network, "(NetworkProcess) NetworkResourceLoadScheduler::servePendingRequests Serving requests for up to %i hosts", m_hosts.size());

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
    LOG(Network, "NetworkResourceLoadScheduler::servePendingRequests Host name='%s'", host->name().utf8().data());

    for (int priority = ResourceLoadPriorityHighest; priority >= minimumPriority; --priority) {
        HostRecord::RequestQueue& requestsPending = host->requestsPending(ResourceLoadPriority(priority));

        while (!requestsPending.isEmpty()) {
            RefPtr<NetworkRequest> request = requestsPending.first();
            
            // This request might be from WebProcess we've lost our connection to.
            // If so we should just skip it.
            if (!request->connectionToWebProcess()) {
                requestsPending.removeFirst();
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
            if (shouldLimitRequests && host->limitRequests(ResourceLoadPriority(priority), request->connectionToWebProcess()->isSerialLoadingEnabled()))
                return;

            requestsPending.removeFirst();
            host->addLoadInProgress(request->identifier());
            
            request->connectionToWebProcess()->connection()->send(Messages::NetworkProcessConnection::StartResourceLoad(request->identifier()), 0);
        }
    }
}

void NetworkResourceLoadScheduler::suspendPendingRequests()
{
    ++m_suspendPendingRequestsCount;
}

void NetworkResourceLoadScheduler::resumePendingRequests()
{
    ASSERT(m_suspendPendingRequestsCount);
    --m_suspendPendingRequestsCount;
    if (m_suspendPendingRequestsCount)
        return;

    if (!m_hosts.isEmpty() || m_nonHTTPProtocolHost->hasRequests())
        scheduleServePendingRequests();
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
