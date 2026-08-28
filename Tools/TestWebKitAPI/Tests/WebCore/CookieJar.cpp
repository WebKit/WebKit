/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/Cookie.h>
#include <WebCore/CookieJar.h>
#include <wtf/URL.h>
#include <wtf/text/MakeString.h>

namespace TestWebKitAPI {

// Expose the protected static method for testing.
struct CookieJarForTesting : public WebCore::CookieJar {
    using WebCore::CookieJar::shouldIncludeSecureCookies;
};

TEST(CookieJar, ShouldIncludeSecureCookiesForHTTPS)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("https://example.com/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("https://8.8.8.8/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("https://example.com:8443/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldNotIncludeSecureCookiesForPlainHTTP)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://example.com/"_s)), WebCore::IncludeSecureCookies::No);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://8.8.8.8/"_s)), WebCore::IncludeSecureCookies::No);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://192.168.1.1/"_s)), WebCore::IncludeSecureCookies::No);
}

TEST(CookieJar, ShouldNotIncludeSecureCookiesForNonLocalHostnames)
{
    // "localhost" must appear exactly or as the rightmost label.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://notlocalhost/"_s)), WebCore::IncludeSecureCookies::No);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://localhost.example.com/"_s)), WebCore::IncludeSecureCookies::No);
}

TEST(CookieJar, ShouldNotIncludeSecureCookiesForNonLoopbackIPv4)
{
    // Starts with "127." but has non-digit characters.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.example.com/"_s)), WebCore::IncludeSecureCookies::No);
}

#if HAVE(LOCALHOST_TIED_TO_LOOPBACK)
TEST(CookieJar, ShouldIncludeSecureCookiesForIPv6Loopback)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://[::1]/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://[::1]:8080/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForIPv4Loopback)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.0.1/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.0.2/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.1.2.3/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.255.255.255/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.0.1:8080/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForLocalhost)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://localhost/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://localhost:3000/"_s)), WebCore::IncludeSecureCookies::Yes);

    // Case insensitive.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://LOCALHOST/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://LocalHost/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForLocalhostSubdomains)
{
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://foo.localhost/"_s)), WebCore::IncludeSecureCookies::Yes);
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://foo.bar.localhost/"_s)), WebCore::IncludeSecureCookies::Yes);

    // Case insensitive.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://foo.LOCALHOST/"_s)), WebCore::IncludeSecureCookies::Yes);
}

TEST(CookieJar, ShouldIncludeSecureCookiesForNormalizedIPv4Loopback)
{
    // The WHATWG URL parser normalizes abbreviated IPv4 addresses, so "127.0.1"
    // expands to "127.0.0.1" before shouldIncludeSecureCookies sees the host.
    EXPECT_EQ(CookieJarForTesting::shouldIncludeSecureCookies(URL("http://127.0.1/"_s)), WebCore::IncludeSecureCookies::Yes);
}
#endif

using WebCore::CookieUtil::CookieNamePrefix;
using WebCore::CookieUtil::cookieNamePrefix;
using WebCore::CookieUtil::cookieNamePrefixRequirementsViolated;
using WebCore::CookieUtil::parseDOMCookieFields;

static bool hasHttpOnlyAttribute(StringView cookieString)
{
    return parseDOMCookieFields(cookieString).hasHttpOnlyAttribute;
}

static bool hasHttpOnlyRequiringPrefix(StringView cookieString)
{
    auto fields = parseDOMCookieFields(cookieString);
    return fields.prefix == CookieNamePrefix::Http || fields.prefix == CookieNamePrefix::HostHttp;
}

static bool violatesNamePrefix(StringView cookieString)
{
    // Matches CookieJar's DOM path: script cannot set HttpOnly, so httpOnly is false.
    auto fields = parseDOMCookieFields(cookieString);
    return cookieNamePrefixRequirementsViolated(fields.prefix, fields.isSecure, false, fields.hasDomain, fields.hasRootPath);
}

TEST(CookieNamePrefix, ClassifiesCanonicalSpellings)
{
    EXPECT_EQ(cookieNamePrefix("__Secure-a"_s), CookieNamePrefix::Secure);
    EXPECT_EQ(cookieNamePrefix("__Host-a"_s), CookieNamePrefix::Host);
    EXPECT_EQ(cookieNamePrefix("__Http-a"_s), CookieNamePrefix::Http);
    EXPECT_EQ(cookieNamePrefix("__Host-Http-a"_s), CookieNamePrefix::HostHttp);
}

TEST(CookieNamePrefix, MatchesASCIICaseInsensitively)
{
    // RFC 6265bis section 5.4 requires user agents to match the prefix case-insensitively.
    EXPECT_EQ(cookieNamePrefix("__SeCuRe-a"_s), CookieNamePrefix::Secure);
    EXPECT_EQ(cookieNamePrefix("__secure-a"_s), CookieNamePrefix::Secure);
    EXPECT_EQ(cookieNamePrefix("__SECURE-a"_s), CookieNamePrefix::Secure);
    EXPECT_EQ(cookieNamePrefix("__HoSt-a"_s), CookieNamePrefix::Host);
    EXPECT_EQ(cookieNamePrefix("__host-a"_s), CookieNamePrefix::Host);
    EXPECT_EQ(cookieNamePrefix("__HtTp-a"_s), CookieNamePrefix::Http);
    EXPECT_EQ(cookieNamePrefix("__HoSt-HtTp-a"_s), CookieNamePrefix::HostHttp);
    EXPECT_EQ(cookieNamePrefix("__host-http-a"_s), CookieNamePrefix::HostHttp);
}

TEST(CookieNamePrefix, PrefersHostHttpOverItsSubstrings)
{
    // "__Host-Http-" starts with "__Host-", so ordering inside the classifier matters.
    EXPECT_EQ(cookieNamePrefix("__Host-Http-a"_s), CookieNamePrefix::HostHttp);
    EXPECT_NE(cookieNamePrefix("__Host-Http-a"_s), CookieNamePrefix::Host);
    // "__Host-Https-" is not a prefix, but it does start with "__Host-".
    EXPECT_EQ(cookieNamePrefix("__Host-Https-a"_s), CookieNamePrefix::Host);
}

TEST(CookieNamePrefix, RejectsNonPrefixes)
{
    EXPECT_EQ(cookieNamePrefix(""_s), CookieNamePrefix::None);
    EXPECT_EQ(cookieNamePrefix("plain"_s), CookieNamePrefix::None);
    EXPECT_EQ(cookieNamePrefix("__"_s), CookieNamePrefix::None);
    EXPECT_EQ(cookieNamePrefix("__Secure"_s), CookieNamePrefix::None); // No trailing hyphen.
    EXPECT_EQ(cookieNamePrefix("_Secure-a"_s), CookieNamePrefix::None); // Only one underscore.
    EXPECT_EQ(cookieNamePrefix("x__Host-a"_s), CookieNamePrefix::None); // Must be anchored.
    EXPECT_EQ(cookieNamePrefix(" __Host-a"_s), CookieNamePrefix::None); // Caller must trim first.
}

TEST(CookieHttpOnlyAttribute, DetectsTheAttributeInAnyCasing)
{
    EXPECT_TRUE(hasHttpOnlyAttribute("a=b; HttpOnly"_s));
    EXPECT_TRUE(hasHttpOnlyAttribute("a=b; httponly"_s));
    EXPECT_TRUE(hasHttpOnlyAttribute("a=b; HTTPONLY"_s));
    EXPECT_TRUE(hasHttpOnlyAttribute("a=b; HtTpOnLy"_s));

    // Surrounded by other attributes, and with optional whitespace.
    EXPECT_TRUE(hasHttpOnlyAttribute("a=b; Secure; HttpOnly; Path=/"_s));
    EXPECT_TRUE(hasHttpOnlyAttribute("a=b;   HttpOnly   "_s));
    EXPECT_TRUE(hasHttpOnlyAttribute("a=b;;HttpOnly"_s));

    // RFC 6265bis discards a value given to a valueless attribute rather than ignoring the
    // attribute, so the flag still counts as present.
    EXPECT_TRUE(hasHttpOnlyAttribute("a=b; HttpOnly=false"_s));
}

TEST(CookieHttpOnlyAttribute, IgnoresTheNameAndValue)
{
    EXPECT_FALSE(hasHttpOnlyAttribute("a=b"_s));
    EXPECT_FALSE(hasHttpOnlyAttribute("a=b; Secure; Path=/"_s));

    // Only the attribute list can carry the flag, so a name or value that merely contains the
    // word does not count.
    EXPECT_FALSE(hasHttpOnlyAttribute("HttpOnly=b"_s));
    EXPECT_FALSE(hasHttpOnlyAttribute("a=HttpOnly"_s));
    EXPECT_FALSE(hasHttpOnlyAttribute("a=b HttpOnly"_s));

    // A near-miss attribute name is not the flag.
    EXPECT_FALSE(hasHttpOnlyAttribute("a=b; HttpOnlyish"_s));
    EXPECT_FALSE(hasHttpOnlyAttribute("a=b; NotHttpOnly"_s));
}

TEST(DOMCookieNamePrefix, LeavesUnprefixedCookiesAlone)
{
    EXPECT_FALSE(violatesNamePrefix("a=b"_s));
    EXPECT_FALSE(violatesNamePrefix("a=b; Path=/sub; Domain=example.com"_s));
    EXPECT_FALSE(violatesNamePrefix("x__Host-a=b"_s));
}

TEST(DOMCookieNamePrefix, SecureRequiresSecureAttributeInAnyCasing)
{
    for (auto name : { "__Secure-a"_s, "__SeCuRe-a"_s, "__secure-a"_s }) {
        EXPECT_TRUE(violatesNamePrefix(makeString(name, "=b"_s)));
        EXPECT_TRUE(violatesNamePrefix(makeString(name, "=b; Path=/"_s)));
        EXPECT_FALSE(violatesNamePrefix(makeString(name, "=b; Secure"_s)));
        // Unlike "__Host-", "__Secure-" permits Domain and a non-root Path.
        EXPECT_FALSE(violatesNamePrefix(makeString(name, "=b; Secure; Domain=example.com"_s)));
        EXPECT_FALSE(violatesNamePrefix(makeString(name, "=b; Secure; Path=/sub"_s)));
    }
}

TEST(DOMCookieNamePrefix, HostRequiresSecureRootPathAndNoDomainInAnyCasing)
{
    for (auto name : { "__Host-a"_s, "__HoSt-a"_s, "__host-a"_s }) {
        EXPECT_FALSE(violatesNamePrefix(makeString(name, "=b; Secure; Path=/"_s)));
        EXPECT_TRUE(violatesNamePrefix(makeString(name, "=b; Path=/"_s)));
        EXPECT_TRUE(violatesNamePrefix(makeString(name, "=b; Secure; Path=/sub"_s)));
        EXPECT_TRUE(violatesNamePrefix(makeString(name, "=b; Secure; Path=/; Domain=example.com"_s)));
        // Section 5.7 requires the Path attribute to be present, not merely to default to "/".
        EXPECT_TRUE(violatesNamePrefix(makeString(name, "=b; Secure"_s)));
    }
}

TEST(DOMCookieNamePrefix, HttpPrefixesAreNeverSettableFromScript)
{
    // Script cannot set HttpOnly, so these can never satisfy their requirements.
    for (auto name : { "__Http-a"_s, "__HtTp-a"_s, "__Host-Http-a"_s, "__HoSt-HtTp-a"_s }) {
        EXPECT_TRUE(violatesNamePrefix(makeString(name, "=b"_s)));
        EXPECT_TRUE(violatesNamePrefix(makeString(name, "=b; Secure; Path=/"_s)));
        // Even naming HttpOnly explicitly does not help; the attribute is dropped for script.
        EXPECT_TRUE(violatesNamePrefix(makeString(name, "=b; Secure; Path=/; HttpOnly"_s)));
    }
}

TEST(DOMCookieNamePrefix, PairWithNoEqualsIsANameWithAnEmptyValue)
{
    // RFC 6265bis section 5.2 would read a pair with no '=' as a value with an empty name, but the
    // platform cookie stores do not, and this check has to agree with the store that will actually
    // apply the prefix. NetworkStorageSession appends '=' before the store sees the string, so
    // "__Secure-a" arrives as "__Secure-a=", which CFNetwork stores as the name "__Secure-a" with an
    // empty value. The ordinary prefix requirements therefore apply to it as a name.
    EXPECT_TRUE(violatesNamePrefix("__Secure-a"_s));
    EXPECT_FALSE(violatesNamePrefix("__Secure-a; Secure"_s));
    EXPECT_FALSE(violatesNamePrefix("__HoSt-a; Secure; Path=/"_s));
    EXPECT_TRUE(violatesNamePrefix("__HoSt-a; Secure"_s));

    // An explicitly empty name carries no prefix; CFNetwork rejects the cookie outright anyway.
    EXPECT_FALSE(violatesNamePrefix("=__Secure-a; Secure"_s));
    EXPECT_FALSE(violatesNamePrefix("=__Secure-a"_s));

    EXPECT_FALSE(violatesNamePrefix("plainvalue"_s));
}

TEST(DOMCookieNamePrefix, AttributeParsingEdgeCases)
{
    // Attribute names are case-insensitive.
    EXPECT_FALSE(violatesNamePrefix("__Host-a=b; SECURE; PATH=/"_s));
    EXPECT_FALSE(violatesNamePrefix("__Host-a=b; sEcUrE; pAtH=/"_s));

    // Optional whitespace around names, values and attributes is trimmed.
    EXPECT_FALSE(violatesNamePrefix("  __Host-a = b ;  Secure ;  Path = /  "_s));

    // An empty Domain is ignored during parsing, so it does not count as present.
    EXPECT_FALSE(violatesNamePrefix("__Host-a=b; Secure; Path=/; Domain="_s));

    // Path is last-wins, so a trailing "Path=/" rescues an earlier non-root Path.
    EXPECT_FALSE(violatesNamePrefix("__Host-a=b; Secure; Path=/sub; Path=/"_s));
    EXPECT_TRUE(violatesNamePrefix("__Host-a=b; Secure; Path=/; Path=/sub"_s));

    // Domain is sticky by contrast: once a non-empty Domain is seen it cannot be undone.
    EXPECT_TRUE(violatesNamePrefix("__Host-a=b; Secure; Path=/; Domain=example.com; Domain="_s));

    // Unknown attributes are ignored.
    EXPECT_FALSE(violatesNamePrefix("__Host-a=b; Secure; Path=/; MaxAge=10; SameSite=Lax"_s));

    // Trailing and doubled separators are tolerated.
    EXPECT_FALSE(violatesNamePrefix("__Host-a=b; Secure; Path=/;"_s));
    EXPECT_FALSE(violatesNamePrefix("__Host-a=b;; Secure;; Path=/"_s));
}

TEST(DOMCookieNamePrefix, OnlyTabAndSpaceAreTrimmed)
{
    // RFC 6265bis OWS is tab and space only. U+00A0 and U+0085 must NOT be trimmed, or a name like
    // "<NBSP>__Host-a" would be treated as prefixed. See rdar://178479104, which confirmed these
    // bytes are preserved on the wire and the server can distinguish the name.
    EXPECT_FALSE(violatesNamePrefix(makeString(u"\u00A0"_str, "__Host-a=b"_s)));
    EXPECT_FALSE(violatesNamePrefix(makeString(u"\u0085"_str, "__Host-a=b"_s)));
    EXPECT_FALSE(violatesNamePrefix(makeString(u"\u2000"_str, "__Host-a=b"_s)));

    // The same bytes inside the prefix itself must not match either.
    EXPECT_FALSE(violatesNamePrefix(makeString("__Host"_s, u"\u00A0"_str, "-a=b"_s)));

    // Tab and space are trimmed, so these do carry the prefix and fail its requirements.
    EXPECT_TRUE(violatesNamePrefix("\t__Host-a=b"_s));
    EXPECT_TRUE(violatesNamePrefix(" __Host-a=b"_s));
}

TEST(DOMCookieNamePrefix, OptionalWhitespaceIsTrimmedBeforeMatching)
{
    // Only SP and HTAB are optional whitespace, and they are trimmed before the prefix is matched,
    // so leading whitespace does not let a cookie escape enforcement.
    EXPECT_TRUE(violatesNamePrefix(" __Host-a=b; Path=/"_s));
    EXPECT_TRUE(violatesNamePrefix("\t__Secure-a=b"_s));
    EXPECT_FALSE(violatesNamePrefix(" __Host-a=b; Secure; Path=/"_s));

    // Non-ASCII whitespace such as U+00A0 or U+0085 is deliberately *not* trimmed and stays part of
    // the cookie name, so the prefix does not match. That matches CFNetwork and the resolution of
    // rdar://178479104. Not asserted here, to keep this file free of non-ASCII literals.
}

TEST(DOMCookieHttpOnlyRequiringPrefix, MatchesOnlyTheHttpPrefixes)
{
    // These require HttpOnly, which script cannot set, so they can never be set via document.cookie
    // on any port. Enforced unguarded: libsoup does not implement these prefixes either.
    for (auto name : { "__Http-a"_s, "__HtTp-a"_s, "__http-a"_s, "__Host-Http-a"_s, "__HoSt-HtTp-a"_s }) {
        EXPECT_TRUE(hasHttpOnlyRequiringPrefix(makeString(name, "=b"_s)));
        EXPECT_TRUE(hasHttpOnlyRequiringPrefix(makeString(name, "=b; Secure; Path=/; HttpOnly"_s)));
    }

    // "__Secure-" and "__Host-" do not require HttpOnly, so they are not this function's business;
    // their requirements are checked separately.
    for (auto name : { "__Secure-a"_s, "__SeCuRe-a"_s, "__Host-a"_s, "__HoSt-a"_s, "plain"_s }) {
        EXPECT_FALSE(hasHttpOnlyRequiringPrefix(makeString(name, "=b"_s)));
        EXPECT_FALSE(hasHttpOnlyRequiringPrefix(makeString(name, "=b; Secure; Path=/"_s)));
    }

    // Leading optional whitespace is trimmed before matching.
    EXPECT_TRUE(hasHttpOnlyRequiringPrefix(" __Http-a=b"_s));

    // A pair with no '=' is a name with an empty value, so its prefix is matched as a name.
    EXPECT_TRUE(hasHttpOnlyRequiringPrefix("__Http-a"_s));
}

} // namespace TestWebKitAPI
