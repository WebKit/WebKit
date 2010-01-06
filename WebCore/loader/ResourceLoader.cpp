/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ResourceLoader.h"

#include "ApplicationCacheHost.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "ResourceHandle.h"
#include "ResourceError.h"
#include "Settings.h"
#include "SharedBuffer.h"

namespace WebCore {

PassRefPtr<SharedBuffer> ResourceLoader::resourceData()
{
    if (m_resourceData)
        return m_resourceData;

    if (ResourceHandle::supportsBufferedData() && m_handle)
        return m_handle->bufferedData();
    
    return 0;
}

ResourceLoader::ResourceLoader(Frame* frame, bool sendResourceLoadCallbacks, bool shouldContentSniff)
    : m_frame(frame)
    , m_documentLoader(frame->loader()->activeDocumentLoader())
    , m_identifier(0)
    , m_reachedTerminalState(false)
    , m_cancelled(false)
    , m_calledDidFinishLoad(false)
    , m_sendResourceLoadCallbacks(sendResourceLoadCallbacks)
    , m_shouldContentSniff(shouldContentSniff)
    , m_shouldBufferData(true)
    , m_defersLoading(frame->page()->defersLoading())
{
}

ResourceLoader::~ResourceLoader()
{
    ASSERT(m_reachedTerminalState);
}

void ResourceLoader::releaseResources()
{
    ASSERT(!m_reachedTerminalState);
    
    // It's possible that when we release the handle, it will be
    // deallocated and release the last reference to this object.
    // We need to retain to avoid accessing the object after it
    // has been deallocated and also to avoid reentering this method.
    RefPtr<ResourceLoader> protector(this);

    m_frame = 0;
    m_documentLoader = 0;
    
    // We need to set reachedTerminalState to true before we release
    // the resources to prevent a double dealloc of WebView <rdar://problem/4372628>
    m_reachedTerminalState = true;

    m_identifier = 0;

    if (m_handle) {
        // Clear out the ResourceHandle's client so that it doesn't try to call
        // us back after we release it, unless it has been replaced by someone else.
        if (m_handle->client() == this)
            m_handle->setClient(0);
        m_handle = 0;
    }

    m_resourceData = 0;
    m_deferredRequest = ResourceRequest();
}

bool ResourceLoader::load(const ResourceRequest& r)
{
    ASSERT(!m_handle);
    ASSERT(m_deferredRequest.isNull());
    ASSERT(!m_documentLoader->isSubstituteLoadPending(this));
    
    ResourceRequest clientRequest(r);
    willSendRequest(clientRequest, ResourceResponse());
    if (clientRequest.isNull()) {
        didFail(frameLoader()->cancelledError(r));
        return false;
    }
    
    if (m_documentLoader->scheduleArchiveLoad(this, clientRequest, r.url()))
        return true;
    
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (m_documentLoader->applicationCacheHost()->maybeLoadResource(this, clientRequest, r.url()))
        return true;
#endif

    if (m_defersLoading) {
        m_deferredRequest = clientRequest;
        return true;
    }
    
    m_handle = ResourceHandle::create(clientRequest, this, m_frame.get(), m_defersLoading, m_shouldContentSniff, true);

    return true;
}

void ResourceLoader::setDefersLoading(bool defers)
{
    m_defersLoading = defers;
    if (m_handle)
        m_handle->setDefersLoading(defers);
    if (!defers && !m_deferredRequest.isNull()) {
        ResourceRequest request(m_deferredRequest);
        m_deferredRequest = ResourceRequest();
        load(request);
    }
}

FrameLoader* ResourceLoader::frameLoader() const
{
    if (!m_frame)
        return 0;
    return m_frame->loader();
}

void ResourceLoader::setShouldBufferData(bool shouldBufferData)
{ 
    m_shouldBufferData = shouldBufferData; 

    // Reset any already buffered data
    if (!m_shouldBufferData)
        m_resourceData = 0;
}
    

void ResourceLoader::addData(const char* data, int length, bool allAtOnce)
{
    if (!m_shouldBufferData)
        return;

    if (allAtOnce) {
        m_resourceData = SharedBuffer::create(data, length);
        return;
    }
        
    if (ResourceHandle::supportsBufferedData()) {
        // Buffer data only if the connection has handed us the data because is has stopped buffering it.
        if (m_resourceData)
            m_resourceData->append(data, length);
    } else {
        if (!m_resourceData)
            m_resourceData = SharedBuffer::create(data, length);
        else
            m_resourceData->append(data, length);
    }
}

void ResourceLoader::clearResourceData()
{
    if (m_resourceData)
        m_resourceData->clear();
}

void ResourceLoader::willSendRequest(ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);

    ASSERT(!m_reachedTerminalState);

    if (m_sendResourceLoadCallbacks) {
        if (!m_identifier) {
            m_identifier = m_frame->page()->progress()->createUniqueIdentifier();
            frameLoader()->notifier()->assignIdentifierToInitialRequest(m_identifier, documentLoader(), request);
        }

        frameLoader()->notifier()->willSendRequest(this, request, redirectResponse);
    }

    m_request = request;
}

void ResourceLoader::didSendData(unsigned long long, unsigned long long)
{
}

void ResourceLoader::didReceiveResponse(const ResourceResponse& r)
{
    ASSERT(!m_reachedTerminalState);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);

    m_response = r;

    if (FormData* data = m_request.httpBody())
        data->removeGeneratedFilesIfNeeded();
        
    if (m_sendResourceLoadCallbacks)
        frameLoader()->notifier()->didReceiveResponse(this, m_response);
}

void ResourceLoader::didReceiveData(const char* data, int length, long long lengthReceived, bool allAtOnce)
{
    // The following assertions are not quite valid here, since a subclass
    // might override didReceiveData in a way that invalidates them. This
    // happens with the steps listed in 3266216
    // ASSERT(con == connection);
    // ASSERT(!m_reachedTerminalState);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);

    addData(data, length, allAtOnce);
    // FIXME: If we get a resource with more than 2B bytes, this code won't do the right thing.
    // However, with today's computers and networking speeds, this won't happen in practice.
    // Could be an issue with a giant local file.
    if (m_sendResourceLoadCallbacks && m_frame)
        frameLoader()->notifier()->didReceiveData(this, data, length, static_cast<int>(lengthReceived));
}

void ResourceLoader::willStopBufferingData(const char* data, int length)
{
    if (!m_shouldBufferData)
        return;

    ASSERT(!m_resourceData);
    m_resourceData = SharedBuffer::create(data, length);
}

void ResourceLoader::didFinishLoading()
{
    // If load has been cancelled after finishing (which could happen with a 
    // JavaScript that changes the window location), do nothing.
    if (m_cancelled)
        return;
    ASSERT(!m_reachedTerminalState);

    didFinishLoadingOnePart();
    releaseResources();
}

void ResourceLoader::didFinishLoadingOnePart()
{
    if (m_cancelled)
        return;
    ASSERT(!m_reachedTerminalState);

    if (m_calledDidFinishLoad)
        return;
    m_calledDidFinishLoad = true;
    if (m_sendResourceLoadCallbacks)
        frameLoader()->notifier()->didFinishLoad(this);
}

void ResourceLoader::didFail(const ResourceError& error)
{
    if (m_cancelled)
        return;
    ASSERT(!m_reachedTerminalState);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);

    if (FormData* data = m_request.httpBody())
        data->removeGeneratedFilesIfNeeded();

    if (m_sendResourceLoadCallbacks && !m_calledDidFinishLoad)
        frameLoader()->notifier()->didFailToLoad(this, error);

    releaseResources();
}

void ResourceLoader::didCancel(const ResourceError& error)
{
    ASSERT(!m_cancelled);
    ASSERT(!m_reachedTerminalState);

    if (FormData* data = m_request.httpBody())
        data->removeGeneratedFilesIfNeeded();

    // This flag prevents bad behavior when loads that finish cause the
    // load itself to be cancelled (which could happen with a javascript that 
    // changes the window location). This is used to prevent both the body
    // of this method and the body of connectionDidFinishLoading: running
    // for a single delegate. Cancelling wins.
    m_cancelled = true;
    
    if (m_handle)
        m_handle->clearAuthentication();

    m_documentLoader->cancelPendingSubstituteLoad(this);
    if (m_handle) {
        m_handle->cancel();
        m_handle = 0;
    }
    if (m_sendResourceLoadCallbacks && !m_calledDidFinishLoad)
        frameLoader()->notifier()->didFailToLoad(this, error);

    releaseResources();
}

void ResourceLoader::cancel()
{
    cancel(ResourceError());
}

void ResourceLoader::cancel(const ResourceError& error)
{
    if (m_reachedTerminalState)
        return;
    if (!error.isNull())
        didCancel(error);
    else
        didCancel(cancelledError());
}

const ResourceResponse& ResourceLoader::response() const
{
    return m_response;
}

ResourceError ResourceLoader::cancelledError()
{
    return frameLoader()->cancelledError(m_request);
}

ResourceError ResourceLoader::blockedError()
{
    return frameLoader()->blockedError(m_request);
}

ResourceError ResourceLoader::cannotShowURLError()
{
    return frameLoader()->cannotShowURLError(m_request);
}

void ResourceLoader::willSendRequest(ResourceHandle*, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (documentLoader()->applicationCacheHost()->maybeLoadFallbackForRedirect(this, request, redirectResponse))
        return;
#endif
    willSendRequest(request, redirectResponse);
}

void ResourceLoader::didSendData(ResourceHandle*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    didSendData(bytesSent, totalBytesToBeSent);
}

void ResourceLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (documentLoader()->applicationCacheHost()->maybeLoadFallbackForResponse(this, response))
        return;
#endif
    didReceiveResponse(response);
}

void ResourceLoader::didReceiveData(ResourceHandle*, const char* data, int length, int lengthReceived)
{
    didReceiveData(data, length, lengthReceived, false);
}

void ResourceLoader::didFinishLoading(ResourceHandle*)
{
    didFinishLoading();
}

void ResourceLoader::didFail(ResourceHandle*, const ResourceError& error)
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (documentLoader()->applicationCacheHost()->maybeLoadFallbackForError(this, error))
        return;
#endif
    didFail(error);
}

void ResourceLoader::wasBlocked(ResourceHandle*)
{
    didFail(blockedError());
}

void ResourceLoader::cannotShowURL(ResourceHandle*)
{
    didFail(cannotShowURLError());
}

bool ResourceLoader::shouldUseCredentialStorage()
{
    RefPtr<ResourceLoader> protector(this);
    return frameLoader()->shouldUseCredentialStorage(this);
}

void ResourceLoader::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);
    frameLoader()->notifier()->didReceiveAuthenticationChallenge(this, challenge);
}

void ResourceLoader::didCancelAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);
    frameLoader()->notifier()->didCancelAuthenticationChallenge(this, challenge);
}

void ResourceLoader::receivedCancellation(const AuthenticationChallenge&)
{
    cancel();
}

void ResourceLoader::willCacheResponse(ResourceHandle*, CacheStoragePolicy& policy)
{
    // When in private browsing mode, prevent caching to disk
    if (policy == StorageAllowed && m_frame->settings()->privateBrowsingEnabled())
        policy = StorageAllowedInMemoryOnly;    
}

}
