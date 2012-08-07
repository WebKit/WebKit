/*
 * Copyright (C) 2006, 2007, 2010, 2011 Apple Inc. All rights reserved.
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
#include "AsyncFileStream.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "InspectorInstrumentation.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceLoadScheduler.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SharedBuffer.h"

namespace WebCore {

PassRefPtr<SharedBuffer> ResourceLoader::resourceData()
{
    return m_resourceData;
}

ResourceLoader::ResourceLoader(Frame* frame, ResourceLoaderOptions options)
    : m_frame(frame)
    , m_documentLoader(frame->loader()->activeDocumentLoader())
    , m_identifier(0)
    , m_reachedTerminalState(false)
    , m_calledWillCancel(false)
    , m_cancelled(false)
    , m_notifiedLoadComplete(false)
    , m_defersLoading(frame->page()->defersLoading())
    , m_options(options)
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

    resourceLoadScheduler()->remove(this);

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

bool ResourceLoader::init(const ResourceRequest& r)
{
    ASSERT(!m_handle);
    ASSERT(m_request.isNull());
    ASSERT(m_deferredRequest.isNull());
    ASSERT(!m_documentLoader->isSubstituteLoadPending(this));
    
    ResourceRequest clientRequest(r);
    
    m_defersLoading = m_frame->page()->defersLoading();
    if (m_options.securityCheck == DoSecurityCheck && !m_frame->document()->securityOrigin()->canDisplay(clientRequest.url())) {
        FrameLoader::reportLocalLoadFailed(m_frame.get(), clientRequest.url().string());
        releaseResources();
        return false;
    }
    
    // https://bugs.webkit.org/show_bug.cgi?id=26391
    // The various plug-in implementations call directly to ResourceLoader::load() instead of piping requests
    // through FrameLoader. As a result, they miss the FrameLoader::addExtraFieldsToRequest() step which sets
    // up the 1st party for cookies URL. Until plug-in implementations can be reigned in to pipe through that
    // method, we need to make sure there is always a 1st party for cookies set.
    if (clientRequest.firstPartyForCookies().isNull()) {
        if (Document* document = m_frame->document())
            clientRequest.setFirstPartyForCookies(document->firstPartyForCookies());
    }

    willSendRequest(clientRequest, ResourceResponse());
    if (clientRequest.isNull()) {
        cancel();
        return false;
    }

    m_originalRequest = m_request = clientRequest;
    return true;
}

void ResourceLoader::start()
{
    ASSERT(!m_handle);
    ASSERT(!m_request.isNull());
    ASSERT(m_deferredRequest.isNull());

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    if (m_documentLoader->scheduleArchiveLoad(this, m_request, m_request.url()))
        return;
#endif

    if (m_documentLoader->applicationCacheHost()->maybeLoadResource(this, m_request, m_request.url()))
        return;

    if (m_defersLoading) {
        m_deferredRequest = m_request;
        return;
    }

    if (!m_reachedTerminalState)
        m_handle = ResourceHandle::create(m_frame->loader()->networkingContext(), m_request, this, m_defersLoading, m_options.sniffContent == SniffContent);
}

void ResourceLoader::setDefersLoading(bool defers)
{
    m_defersLoading = defers;
    if (m_handle)
        m_handle->setDefersLoading(defers);
    if (!defers && !m_deferredRequest.isNull()) {
        m_request = m_deferredRequest;
        m_deferredRequest = ResourceRequest();
        start();
    }
}

FrameLoader* ResourceLoader::frameLoader() const
{
    if (!m_frame)
        return 0;
    return m_frame->loader();
}

void ResourceLoader::setShouldBufferData(DataBufferingPolicy shouldBufferData)
{ 
    m_options.shouldBufferData = shouldBufferData; 

    // Reset any already buffered data
    if (!shouldBufferData)
        m_resourceData = 0;
}
    

void ResourceLoader::addData(const char* data, int length, bool allAtOnce)
{
    if (m_options.shouldBufferData == DoNotBufferData)
        return;

    if (allAtOnce) {
        m_resourceData = SharedBuffer::create(data, length);
        return;
    }
        
    if (!m_resourceData)
        m_resourceData = SharedBuffer::create(data, length);
    else
        m_resourceData->append(data, length);
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

    if (m_options.sendLoadCallbacks == SendCallbacks) {
        if (!m_identifier) {
            m_identifier = m_frame->page()->progress()->createUniqueIdentifier();
            frameLoader()->notifier()->assignIdentifierToInitialRequest(m_identifier, documentLoader(), request);
        }

        frameLoader()->notifier()->willSendRequest(this, request, redirectResponse);
    }

    if (!redirectResponse.isNull())
        resourceLoadScheduler()->crossOriginRedirectReceived(this, request.url());
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
        
    if (m_options.sendLoadCallbacks == SendCallbacks)
        frameLoader()->notifier()->didReceiveResponse(this, m_response);
}

void ResourceLoader::didReceiveData(const char* data, int length, long long encodedDataLength, bool allAtOnce)
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
    if (m_options.sendLoadCallbacks == SendCallbacks && m_frame)
        frameLoader()->notifier()->didReceiveData(this, data, length, static_cast<int>(encodedDataLength));
}

void ResourceLoader::willStopBufferingData(const char* data, int length)
{
    if (m_options.shouldBufferData == DoNotBufferData)
        return;

    ASSERT(!m_resourceData);
    m_resourceData = SharedBuffer::create(data, length);
}

void ResourceLoader::didFinishLoading(double finishTime)
{
    didFinishLoadingOnePart(finishTime);

    // If the load has been cancelled by a delegate in response to didFinishLoad(), do not release
    // the resources a second time, they have been released by cancel.
    if (m_cancelled)
        return;
    releaseResources();
}

void ResourceLoader::didFinishLoadingOnePart(double finishTime)
{
    // If load has been cancelled after finishing (which could happen with a
    // JavaScript that changes the window location), do nothing.
    if (m_cancelled)
        return;
    ASSERT(!m_reachedTerminalState);

    if (m_notifiedLoadComplete)
        return;
    m_notifiedLoadComplete = true;
    if (m_options.sendLoadCallbacks == SendCallbacks)
        frameLoader()->notifier()->didFinishLoad(this, finishTime);
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

    if (!m_notifiedLoadComplete) {
        m_notifiedLoadComplete = true;
        if (m_options.sendLoadCallbacks == SendCallbacks)
            frameLoader()->notifier()->didFailToLoad(this, error);
    }

    releaseResources();
}

void ResourceLoader::cancel()
{
    cancel(ResourceError());
}

void ResourceLoader::cancel(const ResourceError& error)
{
    // If the load has already completed - succeeded, failed, or previously cancelled - do nothing.
    if (m_reachedTerminalState)
        return;
       
    ResourceError nonNullError = error.isNull() ? cancelledError() : error;
    
    // willCancel() and didFailToLoad() both call out to clients that might do 
    // something causing the last reference to this object to go away.
    RefPtr<ResourceLoader> protector(this);
    
    // If we re-enter cancel() from inside willCancel(), we want to pick up from where we left 
    // off without re-running willCancel()
    if (!m_calledWillCancel) {
        m_calledWillCancel = true;
        
        willCancel(nonNullError);
    }

    // If we re-enter cancel() from inside didFailToLoad(), we want to pick up from where we 
    // left off without redoing any of this work.
    if (!m_cancelled) {
        m_cancelled = true;
        
        if (FormData* data = m_request.httpBody())
            data->removeGeneratedFilesIfNeeded();

        if (m_handle)
            m_handle->clearAuthentication();

        m_documentLoader->cancelPendingSubstituteLoad(this);
        if (m_handle) {
            m_handle->cancel();
            m_handle = 0;
        }

        if (m_options.sendLoadCallbacks == SendCallbacks && m_identifier && !m_notifiedLoadComplete)
            frameLoader()->notifier()->didFailToLoad(this, nonNullError);
    }

    // If cancel() completed from within the call to willCancel() or didFailToLoad(),
    // we don't want to redo didCancel() or releasesResources().
    if (m_reachedTerminalState)
        return;

    didCancel(nonNullError);
            
    releaseResources();
}

ResourceError ResourceLoader::cancelledError()
{
    return frameLoader()->cancelledError(m_request);
}

ResourceError ResourceLoader::blockedError()
{
    return frameLoader()->client()->blockedError(m_request);
}

ResourceError ResourceLoader::cannotShowURLError()
{
    return frameLoader()->client()->cannotShowURLError(m_request);
}

void ResourceLoader::willSendRequest(ResourceHandle*, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (documentLoader()->applicationCacheHost()->maybeLoadFallbackForRedirect(this, request, redirectResponse))
        return;
    willSendRequest(request, redirectResponse);
}

void ResourceLoader::didSendData(ResourceHandle*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    didSendData(bytesSent, totalBytesToBeSent);
}

void ResourceLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    if (documentLoader()->applicationCacheHost()->maybeLoadFallbackForResponse(this, response))
        return;
    didReceiveResponse(response);
}

void ResourceLoader::didReceiveData(ResourceHandle*, const char* data, int length, int encodedDataLength)
{
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willReceiveResourceData(m_frame.get(), identifier());
    didReceiveData(data, length, encodedDataLength, false);
    InspectorInstrumentation::didReceiveResourceData(cookie);
}

void ResourceLoader::didFinishLoading(ResourceHandle*, double finishTime)
{
    didFinishLoading(finishTime);
}

void ResourceLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    if (documentLoader()->applicationCacheHost()->maybeLoadFallbackForError(this, error))
        return;
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
    if (m_options.allowCredentials == DoNotAllowStoredCredentials)
        return false;
    
    RefPtr<ResourceLoader> protector(this);
    return frameLoader()->client()->shouldUseCredentialStorage(documentLoader(), identifier());
}

void ResourceLoader::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    ASSERT(handle()->hasAuthenticationChallenge());
    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);

    if (m_options.allowCredentials == AllowStoredCredentials) {
        if (m_options.crossOriginCredentialPolicy == AskClientForCrossOriginCredentials || m_frame->document()->securityOrigin()->canRequest(originalRequest().url())) {
            frameLoader()->notifier()->didReceiveAuthenticationChallenge(this, challenge);
            return;
        }
    }
    // Only these platforms provide a way to continue without credentials.
    // If we can't continue with credentials, we need to cancel the load altogether.
#if PLATFORM(MAC) || USE(CFNETWORK) || USE(CURL)
    handle()->receivedRequestToContinueWithoutCredential(challenge);
    ASSERT(!handle()->hasAuthenticationChallenge());
#else
    didFail(blockedError());
#endif
}

void ResourceLoader::didCancelAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    RefPtr<ResourceLoader> protector(this);
    frameLoader()->notifier()->didCancelAuthenticationChallenge(this, challenge);
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
bool ResourceLoader::canAuthenticateAgainstProtectionSpace(const ProtectionSpace& protectionSpace)
{
    RefPtr<ResourceLoader> protector(this);
    return frameLoader()->client()->canAuthenticateAgainstProtectionSpace(documentLoader(), identifier(), protectionSpace);
}
#endif

void ResourceLoader::receivedCancellation(const AuthenticationChallenge&)
{
    cancel();
}

void ResourceLoader::willCacheResponse(ResourceHandle*, CacheStoragePolicy& policy)
{
    // <rdar://problem/7249553> - There are reports of crashes with this method being called
    // with a null m_frame->settings(), which can only happen if the frame doesn't have a page.
    // Sadly we have no reproducible cases of this.
    // We think that any frame without a page shouldn't have any loads happening in it, yet
    // there is at least one code path where that is not true.
    ASSERT(m_frame->settings());
    
    // When in private browsing mode, prevent caching to disk
    if (policy == StorageAllowed && m_frame->settings() && m_frame->settings()->privateBrowsingEnabled())
        policy = StorageAllowedInMemoryOnly;    
}

#if ENABLE(BLOB)
AsyncFileStream* ResourceLoader::createAsyncFileStream(FileStreamClient* client)
{
    // It is OK to simply return a pointer since AsyncFileStream::create adds an extra ref.
    return AsyncFileStream::create(m_frame->document()->scriptExecutionContext(), client).get();
}
#endif

}
