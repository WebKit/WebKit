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
#include "HTTPValidation.h"
#include "SubresourceLoader.h"
#include "Timer.h"
#include "WebApplicationCacheHost.h"
#include "WebDataSource.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WrappedResourceRequest.h"
#include "WrappedResourceResponse.h"
#include "XMLHttpRequest.h"
#include "platform/WebHTTPHeaderVisitor.h"
#include "platform/WebKitPlatformSupport.h"
#include "platform/WebURLError.h"
#include "platform/WebURLLoaderClient.h"
#include "platform/WebURLRequest.h"

using namespace WebCore;
using namespace WTF;

namespace WebKit {

namespace {

class SafeHTTPHeaderValidator : public WebHTTPHeaderVisitor {
    WTF_MAKE_NONCOPYABLE(SafeHTTPHeaderValidator);
public:
    SafeHTTPHeaderValidator() : m_isSafe(true) { }

    void visitHeader(const WebString& name, const WebString& value);
    bool isSafe() const { return m_isSafe; }

private:
    bool m_isSafe;
};

void SafeHTTPHeaderValidator::visitHeader(const WebString& name, const WebString& value)
{
    m_isSafe = m_isSafe && isValidHTTPToken(name) && XMLHttpRequest::isAllowedHTTPHeader(name) && isValidHTTPHeaderValue(value);
}

}

// This class bridges the interface differences between WebCore and WebKit loader clients.
// It forwards its ThreadableLoaderClient notifications to a WebURLLoaderClient.
class AssociatedURLLoader::ClientAdapter : public DocumentThreadableLoaderClient {
    WTF_MAKE_NONCOPYABLE(ClientAdapter);
public:
    static PassOwnPtr<ClientAdapter> create(AssociatedURLLoader*, WebURLLoaderClient*, bool /*downloadToFile*/);

    virtual void didSendData(unsigned long long /*bytesSent*/, unsigned long long /*totalBytesToBeSent*/);
    virtual void willSendRequest(ResourceRequest& /*newRequest*/, const ResourceResponse& /*redirectResponse*/);

    virtual void didReceiveResponse(unsigned long, const ResourceResponse&);
    virtual void didDownloadData(int /*dataLength*/);
    virtual void didReceiveData(const char*, int /*dataLength*/);
    virtual void didReceiveCachedMetadata(const char*, int /*dataLength*/);
    virtual void didFinishLoading(unsigned long /*identifier*/, double /*finishTime*/);
    virtual void didFail(const ResourceError&);

    virtual bool isDocumentThreadableLoaderClient() { return true; }

    // Sets an error to be reported back to the client, asychronously.
    void setDelayedError(const ResourceError&);

    // Enables forwarding of error notifications to the WebURLLoaderClient. These must be
    // deferred until after the call to AssociatedURLLoader::loadAsynchronously() completes.
    void enableErrorNotifications();

    // Stops loading and releases the DocumentThreadableLoader as early as possible.
    void clearClient() { m_client = 0; } 

private:
    ClientAdapter(AssociatedURLLoader*, WebURLLoaderClient*, bool /*downloadToFile*/);

    void notifyError(Timer<ClientAdapter>*);

    AssociatedURLLoader* m_loader;
    WebURLLoaderClient* m_client;
    WebURLError m_error;

    Timer<ClientAdapter> m_errorTimer;
    bool m_downloadToFile;
    bool m_enableErrorNotifications;
    bool m_didFail;
};

PassOwnPtr<AssociatedURLLoader::ClientAdapter> AssociatedURLLoader::ClientAdapter::create(AssociatedURLLoader* loader, WebURLLoaderClient* client, bool downloadToFile)
{
    return adoptPtr(new ClientAdapter(loader, client, downloadToFile));
}

AssociatedURLLoader::ClientAdapter::ClientAdapter(AssociatedURLLoader* loader, WebURLLoaderClient* client, bool downloadToFile)
    : m_loader(loader)
    , m_client(client)
    , m_errorTimer(this, &ClientAdapter::notifyError)
    , m_downloadToFile(downloadToFile)
    , m_enableErrorNotifications(false)
    , m_didFail(false)
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

void AssociatedURLLoader::ClientAdapter::didReceiveResponse(unsigned long, const ResourceResponse& response)
{
    WrappedResourceResponse wrappedResponse(response);
    m_client->didReceiveResponse(m_loader, wrappedResponse);
}

void AssociatedURLLoader::ClientAdapter::didDownloadData(int dataLength)
{
    if (!m_client)
        return;

    m_client->didDownloadData(m_loader, dataLength);
}

void AssociatedURLLoader::ClientAdapter::didReceiveData(const char* data, int dataLength)
{
    if (!m_client)
        return;

    m_client->didReceiveData(m_loader, data, dataLength, -1);
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

    m_client->didFinishLoading(m_loader, finishTime);
}

void AssociatedURLLoader::ClientAdapter::didFail(const ResourceError& error)
{
    if (!m_client)
        return;

    m_didFail = true;
    m_error = WebURLError(error);
    if (m_enableErrorNotifications)
        notifyError(&m_errorTimer);
}

void AssociatedURLLoader::ClientAdapter::setDelayedError(const ResourceError& error)
{
    didFail(error);
}

void AssociatedURLLoader::ClientAdapter::enableErrorNotifications()
{
    m_enableErrorNotifications = true;
    // If an error has already been received, start a timer to report it to the client
    // after AssociatedURLLoader::loadAsynchronously has returned to the caller.
    if (m_didFail)
        m_errorTimer.startOneShot(0);
}

void AssociatedURLLoader::ClientAdapter::notifyError(Timer<ClientAdapter>* timer)
{
    ASSERT_UNUSED(timer, timer == &m_errorTimer);

    m_client->didFail(m_loader, m_error);
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
    cancel();
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

    bool allowLoad = true;
    WebURLRequest newRequest(request);
    if (m_options.untrustedHTTP) {
        WebString method = newRequest.httpMethod();
        allowLoad = isValidHTTPToken(method) && XMLHttpRequest::isAllowedHTTPMethod(method);
        if (allowLoad) {
            newRequest.setHTTPMethod(XMLHttpRequest::uppercaseKnownHTTPMethod(method));
            SafeHTTPHeaderValidator validator;
            newRequest.visitHTTPHeaderFields(&validator);
            allowLoad = validator.isSafe();
        }
    }

    m_clientAdapter = ClientAdapter::create(this, m_client, request.downloadToFile());

    if (allowLoad) {
        ThreadableLoaderOptions options;
        options.sendLoadCallbacks = SendCallbacks; // Always send callbacks.
        options.sniffContent = m_options.sniffContent ? SniffContent : DoNotSniffContent;
        options.allowCredentials = m_options.allowCredentials ? AllowStoredCredentials : DoNotAllowStoredCredentials;
        options.preflightPolicy = m_options.forcePreflight ? ForcePreflight : ConsiderPreflight;
        options.crossOriginRequestPolicy = static_cast<WebCore::CrossOriginRequestPolicy>(m_options.crossOriginRequestPolicy);
        options.shouldBufferData = DoNotBufferData;

        const ResourceRequest& webcoreRequest = newRequest.toResourceRequest();
        Document* webcoreDocument = m_frameImpl->frame()->document();
        m_loader = DocumentThreadableLoader::create(webcoreDocument, m_clientAdapter.get(), webcoreRequest, options);
    } else {
        // FIXME: return meaningful error codes.
        m_clientAdapter->setDelayedError(ResourceError());
    }
    m_clientAdapter->enableErrorNotifications();
}

void AssociatedURLLoader::cancel()
{
    if (m_clientAdapter)
        m_clientAdapter->clearClient();
    if (m_loader)
        m_loader->cancel();
}

void AssociatedURLLoader::setDefersLoading(bool defersLoading)
{
    if (m_loader)
        m_loader->setDefersLoading(defersLoading);
}

} // namespace WebKit
