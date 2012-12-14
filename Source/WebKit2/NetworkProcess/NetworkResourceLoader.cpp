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
#include "NetworkResourceLoader.h"

#if ENABLE(NETWORK_PROCESS)

#include "DataReference.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoadParameters.h"
#include "RemoteNetworkingContext.h"
#include "SharedMemory.h"
#include "WebCoreArgumentCoders.h"
#include "WebResourceLoaderMessages.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceBuffer.h>
#include <WebCore/ResourceHandle.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

NetworkResourceLoader::NetworkResourceLoader(const NetworkResourceLoadParameters& requestParameters, ResourceLoadIdentifier identifier, NetworkConnectionToWebProcess* connection)
    : m_requestParameters(requestParameters)
    , m_identifier(identifier)
    , m_connection(connection)
{
    ASSERT(isMainThread());
    connection->registerObserver(this);
}

NetworkResourceLoader::~NetworkResourceLoader()
{
    ASSERT(isMainThread());

    if (m_connection)
        m_connection->unregisterObserver(this);
}

CoreIPC::Connection* NetworkResourceLoader::connection() const
{
    return m_connection->connection();
}

ResourceLoadPriority NetworkResourceLoader::priority() const
{
    return m_requestParameters.priority();
}

void NetworkResourceLoader::start()
{
    ASSERT(isMainThread());

    // Explicit ref() balanced by a deref() in NetworkResourceLoader::stop()
    ref();
    
    // FIXME (NetworkProcess): Create RemoteNetworkingContext with actual settings.
    m_networkingContext = RemoteNetworkingContext::create(false, false, m_requestParameters.inPrivateBrowsingMode());

    // FIXME (NetworkProcess): Pass an actual value for defersLoading
    m_handle = ResourceHandle::create(m_networkingContext.get(), m_requestParameters.request(), this, false /* defersLoading */, m_requestParameters.contentSniffingPolicy() == SniffContent);
}

static bool stopRequestsCalled = false;

static Mutex& requestsToStopMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

static HashSet<NetworkResourceLoader*>& requestsToStop()
{
    DEFINE_STATIC_LOCAL(HashSet<NetworkResourceLoader*>, requests, ());
    return requests;
}

void NetworkResourceLoader::scheduleStopOnMainThread()
{
    MutexLocker locker(requestsToStopMutex());

    requestsToStop().add(this);
    if (!stopRequestsCalled) {
        stopRequestsCalled = true;
        callOnMainThread(NetworkResourceLoader::performStops, 0);
    }
}

void NetworkResourceLoader::performStops(void*)
{
    ASSERT(stopRequestsCalled);

    Vector<NetworkResourceLoader*> requests;
    {
        MutexLocker locker(requestsToStopMutex());
        copyToVector(requestsToStop(), requests);
        requestsToStop().clear();
        stopRequestsCalled = false;
    }
    
    for (size_t i = 0; i < requests.size(); ++i)
        requests[i]->stop();
}

void NetworkResourceLoader::stop()
{
    ASSERT(isMainThread());

    // Remove this load identifier soon so we can start more network requests.
    NetworkProcess::shared().networkResourceLoadScheduler().scheduleRemoveLoadIdentifier(m_identifier);
    
    // Explicit deref() balanced by a ref() in NetworkResourceLoader::stop()
    // This might cause the NetworkResourceLoader to be destroyed and therefore we do it last.
    deref();
}

void NetworkResourceLoader::connectionToWebProcessDidClose(NetworkConnectionToWebProcess* connection)
{
    ASSERT_ARG(connection, connection == m_connection.get());
    // FIXME (NetworkProcess): Cancel the load. The request may be long-living, so we don't want it to linger around after all clients are gone.
}

void NetworkResourceLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    // FIXME (NetworkProcess): Cache the response.
    send(Messages::WebResourceLoader::DidReceiveResponse(response));
}

void NetworkResourceLoader::didReceiveData(ResourceHandle*, const char* data, int length, int encodedDataLength)
{
    // FIXME (NetworkProcess): For the memory cache we'll also need to cache the response data here.
    // Such buffering will need to be thread safe, as this callback is happening on a background thread.
    
    CoreIPC::DataReference dataReference(reinterpret_cast<const uint8_t*>(data), length);
    send(Messages::WebResourceLoader::DidReceiveData(dataReference, encodedDataLength, false));
}

void NetworkResourceLoader::didFinishLoading(ResourceHandle*, double finishTime)
{
    // FIXME (NetworkProcess): For the memory cache we'll need to update the finished status of the cached resource here.
    // Such bookkeeping will need to be thread safe, as this callback is happening on a background thread.
    send(Messages::WebResourceLoader::DidFinishResourceLoad(finishTime));
    scheduleStopOnMainThread();
}

void NetworkResourceLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    // FIXME (NetworkProcess): For the memory cache we'll need to update the finished status of the cached resource here.
    // Such bookkeeping will need to be thread safe, as this callback is happening on a background thread.
    send(Messages::WebResourceLoader::DidFailResourceLoad(error));
    scheduleStopOnMainThread();
}

static uint64_t generateWillSendRequestID()
{
    static int64_t uniqueWillSendRequestID;
    return atomicIncrement(&uniqueWillSendRequestID);
}

void NetworkResourceLoader::willSendRequest(ResourceHandle*, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    // We only expect to get the willSendRequest callback from ResourceHandle as the result of a redirect.
    ASSERT(!redirectResponse.isNull());

    uint64_t requestID = generateWillSendRequestID();

    if (!send(Messages::WebResourceLoader::WillSendRequest(requestID, request, redirectResponse))) {
        request = ResourceRequest();
        return;
    }

    OwnPtr<ResourceRequest> newRequest = m_connection->willSendRequestResponseMap().waitForResponse(requestID);
    request = newRequest ? *newRequest : ResourceRequest();

    RunLoop::main()->dispatch(WTF::bind(&NetworkResourceLoadScheduler::receivedRedirect, &NetworkProcess::shared().networkResourceLoadScheduler(), m_identifier, request.url()));
}

void NetworkResourceLoader::willSendRequestHandled(uint64_t requestID, const WebCore::ResourceRequest& newRequest)
{
    m_connection->willSendRequestResponseMap().didReceiveResponse(requestID, adoptPtr(new ResourceRequest(newRequest)));
}

// FIXME (NetworkProcess): Many of the following ResourceHandleClient methods definitely need implementations. A few will not.
// Once we know what they are they can be removed.

void NetworkResourceLoader::didSendData(WebCore::ResourceHandle*, unsigned long long /*bytesSent*/, unsigned long long /*totalBytesToBeSent*/)
{
    notImplemented();
}

void NetworkResourceLoader::didReceiveCachedMetadata(WebCore::ResourceHandle*, const char*, int)
{
    notImplemented();
}

void NetworkResourceLoader::wasBlocked(WebCore::ResourceHandle*)
{
    notImplemented();
}

void NetworkResourceLoader::cannotShowURL(WebCore::ResourceHandle*)
{
    notImplemented();
}

bool NetworkResourceLoader::shouldUseCredentialStorage(ResourceHandle*)
{
    // When the WebProcess is handling loading a client is consulted each time this shouldUseCredentialStorage question is asked.
    // In NetworkProcess mode we ask the WebProcess client up front once and then reuse the cached answer.

    return m_requestParameters.allowStoredCredentials() == AllowStoredCredentials;
}

void NetworkResourceLoader::didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge& challenge)
{
    ASSERT(!m_currentAuthenticationChallenge);
    m_currentAuthenticationChallenge = adoptPtr(new AuthenticationChallenge(challenge));

    send(Messages::WebResourceLoader::DidReceiveAuthenticationChallenge(*m_currentAuthenticationChallenge));
}

void NetworkResourceLoader::didCancelAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge& challenge)
{
    ASSERT(m_currentAuthenticationChallenge);
    ASSERT(m_currentAuthenticationChallenge->identifier() == challenge.identifier());

    send(Messages::WebResourceLoader::DidCancelAuthenticationChallenge(*m_currentAuthenticationChallenge));

    m_currentAuthenticationChallenge.clear();
}

void NetworkResourceLoader::receivedCancellation(ResourceHandle*, const AuthenticationChallenge& challenge)
{
    receivedAuthenticationCancellation(challenge);
}

void NetworkResourceLoader::receivedAuthenticationCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    ASSERT(m_currentAuthenticationChallenge);
    ASSERT(m_currentAuthenticationChallenge->authenticationClient());

    if (m_currentAuthenticationChallenge->identifier() != challenge.identifier())
        return;
    
    m_currentAuthenticationChallenge->authenticationClient()->receivedCredential(*m_currentAuthenticationChallenge, credential);
    m_currentAuthenticationChallenge.clear();
}

void NetworkResourceLoader::receivedRequestToContinueWithoutAuthenticationCredential(const AuthenticationChallenge& challenge)
{
    ASSERT(m_currentAuthenticationChallenge);
    ASSERT(m_currentAuthenticationChallenge->authenticationClient());

    if (m_currentAuthenticationChallenge->identifier() != challenge.identifier())
        return;

    m_currentAuthenticationChallenge->authenticationClient()->receivedRequestToContinueWithoutCredential(*m_currentAuthenticationChallenge);
    m_currentAuthenticationChallenge.clear();
}

void NetworkResourceLoader::receivedAuthenticationCancellation(const AuthenticationChallenge& challenge)
{
    ASSERT(m_currentAuthenticationChallenge);
    ASSERT(m_currentAuthenticationChallenge->authenticationClient());

    if (m_currentAuthenticationChallenge->identifier() != challenge.identifier())
        return;

    m_currentAuthenticationChallenge->authenticationClient()->receivedCancellation(*m_currentAuthenticationChallenge);
    m_currentAuthenticationChallenge.clear();
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
static uint64_t generateCanAuthenticateAgainstProtectionSpaceID()
{
    static int64_t uniqueCanAuthenticateAgainstProtectionSpaceID;
    return atomicIncrement(&uniqueCanAuthenticateAgainstProtectionSpaceID);
}

bool NetworkResourceLoader::canAuthenticateAgainstProtectionSpace(ResourceHandle*, const ProtectionSpace& protectionSpace)
{
    uint64_t requestID = generateCanAuthenticateAgainstProtectionSpaceID();

    if (!send(Messages::WebResourceLoader::CanAuthenticateAgainstProtectionSpace(requestID, protectionSpace)))
        return false;

    return m_connection->canAuthenticateAgainstProtectionSpaceResponseMap().waitForResponse(requestID);
}

void NetworkResourceLoader::canAuthenticateAgainstProtectionSpaceHandled(uint64_t requestID, bool canAuthenticate)
{
    m_connection->canAuthenticateAgainstProtectionSpaceResponseMap().didReceiveResponse(requestID, canAuthenticate);
}
#endif

#if HAVE(NETWORK_CFDATA_ARRAY_CALLBACK)
bool NetworkResourceLoader::supportsDataArray()
{
    notImplemented();
    return false;
}

void NetworkResourceLoader::didReceiveDataArray(WebCore::ResourceHandle*, CFArrayRef)
{
    notImplemented();
}
#endif

#if PLATFORM(MAC)
#if USE(CFNETWORK)
CFCachedURLResponseRef NetworkResourceLoader::willCacheResponse(WebCore::ResourceHandle*, CFCachedURLResponseRef response)
{
    notImplemented();
    return response;
}
#else
NSCachedURLResponse* NetworkResourceLoader::willCacheResponse(WebCore::ResourceHandle*, NSCachedURLResponse* response)
{
    notImplemented();
    return response;
}
#endif

void NetworkResourceLoader::willStopBufferingData(WebCore::ResourceHandle*, const char*, int)
{
    notImplemented();
}
#endif // PLATFORM(MAC)

#if ENABLE(BLOB)
WebCore::AsyncFileStream* NetworkResourceLoader::createAsyncFileStream(WebCore::FileStreamClient*)
{
    notImplemented();
    return 0;
}
#endif

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
