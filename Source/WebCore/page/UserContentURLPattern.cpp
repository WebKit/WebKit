/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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

bool UserContentURLPattern::parse(const String& pattern)
{
    static NeverDestroyed<const String> schemeSeparator(MAKE_STATIC_STRING_IMPL("://"));

    size_t schemeEndPos = pattern.find(schemeSeparator);
    if (schemeEndPos == notFound)
        return false;

    m_scheme = pattern.left(schemeEndPos);

    unsigned hostStartPos = schemeEndPos + schemeSeparator.get().length();
    if (hostStartPos >= pattern.length())
        return false;

    int pathStartPos = 0;

    if (equalLettersIgnoringASCIICase(m_scheme, "file"))
        pathStartPos = hostStartPos;
    else {
        size_t hostEndPos = pattern.find('/', hostStartPos);
        if (hostEndPos == notFound)
            return false;

        m_host = pattern.substring(hostStartPos, hostEndPos - hostStartPos);
        m_matchSubdomains = false;

        if (m_host == "*") {
            // The pattern can be just '*', which means match all domains.
            m_host = emptyString();
            m_matchSubdomains = true;
        } else if (m_host.startsWith("*.")) {
            // The first component can be '*', which means to match all subdomains.
            m_host = m_host.substring(2); // Length of "*."
            m_matchSubdomains = true;
        }

        // No other '*' can occur in the host.
        if (m_host.find('*') != notFound)
            return false;

        pathStartPos = hostEndPos;
    }

    m_path = pattern.right(pattern.length() - pathStartPos);

    return true;
}

bool UserContentURLPattern::matches(const URL& test) const
{
    if (m_invalid)
        return false;

    if (m_scheme != "*" && !equalIgnoringASCIICase(test.protocol(), m_scheme))
        return false;

    if (!equalLettersIgnoringASCIICase(m_scheme, "file") && !matchesHost(test))
        return false;

    return matchesPath(test);
}

bool UserContentURLPattern::matchesHost(const URL& test) const
{
    auto host = test.host();
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

bool UserContentURLPattern::matchesPath(const URL& test) const
{
    return MatchTester(m_path, test.path()).test();
}

} // namespace WebCore
