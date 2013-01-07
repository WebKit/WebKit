/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ResourceHandle.h"

#include "NetworkingContext.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include "WrappedResourceRequest.h"
#include "WrappedResourceResponse.h"
#include <public/Platform.h>
#include <public/WebURLError.h>
#include <public/WebURLLoader.h>
#include <public/WebURLLoaderClient.h>
#include <public/WebURLRequest.h>
#include <public/WebURLResponse.h>

using namespace WebKit;

namespace WebCore {

// ResourceHandleInternal -----------------------------------------------------
ResourceHandleInternal::ResourceHandleInternal(const ResourceRequest& request, ResourceHandleClient* client)
    : m_request(request)
    , m_owner(0)
    , m_client(client)
    , m_state(ConnectionStateNew)
{
}

void ResourceHandleInternal::start()
{
    if (m_state != ConnectionStateNew)
        CRASH();
    m_state = ConnectionStateStarted;

    m_loader = adoptPtr(Platform::current()->createURLLoader());
    ASSERT(m_loader);

    WrappedResourceRequest wrappedRequest(m_request);
    wrappedRequest.setAllowStoredCredentials(allowStoredCredentials());
    m_loader->loadAsynchronously(wrappedRequest, this);
}

void ResourceHandleInternal::cancel()
{
    m_state = ConnectionStateCanceled;
    m_loader->cancel();

    // Do not make any further calls to the client.
    m_client = 0;
}

void ResourceHandleInternal::setDefersLoading(bool value)
{
    m_loader->setDefersLoading(value);
}

bool ResourceHandleInternal::allowStoredCredentials() const
{
    return m_client && m_client->shouldUseCredentialStorage(m_owner);
}

void ResourceHandleInternal::willSendRequest(
    WebURLLoader*, WebURLRequest& request, const WebURLResponse& response)
{
    ASSERT(m_client);
    ASSERT(!request.isNull());
    ASSERT(!response.isNull());
    m_client->willSendRequest(m_owner, request.toMutableResourceRequest(), response.toResourceResponse());
}

void ResourceHandleInternal::didSendData(
    WebURLLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    ASSERT(m_client);
    m_client->didSendData(m_owner, bytesSent, totalBytesToBeSent);
}

void ResourceHandleInternal::didReceiveResponse(WebURLLoader*, const WebURLResponse& response)
{
    ASSERT(m_client);
    ASSERT(!response.isNull());
    bool isMultipart = response.isMultipartPayload();
    bool isValidStateTransition = (m_state == ConnectionStateStarted || m_state == ConnectionStateReceivedResponse);
    // In the case of multipart loads, calls to didReceiveData & didReceiveResponse can be interleaved.
    if (!isMultipart && !isValidStateTransition)
        CRASH();
    m_state = ConnectionStateReceivedResponse;
    m_client->didReceiveResponse(m_owner, response.toResourceResponse());
}

void ResourceHandleInternal::didDownloadData(WebURLLoader*, int dataLength)
{
    ASSERT(m_client);
    if (m_state != ConnectionStateReceivedResponse)
        CRASH();

    m_client->didDownloadData(m_owner, dataLength);
}

void ResourceHandleInternal::didReceiveData(WebURLLoader*, const char* data, int dataLength, int encodedDataLength)
{
    ASSERT(m_client);
    if (m_state != ConnectionStateReceivedResponse && m_state != ConnectionStateReceivingData)
        CRASH();
    m_state = ConnectionStateReceivingData;

    m_client->didReceiveData(m_owner, data, dataLength, encodedDataLength);
}

void ResourceHandleInternal::didReceiveCachedMetadata(WebURLLoader*, const char* data, int dataLength)
{
    ASSERT(m_client);
    if (m_state != ConnectionStateReceivedResponse && m_state != ConnectionStateReceivingData)
        CRASH();

    m_client->didReceiveCachedMetadata(m_owner, data, dataLength);
}

void ResourceHandleInternal::didFinishLoading(WebURLLoader*, double finishTime)
{
    ASSERT(m_client);
    if (m_state != ConnectionStateReceivedResponse && m_state != ConnectionStateReceivingData)
        CRASH();
    m_state = ConnectionStateFinishedLoading;
    m_client->didFinishLoading(m_owner, finishTime);
}

void ResourceHandleInternal::didFail(WebURLLoader*, const WebURLError& error)
{
    ASSERT(m_client);
    m_state = ConnectionStateFailed;
    m_client->didFail(m_owner, error);
}

ResourceHandleInternal* ResourceHandleInternal::FromResourceHandle(ResourceHandle* handle)
{
    return handle->d.get();
}

// ResourceHandle -------------------------------------------------------------

ResourceHandle::ResourceHandle(const ResourceRequest& request,
                               ResourceHandleClient* client,
                               bool defersLoading,
                               bool shouldContentSniff)
    : d(adoptPtr(new ResourceHandleInternal(request, client)))
{
    d->setOwner(this);

    // FIXME: Figure out what to do with the bool params.
}

PassRefPtr<ResourceHandle> ResourceHandle::create(NetworkingContext* context,
                                                  const ResourceRequest& request,
                                                  ResourceHandleClient* client,
                                                  bool defersLoading,
                                                  bool shouldContentSniff)
{
    RefPtr<ResourceHandle> newHandle = adoptRef(new ResourceHandle(
        request, client, defersLoading, shouldContentSniff));

    if (newHandle->start(context))
        return newHandle.release();

    return 0;
}

ResourceRequest& ResourceHandle::firstRequest()
{
    return d->request();
}

ResourceHandleClient* ResourceHandle::client() const
{
    return d->client();
}

void ResourceHandle::setClient(ResourceHandleClient* client)
{
    d->setClient(client);
}

void ResourceHandle::setDefersLoading(bool value)
{
    d->setDefersLoading(value);
}

bool ResourceHandle::start(NetworkingContext* context)
{
    if (!context)
        return false;

    d->start();
    return true;
}

bool ResourceHandle::hasAuthenticationChallenge() const
{
    return false;
}

void ResourceHandle::clearAuthentication()
{
}

void ResourceHandle::cancel()
{
    d->cancel();
}

ResourceHandle::~ResourceHandle()
{
    d->setOwner(0);
}

bool ResourceHandle::loadsBlocked()
{
    return false; // This seems to be related to sync XMLHttpRequest...
}

// static
void ResourceHandle::loadResourceSynchronously(NetworkingContext* context,
                                               const ResourceRequest& request,
                                               StoredCredentials storedCredentials,
                                               ResourceError& error,
                                               ResourceResponse& response,
                                               Vector<char>& data)
{
    OwnPtr<WebURLLoader> loader = adoptPtr(Platform::current()->createURLLoader());
    ASSERT(loader);

    WrappedResourceRequest requestIn(request);
    requestIn.setAllowStoredCredentials(storedCredentials == AllowStoredCredentials);
    WrappedResourceResponse responseOut(response);
    WebURLError errorOut;
    WebData dataOut;

    loader->loadSynchronously(requestIn, responseOut, errorOut, dataOut);

    error = errorOut;
    data.clear();
    data.append(dataOut.data(), dataOut.size());
}

// static
void ResourceHandle::cacheMetadata(const ResourceResponse& response, const Vector<char>& data)
{
    WebKit::Platform::current()->cacheMetadata(response.url(), response.responseTime(), data.data(), data.size());
}

} // namespace WebCore
