/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class PublicSuffixStore {
public:
    WEBCORE_EXPORT static PublicSuffixStore& singleton();

    // https://url.spec.whatwg.org/#host-public-suffix
    WEBCORE_EXPORT bool isPublicSuffix(StringView domain) const;
    WEBCORE_EXPORT String publicSuffix(const URL&) const;
    WEBCORE_EXPORT String topPrivatelyControlledDomain(const String& host) const;
    WEBCORE_EXPORT void clearHostTopPrivatelyControlledDomainCache();

#if PLATFORM(COCOA)
    enum class CanAcceptCustomPublicSuffix : bool { No, Yes };
    WEBCORE_EXPORT void enablePublicSuffixCache(CanAcceptCustomPublicSuffix = CanAcceptCustomPublicSuffix::No);
    WEBCORE_EXPORT void addPublicSuffix(const String& publicSuffix);
#endif

private:
    friend LazyNeverDestroyed<PublicSuffixStore>;
    PublicSuffixStore() = default;

    bool platformIsPublicSuffix(StringView domain) const;
    String platformTopPrivatelyControlledDomain(const String& host) const;

    mutable Lock m_HostTopPrivatelyControlledDomainCacheLock;
    mutable HashMap<String, String, ASCIICaseInsensitiveHash> m_hostTopPrivatelyControlledDomainCache WTF_GUARDED_BY_LOCK(m_HostTopPrivatelyControlledDomainCacheLock);
#if PLATFORM(COCOA)
    mutable Lock m_publicSuffixCacheLock;
    std::optional<HashSet<String, ASCIICaseInsensitiveHash>> m_publicSuffixCache WTF_GUARDED_BY_LOCK(m_publicSuffixCacheLock);
    bool m_canAcceptCustomPublicSuffix WTF_GUARDED_BY_LOCK(m_publicSuffixCacheLock) { false };
#endif
};

} // namespace WebCore
