/*
 * Copyright (C) 2009-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
 
#pragma once

#include <wtf/Forward.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RegistrableDomain;

class UserContentURLPattern {
public:
    UserContentURLPattern() = default;

    enum class Error : uint8_t {
        None,
        Invalid,
        MissingScheme,
        MissingHost,
        InvalidHost,
        MissingPath,
    };

    explicit UserContentURLPattern(StringView pattern)
    {
        m_error = parse(pattern);
    }

    WEBCORE_EXPORT UserContentURLPattern(StringView scheme, StringView host, StringView path);

    bool isValid() const { return m_error == Error::None; }
    Error error() const { return m_error; }

    template <typename T>
    bool matches(const T& test) const
    {
        if (!isValid())
            return false;
        return matchesScheme(test) && matchesHost(test) && matchesPath(test);
    }

    const RefPtr<StringImpl>& scheme() const LIFETIME_BOUND { return m_scheme; }
    const RefPtr<StringImpl>& host() const LIFETIME_BOUND { return m_host; }
    const RefPtr<StringImpl>& path() const LIFETIME_BOUND { return m_path; }

    bool matchAllHosts() const { return m_matchSubdomains && isEmpty(m_host); }
    bool matchSubdomains() const { return m_matchSubdomains; }

    // The host with the '*' wildcard, if matchSubdomains is true, otherwise same as host().
    WEBCORE_EXPORT RefPtr<StringImpl> originalHost() const;

    WEBCORE_EXPORT bool matchesScheme(const URL&) const;
    bool matchesHost(const URL& url) const { return matchesHost(url.host().toStringWithoutCopying()); }
    bool matchesPath(const URL& url) const { return matchesPath(url.path().toStringWithoutCopying()); }

    WEBCORE_EXPORT bool matchesScheme(const UserContentURLPattern&) const;
    bool matchesHost(const UserContentURLPattern& other) const { return matchesHost(other.host()); }
    bool matchesPath(const UserContentURLPattern& other) const { return matchesPath(other.path()); }

    NODELETE WEBCORE_EXPORT bool operator==(const UserContentURLPattern& other) const;

    static bool matchesPatterns(const URL&, const Vector<RefPtr<StringImpl>>& allowlist, const Vector<RefPtr<StringImpl>>& blocklist);

    WEBCORE_EXPORT static std::optional<RegistrableDomain> siteMatchedByPatterns(const Vector<UserContentURLPattern>&);

private:
    WEBCORE_EXPORT Error parse(StringView pattern);
    void normalizeHostAndSetMatchSubdomains();

    NODELETE WEBCORE_EXPORT bool matchesHost(StringImpl*) const;
    NODELETE WEBCORE_EXPORT bool matchesPath(StringImpl*) const;

    RefPtr<StringImpl> m_scheme;
    RefPtr<StringImpl> m_host;
    RefPtr<StringImpl> m_path;

    Error m_error { Error::Invalid };
    bool m_matchSubdomains { false };
};

NODELETE WEBCORE_EXPORT bool matchesWildcardPattern(StringImpl* pattern, StringImpl* testString);

} // namespace WebCore
