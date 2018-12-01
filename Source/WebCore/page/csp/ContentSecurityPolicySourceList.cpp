/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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
#include "ContentSecurityPolicySourceList.h"

#include "ContentSecurityPolicy.h"
#include "ContentSecurityPolicyDirectiveNames.h"
#include "ParsingUtilities.h"
#include "TextEncoding.h"
#include <wtf/ASCIICType.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <wtf/text/Base64.h>

namespace WebCore {

static bool isCSPDirectiveName(const String& name)
{
    return equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::baseURI)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::connectSrc)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::defaultSrc)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::fontSrc)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::formAction)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::frameSrc)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::imgSrc)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::mediaSrc)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::objectSrc)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::pluginTypes)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::reportURI)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::sandbox)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::scriptSrc)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::styleSrc);
}

static bool isSourceCharacter(UChar c)
{
    return !isASCIISpace(c);
}

static bool isHostCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '-';
}

static bool isPathComponentCharacter(UChar c)
{
    return c != '?' && c != '#';
}

static bool isSchemeContinuationCharacter(UChar c)
{
    return isASCIIAlphanumeric(c) || c == '+' || c == '-' || c == '.';
}

static bool isNotColonOrSlash(UChar c)
{
    return c != ':' && c != '/';
}

static bool isSourceListNone(const String& value)
{
    auto characters = StringView(value).upconvertedCharacters();
    const UChar* begin = characters;
    const UChar* end = characters + value.length();
    skipWhile<UChar, isASCIISpace>(begin, end);

    const UChar* position = begin;
    skipWhile<UChar, isSourceCharacter>(position, end);
    if (!equalLettersIgnoringASCIICase(begin, position - begin, "'none'"))
        return false;

    skipWhile<UChar, isASCIISpace>(position, end);
    if (position != end)
        return false;
    
    return true;
}

ContentSecurityPolicySourceList::ContentSecurityPolicySourceList(const ContentSecurityPolicy& policy, const String& directiveName)
    : m_policy(policy)
    , m_directiveName(directiveName)
{
}

void ContentSecurityPolicySourceList::parse(const String& value)
{
    if (isSourceListNone(value)) {
        m_isNone = true;
        return;
    }
    auto characters = StringView(value).upconvertedCharacters();
    parse(characters, characters + value.length());
}

bool ContentSecurityPolicySourceList::isProtocolAllowedByStar(const URL& url) const
{
    if (m_policy.allowContentSecurityPolicySourceStarToMatchAnyProtocol())
        return true;

    // Although not allowed by the Content Security Policy Level 3 spec., we allow a data URL to match
    // "img-src *" and either a data URL or blob URL to match "media-src *" for web compatibility.
    bool isAllowed = url.protocolIsInHTTPFamily() || url.protocolIs("ws") || url.protocolIs("wss") || m_policy.protocolMatchesSelf(url);
    if (equalIgnoringASCIICase(m_directiveName, ContentSecurityPolicyDirectiveNames::imgSrc))
        isAllowed |= url.protocolIsData();
    else if (equalIgnoringASCIICase(m_directiveName, ContentSecurityPolicyDirectiveNames::mediaSrc))
        isAllowed |= url.protocolIsData() || url.protocolIsBlob();
    return isAllowed;
}

bool ContentSecurityPolicySourceList::matches(const URL& url, bool didReceiveRedirectResponse) const
{
    if (m_allowStar && isProtocolAllowedByStar(url))
        return true;

    if (m_allowSelf && m_policy.urlMatchesSelf(url))
        return true;

    for (auto& entry : m_list) {
        if (entry.matches(url, didReceiveRedirectResponse))
            return true;
    }

    return false;
}

bool ContentSecurityPolicySourceList::matches(const ContentSecurityPolicyHash& hash) const
{
    return m_hashes.contains(hash);
}

bool ContentSecurityPolicySourceList::matches(const String& nonce) const
{
    return m_nonces.contains(nonce);
}

// source-list       = *WSP [ source *( 1*WSP source ) *WSP ]
//                   / *WSP "'none'" *WSP
//
void ContentSecurityPolicySourceList::parse(const UChar* begin, const UChar* end)
{
    const UChar* position = begin;

    while (position < end) {
        skipWhile<UChar, isASCIISpace>(position, end);
        if (position == end)
            return;

        const UChar* beginSource = position;
        skipWhile<UChar, isSourceCharacter>(position, end);

        String scheme, host, path;
        std::optional<uint16_t> port;
        bool hostHasWildcard = false;
        bool portHasWildcard = false;

        if (parseNonceSource(beginSource, position))
            continue;

        if (parseHashSource(beginSource, position))
            continue;

        if (parseSource(beginSource, position, scheme, host, port, path, hostHasWildcard, portHasWildcard)) {
            // Wildcard hosts and keyword sources ('self', 'unsafe-inline',
            // etc.) aren't stored in m_list, but as attributes on the source
            // list itself.
            if (scheme.isEmpty() && host.isEmpty())
                continue;
            if (isCSPDirectiveName(host))
                m_policy.reportDirectiveAsSourceExpression(m_directiveName, host);
            m_list.append(ContentSecurityPolicySource(m_policy, scheme, host, port, path, hostHasWildcard, portHasWildcard));
        } else
            m_policy.reportInvalidSourceExpression(m_directiveName, String(beginSource, position - beginSource));

        ASSERT(position == end || isASCIISpace(*position));
    }
    
    m_list.shrinkToFit();
}

// source            = scheme ":"
//                   / ( [ scheme "://" ] host [ port ] [ path ] )
//                   / "'self'"
//
bool ContentSecurityPolicySourceList::parseSource(const UChar* begin, const UChar* end, String& scheme, String& host, std::optional<uint16_t>& port, String& path, bool& hostHasWildcard, bool& portHasWildcard)
{
    if (begin == end)
        return false;

    if (equalLettersIgnoringASCIICase(begin, end - begin, "'none'"))
        return false;

    if (end - begin == 1 && *begin == '*') {
        m_allowStar = true;
        return true;
    }

    if (equalLettersIgnoringASCIICase(begin, end - begin, "'self'")) {
        m_allowSelf = true;
        return true;
    }

    if (equalLettersIgnoringASCIICase(begin, end - begin, "'unsafe-inline'")) {
        m_allowInline = true;
        return true;
    }

    if (equalLettersIgnoringASCIICase(begin, end - begin, "'unsafe-eval'")) {
        m_allowEval = true;
        return true;
    }

    const UChar* position = begin;
    const UChar* beginHost = begin;
    const UChar* beginPath = end;
    const UChar* beginPort = nullptr;

    skipWhile<UChar, isNotColonOrSlash>(position, end);

    if (position == end) {
        // host
        //     ^
        return parseHost(beginHost, position, host, hostHasWildcard);
    }

    if (position < end && *position == '/') {
        // host/path || host/ || /
        //     ^            ^    ^
        return parseHost(beginHost, position, host, hostHasWildcard) && parsePath(position, end, path);
    }

    if (position < end && *position == ':') {
        if (end - position == 1) {
            // scheme:
            //       ^
            return parseScheme(begin, position, scheme);
        }

        if (position[1] == '/') {
            // scheme://host || scheme://
            //       ^                ^
            if (!parseScheme(begin, position, scheme)
                || !skipExactly<UChar>(position, end, ':')
                || !skipExactly<UChar>(position, end, '/')
                || !skipExactly<UChar>(position, end, '/'))
                return false;
            if (position == end)
                return false;
            beginHost = position;
            skipWhile<UChar, isNotColonOrSlash>(position, end);
        }

        if (position < end && *position == ':') {
            // host:port || scheme://host:port
            //     ^                     ^
            beginPort = position;
            skipUntil<UChar>(position, end, '/');
        }
    }

    if (position < end && *position == '/') {
        // scheme://host/path || scheme://host:port/path
        //              ^                          ^
        if (position == beginHost)
            return false;

        beginPath = position;
    }

    if (!parseHost(beginHost, beginPort ? beginPort : beginPath, host, hostHasWildcard))
        return false;

    if (!beginPort)
        port = std::nullopt;
    else {
        if (!parsePort(beginPort, beginPath, port, portHasWildcard))
            return false;
    }

    if (beginPath != end) {
        if (!parsePath(beginPath, end, path))
            return false;
    }

    return true;
}

//                     ; <scheme> production from RFC 3986
// scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
//
bool ContentSecurityPolicySourceList::parseScheme(const UChar* begin, const UChar* end, String& scheme)
{
    ASSERT(begin <= end);
    ASSERT(scheme.isEmpty());

    if (begin == end)
        return false;

    const UChar* position = begin;

    if (!skipExactly<UChar, isASCIIAlpha>(position, end))
        return false;

    skipWhile<UChar, isSchemeContinuationCharacter>(position, end);

    if (position != end)
        return false;

    scheme = String(begin, end - begin);
    return true;
}

// host              = [ "*." ] 1*host-char *( "." 1*host-char )
//                   / "*"
// host-char         = ALPHA / DIGIT / "-"
//
bool ContentSecurityPolicySourceList::parseHost(const UChar* begin, const UChar* end, String& host, bool& hostHasWildcard)
{
    ASSERT(begin <= end);
    ASSERT(host.isEmpty());
    ASSERT(!hostHasWildcard);

    if (begin == end)
        return false;

    const UChar* position = begin;

    if (skipExactly<UChar>(position, end, '*')) {
        hostHasWildcard = true;

        if (position == end)
            return true;

        if (!skipExactly<UChar>(position, end, '.'))
            return false;
    }

    const UChar* hostBegin = position;

    while (position < end) {
        if (!skipExactly<UChar, isHostCharacter>(position, end))
            return false;

        skipWhile<UChar, isHostCharacter>(position, end);

        if (position < end && !skipExactly<UChar>(position, end, '.'))
            return false;
    }

    ASSERT(position == end);
    host = String(hostBegin, end - hostBegin);
    return true;
}

bool ContentSecurityPolicySourceList::parsePath(const UChar* begin, const UChar* end, String& path)
{
    ASSERT(begin <= end);
    ASSERT(path.isEmpty());
    
    const UChar* position = begin;
    skipWhile<UChar, isPathComponentCharacter>(position, end);
    // path/to/file.js?query=string || path/to/file.js#anchor
    //                ^                               ^
    if (position < end)
        m_policy.reportInvalidPathCharacter(m_directiveName, String(begin, end - begin), *position);
    
    path = decodeURLEscapeSequences(String(begin, position - begin));
    
    ASSERT(position <= end);
    ASSERT(position == end || (*position == '#' || *position == '?'));
    return true;
}

// port              = ":" ( 1*DIGIT / "*" )
//
bool ContentSecurityPolicySourceList::parsePort(const UChar* begin, const UChar* end, std::optional<uint16_t>& port, bool& portHasWildcard)
{
    ASSERT(begin <= end);
    ASSERT(!port);
    ASSERT(!portHasWildcard);
    
    if (!skipExactly<UChar>(begin, end, ':'))
        ASSERT_NOT_REACHED();
    
    if (begin == end)
        return false;
    
    if (end - begin == 1 && *begin == '*') {
        port = std::nullopt;
        portHasWildcard = true;
        return true;
    }
    
    const UChar* position = begin;
    skipWhile<UChar, isASCIIDigit>(position, end);
    
    if (position != end)
        return false;
    
    bool ok;
    int portInt = charactersToIntStrict(begin, end - begin, &ok);
    if (portInt < 0 || portInt > std::numeric_limits<uint16_t>::max())
        return false;
    port = portInt;
    return ok;
}

// Match Blink's behavior of allowing an equal sign to appear anywhere in the value of the nonce
// even though this does not match the behavior of Content Security Policy Level 3 spec.,
// <https://w3c.github.io/webappsec-csp/> (29 February 2016).
static bool isNonceCharacter(UChar c)
{
    return isBase64OrBase64URLCharacter(c) || c == '=';
}

// nonce-source    = "'nonce-" nonce-value "'"
// nonce-value     = base64-value
bool ContentSecurityPolicySourceList::parseNonceSource(const UChar* begin, const UChar* end)
{
    const unsigned noncePrefixLength = 7;
    if (!StringView(begin, end - begin).startsWithIgnoringASCIICase("'nonce-"))
        return false;
    const UChar* position = begin + noncePrefixLength;
    const UChar* beginNonceValue = position;
    skipWhile<UChar, isNonceCharacter>(position, end);
    if (position >= end || position == beginNonceValue || *position != '\'')
        return false;
    m_nonces.add(String(beginNonceValue, position - beginNonceValue));
    return true;
}

// hash-source    = "'" hash-algorithm "-" base64-value "'"
// hash-algorithm = "sha256" / "sha384" / "sha512"
// base64-value  = 1*( ALPHA / DIGIT / "+" / "/" / "-" / "_" )*2( "=" )
bool ContentSecurityPolicySourceList::parseHashSource(const UChar* begin, const UChar* end)
{
    if (begin == end)
        return false;

    const UChar* position = begin;
    if (!skipExactly<UChar>(position, end, '\''))
        return false;

    auto digest = parseCryptographicDigest(position, end);
    if (!digest)
        return false;

    if (position >= end || *position != '\'')
        return false;

    if (digest->value.size() > ContentSecurityPolicyHash::maximumDigestLength)
        return false;

    m_hashAlgorithmsUsed.add(digest->algorithm);
    m_hashes.add(WTFMove(*digest));
    return true;
}

} // namespace WebCore
