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
#include "DocumentThreadableLoaderClient.h"
#include "SubresourceLoader.h"
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
class AssociatedURLLoader::ClientAdapter : public DocumentThreadableLoaderClient {
public:
    static PassOwnPtr<ClientAdapter> create(AssociatedURLLoader*, WebURLLoaderClient*, bool /*downloadToFile*/);

    virtual void didSendData(unsigned long long /*bytesSent*/, unsigned long long /*totalBytesToBeSent*/);
    virtual void willSendRequest(ResourceRequest& /*newRequest*/, const ResourceResponse& /*redirectResponse*/);

    virtual void didReceiveResponse(const ResourceResponse&);
    virtual void didReceiveData(const char*, int /*dataLength*/);
    virtual void didReceiveCachedMetadata(const char*, int /*dataLength*/);
    virtual void didFinishLoading(unsigned long /*identifier*/, double /*finishTime*/);
    virtual void didFail(const ResourceError&);

    virtual bool isDocumentThreadableLoaderClient() { return true; }

    // This method stops loading and releases the DocumentThreadableLoader as early as possible.
    void clearClient() { m_client = 0; }

private:
    ClientAdapter(AssociatedURLLoader*, WebURLLoaderClient*, bool /*downloadToFile*/);

    AssociatedURLLoader* m_loader;
    WebURLLoaderClient* m_client;
    unsigned long m_downloadLength;
    bool m_downloadToFile;
};

PassOwnPtr<AssociatedURLLoader::ClientAdapter> AssociatedURLLoader::ClientAdapter::create(AssociatedURLLoader* loader, WebURLLoaderClient* client, bool downloadToFile)
{
    return adoptPtr(new ClientAdapter(loader, client, downloadToFile));
}

AssociatedURLLoader::ClientAdapter::ClientAdapter(AssociatedURLLoader* loader, WebURLLoaderClient* client, bool downloadToFile)
    : m_loader(loader)
    , m_client(client)
    , m_downloadLength(0)
    , m_downloadToFile(downloadToFile)
{
    ASSERT(m_loader);
    ASSERT(m_client);
}

void AssociatedURLLoader::ClientAdapter::willSendRequest(ResourceRequest& newRequest, const ResourceResponse& redirectResponse)
{
    if (!m_client)
        return;

    WrappedResourceRequest wrappedNewRequest(newRequest);
    WrappedResourceResponse wrappedRedirectResponse(redirectResponse);
    m_client->willSendRequest(m_loader, wrappedNewRequest, wrappedRedirectResponse);
}

void AssociatedURLLoader::ClientAdapter::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    if (!m_client)
        return;

    m_client->didSendData(m_loader, bytesSent, totalBytesToBeSent);
}

void AssociatedURLLoader::ClientAdapter::didReceiveResponse(const ResourceResponse& response)
{
    WrappedResourceResponse wrappedResponse(response);
    m_client->didReceiveResponse(m_loader, wrappedResponse);
}

void AssociatedURLLoader::ClientAdapter::didReceiveData(const char* data, int dataLength)
{
    if (!m_client)
        return;

    m_client->didReceiveData(m_loader, data, dataLength, -1);
    m_downloadLength += dataLength;
}

void AssociatedURLLoader::ClientAdapter::didReceiveCachedMetadata(const char* data, int dataLength)
{
    if (!m_client)
        return;

    m_client->didReceiveCachedMetadata(m_loader, data, dataLength);
}

void AssociatedURLLoader::ClientAdapter::didFinishLoading(unsigned long identifier, double finishTime)
{
    if (!m_client)
        return;

    if (m_downloadToFile) {
        int downloadLength = m_downloadLength <= INT_MAX ? m_downloadLength : INT_MAX;
        m_client->didDownloadData(m_loader, downloadLength);
        // While the client could have cancelled, continue, since the load finished. 
    }

    m_client->didFinishLoading(m_loader, finishTime);
}

void AssociatedURLLoader::ClientAdapter::didFail(const ResourceError& error)
{
    if (!m_client)
        return;

    WebURLError webError(error);
    m_client->didFail(m_loader, webError);
}

AssociatedURLLoader::AssociatedURLLoader(PassRefPtr<WebFrameImpl> frameImpl)
    : m_frameImpl(frameImpl)
    , m_client(0)
{
    ASSERT(m_frameImpl);

    m_options.sniffContent = false;
    m_options.allowCredentials = true;
    m_options.forcePreflight = false;
    m_options.crossOriginRequestPolicy = WebURLLoaderOptions::CrossOriginRequestPolicyAllow; // FIXME We should deny by default, but this would break some tests.
}

AssociatedURLLoader::AssociatedURLLoader(PassRefPtr<WebFrameImpl> frameImpl, const WebURLLoaderOptions& options)
    : m_frameImpl(frameImpl)
    , m_options(options)
    , m_client(0)
{
    ASSERT(m_frameImpl);
}

AssociatedURLLoader::~AssociatedURLLoader()
{
    if (m_clientAdapter)
        m_clientAdapter->clearClient();
}

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, webcore_name) \
    COMPILE_ASSERT(static_cast<int>(WebKit::webkit_name) == static_cast<int>(WebCore::webcore_name), mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(WebURLLoaderOptions::CrossOriginRequestPolicyDeny, DenyCrossOriginRequests);
COMPILE_ASSERT_MATCHING_ENUM(WebURLLoaderOptions::CrossOriginRequestPolicyUseAccessControl, UseAccessControl);
COMPILE_ASSERT_MATCHING_ENUM(WebURLLoaderOptions::CrossOriginRequestPolicyAllow, AllowCrossOriginRequests);

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
    options.sendLoadCallbacks = true; // Always send callbacks.
    options.sniffContent = m_options.sniffContent;
    options.allowCredentials = m_options.allowCredentials;
    options.forcePreflight = m_options.forcePreflight;
    options.crossOriginRequestPolicy = static_cast<WebCore::CrossOriginRequestPolicy>(m_options.crossOriginRequestPolicy);
    options.shouldBufferData = false;

    const ResourceRequest& webcoreRequest = request.toResourceRequest();
    Document* webcoreDocument = m_frameImpl->frame()->document();
    m_clientAdapter = ClientAdapter::create(this, m_client, request.downloadToFile());

    m_loader = DocumentThreadableLoader::create(webcoreDocument, m_clientAdapter.get(), webcoreRequest, options);
}

void AssociatedURLLoader::cancel()
{
    if (m_loader) {
        m_clientAdapter->clearClient();
        m_loader->cancel();
    }
}

void AssociatedURLLoader::setDefersLoading(bool defersLoading)
{
    if (m_loader)
        m_loader->setDefersLoading(defersLoading);
}

} // namespace WebKit
