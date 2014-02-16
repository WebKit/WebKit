/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#if ENABLE(NETWORK_PROCESS)

#include "DataReference.h"
#include "Logging.h"
#include "NetworkProcessConnection.h"
#include "NetworkResourceLoaderMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebErrors.h"
#include "WebProcess.h"
#include <WebCore/CertificateInfo.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/ResourceBuffer.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceLoader.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebResourceLoader> WebResourceLoader::create(PassRefPtr<ResourceLoader> coreLoader)
{
    return adoptRef(new WebResourceLoader(coreLoader));
}

WebResourceLoader::WebResourceLoader(PassRefPtr<WebCore::ResourceLoader> coreLoader)
    : m_coreLoader(coreLoader)
{
}

WebResourceLoader::~WebResourceLoader()
{
}

IPC::Connection* WebResourceLoader::messageSenderConnection()
{
    return WebProcess::shared().networkConnection()->connection();
}

uint64_t WebResourceLoader::messageSenderDestinationID()
{
    return m_coreLoader->identifier();
}

void WebResourceLoader::cancelResourceLoader()
{
    m_coreLoader->cancel();
}

void WebResourceLoader::detachFromCoreLoader()
{
    m_coreLoader = 0;
}

void WebResourceLoader::willSendRequest(const ResourceRequest& proposedRequest, const ResourceResponse& redirectResponse)
{
    LOG(Network, "(WebProcess) WebResourceLoader::willSendRequest to '%s'", proposedRequest.url().string().utf8().data());

    Ref<WebResourceLoader> protect(*this);
    
    ResourceRequest newRequest = proposedRequest;
    m_coreLoader->willSendRequest(newRequest, redirectResponse);
    
    if (!m_coreLoader)
        return;
    
    send(Messages::NetworkResourceLoader::ContinueWillSendRequest(newRequest));
}

void WebResourceLoader::didSendData(uint64_t bytesSent, uint64_t totalBytesToBeSent)
{
    m_coreLoader->didSendData(bytesSent, totalBytesToBeSent);
}

void WebResourceLoader::didReceiveResponseWithCertificateInfo(const ResourceResponse& response, const CertificateInfo& certificateInfo, bool needsContinueDidReceiveResponseMessage)
{
    LOG(Network, "(WebProcess) WebResourceLoader::didReceiveResponseWithCertificateInfo for '%s'. Status %d.", m_coreLoader->url().string().utf8().data(), response.httpStatusCode());

    Ref<WebResourceLoader> protect(*this);

    ResourceResponse responseCopy(response);
    // FIXME: This should use CertificateInfo to avoid the platform ifdefs. See https://bugs.webkit.org/show_bug.cgi?id=124724.
#if PLATFORM(COCOA)
    responseCopy.setCertificateChain(certificateInfo.certificateChain());
#elif USE(SOUP)
    responseCopy.setSoupMessageCertificate(certificateInfo.certificate());
    responseCopy.setSoupMessageTLSErrors(certificateInfo.tlsErrors());
#endif
    m_coreLoader->didReceiveResponse(responseCopy);

    // If m_coreLoader becomes null as a result of the didReceiveResponse callback, we can't use the send function(). 
    if (!m_coreLoader)
        return;

    if (needsContinueDidReceiveResponseMessage)
        send(Messages::NetworkResourceLoader::ContinueDidReceiveResponse());
}

void WebResourceLoader::didReceiveData(const IPC::DataReference& data, int64_t encodedDataLength)
{
    LOG(Network, "(WebProcess) WebResourceLoader::didReceiveData of size %i for '%s'", (int)data.size(), m_coreLoader->url().string().utf8().data());
    m_coreLoader->didReceiveData(reinterpret_cast<const char*>(data.data()), data.size(), encodedDataLength, DataPayloadBytes);
}

void WebResourceLoader::didFinishResourceLoad(double finishTime)
{
    LOG(Network, "(WebProcess) WebResourceLoader::didFinishResourceLoad for '%s'", m_coreLoader->url().string().utf8().data());
    m_coreLoader->didFinishLoading(finishTime);
}

void WebResourceLoader::didFailResourceLoad(const ResourceError& error)
{
    LOG(Network, "(WebProcess) WebResourceLoader::didFailResourceLoad for '%s'", m_coreLoader->url().string().utf8().data());
    
    m_coreLoader->didFail(error);
}

#if ENABLE(SHAREABLE_RESOURCE)
void WebResourceLoader::didReceiveResource(const ShareableResource::Handle& handle, double finishTime)
{
    LOG(Network, "(WebProcess) WebResourceLoader::didReceiveResource for '%s'", m_coreLoader->url().string().utf8().data());

    RefPtr<SharedBuffer> buffer = handle.tryWrapInSharedBuffer();
    if (!buffer) {
        LOG_ERROR("Unable to create buffer from ShareableResource sent from the network process.");
        m_coreLoader->didFail(internalError(m_coreLoader->request().url()));
        return;
    }

    Ref<WebResourceLoader> protect(*this);

    // Only send data to the didReceiveData callback if it exists.
    if (buffer->size())
        m_coreLoader->didReceiveBuffer(buffer.get(), buffer->size(), DataPayloadWholeResource);

    if (!m_coreLoader)
        return;

    m_coreLoader->didFinishLoading(finishTime);
}
#endif

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void WebResourceLoader::canAuthenticateAgainstProtectionSpace(const ProtectionSpace& protectionSpace)
{
    Ref<WebResourceLoader> protect(*this);

    bool result = m_coreLoader->canAuthenticateAgainstProtectionSpace(protectionSpace);

    if (!m_coreLoader)
        return;

    send(Messages::NetworkResourceLoader::ContinueCanAuthenticateAgainstProtectionSpace(result));
}
#endif

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
