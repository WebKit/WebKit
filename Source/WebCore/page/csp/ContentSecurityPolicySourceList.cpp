/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "PublicSuffixStore.h"
#include <pal/text/TextEncoding.h>
#include <wtf/ASCIICType.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/URL.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringParsingBuffer.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

static bool isCSPDirectiveName(StringView name)
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
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::reportTo)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::reportURI)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::sandbox)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::scriptSrc)
        || equalIgnoringASCIICase(name, ContentSecurityPolicyDirectiveNames::styleSrc);
}

template<typename CharacterType> static bool isSourceCharacter(CharacterType c)
{
    return !isUnicodeCompatibleASCIIWhitespace(c);
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
    skipWhile<isUnicodeCompatibleASCIIWhitespace>(buffer);

    if (!skipExactlyIgnoringASCIICase(buffer, "'none'"_s))
        return false;

    skipWhile<isUnicodeCompatibleASCIIWhitespace>(buffer);

    return buffer.atEnd();
}

ContentSecurityPolicySourceList::ContentSecurityPolicySourceList(const ContentSecurityPolicy& policy, const String& directiveName)
    : m_policy(policy)
    , m_directiveName(directiveName)
    , m_contentSecurityPolicyModeForExtension(m_policy.contentSecurityPolicyModeForExtension())
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

    // This is counter to the CSP3 spec which only allows HTTPS but Chromium also allows it.
    bool isAllowed = url.protocolIsInHTTPFamily() || url.protocolIs("ws"_s) || url.protocolIs("wss"_s) || url.protocolIs(m_policy.selfProtocol());
    // Also not allowed by the Content Security Policy Level 3 spec., we allow a data URL to match
    // "img-src *" and either a data URL or blob URL to match "media-src *" for web compatibility.
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

    if (m_allowSelf && m_policy.urlMatchesSelf(url, equalIgnoringASCIICase(m_directiveName, ContentSecurityPolicyDirectiveNames::frameSrc)
))
        return true;

    for (auto& entry : m_list) {
        if (entry.matches(url, didReceiveRedirectResponse))
            return true;
    }

    return false;
}

bool ContentSecurityPolicySourceList::matches(const Vector<ContentSecurityPolicyHash>& hashes) const
{
    for (auto& hash : hashes) {
        if (m_hashes.contains(hash))
            return true;
    }

    return false;
}

bool ContentSecurityPolicySourceList::matchesAll(const Vector<ContentSecurityPolicyHash>& hashes) const
{
    if (hashes.isEmpty())
        return false;

    for (auto& hash : hashes) {
        if (!m_hashes.contains(hash))
            return false;
    }

    return true;
}

bool ContentSecurityPolicySourceList::matches(const String& nonce) const
{
    if (nonce.isEmpty())
        return false;

    return m_nonces.contains(nonce);
}

static bool schemeIsInHttpFamily(StringView scheme)
{
    return equalLettersIgnoringASCIICase(scheme, "https"_s) || equalLettersIgnoringASCIICase(scheme, "http"_s);
}

static bool isRestrictedDirectiveForMode(const String& directive, ContentSecurityPolicyModeForExtension mode)
{
    switch (mode) {
    case ContentSecurityPolicyModeForExtension::None:
        return false;
    // FIXME: If the script-src directive is strict enough, we should allow default-src to have more values.
    case ContentSecurityPolicyModeForExtension::ManifestV2:
        return directive == ContentSecurityPolicyDirectiveNames::scriptSrc
            || directive == ContentSecurityPolicyDirectiveNames::defaultSrc;
    case ContentSecurityPolicyModeForExtension::ManifestV3:
        return directive == ContentSecurityPolicyDirectiveNames::scriptSrc
            || directive == ContentSecurityPolicyDirectiveNames::objectSrc
            || directive == ContentSecurityPolicyDirectiveNames::workerSrc
            || directive == ContentSecurityPolicyDirectiveNames::defaultSrc;
    }
    return false;
}

bool ContentSecurityPolicySourceList::isValidSourceForExtensionMode(const ContentSecurityPolicySourceList::Source& parsedSource)
{
    switch (m_contentSecurityPolicyModeForExtension) {
    case ContentSecurityPolicyModeForExtension::None:
        return true;
    case ContentSecurityPolicyModeForExtension::ManifestV2:
        if (!isRestrictedDirectiveForMode(m_directiveName, ContentSecurityPolicyModeForExtension::ManifestV2))
            return true;

        if (parsedSource.host.hasWildcard && PublicSuffixStore::singleton().isPublicSuffix(parsedSource.host.value))
            return false;

        if (equalLettersIgnoringASCIICase(parsedSource.scheme, "blob"_s))
            return true;

        if (!equalLettersIgnoringASCIICase(parsedSource.scheme, "https"_s) || parsedSource.host.value.isEmpty())
            return false;
        break;
    case ContentSecurityPolicyModeForExtension::ManifestV3:
        if (!isRestrictedDirectiveForMode(m_directiveName, ContentSecurityPolicyModeForExtension::ManifestV3))
            return true;

        if (!schemeIsInHttpFamily(parsedSource.scheme) || !SecurityOrigin::isLocalHostOrLoopbackIPAddress(parsedSource.host.value))
            return false;
    }
    return true;
}

static bool extensionModeAllowsKeywordsForDirective(ContentSecurityPolicyModeForExtension mode, const String& directiveName)
{
    return mode != ContentSecurityPolicyModeForExtension::ManifestV3 || !isRestrictedDirectiveForMode(directiveName, mode);
}

// source-list       = *WSP [ source *( 1*WSP source ) *WSP ]
//                   / *WSP "'none'" *WSP
//
template<typename CharacterType> void ContentSecurityPolicySourceList::parse(StringParsingBuffer<CharacterType> buffer)
{
    while (buffer.hasCharactersRemaining()) {
        skipWhile<isUnicodeCompatibleASCIIWhitespace>(buffer);
        if (buffer.atEnd())
            return;

        auto beginSource = buffer.span();
        skipWhile<isSourceCharacter>(buffer);

        StringParsingBuffer sourceBuffer(beginSource.first(buffer.position() - beginSource.data()));

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
            if (isValidSourceForExtensionMode(source.value()))
                m_list.append(ContentSecurityPolicySource(m_policy, source->scheme.convertToASCIILowercase(), source->host.value.toString(), source->port.value, source->path, source->host.hasWildcard, source->port.hasWildcard, IsSelfSource::No));
        } else
            m_policy.reportInvalidSourceExpression(m_directiveName, beginSource.first(buffer.position() - beginSource.data()));

        ASSERT(buffer.atEnd() || isUnicodeCompatibleASCIIWhitespace(*buffer));
    }
    
    m_list.shrinkToFit();
}

// source            = scheme ":"
//                   / ( [ scheme "://" ] host [ port ] [ path ] )
//                   / "'self'"
//
template<typename CharacterType> std::optional<ContentSecurityPolicySourceList::Source> ContentSecurityPolicySourceList::parseSource(StringParsingBuffer<CharacterType> buffer)
{
    if (buffer.atEnd())
        return std::nullopt;

    if (skipExactlyIgnoringASCIICase(buffer, "'none'"_s))
        return std::nullopt;

    Source source;

    if (buffer.lengthRemaining() == 1 && *buffer == '*' && !isRestrictedDirectiveForMode(m_directiveName, m_contentSecurityPolicyModeForExtension)) {
        m_allowStar = true;
        return source;
    }

    if (skipExactlyIgnoringASCIICase(buffer, "'strict-dynamic'"_s)
        && extensionModeAllowsKeywordsForDirective(m_contentSecurityPolicyModeForExtension, m_directiveName)
        && (m_directiveName == ContentSecurityPolicyDirectiveNames::scriptSrc
            || m_directiveName == ContentSecurityPolicyDirectiveNames::scriptSrcElem || m_directiveName == ContentSecurityPolicyDirectiveNames::defaultSrc)) {
        m_allowNonParserInsertedScripts = true;
        m_allowSelf = false;
        m_allowInline = false;
        return source;
    }

    if (skipExactlyIgnoringASCIICase(buffer, "'self'"_s)) {
        m_allowSelf = !m_allowNonParserInsertedScripts;
        return source;
    }

    if (skipExactlyIgnoringASCIICase(buffer, "'unsafe-inline'"_s) && !isRestrictedDirectiveForMode(m_directiveName, m_contentSecurityPolicyModeForExtension)) {
        m_allowInline = !m_allowNonParserInsertedScripts;
        return source;
    }

    if (skipExactlyIgnoringASCIICase(buffer, "'unsafe-eval'"_s) && extensionModeAllowsKeywordsForDirective(m_contentSecurityPolicyModeForExtension, m_directiveName)) {
        m_allowEval = true;
        m_allowWasmEval = true;
        return source;
    }

    if (skipExactlyIgnoringASCIICase(buffer, "'wasm-unsafe-eval'"_s) && extensionModeAllowsKeywordsForDirective(m_contentSecurityPolicyModeForExtension, m_directiveName)) {
        m_allowWasmEval = true;
        return source;
    }

    if (skipExactlyIgnoringASCIICase(buffer, "'unsafe-hashes'"_s) && extensionModeAllowsKeywordsForDirective(m_contentSecurityPolicyModeForExtension, m_directiveName)) {
        m_allowUnsafeHashes = true;
        return source;
    }

    if (skipExactlyIgnoringASCIICase(buffer, "'report-sample'"_s) && extensionModeAllowsKeywordsForDirective(m_contentSecurityPolicyModeForExtension, m_directiveName)) {
        m_reportSample = true;
        return source;
    }

    if (m_allowNonParserInsertedScripts)
        return source;

    auto begin = buffer.span();
    auto beginHost = begin;
    auto beginPath = begin.subspan(begin.size());
    std::span<const CharacterType> beginPort;

    skipWhile<isNotColonOrSlash>(buffer);

    if (buffer.atEnd()) {
        // host
        //     ^
        auto host = parseHost(beginHost.first(buffer.position() - beginHost.data()));
        if (!host)
            return std::nullopt;

        source.host = WTFMove(*host);
        return source;
    }

    if (buffer.hasCharactersRemaining() && *buffer == '/') {
        // host/path || host/ || /
        //     ^            ^    ^
        auto host = parseHost(beginHost.first(buffer.position() - beginHost.data()));
        if (!host)
            return std::nullopt;

        auto path = parsePath(buffer.span());
        if (!path)
            return std::nullopt;

        source.host = WTFMove(*host);
        source.path = WTFMove(path);
        return source;
    }

    if (buffer.hasCharactersRemaining() && *buffer == ':') {
        if (buffer.lengthRemaining() == 1) {
            // scheme:
            //       ^
            auto scheme = parseScheme(StringParsingBuffer(begin.first(buffer.position() - begin.data())));
            if (!scheme)
                return std::nullopt;

            source.scheme = WTFMove(scheme);
            return source;
        }

        if (buffer[1] == '/') {
            // scheme://host || scheme://
            //       ^                ^
            auto scheme = parseScheme(StringParsingBuffer(begin.first(buffer.position() - begin.data())));
            if (!scheme
                || !skipExactly(buffer, ':')
                || !skipExactly(buffer, '/')
                || !skipExactly(buffer, '/'))
                return std::nullopt;
            if (buffer.atEnd())
                return std::nullopt;

            source.scheme = WTFMove(scheme);

            beginHost = buffer.span();
            skipWhile<isNotColonOrSlash>(buffer);
        }

        if (buffer.hasCharactersRemaining() && *buffer == ':') {
            // host:port || scheme://host:port
            //     ^                     ^
            beginPort = buffer.span();
            skipUntil(buffer, '/');
        }
    }

    if (buffer.hasCharactersRemaining() && *buffer == '/') {
        // scheme://host/path || scheme://host:port/path
        //              ^                          ^
        if (buffer.position() == beginHost.data())
            return std::nullopt;

        beginPath = buffer.span();
    }

    auto host = parseHost(beginHost.first((beginPort.data() ? beginPort.data() : beginPath.data()) - beginHost.data()));
    if (!host)
        return std::nullopt;

    if (beginPort.data()) {
        auto port = parsePort(beginPort.first(beginPath.data() - beginPort.data()));
        if (!port)
            return std::nullopt;

        source.port = WTFMove(*port);
    }

    if (!beginPath.empty()) {
        auto path = parsePath(beginPath);
        if (!path)
            return std::nullopt;

        source.path = WTFMove(path);
    }

    source.host = WTFMove(*host);
    return source;
}

//                     ; <scheme> production from RFC 3986
// scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
//
template<typename CharacterType> StringView ContentSecurityPolicySourceList::parseScheme(StringParsingBuffer<CharacterType> buffer)
{
    ASSERT(buffer.position() <= buffer.end());

    if (buffer.atEnd())
        return { };

    auto begin = buffer.span();

    if (!skipExactly<isASCIIAlpha>(buffer))
        return { };

    skipWhile<isSchemeContinuationCharacter>(buffer);

    if (!buffer.atEnd())
        return { };

    return begin.first(buffer.position() - begin.data());
}

// host              = [ "*." ] 1*host-char *( "." 1*host-char )
//                   / "*"
// host-char         = ALPHA / DIGIT / "-"
//
template<typename CharacterType> std::optional<ContentSecurityPolicySourceList::Host> ContentSecurityPolicySourceList::parseHost(std::span<const CharacterType> span)
{
    StringParsingBuffer buffer { span };
    ASSERT(buffer.position() <= buffer.end());

    if (buffer.atEnd())
        return std::nullopt;

    Host host;

    if (skipExactly(buffer, '*')) {
        host.hasWildcard = true;

        if (buffer.atEnd())
            return host;

        if (!skipExactly(buffer, '.'))
            return std::nullopt;
    }

    auto hostBegin = buffer.span();

    while (buffer.hasCharactersRemaining()) {
        if (!skipExactly<isHostCharacter>(buffer))
            return std::nullopt;

        skipWhile<isHostCharacter>(buffer);

        if (buffer.hasCharactersRemaining() && !skipExactly(buffer, '.'))
            return std::nullopt;
    }

    ASSERT(buffer.atEnd());
    host.value = hostBegin.first(buffer.position() - hostBegin.data());
    return host;
}

template<typename CharacterType> String ContentSecurityPolicySourceList::parsePath(std::span<const CharacterType> span)
{
    StringParsingBuffer buffer { span };
    ASSERT(buffer.position() <= buffer.end());
    
    auto begin = buffer.span();
    skipWhile<isPathComponentCharacter>(buffer);
    // path/to/file.js?query=string || path/to/file.js#anchor
    //                ^                               ^
    if (buffer.hasCharactersRemaining())
        m_policy.reportInvalidPathCharacter(m_directiveName, begin, *buffer);

    ASSERT(buffer.position() <= buffer.end());
    ASSERT(buffer.atEnd() || (*buffer == '#' || *buffer == '?'));

    return PAL::decodeURLEscapeSequences(begin.first(buffer.position() - begin.data()));
}

// port              = ":" ( 1*DIGIT / "*" )
//
template<typename CharacterType> std::optional<ContentSecurityPolicySourceList::Port> ContentSecurityPolicySourceList::parsePort(std::span<const CharacterType> span)
{
    StringParsingBuffer buffer { span };
    ASSERT(buffer.position() <= buffer.end());
    
    if (!skipExactly(buffer, ':'))
        ASSERT_NOT_REACHED();
    
    if (buffer.atEnd())
        return std::nullopt;
    
    if (buffer.lengthRemaining() == 1 && *buffer == '*') {
        Port port;
        port.hasWildcard = true;
        return port;
    }
    
    auto begin = buffer.span();
    skipWhile<isASCIIDigit>(buffer);
    
    if (!buffer.atEnd())
        return std::nullopt;

    unsigned length = buffer.position() - begin.data();
    auto portInteger = parseInteger<uint16_t>(begin.first(length)).value_or(0);
    if (!portInteger)
        return std::nullopt;

    Port port;
    port.value = portInteger;
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
    if (!skipExactlyIgnoringASCIICase(buffer, "'nonce-"_s))
        return false;

    auto beginNonceValue = buffer.span();
    skipWhile<isNonceCharacter>(buffer);
    if (buffer.atEnd() || buffer.position() == beginNonceValue.data() || *buffer != '\'')
        return false;
    if (extensionModeAllowsKeywordsForDirective(m_contentSecurityPolicyModeForExtension, m_directiveName))
        m_nonces.add(beginNonceValue.first(buffer.position() - beginNonceValue.data()));
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

    if (extensionModeAllowsKeywordsForDirective(m_contentSecurityPolicyModeForExtension, m_directiveName)) {
        m_hashAlgorithmsUsed.add(digest->algorithm);
        m_hashes.add(WTFMove(*digest));
    }
    return true;
}

} // namespace WebCore
