/*
 * Copyright (C) 2024 Apple, Inc. All rights reserved.
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
#include "CacheStorageRecord.h"

namespace WebKit {

CacheStorageRecordInformation::CacheStorageRecordInformation(NetworkCache::Key&& key, double insertionTime, uint64_t identifier, uint64_t updateResponseCounter, uint64_t size, URL&& url, bool hasVaryStar, HashMap<String, String>&& varyHeaders)
    : m_key(WTFMove(key))
    , m_insertionTime(insertionTime)
    , m_identifier(identifier)
    , m_updateResponseCounter(updateResponseCounter)
    , m_size(size)
    , m_url(WTFMove(url))
    , m_hasVaryStar(hasVaryStar)
    , m_varyHeaders(WTFMove(varyHeaders))
{
    RELEASE_ASSERT(!m_url.string().impl()->isAtom());
}

void CacheStorageRecordInformation::updateVaryHeaders(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse::CrossThreadData& response)
{
    auto varyValue = response.httpHeaderFields.get(WebCore::HTTPHeaderName::Vary);
    if (varyValue.isNull() || response.tainting == WebCore::ResourceResponse::Tainting::Opaque || response.tainting == WebCore::ResourceResponse::Tainting::Opaqueredirect) {
        m_hasVaryStar = false;
        m_varyHeaders = { };
        return;
    }

    varyValue.split(',', [&](StringView view) {
        if (!m_hasVaryStar && view.trim(isASCIIWhitespaceWithoutFF<UChar>) == "*"_s)
            m_hasVaryStar = true;
        m_varyHeaders.add(view.toString(), request.httpHeaderField(view));
    });

    if (m_hasVaryStar)
        m_varyHeaders = { };
}

CacheStorageRecordInformation CacheStorageRecordInformation::isolatedCopy() && {
    return {
        crossThreadCopy(WTFMove(m_key)),
        m_insertionTime,
        m_identifier,
        m_updateResponseCounter,
        m_size,
        crossThreadCopy(WTFMove(m_url)),
        m_hasVaryStar,
        crossThreadCopy(WTFMove(m_varyHeaders))
    };
}

void CacheStorageRecordInformation::setURL(URL&& url)
{
    RELEASE_ASSERT(!url.string().impl()->isAtom());
    m_url = WTFMove(url);
}

} // namespace WebKit
