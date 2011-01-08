/*
 * Copyright (C) 2010 Google, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ResourceResponse.h"

namespace WebCore {

PassOwnPtr<CrossThreadResourceResponseData> ResourceResponse::doPlatformCopyData(PassOwnPtr<CrossThreadResourceResponseData> data) const
{
    data->m_appCacheID = m_appCacheID;
    data->m_appCacheManifestURL = m_appCacheManifestURL.copy();
    data->m_isContentFiltered = m_isContentFiltered;
    data->m_isMultipartPayload = m_isMultipartPayload;
    data->m_wasFetchedViaSPDY = m_wasFetchedViaSPDY;
    data->m_wasNpnNegotiated = m_wasNpnNegotiated;
    data->m_wasAlternateProtocolAvailable = m_wasAlternateProtocolAvailable;
    data->m_wasFetchedViaProxy = m_wasFetchedViaProxy;
    data->m_responseTime = m_responseTime;
    return data;
}

void ResourceResponse::doPlatformAdopt(PassOwnPtr<CrossThreadResourceResponseData> data)
{
    m_appCacheID = data->m_appCacheID;
    m_appCacheManifestURL = data->m_appCacheManifestURL.copy();
    m_isContentFiltered = data->m_isContentFiltered;
    m_isMultipartPayload = data->m_isMultipartPayload;
    m_wasFetchedViaSPDY = data->m_wasFetchedViaSPDY;
    m_wasNpnNegotiated = data->m_wasNpnNegotiated;
    m_wasAlternateProtocolAvailable = data->m_wasAlternateProtocolAvailable;
    m_wasFetchedViaProxy = data->m_wasFetchedViaProxy;
    m_responseTime = data->m_responseTime;
}

} // namespace WebCore
