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
#include "NetworkRequest.h"

#if ENABLE(NETWORK_PROCESS)

#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkProcessConnectionMessages.h"
#include "RemoteNetworkingContext.h"
#include "SharedMemory.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceBuffer.h>
#include <WebCore/ResourceHandle.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

NetworkRequest::NetworkRequest(const WebCore::ResourceRequest& request, ResourceLoadIdentifier identifier, NetworkConnectionToWebProcess* connection)
    : m_request(request)
    , m_identifier(identifier)
    , m_connection(connection)
{
    ASSERT(isMainThread());
    connection->registerObserver(this);
}

NetworkRequest::~NetworkRequest()
{
    ASSERT(isMainThread());

    if (m_connection)
        m_connection->unregisterObserver(this);
}

void NetworkRequest::start()
{
    ASSERT(isMainThread());

    // Explicit ref() balanced by a deref() in NetworkRequest::stop()
    ref();
    
    // FIXME (NetworkProcess): Create RemoteNetworkingContext with actual settings.
    m_networkingContext = RemoteNetworkingContext::create(false, false);

    // FIXME (NetworkProcess): Pass an actual value for shouldContentSniff (XMLHttpRequest needs that).
    m_handle = ResourceHandle::create(m_networkingContext.get(), m_request, this, false, false);
}

static bool stopRequestsCalled = false;

static Mutex& requestsToStopMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

static HashSet<NetworkRequest*>& requestsToStop()
{
    DEFINE_STATIC_LOCAL(HashSet<NetworkRequest*>, requests, ());
    return requests;
}

void NetworkRequest::scheduleStopOnMainThread()
{
    MutexLocker locker(requestsToStopMutex());

    requestsToStop().add(this);
    if (!stopRequestsCalled) {
        stopRequestsCalled = true;
        callOnMainThread(NetworkRequest::performStops, 0);
    }
}

void NetworkRequest::performStops(void*)
{
    ASSERT(stopRequestsCalled);

    Vector<NetworkRequest*> requests;
    {
        MutexLocker locker(requestsToStopMutex());
        copyToVector(requestsToStop(), requests);
        requestsToStop().clear();
        stopRequestsCalled = false;
    }
    
    for (size_t i = 0; i < requests.size(); ++i)
        requests[i]->stop();
}

void NetworkRequest::stop()
{
    ASSERT(isMainThread());

    // Remove this load identifier soon so we can start more network requests.
    NetworkProcess::shared().networkResourceLoadScheduler().scheduleRemoveLoadIdentifier(m_identifier);
    
    // Explicit deref() balanced by a ref() in NetworkRequest::stop()
    // This might cause the NetworkRequest to be destroyed and therefore we do it last.
    deref();
}

void NetworkRequest::connectionToWebProcessDidClose(NetworkConnectionToWebProcess* connection)
{
    ASSERT_ARG(connection, connection == m_connection.get());
    m_connection->unregisterObserver(this);
    m_connection = 0;
}

void NetworkRequest::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    // FIXME (NetworkProcess): Cache the response.
    connectionToWebProcess()->connection()->send(Messages::NetworkProcessConnection::DidReceiveResponse(m_identifier, response), 0);
}

void NetworkRequest::didReceiveData(ResourceHandle*, const char* data, int length, int /*encodedDataLength*/)
{
    // FIXME (NetworkProcess): We have to progressively notify the WebProcess as data is received.
    if (!m_buffer)
        m_buffer = ResourceBuffer::create();
    m_buffer->append(data, length);
}

void NetworkRequest::didFinishLoading(ResourceHandle*, double finishTime)
{
    // FIXME (NetworkProcess): Currently this callback can come in on a non-main thread.
    // This is okay for now since resource buffers are per-NetworkRequest.
    // Once we manage memory in an actual memory cache that also includes SharedMemory blocks this will get complicated.
    // Maybe we should marshall it to the main thread?

    ShareableResource::Handle handle;

    if (m_buffer && m_buffer->size()) {
        // FIXME (NetworkProcess): We shouldn't be creating this SharedMemory on demand here.
        // SharedMemory blocks need to be managed as part of the cache backing store.
        RefPtr<SharedMemory> sharedBuffer = SharedMemory::create(m_buffer->size());
        memcpy(sharedBuffer->data(), m_buffer->data(), m_buffer->size());

        RefPtr<ShareableResource> shareableResource = ShareableResource::create(sharedBuffer.release(), 0, m_buffer->size());

        // FIXME (NetworkProcess): Handle errors from createHandle();
        if (!shareableResource->createHandle(handle))
            LOG_ERROR("Failed to create handle to shareable resource memory\n");
    }

    connectionToWebProcess()->connection()->send(Messages::NetworkProcessConnection::DidReceiveResource(m_identifier, handle, finishTime), 0);
    scheduleStopOnMainThread();
}

void NetworkRequest::didFail(WebCore::ResourceHandle*, const WebCore::ResourceError& error)
{
    connectionToWebProcess()->connection()->send(Messages::NetworkProcessConnection::DidFailResourceLoad(m_identifier, error), 0);
    scheduleStopOnMainThread();
}

// FIXME (NetworkProcess): Many of the following ResourceHandleClient methods definitely need implementations. A few will not.
// Once we know what they are they can be removed.

void NetworkRequest::willSendRequest(WebCore::ResourceHandle*, WebCore::ResourceRequest&, const WebCore::ResourceResponse& /*redirectResponse*/)
{
    notImplemented();
}

void NetworkRequest::didSendData(WebCore::ResourceHandle*, unsigned long long /*bytesSent*/, unsigned long long /*totalBytesToBeSent*/)
{
    notImplemented();
}

void NetworkRequest::didReceiveCachedMetadata(WebCore::ResourceHandle*, const char*, int)
{
    notImplemented();
}

void NetworkRequest::wasBlocked(WebCore::ResourceHandle*)
{
    notImplemented();
}

void NetworkRequest::cannotShowURL(WebCore::ResourceHandle*)
{
    notImplemented();
}

void NetworkRequest::willCacheResponse(WebCore::ResourceHandle*, WebCore::CacheStoragePolicy&)
{
    notImplemented();
}

bool NetworkRequest::shouldUseCredentialStorage(WebCore::ResourceHandle*)
{
    notImplemented();
    return false;
}

void NetworkRequest::didReceiveAuthenticationChallenge(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&)
{
    notImplemented();
}

void NetworkRequest::didCancelAuthenticationChallenge(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&)
{
    notImplemented();
}

void NetworkRequest::receivedCancellation(WebCore::ResourceHandle*, const WebCore::AuthenticationChallenge&)
{
    notImplemented();
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool NetworkRequest::canAuthenticateAgainstProtectionSpace(WebCore::ResourceHandle*, const WebCore::ProtectionSpace&)
{
    notImplemented();
    return false;
}
#endif

#if HAVE(NETWORK_CFDATA_ARRAY_CALLBACK)
bool NetworkRequest::supportsDataArray()
{
    notImplemented();
    return false;
}

void NetworkRequest::didReceiveDataArray(WebCore::ResourceHandle*, CFArrayRef)
{
    notImplemented();
}
#endif

#if PLATFORM(MAC)
#if USE(CFNETWORK)
CFCachedURLResponseRef NetworkRequest::willCacheResponse(WebCore::ResourceHandle*, CFCachedURLResponseRef response)
{
    notImplemented();
    return response;
}
#else
NSCachedURLResponse* NetworkRequest::willCacheResponse(WebCore::ResourceHandle*, NSCachedURLResponse* response)
{
    notImplemented();
    return response;
}
#endif

void NetworkRequest::willStopBufferingData(WebCore::ResourceHandle*, const char*, int)
{
    notImplemented();
}
#endif // PLATFORM(MAC)

#if ENABLE(BLOB)
WebCore::AsyncFileStream* NetworkRequest::createAsyncFileStream(WebCore::FileStreamClient*)
{
    notImplemented();
    return 0;
}
#endif

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
