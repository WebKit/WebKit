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
#include <wtf/text/StringParsingBuffer.h>

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

template<typename CharacterType> static bool isSourceCharacter(CharacterType c)
{
    return !isASCIISpace(c);
}

template<typename CharacterType> static bool isHostCharacter(CharacterType c)
{
    return isASCIIAlphanumeric(c) || c == '-';
}

template<typename CharacterType> static bool isPathComponentCharacter(CharacterType c)
{
    return c != '?' && c != '#';
}

template<typename CharacterType> static bool isSchemeContinuationCharacter(CharacterType c)
{
    return isASCIIAlphanumeric(c) || c == '+' || c == '-' || c == '.';
}

template<typename CharacterType> static bool isNotColonOrSlash(CharacterType c)
{
    return c != ':' && c != '/';
}

template<typename CharacterType> static bool isSourceListNone(StringParsingBuffer<CharacterType> buffer)
{
    skipWhile<CharacterType, isASCIISpace>(buffer);

    if (!skipExactlyIgnoringASCIICase(buffer, "'none'"))
        return false;

    skipWhile<CharacterType, isASCIISpace>(buffer);

    return buffer.atEnd();
}

ContentSecurityPolicySourceList::ContentSecurityPolicySourceList(const ContentSecurityPolicy& policy, const String& directiveName)
    : m_policy(policy)
    , m_directiveName(directiveName)
{
}

void ContentSecurityPolicySourceList::parse(const String& value)
{
    readCharactersForParsing(value, [&](auto buffer) {
        if (isSourceListNone(buffer)) {
            m_isNone = true;
            return;
        }

        parse(buffer);
    });
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
template<typename CharacterType> void ContentSecurityPolicySourceList::parse(StringParsingBuffer<CharacterType> buffer)
{
    while (buffer.hasCharactersRemaining()) {
        skipWhile<CharacterType, isASCIISpace>(buffer);
        if (buffer.atEnd())
            return;

        auto beginSource = buffer.position();
        skipWhile<CharacterType, isSourceCharacter>(buffer);

        auto sourceBuffer = StringParsingBuffer { beginSource, buffer.position() };

        if (parseNonceSource(sourceBuffer))
            continue;

        if (parseHashSource(sourceBuffer))
            continue;

        if (auto source = parseSource(sourceBuffer)) {
            // Wildcard hosts and keyword sources ('self', 'unsafe-inline',
            // etc.) aren't stored in m_list, but as attributes on the source
            // list itself.
            if (source->scheme.isEmpty() && source->host.value.isEmpty())
                continue;
            if (isCSPDirectiveName(source->host.value))
                m_policy.reportDirectiveAsSourceExpression(m_directiveName, source->host.value);
            m_list.append(ContentSecurityPolicySource(m_policy, source->scheme, source->host.value, source->port.value, source->path, source->host.hasWildcard, source->port.hasWildcard));
        } else
            m_policy.reportInvalidSourceExpression(m_directiveName, String(beginSource, buffer.position() - beginSource));

        ASSERT(buffer.atEnd() || isASCIISpace(*buffer));
    }
    
    m_list.shrinkToFit();
}

// source            = scheme ":"
//                   / ( [ scheme "://" ] host [ port ] [ path ] )
//                   / "'self'"
//
template<typename CharacterType> Optional<ContentSecurityPolicySourceList::Source> ContentSecurityPolicySourceList::parseSource(StringParsingBuffer<CharacterType> buffer)
{
    if (buffer.atEnd())
        return WTF::nullopt;

    if (skipExactlyIgnoringASCIICase(buffer, "'none'"))
        return WTF::nullopt;

    Source source;

    if (buffer.lengthRemaining() == 1 && *buffer == '*') {
        m_allowStar = true;
        return source;
    }

    if (skipExactlyIgnoringASCIICase(buffer, "'self'")) {
        m_allowSelf = true;
        return source;
    }

    if (skipExactlyIgnoringASCIICase(buffer, "'unsafe-inline'")) {
        m_allowInline = true;
        return source;
    }

    if (skipExactlyIgnoringASCIICase(buffer, "'unsafe-eval'")) {
        m_allowEval = true;
        return source;
    }

    auto begin = buffer.position();
    auto beginHost = begin;
    auto beginPath = buffer.end();
    const CharacterType* beginPort = nullptr;

    skipWhile<CharacterType, isNotColonOrSlash>(buffer);

    if (buffer.atEnd()) {
        // host
        //     ^
        auto host = parseHost(StringParsingBuffer { beginHost, buffer.position() });
        if (!host)
            return WTF::nullopt;

        source.host = WTFMove(*host);
        return source;
    }

    if (buffer.hasCharactersRemaining() && *buffer == '/') {
        // host/path || host/ || /
        //     ^            ^    ^
        auto host = parseHost(StringParsingBuffer { beginHost, buffer.position() });
        if (!host)
            return WTF::nullopt;

        auto path = parsePath(buffer);
        if (!path)
            return WTF::nullopt;

        source.host = WTFMove(*host);
        source.path = WTFMove(*path);
        return source;
    }

    if (buffer.hasCharactersRemaining() && *buffer == ':') {
        if (buffer.lengthRemaining() == 1) {
            // scheme:
            //       ^
            auto scheme = parseScheme(StringParsingBuffer { begin, buffer.position() });
            if (!scheme)
                return WTF::nullopt;

            source.scheme = WTFMove(*scheme);
            return source;
        }

        if (buffer[1] == '/') {
            // scheme://host || scheme://
            //       ^                ^
            auto scheme = parseScheme(StringParsingBuffer { begin, buffer.position() });
            if (!scheme
                || !skipExactly(buffer, ':')
                || !skipExactly(buffer, '/')
                || !skipExactly(buffer, '/'))
                return WTF::nullopt;
            if (buffer.atEnd())
                return WTF::nullopt;

            source.scheme = WTFMove(*scheme);

            beginHost = buffer.position();
            skipWhile<CharacterType, isNotColonOrSlash>(buffer);
        }

        if (buffer.hasCharactersRemaining() && *buffer == ':') {
            // host:port || scheme://host:port
            //     ^                     ^
            beginPort = buffer.position();
            skipUntil(buffer, '/');
        }
    }

    if (buffer.hasCharactersRemaining() && *buffer == '/') {
        // scheme://host/path || scheme://host:port/path
        //              ^                          ^
        if (buffer.position() == beginHost)
            return WTF::nullopt;

        beginPath = buffer.position();
    }

    auto host = parseHost(StringParsingBuffer { beginHost, beginPort ? beginPort : beginPath });
    if (!host)
        return WTF::nullopt;

    if (beginPort) {
        auto port = parsePort(StringParsingBuffer { beginPort, beginPath });
        if (!port)
            return WTF::nullopt;

        source.port = WTFMove(*port);
    }

    if (beginPath != buffer.end()) {
        auto path = parsePath(StringParsingBuffer { beginPath, buffer.end() });
        if (!path)
            return WTF::nullopt;

        source.path = WTFMove(*path);
    }

    source.host = WTFMove(*host);
    return source;
}

//                     ; <scheme> production from RFC 3986
// scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
//
template<typename CharacterType> Optional<String> ContentSecurityPolicySourceList::parseScheme(StringParsingBuffer<CharacterType> buffer)
{
    ASSERT(buffer.position() <= buffer.end());

    if (buffer.atEnd())
        return WTF::nullopt;

    auto begin = buffer.position();

    if (!skipExactly<CharacterType, isASCIIAlpha>(buffer))
        return WTF::nullopt;

    skipWhile<CharacterType, isSchemeContinuationCharacter>(buffer);

    if (!buffer.atEnd())
        return WTF::nullopt;

    return String(begin, buffer.position() - begin);
}

// host              = [ "*." ] 1*host-char *( "." 1*host-char )
//                   / "*"
// host-char         = ALPHA / DIGIT / "-"
//
template<typename CharacterType> Optional<ContentSecurityPolicySourceList::Host> ContentSecurityPolicySourceList::parseHost(StringParsingBuffer<CharacterType> buffer)
{
    ASSERT(buffer.position() <= buffer.end());

    if (buffer.atEnd())
        return WTF::nullopt;

    Host host;

    if (skipExactly(buffer, '*')) {
        host.hasWildcard = true;

        if (buffer.atEnd())
            return host;

        if (!skipExactly(buffer, '.'))
            return WTF::nullopt;
    }

    auto hostBegin = buffer.position();

    while (buffer.hasCharactersRemaining()) {
        if (!skipExactly<CharacterType, isHostCharacter>(buffer))
            return WTF::nullopt;

        skipWhile<CharacterType, isHostCharacter>(buffer);

        if (buffer.hasCharactersRemaining() && !skipExactly(buffer, '.'))
            return WTF::nullopt;
    }

    ASSERT(buffer.atEnd());
    host.value = String(hostBegin, buffer.position() - hostBegin);
    return host;
}

template<typename CharacterType> Optional<String> ContentSecurityPolicySourceList::parsePath(StringParsingBuffer<CharacterType> buffer)
{
    ASSERT(buffer.position() <= buffer.end());
    
    auto begin = buffer.position();
    skipWhile<CharacterType, isPathComponentCharacter>(buffer);
    // path/to/file.js?query=string || path/to/file.js#anchor
    //                ^                               ^
    if (buffer.hasCharactersRemaining())
        m_policy.reportInvalidPathCharacter(m_directiveName, String(begin, buffer.end() - begin), *buffer);

    ASSERT(buffer.position() <= buffer.end());
    ASSERT(buffer.atEnd() || (*buffer == '#' || *buffer == '?'));

    return decodeURLEscapeSequences(StringView(begin, buffer.position() - begin));
}

// port              = ":" ( 1*DIGIT / "*" )
//
template<typename CharacterType> Optional<ContentSecurityPolicySourceList::Port> ContentSecurityPolicySourceList::parsePort(StringParsingBuffer<CharacterType> buffer)
{
    ASSERT(buffer.position() <= buffer.end());
    
    if (!skipExactly(buffer, ':'))
        ASSERT_NOT_REACHED();
    
    if (buffer.atEnd())
        return WTF::nullopt;
    
    if (buffer.lengthRemaining() == 1 && *buffer == '*') {
        Port port;
        port.hasWildcard = true;
        return port;
    }
    
    auto begin = buffer.position();
    skipWhile<CharacterType, isASCIIDigit>(buffer);
    
    if (!buffer.atEnd())
        return WTF::nullopt;
    
    bool ok;
    int portInt = charactersToIntStrict(begin, buffer.position() - begin, &ok);
    if (!ok || portInt < 0 || portInt > std::numeric_limits<uint16_t>::max())
        return WTF::nullopt;

    Port port;
    port.value = portInt;
    return port;
}

// Match Blink's behavior of allowing an equal sign to appear anywhere in the value of the nonce
// even though this does not match the behavior of Content Security Policy Level 3 spec.,
// <https://w3c.github.io/webappsec-csp/> (29 February 2016).
template<typename CharacterType> static bool isNonceCharacter(CharacterType c)
{
    return isBase64OrBase64URLCharacter(c) || c == '=';
}

// nonce-source    = "'nonce-" nonce-value "'"
// nonce-value     = base64-value
template<typename CharacterType> bool ContentSecurityPolicySourceList::parseNonceSource(StringParsingBuffer<CharacterType> buffer)
{
    if (!skipExactlyIgnoringASCIICase(buffer, "'nonce-"))
        return false;

    auto beginNonceValue = buffer.position();
    skipWhile<CharacterType, isNonceCharacter>(buffer);
    if (buffer.atEnd() || buffer.position() == beginNonceValue || *buffer != '\'')
        return false;
    m_nonces.add(String(beginNonceValue, buffer.position() - beginNonceValue));
    return true;
}

// hash-source    = "'" hash-algorithm "-" base64-value "'"
// hash-algorithm = "sha256" / "sha384" / "sha512"
// base64-value  = 1*( ALPHA / DIGIT / "+" / "/" / "-" / "_" )*2( "=" )
template<typename CharacterType> bool ContentSecurityPolicySourceList::parseHashSource(StringParsingBuffer<CharacterType> buffer)
{
    if (buffer.atEnd())
        return false;

    if (!skipExactly(buffer, '\''))
        return false;

    auto digest = parseCryptographicDigest(buffer);
    if (!digest)
        return false;

    if (buffer.atEnd() || *buffer != '\'')
        return false;

    if (digest->value.size() > ContentSecurityPolicyHash::maximumDigestLength)
        return false;

    m_hashAlgorithmsUsed.add(digest->algorithm);
    m_hashes.add(WTFMove(*digest));
    return true;
}

} // namespace WebCore
