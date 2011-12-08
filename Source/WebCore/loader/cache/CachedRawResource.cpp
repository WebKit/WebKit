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
#include "SharedBuffer.h"
#include "SubresourceLoader.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

CachedRawResource::CachedRawResource(ResourceRequest& resourceRequest)
    : CachedResource(resourceRequest, RawResource)
    , m_dataLength(0)
{
}

void CachedRawResource::data(PassRefPtr<SharedBuffer> data, bool allDataReceived)
{
    CachedResourceHandle<CachedRawResource> protect(this);
    if (data) {
        // If we are buffering data, then we are saving the buffer in m_data and need to manually
        // calculate the incremental data. If we are not buffering, then m_data will be null and
        // the buffer contains only the incremental data.
        size_t previousDataLength = (m_options.shouldBufferData == BufferData) ? m_dataLength : 0;
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
        m_dataLength = data ? data->size() : 0;
        m_data = data;
    }
    CachedResource::data(m_data, allDataReceived);
}

void CachedRawResource::didAddClient(CachedResourceClient* c)
{
    if (m_data) {
        static_cast<CachedRawResourceClient*>(c)->responseReceived(this, m_response);
        static_cast<CachedRawResourceClient*>(c)->dataReceived(this, m_data->data(), m_data->size());
    }
    CachedResource::didAddClient(c);
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

unsigned long CachedRawResource::identifier() const
{
    return m_loader ? m_loader->identifier() : 0;
}

} // namespace WebCore
