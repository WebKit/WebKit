/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
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
#include "SubresourceLoader.h"

#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "ResourceHandle.h"
#include "SecurityOrigin.h"
#include "SubresourceLoaderClient.h"
#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {

#ifndef NDEBUG    
static WTF::RefCountedLeakCounter subresourceLoaderCounter("SubresourceLoader");
#endif

SubresourceLoader::SubresourceLoader(Frame* frame, SubresourceLoaderClient* client, bool sendResourceLoadCallbacks, bool shouldContentSniff)
    : ResourceLoader(frame, sendResourceLoadCallbacks, shouldContentSniff)
    , m_client(client)
    , m_loadingMultipartContent(false)
{
#ifndef NDEBUG
    subresourceLoaderCounter.increment();
#endif
    m_documentLoader->addSubresourceLoader(this);
}

SubresourceLoader::~SubresourceLoader()
{
#ifndef NDEBUG
    subresourceLoaderCounter.decrement();
#endif
}

PassRefPtr<SubresourceLoader> SubresourceLoader::create(Frame* frame, SubresourceLoaderClient* client, const ResourceRequest& request, bool skipCanLoadCheck, bool sendResourceLoadCallbacks, bool shouldContentSniff)
{
    if (!frame)
        return 0;

    FrameLoader* fl = frame->loader();
    if (!skipCanLoadCheck && (fl->state() == FrameStateProvisional || fl->activeDocumentLoader()->isStopping()))
        return 0;

    ResourceRequest newRequest = request;

    if (!skipCanLoadCheck
            && SecurityOrigin::restrictAccessToLocal()
            && !SecurityOrigin::canLoad(request.url(), String(), frame->document())) {
        FrameLoader::reportLocalLoadFailed(frame, request.url().string());
        return 0;
    }
    
    if (SecurityOrigin::shouldHideReferrer(request.url(), fl->outgoingReferrer()))
        newRequest.clearHTTPReferrer();
    else if (!request.httpReferrer())
        newRequest.setHTTPReferrer(fl->outgoingReferrer());
    FrameLoader::addHTTPOriginIfNeeded(newRequest, fl->outgoingOrigin());

    // Use the original request's cache policy for two reasons:
    // 1. For POST requests, we mutate the cache policy for the main resource,
    //    but we do not want this to apply to subresources
    // 2. Delegates that modify the cache policy using willSendRequest: should
    //    not affect any other resources. Such changes need to be done
    //    per request.
    if (newRequest.isConditional())
        newRequest.setCachePolicy(ReloadIgnoringCacheData);
    else
        newRequest.setCachePolicy(fl->originalRequest().cachePolicy());

    fl->addExtraFieldsToSubresourceRequest(newRequest);

    RefPtr<SubresourceLoader> subloader(adoptRef(new SubresourceLoader(frame, client, sendResourceLoadCallbacks, shouldContentSniff)));
    if (!subloader->load(newRequest))
        return 0;

    return subloader.release();
}

void SubresourceLoader::willSendRequest(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    // Store the previous URL because the call to ResourceLoader::willSendRequest will modify it.
    KURL previousURL = request().url();
    
    ResourceLoader::willSendRequest(newRequest, redirectResponse);
    if (!previousURL.isNull() && !newRequest.isNull() && previousURL != newRequest.url() && m_client)
        m_client->willSendRequest(this, newRequest, redirectResponse);
}

void SubresourceLoader::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    RefPtr<SubresourceLoader> protect(this);

    if (m_client)
        m_client->didSendData(this, bytesSent, totalBytesToBeSent);
}

void SubresourceLoader::didReceiveResponse(const ResourceResponse& r)
{
    ASSERT(!r.isNull());

    if (r.isMultipart())
        m_loadingMultipartContent = true;

    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object; one example of this is 3266216.
    RefPtr<SubresourceLoader> protect(this);

    if (m_client)
        m_client->didReceiveResponse(this, r);
    
    // The loader can cancel a load if it receives a multipart response for a non-image
    if (reachedTerminalState())
        return;
    ResourceLoader::didReceiveResponse(r);
    
    RefPtr<SharedBuffer> buffer = resourceData();
    if (m_loadingMultipartContent && buffer && buffer->size()) {
        // Since a subresource loader does not load multipart sections progressively,
        // deliver the previously received data to the loader all at once now.
        // Then clear the data to make way for the next multipart section.
        if (m_client)
            m_client->didReceiveData(this, buffer->data(), buffer->size());
        clearResourceData();
        
        // After the first multipart section is complete, signal to delegates that this load is "finished" 
        m_documentLoader->subresourceLoaderFinishedLoadingOnePart(this);
        didFinishLoadingOnePart();
    }
}

void SubresourceLoader::didReceiveData(const char* data, int length, long long lengthReceived, bool allAtOnce)
{
    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object; one example of this is 3266216.
    RefPtr<SubresourceLoader> protect(this);
    
    ResourceLoader::didReceiveData(data, length, lengthReceived, allAtOnce);

    // A subresource loader does not load multipart sections progressively.
    // So don't deliver any data to the loader yet.
    if (!m_loadingMultipartContent && m_client)
        m_client->didReceiveData(this, data, length);
}

void SubresourceLoader::didFinishLoading()
{
    if (cancelled())
        return;
    ASSERT(!reachedTerminalState());

    // Calling removeSubresourceLoader will likely result in a call to deref, so we must protect ourselves.
    RefPtr<SubresourceLoader> protect(this);

    if (m_client)
        m_client->didFinishLoading(this);
    
    m_handle = 0;

    if (cancelled())
        return;
    m_documentLoader->removeSubresourceLoader(this);
    ResourceLoader::didFinishLoading();
}

void SubresourceLoader::didFail(const ResourceError& error)
{
    if (cancelled())
        return;
    ASSERT(!reachedTerminalState());

    // Calling removeSubresourceLoader will likely result in a call to deref, so we must protect ourselves.
    RefPtr<SubresourceLoader> protect(this);

    if (m_client)
        m_client->didFail(this, error);
    
    m_handle = 0;
    
    if (cancelled())
        return;
    m_documentLoader->removeSubresourceLoader(this);
    ResourceLoader::didFail(error);
}

void SubresourceLoader::didCancel(const ResourceError& error)
{
    ASSERT(!reachedTerminalState());

    // Calling removeSubresourceLoader will likely result in a call to deref, so we must protect ourselves.
    RefPtr<SubresourceLoader> protect(this);

    if (m_client)
        m_client->didFail(this, error);
    
    if (cancelled())
        return;
    
    // The only way the subresource loader can reach the terminal state here is if the run loop spins when calling
    // m_client->didFail. This should in theory not happen which is why the assert is here. 
    ASSERT(!reachedTerminalState());
    if (reachedTerminalState())
        return;
    
    m_documentLoader->removeSubresourceLoader(this);
    ResourceLoader::didCancel(error);
}

bool SubresourceLoader::shouldUseCredentialStorage()
{
    RefPtr<SubresourceLoader> protect(this);

    bool shouldUse;
    if (m_client && m_client->getShouldUseCredentialStorage(this, shouldUse))
        return shouldUse;

    return ResourceLoader::shouldUseCredentialStorage();
}

void SubresourceLoader::didReceiveAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    RefPtr<SubresourceLoader> protect(this);

    if (m_client)
        m_client->didReceiveAuthenticationChallenge(this, challenge);
    
    // The SubResourceLoaderClient may have cancelled this ResourceLoader in response to the challenge.  
    // If that's the case, don't call didReceiveAuthenticationChallenge
    if (reachedTerminalState())
        return;
        
    ResourceLoader::didReceiveAuthenticationChallenge(challenge);
}

void SubresourceLoader::receivedCancellation(const AuthenticationChallenge& challenge)
{
    ASSERT(!reachedTerminalState());
        
    RefPtr<SubresourceLoader> protect(this);

    if (m_client)
        m_client->receivedCancellation(this, challenge);
    
    ResourceLoader::receivedCancellation(challenge);
}
    

}
