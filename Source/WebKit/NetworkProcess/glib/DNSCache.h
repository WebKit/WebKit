/*
 * Copyright (C) 2019 Igalia S.L.
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

#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RunLoop.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

typedef struct _GInetAddress GInetAddress;

namespace WebKit {

class DNSCache {
public:
    DNSCache();
    ~DNSCache() = default;

    enum class Type { Default, IPv4Only, IPv6Only };
    std::optional<Vector<GRefPtr<GInetAddress>>> lookup(const CString& host, Type = Type::Default);
    void update(const CString& host, Vector<GRefPtr<GInetAddress>>&&, Type = Type::Default);
    void clear();

private:
    struct CachedResponse {
        Vector<GRefPtr<GInetAddress>> addressList;
        MonotonicTime expirationTime;
    };

    using DNSCacheMap = HashMap<CString, CachedResponse>;

    DNSCacheMap& mapForType(Type) WTF_REQUIRES_LOCK(m_lock);
    void removeExpiredResponsesFired();
    void removeExpiredResponsesInMap(DNSCacheMap&);
    void pruneResponsesInMap(DNSCacheMap&);

    Lock m_lock;
    DNSCacheMap m_dnsMap WTF_GUARDED_BY_LOCK(m_lock);
#if GLIB_CHECK_VERSION(2, 59, 0)
    DNSCacheMap m_ipv4Map;
    DNSCacheMap m_ipv6Map;
#endif
    RunLoop::Timer m_expiredTimer;
};

} // namespace WebKit
