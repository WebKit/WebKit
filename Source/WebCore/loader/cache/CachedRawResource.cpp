/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CachedRawResource.h"

#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "CachedResourceLoader.h"
#include "MemoryInstrumentation.h"
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

CachedRawResource::CachedRawResource(ResourceRequest& resourceRequest)
    : CachedResource(resourceRequest, RawResource)
    , m_identifier(0)
{
}

void CachedRawResource::data(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    CachedResourceHandle<CachedRawResource> protect(this);
    if (data) {
        // If we are buffering data, then we are saving the buffer in m_data and need to manually
        // calculate the incremental data. If we are not buffering, then m_data will be null and
        // the buffer contains only the incremental data.
        size_t previousDataLength = (m_options.shouldBufferData == BufferData) ? encodedSize() : 0;
        ASSERT(data->size() >= previousDataLength);
        const char* incrementalData = data->data() + previousDataLength;
        size_t incrementalDataLength = data->size() - previousDataLength;

        if (incrementalDataLength) {
            CachedResourceClientWalker<CachedRawResourceClient> w(m_clients);
            while (CachedRawResourceClient* c = w.next())
                c->dataReceived(this, incrementalData, incrementalDataLength);
        }
    }
    
    if (m_options.shouldBufferData == BufferData) {
        if (data)
            setEncodedSize(data->size());
        m_data = data;
    }
    CachedResource::data(m_data, allDataReceived);
}

void CachedRawResource::didAddClient(CachedResourceClient* c)
{
    if (m_response.isNull() || !hasClient(c))
        return;
    // The calls to the client can result in events running, potentially causing
    // this resource to be evicted from the cache and all clients to be removed,
    // so a protector is necessary.
    CachedResourceHandle<CachedRawResource> protect(this);
    CachedRawResourceClient* client = static_cast<CachedRawResourceClient*>(c);
    client->responseReceived(this, m_response);
    if (!hasClient(c))
        return;
    if (m_data)
        client->dataReceived(this, m_data->data(), m_data->size());
    if (!hasClient(c))
       return;
    CachedResource::didAddClient(client);
}

void CachedRawResource::allClientsRemoved()
{
    if (m_loader)
        m_loader->cancelIfNotFinishing();
}

void CachedRawResource::willSendRequest(ResourceRequest& request, const ResourceResponse& response)
{
    if (!response.isNull()) {
        CachedResourceClientWalker<CachedRawResourceClient> w(m_clients);
        while (CachedRawResourceClient* c = w.next())
            c->redirectReceived(this, request, response);
    }
    CachedResource::willSendRequest(request, response);
}

void CachedRawResource::setResponse(const ResourceResponse& response)
{
    if (!m_identifier)
        m_identifier = m_loader->identifier();
    CachedResource::setResponse(response);
    CachedResourceClientWalker<CachedRawResourceClient> w(m_clients);
    while (CachedRawResourceClient* c = w.next())
        c->responseReceived(this, m_response);
}

void CachedRawResource::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    CachedResourceClientWalker<CachedRawResourceClient> w(m_clients);
    while (CachedRawResourceClient* c = w.next())
        c->dataSent(this, bytesSent, totalBytesToBeSent);
}

void CachedRawResource::setDefersLoading(bool defers)
{
    if (m_loader)
        m_loader->setDefersLoading(defers);
}

bool CachedRawResource::canReuse(const ResourceRequest& newRequest) const
{
    if (m_options.shouldBufferData == DoNotBufferData)
        return false;

    if (m_resourceRequest.httpMethod() != newRequest.httpMethod())
        return false;

    if (m_resourceRequest.httpBody() != newRequest.httpBody())
        return false;

    if (m_resourceRequest.allowCookies() != newRequest.allowCookies())
        return false;

    // Ensure all headers match the existing headers before continuing.
    // Note that only headers set by our client will be present in either
    // ResourceRequest, since SubresourceLoader creates a separate copy
    // for its purposes.
    // FIXME: There might be some headers that shouldn't block reuse.
    const HTTPHeaderMap& newHeaders = newRequest.httpHeaderFields();
    const HTTPHeaderMap& oldHeaders = m_resourceRequest.httpHeaderFields();
    if (newHeaders.size() != oldHeaders.size())
        return false;

    HTTPHeaderMap::const_iterator end = newHeaders.end();
    for (HTTPHeaderMap::const_iterator i = newHeaders.begin(); i != end; ++i) {
        AtomicString headerName = i->first;
        if (i->second != oldHeaders.get(headerName))
            return false;
    }
    return true;
}

void CachedRawResource::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CachedResource);
    CachedResource::reportMemoryUsage(memoryObjectInfo);
}


} // namespace WebCore
