/*
 * Copyright (C) 2009 Google, Inc.
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
#include "ResourceRequest.h"

namespace WebCore {

// This is used by the loader to control the number of issued parallel load requests. 
unsigned initializeMaximumHTTPConnectionCountPerHost()
{
    // The chromium network stack already handles limiting the number of
    // parallel requests per host, so there's no need to do it here.  Therefore,
    // this is set to a high value that should never be hit in practice.
    return 10000;
}

PassOwnPtr<CrossThreadResourceRequestData> ResourceRequest::doPlatformCopyData(PassOwnPtr<CrossThreadResourceRequestData> data) const
{
    data->m_requestorID = m_requestorID;
    data->m_requestorProcessID = m_requestorProcessID;
    data->m_appCacheHostID = m_appCacheHostID;
    data->m_hasUserGesture = m_hasUserGesture;
    data->m_downloadToFile = m_downloadToFile;
    data->m_targetType = m_targetType;
    return data;
}

void ResourceRequest::doPlatformAdopt(PassOwnPtr<CrossThreadResourceRequestData> data)
{
    m_requestorID = data->m_requestorID;
    m_requestorProcessID = data->m_requestorProcessID;
    m_appCacheHostID = data->m_appCacheHostID;
    m_hasUserGesture = data->m_hasUserGesture;
    m_downloadToFile = data->m_downloadToFile;
    m_targetType = data->m_targetType;
}

} // namespace WebCore
