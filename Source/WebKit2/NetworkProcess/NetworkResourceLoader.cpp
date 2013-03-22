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
#include "NetworkProcessConnectionMessages.h"
#include "NetworkResourceLoadParameters.h"
#include "PlatformCertificateInfo.h"
#include "RemoteNetworkingContext.h"
#include "ShareableResource.h"
#include "SharedMemory.h"
#include "WebCoreArgumentCoders.h"
#include "WebResourceLoaderMessages.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceBuffer.h>
#include <WebCore/ResourceHandle.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>

using namespace WebCore;

namespace WebKit {

NetworkResourceLoader::NetworkResourceLoader(const NetworkResourceLoadParameters& loadParameters, NetworkConnectionToWebProcess* connection)
    : SchedulableLoader(loadParameters, connection)
    , m_bytesReceived(0)
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    , m_diskCacheTimer(RunLoop::main(), this, &NetworkResourceLoader::diskCacheTimerFired)
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
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

static bool performCleanupsCalled = false;

static Mutex& requestsToCleanupMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

static HashSet<NetworkResourceLoader*>& requestsToCleanup()
{
    DEFINE_STATIC_LOCAL(HashSet<NetworkResourceLoader*>, requests, ());
    return requests;
}

void NetworkResourceLoader::scheduleCleanupOnMainThread()
{
    MutexLocker locker(requestsToCleanupMutex());

    requestsToCleanup().add(this);
    if (!performCleanupsCalled) {
        performCleanupsCalled = true;
        callOnMainThread(NetworkResourceLoader::performCleanups, 0);
    }
}

void NetworkResourceLoader::performCleanups(void*)
{
    ASSERT(performCleanupsCalled);

    Vector<NetworkResourceLoader*> requests;
    {
        MutexLocker locker(requestsToCleanupMutex());
        copyToVector(requestsToCleanup(), requests);
        requestsToCleanup().clear();
        performCleanupsCalled = false;
    }
    
    for (size_t i = 0; i < requests.size(); ++i)
        requests[i]->cleanup();
}

void NetworkResourceLoader::cleanup()
{
    ASSERT(isMainThread());

    if (FormData* formData = request().httpBody())
        formData->removeGeneratedFilesIfNeeded();

    // Tell the scheduler about this finished loader soon so it can start more network requests.
    NetworkProcess::shared().networkResourceLoadScheduler().scheduleRemoveLoader(this);

    if (m_handle) {
        // Explicit deref() balanced by a ref() in NetworkResourceLoader::start()
        // This might cause the NetworkResourceLoader to be destroyed and therefore we do it last.
        m_handle = 0;
        deref();
    }
}

void NetworkResourceLoader::connectionToWebProcessDidClose()
{
    ASSERT(isMainThread());

    // If this loader already has a resource handle then it is already in progress on a background thread.
    // On that thread it will notice that its connection to its WebProcess has been invalidated and it will "gracefully" abort.
    if (m_handle)
        return;

#if !ASSERT_DISABLED
    // Since there's no handle, this loader should never have been started, and therefore it should never be in the
    // set of loaders to cleanup on the main thread.
    // Let's make sure that's true.
    {
        MutexLocker locker(requestsToCleanupMutex());
        ASSERT(!requestsToCleanup().contains(this));
    }
#endif

    cleanup();
}

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
void NetworkResourceLoader::diskCacheTimerFired()
{
    ASSERT(isMainThread());
    RefPtr<NetworkResourceLoader> adoptedRef = adoptRef(this); // Balance out the ref() when setting the timer.

    ShareableResource::Handle handle;
    tryGetShareableHandleForResource(handle);
    if (handle.isNull())
        return;

    send(Messages::NetworkProcessConnection::DidCacheResource(request(), handle));
}
#endif // #if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090

template<typename U> bool NetworkResourceLoader::sendAbortingOnFailure(const U& message)
{
    bool result = send(message);
    if (!result)
        abortInProgressLoad();
    return result;
}

template<typename U> bool NetworkResourceLoader::sendSyncAbortingOnFailure(const U& message, const typename U::Reply& reply)
{
    bool result = sendSync(message, reply);
    if (!result)
        abortInProgressLoad();
    return result;
}

void NetworkResourceLoader::abortInProgressLoad()
{
    ASSERT(m_handle);
    ASSERT(!isMainThread());
 
    m_handle->cancel();

    scheduleCleanupOnMainThread();
}

void NetworkResourceLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    // FIXME (NetworkProcess): Cache the response.
    if (FormData* formData = request().httpBody())
        formData->removeGeneratedFilesIfNeeded();

    if (!sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveResponseWithCertificateInfo(response, PlatformCertificateInfo(response))))
        return;

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    ShareableResource::Handle handle;
    tryGetShareableHandleForResource(handle);
    if (handle.isNull())
        return;

    // Since we're delivering this resource by ourselves all at once, we'll abort the resource handle since we don't need anymore callbacks from ResourceHandle.
    abortInProgressLoad();
    
    send(Messages::WebResourceLoader::DidReceiveResource(handle, currentTime()));
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
}

void NetworkResourceLoader::didReceiveData(ResourceHandle*, const char* data, int length, int encodedDataLength)
{
    // FIXME (NetworkProcess): For the memory cache we'll also need to cache the response data here.
    // Such buffering will need to be thread safe, as this callback is happening on a background thread.
    
    m_bytesReceived += length;

    CoreIPC::DataReference dataReference(reinterpret_cast<const uint8_t*>(data), length);
    sendAbortingOnFailure(Messages::WebResourceLoader::DidReceiveData(dataReference, encodedDataLength));
}

void NetworkResourceLoader::didFinishLoading(ResourceHandle*, double finishTime)
{
    // FIXME (NetworkProcess): For the memory cache we'll need to update the finished status of the cached resource here.
    // Such bookkeeping will need to be thread safe, as this callback is happening on a background thread.
    invalidateSandboxExtensions();
    send(Messages::WebResourceLoader::DidFinishResourceLoad(finishTime));

#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    // If this resource was large enough that it should be cached to disk as a separate file,
    // then we should try to re-deliver the resource to the WebProcess once it *is* saved as a separate file.    
    if (m_bytesReceived >= fileBackedResourceMinimumSize()) {
        // FIXME: Once a notification API exists that obliviates this timer, use that instead.
        ref(); // Balanced by an adoptRef() in diskCacheTimerFired().
        m_diskCacheTimer.startOneShot(10);
    }
#endif // __MAC_OS_X_VERSION_MIN_REQUIRED >= 1090
    
    scheduleCleanupOnMainThread();
}

void NetworkResourceLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    // FIXME (NetworkProcess): For the memory cache we'll need to update the finished status of the cached resource here.
    // Such bookkeeping will need to be thread safe, as this callback is happening on a background thread.
    invalidateSandboxExtensions();
    send(Messages::WebResourceLoader::DidFailResourceLoad(error));
    scheduleCleanupOnMainThread();
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
    if (!sendSyncAbortingOnFailure(Messages::WebResourceLoader::WillSendRequest(request, redirectResponse), Messages::WebResourceLoader::WillSendRequest::Reply(returnedRequest)))
        return;
    
    request.updateFromDelegatePreservingOldHTTPBody(returnedRequest.nsURLRequest(DoNotUpdateHTTPBody));

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
    if (!sendSyncAbortingOnFailure(Messages::WebResourceLoader::CanAuthenticateAgainstProtectionSpace(protectionSpace), Messages::WebResourceLoader::CanAuthenticateAgainstProtectionSpace::Reply(result)))
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

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
