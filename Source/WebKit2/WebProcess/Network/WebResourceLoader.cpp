/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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
#include "WebResourceLoader.h"

#include "DataReference.h"
#include "Logging.h"
#include "NetworkProcessConnection.h"
#include "NetworkResourceLoaderMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebProcess.h"
#include <WebCore/ApplicationCacheHost.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/DiagnosticLoggingClient.h>
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/Page.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoader.h>
#include <WebCore/SubresourceLoader.h>

using namespace WebCore;

#define RELEASE_LOG_IF_ALLOWED(fmt, ...) RELEASE_LOG_IF(isAlwaysOnLoggingAllowed(), Network, "%p - WebResourceLoader::" fmt, this, ##__VA_ARGS__)

namespace WebKit {

Ref<WebResourceLoader> WebResourceLoader::create(Ref<ResourceLoader>&& coreLoader, const TrackingParameters& trackingParameters)
{
    return adoptRef(*new WebResourceLoader(WTFMove(coreLoader), trackingParameters));
}

WebResourceLoader::WebResourceLoader(Ref<WebCore::ResourceLoader>&& coreLoader, const TrackingParameters& trackingParameters)
    : m_coreLoader(WTFMove(coreLoader))
    , m_trackingParameters(trackingParameters)
{
}

WebResourceLoader::~WebResourceLoader()
{
}

IPC::Connection* WebResourceLoader::messageSenderConnection()
{
    return &WebProcess::singleton().networkConnection().connection();
}

uint64_t WebResourceLoader::messageSenderDestinationID()
{
    return m_coreLoader->identifier();
}

void WebResourceLoader::detachFromCoreLoader()
{
    m_coreLoader = nullptr;
}

void WebResourceLoader::willSendRequest(ResourceRequest&& proposedRequest, ResourceResponse&& redirectResponse)
{
    LOG(Network, "(WebProcess) WebResourceLoader::willSendRequest to '%s'", proposedRequest.url().string().latin1().data());
    RELEASE_LOG_IF_ALLOWED("willSendRequest: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_trackingParameters.pageID, m_trackingParameters.frameID, m_trackingParameters.resourceID);

    RefPtr<WebResourceLoader> protectedThis(this);

    if (m_coreLoader->documentLoader()->applicationCacheHost().maybeLoadFallbackForRedirect(m_coreLoader.get(), proposedRequest, redirectResponse))
        return;

    m_coreLoader->willSendRequest(WTFMove(proposedRequest), redirectResponse, [protectedThis](ResourceRequest&& request) {
        if (!protectedThis->m_coreLoader)
            return;

        protectedThis->send(Messages::NetworkResourceLoader::ContinueWillSendRequest(request));
    });
}

void WebResourceLoader::didSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    m_coreLoader->didSendData(bytesSent, totalBytesToBeSent);
}

void WebResourceLoader::didReceiveResponse(const ResourceResponse& response, bool needsContinueDidReceiveResponseMessage)
{
    LOG(Network, "(WebProcess) WebResourceLoader::didReceiveResponse for '%s'. Status %d.", m_coreLoader->url().string().latin1().data(), response.httpStatusCode());
    RELEASE_LOG_IF_ALLOWED("didReceiveResponse: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ", status = %d)", m_trackingParameters.pageID, m_trackingParameters.frameID, m_trackingParameters.resourceID, response.httpStatusCode());

    Ref<WebResourceLoader> protect(*this);

    if (m_coreLoader->documentLoader()->applicationCacheHost().maybeLoadFallbackForResponse(m_coreLoader.get(), response))
        return;

    m_coreLoader->didReceiveResponse(response);

    // If m_coreLoader becomes null as a result of the didReceiveResponse callback, we can't use the send function(). 
    if (!m_coreLoader)
        return;

    if (needsContinueDidReceiveResponseMessage)
        send(Messages::NetworkResourceLoader::ContinueDidReceiveResponse());
}

void WebResourceLoader::didReceiveData(const IPC::DataReference& data, int64_t encodedDataLength)
{
    LOG(Network, "(WebProcess) WebResourceLoader::didReceiveData of size %lu for '%s'", data.size(), m_coreLoader->url().string().latin1().data());

    if (!m_hasReceivedData) {
        RELEASE_LOG_IF_ALLOWED("didReceiveData: Started receiving data (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_trackingParameters.pageID, m_trackingParameters.frameID, m_trackingParameters.resourceID);
        m_hasReceivedData = true;
    }

    m_coreLoader->didReceiveData(reinterpret_cast<const char*>(data.data()), data.size(), encodedDataLength, DataPayloadBytes);
}

void WebResourceLoader::didRetrieveDerivedData(const String& type, const IPC::DataReference& data)
{
    LOG(Network, "(WebProcess) WebResourceLoader::didRetrieveDerivedData of size %lu for '%s'", data.size(), m_coreLoader->url().string().latin1().data());

    auto buffer = SharedBuffer::create(data.data(), data.size());
    m_coreLoader->didRetrieveDerivedDataFromCache(type, buffer.get());
}

void WebResourceLoader::didFinishResourceLoad(double finishTime)
{
    LOG(Network, "(WebProcess) WebResourceLoader::didFinishResourceLoad for '%s'", m_coreLoader->url().string().latin1().data());
    RELEASE_LOG_IF_ALLOWED("didFinishResourceLoad: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_trackingParameters.pageID, m_trackingParameters.frameID, m_trackingParameters.resourceID);

    m_coreLoader->didFinishLoading(finishTime);
}

void WebResourceLoader::didFailResourceLoad(const ResourceError& error)
{
    LOG(Network, "(WebProcess) WebResourceLoader::didFailResourceLoad for '%s'", m_coreLoader->url().string().latin1().data());
    RELEASE_LOG_IF_ALLOWED("didFailResourceLoad: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_trackingParameters.pageID, m_trackingParameters.frameID, m_trackingParameters.resourceID);

    if (m_coreLoader->documentLoader()->applicationCacheHost().maybeLoadFallbackForError(m_coreLoader.get(), error))
        return;
    m_coreLoader->didFail(error);
}

#if ENABLE(SHAREABLE_RESOURCE)
void WebResourceLoader::didReceiveResource(const ShareableResource::Handle& handle, double finishTime)
{
    LOG(Network, "(WebProcess) WebResourceLoader::didReceiveResource for '%s'", m_coreLoader->url().string().latin1().data());
    RELEASE_LOG_IF_ALLOWED("didReceiveResource: (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_trackingParameters.pageID, m_trackingParameters.frameID, m_trackingParameters.resourceID);

    RefPtr<SharedBuffer> buffer = handle.tryWrapInSharedBuffer();

    if (!buffer) {
        LOG_ERROR("Unable to create buffer from ShareableResource sent from the network process.");
        RELEASE_LOG_IF_ALLOWED("didReceiveResource: Unable to create SharedBuffer (pageID = %" PRIu64 ", frameID = %" PRIu64 ", resourceID = %" PRIu64 ")", m_trackingParameters.pageID, m_trackingParameters.frameID, m_trackingParameters.resourceID);
        if (auto* frame = m_coreLoader->frame()) {
            if (auto* page = frame->page())
                page->diagnosticLoggingClient().logDiagnosticMessage(WebCore::DiagnosticLoggingKeys::internalErrorKey(), WebCore::DiagnosticLoggingKeys::createSharedBufferFailedKey(), WebCore::ShouldSample::No);
        }
        m_coreLoader->didFail(internalError(m_coreLoader->request().url()));
        return;
    }

    Ref<WebResourceLoader> protect(*this);

    // Only send data to the didReceiveData callback if it exists.
    if (unsigned bufferSize = buffer->size())
        m_coreLoader->didReceiveBuffer(buffer.releaseNonNull(), bufferSize, DataPayloadWholeResource);

    if (!m_coreLoader)
        return;

    m_coreLoader->didFinishLoading(finishTime);
}
#endif

bool WebResourceLoader::isAlwaysOnLoggingAllowed() const
{
    return resourceLoader() && resourceLoader()->isAlwaysOnLoggingAllowed();
}

} // namespace WebKit
