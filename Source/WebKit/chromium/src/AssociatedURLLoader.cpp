/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "AssociatedURLLoader.h"

#include "WebApplicationCacheHost.h"
#include "WebDataSource.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebURLRequest.h"

namespace WebKit {

AssociatedURLLoader::AssociatedURLLoader(PassRefPtr<WebFrameImpl> frameImpl)
    : m_frameImpl(frameImpl),
      m_realLoader(webKitClient()->createURLLoader()),
      m_realClient(0)
{
}

AssociatedURLLoader::~AssociatedURLLoader()
{
}

void AssociatedURLLoader::loadSynchronously(const WebURLRequest& request, WebURLResponse& response, WebURLError& error, WebData& data)
{
    ASSERT(!m_realClient);

    WebURLRequest requestCopy(request);
    prepareRequest(requestCopy);

    m_realLoader->loadSynchronously(requestCopy, response, error, data);
}

void AssociatedURLLoader::loadAsynchronously(const WebURLRequest& request, WebURLLoaderClient* client)
{
    ASSERT(!m_realClient);

    WebURLRequest requestCopy(request);
    prepareRequest(requestCopy);

    m_realClient = client;
    m_realLoader->loadAsynchronously(requestCopy, this);
}

void AssociatedURLLoader::cancel()
{
    m_realLoader->cancel();
}

void AssociatedURLLoader::setDefersLoading(bool defersLoading)
{
    m_realLoader->setDefersLoading(defersLoading);
}

void AssociatedURLLoader::prepareRequest(WebURLRequest& request)
{
    WebApplicationCacheHost* applicationCacheHost = m_frameImpl->dataSource()->applicationCacheHost();
    if (applicationCacheHost)
        applicationCacheHost->willStartSubResourceRequest(request);
    m_frameImpl->dispatchWillSendRequest(request);
}

void AssociatedURLLoader::willSendRequest(WebURLLoader*, WebURLRequest& newRequest, const WebURLResponse& redirectResponse)
{
    m_realClient->willSendRequest(this, newRequest, redirectResponse);
}

void AssociatedURLLoader::didSendData(WebURLLoader*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    m_realClient->didSendData(this, bytesSent, totalBytesToBeSent);
}

void AssociatedURLLoader::didReceiveResponse(WebURLLoader*, const WebURLResponse& response)
{
    m_realClient->didReceiveResponse(this, response);
}

void AssociatedURLLoader::didDownloadData(WebURLLoader*, int dataLength)
{
    m_realClient->didDownloadData(this, dataLength);
}

void AssociatedURLLoader::didReceiveData(WebURLLoader*, const char* data, int dataLength)
{
    m_realClient->didReceiveData(this, data, dataLength);
}

void AssociatedURLLoader::didReceiveCachedMetadata(WebURLLoader*, const char* data, int dataLength)
{
    m_realClient->didReceiveCachedMetadata(this, data, dataLength);
}

void AssociatedURLLoader::didFinishLoading(WebURLLoader*, double finishTime)
{
    m_realClient->didFinishLoading(this, finishTime);
}

void AssociatedURLLoader::didFail(WebURLLoader*, const WebURLError& error)
{
    m_realClient->didFail(this, error);
}

} // namespace WebKit
