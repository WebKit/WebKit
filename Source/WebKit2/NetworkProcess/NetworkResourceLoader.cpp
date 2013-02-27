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

#include "AuthenticationManager.h"
#include "DataReference.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoadParameters.h"
#include "PlatformCertificateInfo.h"
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

NetworkResourceLoader::NetworkResourceLoader(const NetworkResourceLoadParameters& loadParameters, NetworkConnectionToWebProcess* connection)
    : SchedulableLoader(loadParameters, connection)
{
    ASSERT(isMainThread());
}

NetworkResourceLoader::~NetworkResourceLoader()
{
    ASSERT(isMainThread());
    ASSERT(!m_handle);
}

CoreIPC::Connection* NetworkResourceLoader::connection() const
{
    return connectionToWebProcess()->connection();
}

uint64_t NetworkResourceLoader::destinationID() const
{
    return identifier();
}

void NetworkResourceLoader::start()
{
    ASSERT(isMainThread());

    // Explicit ref() balanced by a deref() in NetworkResourceLoader::resourceHandleStopped()
    ref();
    
    // FIXME (NetworkProcess): Create RemoteNetworkingContext with actual settings.
    m_networkingContext = RemoteNetworkingContext::create(false, false, inPrivateBrowsingMode(), shouldClearReferrerOnHTTPSToHTTPRedirect());

    consumeSandboxExtensions();

    // FIXME (NetworkProcess): Pass an actual value for defersLoading
    m_handle = ResourceHandle::create(m_networkingContext.get(), request(), this, false /* defersLoading */, contentSniffingPolicy() == SniffContent);
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
        requests[i]->resourceHandleStopped();
}

void NetworkResourceLoader::resourceHandleStopped()
{
    ASSERT(isMainThread());

    if (FormData* formData = request().httpBody())
        formData->removeGeneratedFilesIfNeeded();

    m_handle = 0;

    // Tell the scheduler about this finished loader soon so it can start more network requests.
    NetworkProcess::shared().networkResourceLoadScheduler().scheduleRemoveLoader(this);

    // Explicit deref() balanced by a ref() in NetworkResourceLoader::start()
    // This might cause the NetworkResourceLoader to be destroyed and therefore we do it last.
    deref();
}

void NetworkResourceLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    // FIXME (NetworkProcess): Cache the response.
    if (FormData* formData = request().httpBody())
        formData->removeGeneratedFilesIfNeeded();
    send(Messages::WebResourceLoader::DidReceiveResponseWithCertificateInfo(response, PlatformCertificateInfo(response)));
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
    invalidateSandboxExtensions();
    send(Messages::WebResourceLoader::DidFinishResourceLoad(finishTime));
    scheduleStopOnMainThread();
}

void NetworkResourceLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    // FIXME (NetworkProcess): For the memory cache we'll need to update the finished status of the cached resource here.
    // Such bookkeeping will need to be thread safe, as this callback is happening on a background thread.
    invalidateSandboxExtensions();
    send(Messages::WebResourceLoader::DidFailResourceLoad(error));
    scheduleStopOnMainThread();
}

void NetworkResourceLoader::willSendRequest(ResourceHandle*, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    // We only expect to get the willSendRequest callback from ResourceHandle as the result of a redirect.
    ASSERT(!redirectResponse.isNull());
    ASSERT(!isMainThread());

    // IMPORTANT: The fact that this message to the WebProcess is sync is what makes our current approach to synchronous XMLHttpRequests safe.
    // If this message changes to be asynchronous we might introduce a situation where the NetworkProcess is deadlocked waiting for 6 connections
    // to complete while the WebProcess is waiting for a 7th to complete.
    // If we ever change this message to be asynchronous we have to include safeguards to make sure the new design interacts well with sync XHR.
    ResourceRequest returnedRequest;
    if (sendSync(Messages::WebResourceLoader::WillSendRequest(request, redirectResponse), Messages::WebResourceLoader::WillSendRequest::Reply(returnedRequest)))
        request.updateFromDelegatePreservingOldHTTPBody(returnedRequest.nsURLRequest(DoNotUpdateHTTPBody));
    else
        request = ResourceRequest();

    RunLoop::main()->dispatch(bind(&NetworkResourceLoadScheduler::receivedRedirect, &NetworkProcess::shared().networkResourceLoadScheduler(), this, request.url()));
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

    return allowStoredCredentials() == AllowStoredCredentials;
}

void NetworkResourceLoader::didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge& challenge)
{
    NetworkProcess::shared().authenticationManager().didReceiveAuthenticationChallenge(webPageID(), webFrameID(), challenge);
}

void NetworkResourceLoader::didCancelAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge& challenge)
{
    // FIXME (NetworkProcess): Tell AuthenticationManager.
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool NetworkResourceLoader::canAuthenticateAgainstProtectionSpace(ResourceHandle*, const ProtectionSpace& protectionSpace)
{
    ASSERT(!isMainThread());

    // IMPORTANT: The fact that this message to the WebProcess is sync is what makes our current approach to synchronous XMLHttpRequests safe.
    // If this message changes to be asynchronous we might introduce a situation where the NetworkProcess is deadlocked waiting for 6 connections
    // to complete while the WebProcess is waiting for a 7th to complete.
    // If we ever change this message to be asynchronous we have to include safeguards to make sure the new design interacts well with sync XHR.
    bool result;
    if (!sendSync(Messages::WebResourceLoader::CanAuthenticateAgainstProtectionSpace(protectionSpace), Messages::WebResourceLoader::CanAuthenticateAgainstProtectionSpace::Reply(result)))
        return false;

    return result;
}
#endif

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
bool NetworkResourceLoader::supportsDataArray()
{
    notImplemented();
    return false;
}

void NetworkResourceLoader::didReceiveDataArray(WebCore::ResourceHandle*, CFArrayRef)
{
    ASSERT_NOT_REACHED();
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
