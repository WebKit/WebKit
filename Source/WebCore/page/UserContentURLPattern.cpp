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

#include "config.h"
#include "UserContentURLPattern.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>

namespace WebCore {

UserContentURLPattern::UserContentURLPattern(StringView scheme, StringView host, StringView path)
    : m_scheme(scheme.toString())
{
    if (m_scheme.isEmpty()) {
        m_error = Error::MissingScheme;
        return;
    }

    m_host = host.toString();
    if (m_host.isEmpty()) {
        m_error = Error::MissingHost;
        return;
    }

    normalizeHostAndSetMatchSubdomains();

    // No other '*' can occur in the host after it is normalized.
    if (m_host.find('*') != notFound) {
        m_error = Error::InvalidHost;
        return;
    }

    m_path = path.toString();
    if (!m_path.startsWith("/"_s)) {
        m_error = Error::MissingPath;
        return;
    }

    m_error = Error::None;
}

bool UserContentURLPattern::operator==(const UserContentURLPattern& other) const
{
    return this == &other || (m_error == other.m_error && m_matchSubdomains == other.m_matchSubdomains
        && m_scheme == other.m_scheme && m_host == other.m_host && m_path == other.m_path);
}

bool UserContentURLPattern::matchesPatterns(const URL& url, const Vector<String>& allowlist, const Vector<String>& blocklist)
{
    // In order for a URL to be a match it has to be present in the allowlist and not present in the blocklist.
    // If there is no allowlist at all, then all URLs are assumed to be in the allowlist.
    bool matchesAllowlist = allowlist.isEmpty();
    if (!matchesAllowlist) {
        for (auto& entry : allowlist) {
            UserContentURLPattern contentPattern(entry);
            if (contentPattern.matches(url)) {
                matchesAllowlist = true;
                break;
            }
        }
    }

    bool matchesBlocklist = false;
    if (!blocklist.isEmpty()) {
        for (auto& entry : blocklist) {
            UserContentURLPattern contentPattern(entry);
            if (contentPattern.matches(url)) {
                matchesBlocklist = true;
                break;
            }
        }
    }

    return matchesAllowlist && !matchesBlocklist;
}

void UserContentURLPattern::normalizeHostAndSetMatchSubdomains()
{
    ASSERT(!m_matchSubdomains);

    if (m_host == "*"_s) {
        // The pattern can be just '*', which means match all domains.
        m_host = emptyString();
        m_matchSubdomains = true;
    } else if (m_host.startsWith("*."_s)) {
        // The first component can be '*', which means to match all subdomains.
        m_host = m_host.substring(2); // Length of "*."
        m_matchSubdomains = true;
    } else if (equalLettersIgnoringASCIICase(m_scheme, "file"_s) && equalLettersIgnoringASCIICase(m_host, "localhost"_s)) {
        // A localhost for the file scheme is also empty string. This matches what URLParser does for URL.
        m_host = emptyString();
    }
}

UserContentURLPattern::Error UserContentURLPattern::parse(StringView pattern)
{
    static constexpr ASCIILiteral schemeSeparator = "://"_s;

    size_t schemeEndPos = pattern.find(schemeSeparator);
    if (schemeEndPos == notFound)
        return Error::MissingScheme;

    m_scheme = pattern.left(schemeEndPos).toString();

    bool isFileScheme = equalLettersIgnoringASCIICase(m_scheme, "file"_s);
    size_t hostStartPos = schemeEndPos + schemeSeparator.length();
    if (!isFileScheme && hostStartPos >= pattern.length())
        return Error::MissingHost;

    size_t pathStartPos = pattern.find('/', hostStartPos);
    if (pathStartPos == notFound)
        return Error::MissingPath;

    m_host = pattern.substring(hostStartPos, pathStartPos - hostStartPos).toString();
    m_matchSubdomains = false;

    normalizeHostAndSetMatchSubdomains();

    // No other '*' can occur in the host after it is normalized.
    if (m_host.find('*') != notFound)
        return Error::InvalidHost;

    m_path = pattern.right(pattern.length() - pathStartPos).toString();

    return Error::None;
}

String UserContentURLPattern::originalHost() const
{
    if (matchAllHosts())
        return "*"_s;

    if (m_matchSubdomains)
        return makeString("*."_s, m_host);

    return m_host;
}

bool UserContentURLPattern::matchesScheme(const URL& url) const
{
    ASSERT(isValid());

    if (m_scheme == "*"_s)
        return url.protocolIsInHTTPFamily();

    return url.protocolIs(m_scheme);
}

bool UserContentURLPattern::matchesScheme(const UserContentURLPattern& other) const
{
    ASSERT(isValid());

    if (m_scheme == "*"_s)
        return other.scheme() == "*"_s || equalIgnoringASCIICase(other.scheme(), "https"_s) || equalIgnoringASCIICase(other.scheme(), "http"_s);

    return equalIgnoringASCIICase(other.scheme(), m_scheme);
}

bool UserContentURLPattern::matchesHost(const String& host) const
{
    ASSERT(isValid());

    if (equalIgnoringASCIICase(host, m_host))
        return true;

    if (!m_matchSubdomains)
        return false;

    // If we're matching subdomains, and we have no host, that means the pattern
    // was <scheme>://*/<whatever>, so we match anything.
    if (!m_host.length())
        return true;

    // Check if the domain is a subdomain of our host.
    if (!host.endsWithIgnoringASCIICase(m_host))
        return false;

    ASSERT(host.length() > m_host.length());

    // Check that the character before the suffix is a period.
    return host[host.length() - m_host.length() - 1] == '.';
}

struct MatchTester {
    StringView m_pattern;
    unsigned m_patternIndex { 0 };

    StringView m_test;
    unsigned m_testIndex { 0 };

    MatchTester(StringView pattern, StringView test)
        : m_pattern(pattern)
        , m_test(test)
    {
    }

    bool testStringFinished() const { return m_testIndex >= m_test.length(); }
    bool patternStringFinished() const { return m_patternIndex >= m_pattern.length(); }

    void eatWildcard()
    {
        while (!patternStringFinished()) {
            if (m_pattern[m_patternIndex] != '*')
                return;
            m_patternIndex++;
        }
    }

    void eatSameChars()
    {
        while (!patternStringFinished() && !testStringFinished()) {
            if (m_pattern[m_patternIndex] == '*')
                return;
            if (m_pattern[m_patternIndex] != m_test[m_testIndex])
                return;
            m_patternIndex++;
            m_testIndex++;
        }
    }

    bool test()
    {
        // Eat all the matching chars.
        eatSameChars();

        // If the string is finished, then the pattern must be empty too, or contains
        // only wildcards.
        if (testStringFinished()) {
            eatWildcard();
            if (patternStringFinished())
                return true;
            return false;
        }

        // Pattern is empty but not string, this is not a match.
        if (patternStringFinished())
            return false;

        // If we don't encounter a *, then we're hosed.
        if (m_pattern[m_patternIndex] != '*')
            return false;

        while (!testStringFinished()) {
            MatchTester nextMatch(*this);
            nextMatch.m_patternIndex++;
            if (nextMatch.test())
                return true;
            m_testIndex++;
        }

        // We reached the end of the string.  Let's see if the pattern contains only
        // wildcards.
        eatWildcard();
        return patternStringFinished();
    }
};

bool UserContentURLPattern::matchesPath(const String& path) const
{
    ASSERT(isValid());

    return MatchTester(m_path, path).test();
}

} // namespace WebCore
