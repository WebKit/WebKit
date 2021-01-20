/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "CachedTextTrack.h"

#include "CachedResourceClient.h"
#include "CachedResourceClientWalker.h"
#include "CachedResourceLoader.h"
#include "SharedBuffer.h"
#include "TextResourceDecoder.h"

namespace WebCore {

CachedTextTrack::CachedTextTrack(CachedResourceRequest&& request, const PAL::SessionID& sessionID, const CookieJar* cookieJar)
    : CachedResource(WTFMove(request), Type::TextTrackResource, sessionID, cookieJar)
{
}

void CachedTextTrack::doUpdateBuffer(SharedBuffer* data)
{
    ASSERT(dataBufferingPolicy() == DataBufferingPolicy::BufferData);
    m_data = data;
    setEncodedSize(data ? data->size() : 0);

    CachedResourceClientWalker<CachedResourceClient> walker(m_clients);
    while (CachedResourceClient* client = walker.next())
        client->deprecatedDidReceiveCachedResource(*this);
}

void CachedTextTrack::updateBuffer(SharedBuffer& data)
{
    doUpdateBuffer(&data);
    CachedResource::updateBuffer(data);
}

void CachedTextTrack::finishLoading(SharedBuffer* data, const NetworkLoadMetrics& metrics)
{
    doUpdateBuffer(data);
    CachedResource::finishLoading(data, metrics);
}

}
