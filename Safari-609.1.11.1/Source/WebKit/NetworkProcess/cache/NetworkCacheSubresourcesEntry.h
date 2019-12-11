/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)

#include "NetworkCacheStorage.h"
#include <WebCore/ResourceRequest.h>
#include <wtf/HashMap.h>
#include <wtf/URL.h>

namespace WebKit {
namespace NetworkCache {

class SubresourceInfo {
    WTF_MAKE_FAST_ALLOCATED;
public:
    void encode(WTF::Persistence::Encoder&) const;
    static bool decode(WTF::Persistence::Decoder&, SubresourceInfo&);

    SubresourceInfo() = default;
    SubresourceInfo(const Key&, const WebCore::ResourceRequest&, const SubresourceInfo* previousInfo);

    const Key& key() const { return m_key; }
    WallTime lastSeen() const { return m_lastSeen; }
    WallTime firstSeen() const { return m_firstSeen; }

    bool isTransient() const { return m_isTransient; }
    const URL& firstPartyForCookies() const { ASSERT(!m_isTransient); return m_firstPartyForCookies; }
    const WebCore::HTTPHeaderMap& requestHeaders() const { ASSERT(!m_isTransient); return m_requestHeaders; }
    WebCore::ResourceLoadPriority priority() const { ASSERT(!m_isTransient); return m_priority; }

    bool isSameSite() const { ASSERT(!m_isTransient); return m_isSameSite; }
    bool isTopSite() const { return false; }

    void setNonTransient() { m_isTransient = false; }

    bool isFirstParty() const;

private:
    Key m_key;
    WallTime m_lastSeen;
    WallTime m_firstSeen;
    bool m_isTransient { false };
    bool m_isSameSite { false };
    URL m_firstPartyForCookies;
    WebCore::HTTPHeaderMap m_requestHeaders;
    WebCore::ResourceLoadPriority m_priority;
};

struct SubresourceLoad {
    WTF_MAKE_NONCOPYABLE(SubresourceLoad); WTF_MAKE_FAST_ALLOCATED;
public:
    SubresourceLoad(const WebCore::ResourceRequest& request, const Key& key)
        : request(request)
        , key(key)
    { }

    WebCore::ResourceRequest request;
    Key key;
};

class SubresourcesEntry {
    WTF_MAKE_NONCOPYABLE(SubresourcesEntry); WTF_MAKE_FAST_ALLOCATED;
public:
    SubresourcesEntry(Key&&, const Vector<std::unique_ptr<SubresourceLoad>>&);
    explicit SubresourcesEntry(const Storage::Record&);

    Storage::Record encodeAsStorageRecord() const;
    static std::unique_ptr<SubresourcesEntry> decodeStorageRecord(const Storage::Record&);

    const Key& key() const { return m_key; }
    WallTime timeStamp() const { return m_timeStamp; }
    const Vector<SubresourceInfo>& subresources() const { return m_subresources; }

    void updateSubresourceLoads(const Vector<std::unique_ptr<SubresourceLoad>>&);

private:
    Key m_key;
    WallTime m_timeStamp;
    Vector<SubresourceInfo> m_subresources;
};

} // namespace WebKit
} // namespace NetworkCache

#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
