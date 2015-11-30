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

#ifndef NetworkCacheSubresourcesEntry_h
#define NetworkCacheSubresourcesEntry_h

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)

#include "NetworkCacheDecoder.h"
#include "NetworkCacheEncoder.h"
#include "NetworkCacheStorage.h"
#include <wtf/HashMap.h>

namespace WebKit {
namespace NetworkCache {

class SubresourcesEntry {
    WTF_MAKE_NONCOPYABLE(SubresourcesEntry); WTF_MAKE_FAST_ALLOCATED;
public:
    struct SubresourceInfo {
        void encode(Encoder& encoder) const { encoder << isTransient; }
        static bool decode(Decoder& decoder, SubresourceInfo& info) { return decoder.decode(info.isTransient); }

        SubresourceInfo() = default;
        SubresourceInfo(bool isTransient) : isTransient(isTransient) { }

        bool isTransient { false };
    };
    SubresourcesEntry(Key&&, const Vector<Key>& subresourceKeys);
    explicit SubresourcesEntry(const Storage::Record&);

    Storage::Record encodeAsStorageRecord() const;
    static std::unique_ptr<SubresourcesEntry> decodeStorageRecord(const Storage::Record&);

    const Key& key() const { return m_key; }
    std::chrono::system_clock::time_point timeStamp() const { return m_timeStamp; }
    const HashMap<Key, SubresourceInfo>& subresources() const { return m_subresources; }

    void updateSubresourceKeys(const Vector<Key>&);

private:
    Key m_key;
    std::chrono::system_clock::time_point m_timeStamp;
    HashMap<Key, SubresourceInfo> m_subresources;
};

} // namespace WebKit
} // namespace NetworkCache

#endif // ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
#endif // NetworkCacheSubresourcesEntry_h
