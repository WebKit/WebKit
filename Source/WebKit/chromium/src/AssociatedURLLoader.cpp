/*
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
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

#include "DocumentThreadableLoader.h"
#include "SubresourceLoader.h"
#include "ThreadableLoaderClient.h"
#include "WebApplicationCacheHost.h"
#include "WebDataSource.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WebKitClient.h"
#include "WebURLError.h"
#include "WebURLLoaderClient.h"
#include "WebURLRequest.h"
#include "WrappedResourceRequest.h"
#include "WrappedResourceResponse.h"

using namespace WebCore;
using namespace WebKit;
using namespace WTF;

namespace WebKit {

// This class bridges the interface differences between WebCore and WebKit loader clients.
// It forwards its ThreadableLoaderClient notifications to a WebURLLoaderClient.
class AssociatedURLLoader::ClientAdapter : public ThreadableLoaderClient {
public:
    static PassOwnPtr<ClientAdapter> create(WebURLLoader*, WebURLLoaderClient*, bool /*downloadToFile*/);

    virtual void didSendData(unsigned long long /*bytesSent*/, unsigned long long /*totalBytesToBeSent*/);
    virtual void willSendRequest(ResourceRequest& /*newRequest*/, const ResourceResponse& /*redirectResponse*/);

    virtual void didReceiveResponse(const ResourceResponse&);
    virtual void didReceiveData(const char*, int /*dataLength*/);
    virtual void didReceiveCachedMetadata(const char*, int /*dataLength*/);
    virtual void didFinishLoading(unsigned long /*identifier*/, double /*finishTime*/);
    virtual void didFail(const ResourceError&);

private:
    ClientAdapter(WebURLLoader*, WebURLLoaderClient*, bool /*downloadingToFile*/);

    WebURLLoader* m_loader;
    WebURLLoaderClient* m_client;
    unsigned long m_downloadLength;
    bool m_downloadingToFile;
};

PassOwnPtr<AssociatedURLLoader::ClientAdapter> AssociatedURLLoader::ClientAdapter::create(WebURLLoader* loader, WebURLLoaderClient* client, bool downloadToFile)
{
    return adoptPtr(new ClientAdapter(loader, client, downloadToFile));
}

AssociatedURLLoader::ClientAdapter::ClientAdapter(WebURLLoader* loader, WebURLLoaderClient* client, bool downloadingToFile)
    : m_loader(loader)
    , m_client(client)
    , m_downloadLength(0)
    , m_downloadingToFile(downloadingToFile)
{
    ASSERT(m_loader);
    ASSERT(m_client);
}

void AssociatedURLLoader::ClientAdapter::willSendRequest(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    WrappedResourceRequest wrappedNewRequest(newRequest);
    WrappedResourceResponse wrappedRedirectResponse(redirectResponse);
    m_client->willSendRequest(m_loader, wrappedNewRequest, wrappedRedirectResponse);
}

void AssociatedURLLoader::ClientAdapter::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    m_client->didSendData(m_loader, bytesSent, totalBytesToBeSent);
}

void AssociatedURLLoader::ClientAdapter::didReceiveResponse(const ResourceResponse& response)
{
    WrappedResourceResponse wrappedResponse(response);
    m_client->didReceiveResponse(m_loader, wrappedResponse);
}

void AssociatedURLLoader::ClientAdapter::didReceiveData(const char* data, int lengthReceived)
{
    m_client->didReceiveData(m_loader, data, lengthReceived);
    m_downloadLength += lengthReceived;
}

void AssociatedURLLoader::ClientAdapter::didReceiveCachedMetadata(const char* data, int lengthReceived)
{
    m_client->didReceiveCachedMetadata(m_loader, data, lengthReceived);
}

void AssociatedURLLoader::ClientAdapter::didFinishLoading(unsigned long identifier, double finishTime)
{
    if (m_downloadingToFile) {
        int downloadLength = m_downloadLength <= INT_MAX ? m_downloadLength : INT_MAX;
        m_client->didDownloadData(m_loader, downloadLength);
    }

    m_client->didFinishLoading(m_loader, finishTime);
}

void AssociatedURLLoader::ClientAdapter::didFail(const ResourceError& error)
{
    WebURLError webError(error);
    m_client->didFail(m_loader, webError);
}

AssociatedURLLoader::AssociatedURLLoader(PassRefPtr<WebFrameImpl> frameImpl)
    : m_frameImpl(frameImpl)
    , m_client(0)
{
    ASSERT(m_frameImpl);
}

AssociatedURLLoader::AssociatedURLLoader(PassRefPtr<WebFrameImpl> frameImpl, const AssociatedURLLoaderOptions& options)
    : m_frameImpl(frameImpl)
    , m_options(options)
    , m_client(0)
{
    ASSERT(m_frameImpl);
}

AssociatedURLLoader::~AssociatedURLLoader()
{
}

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, webcore_name) \
    COMPILE_ASSERT(static_cast<int>(WebKit::webkit_name) == static_cast<int>(WebCore::webcore_name), mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(DenyCrossOriginRequests, DenyCrossOriginRequests);
COMPILE_ASSERT_MATCHING_ENUM(UseAccessControl, UseAccessControl);
COMPILE_ASSERT_MATCHING_ENUM(AllowCrossOriginRequests, AllowCrossOriginRequests);

void AssociatedURLLoader::loadSynchronously(const WebURLRequest& request, WebURLResponse& response, WebURLError& error, WebData& data)
{
    ASSERT(0); // Synchronous loading is not supported.
}

void AssociatedURLLoader::loadAsynchronously(const WebURLRequest& request, WebURLLoaderClient* client)
{
    ASSERT(!m_client);

    m_client = client;
    ASSERT(m_client);

    ThreadableLoaderOptions options;
    options.sendLoadCallbacks = m_options.sendLoadCallbacks;
    options.sniffContent = m_options.sniffContent;
    options.allowCredentials = m_options.allowCredentials;
    options.forcePreflight = m_options.forcePreflight;
    options.crossOriginRequestPolicy = static_cast<WebCore::CrossOriginRequestPolicy>(options.crossOriginRequestPolicy);

    WebURLRequest requestCopy(request);
    const ResourceRequest& webcoreRequest = request.toResourceRequest();
    Document* webcoreDocument = m_frameImpl->frame()->document();
    m_clientAdapter = ClientAdapter::create(this, m_client, request.downloadToFile());

    m_loader = DocumentThreadableLoader::create(webcoreDocument, m_clientAdapter.get(), webcoreRequest, options);
}

void AssociatedURLLoader::cancel()
{
    if (m_loader) {
        m_loader->cancel();
        m_loader = 0;
    }
    m_client = 0;
}

void AssociatedURLLoader::setDefersLoading(bool defersLoading)
{
    if (m_loader)
        m_loader->setDefersLoading(defersLoading);
}

} // namespace WebKit
