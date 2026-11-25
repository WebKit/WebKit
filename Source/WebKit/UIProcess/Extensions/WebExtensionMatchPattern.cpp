/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
#include "WebExtensionMatchPattern.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include <WebCore/LegacySchemeRegistry.h>
#include <WebCore/LocalizedStrings.h>
#include <WebCore/PublicSuffixStore.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/URLParser.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringHash.h>

namespace WebKit {

using namespace WTF;
using namespace WebCore;

static constexpr ASCIILiteral allURLsPattern = "<all_urls>"_s;
static constexpr ASCIILiteral allHostsAndSchemesPattern = "*://*/*"_s;

WebExtensionMatchPattern::URLSchemeSet& WebExtensionMatchPattern::extensionSchemes()
{
    static MainRunLoopNeverDestroyed<URLSchemeSet> schemes = std::initializer_list<RefPtr<StringImpl>> { "webkit-extension"_si };
    return schemes;
}

WebExtensionMatchPattern::URLSchemeSet& WebExtensionMatchPattern::validSchemes()
{
    static MainRunLoopNeverDestroyed<URLSchemeSet> schemes = std::initializer_list<RefPtr<StringImpl>> { "*"_si, "http"_si, "https"_si, "file"_si, "ftp"_si, "webkit-extension"_si };
    return schemes;
}

WebExtensionMatchPattern::URLSchemeSet& WebExtensionMatchPattern::supportedSchemes()
{
    static MainRunLoopNeverDestroyed<URLSchemeSet> schemes = std::initializer_list<RefPtr<StringImpl>> { "*"_si, "http"_si, "https"_si, "webkit-extension"_si };
    return schemes;
}

bool WebExtensionMatchPattern::patternsMatchURL(const MatchPatternSet& matchPatterns, const URL& url)
{
    for (auto& matchPattern : matchPatterns) {
        if (matchPattern->matchesURL(url))
            return true;
    }

    return false;
}

bool WebExtensionMatchPattern::patternsMatchPattern(const MatchPatternSet& matchPatterns, const WebExtensionMatchPattern& otherPattern)
{
    for (auto& matchPattern : matchPatterns) {
        if (matchPattern->matchesPattern(otherPattern))
            return true;
    }

    return false;
}

NODELETE static HashMap<RefPtr<StringImpl>, RefPtr<WebExtensionMatchPattern>>& patternCache()
{
    static MainRunLoopNeverDestroyed<HashMap<RefPtr<StringImpl>, RefPtr<WebExtensionMatchPattern>>> cache;
    return cache;
}

void WebExtensionMatchPattern::registerCustomURLScheme(RefPtr<StringImpl> urlScheme)
{
    auto canonicalScheme = URLParser::maybeCanonicalizeScheme(RefPtr<StringImpl>(urlScheme));
    ASSERT(canonicalScheme);

    extensionSchemes().addVoid(canonicalScheme.value());
    validSchemes().addVoid(canonicalScheme.value());
    supportedSchemes().addVoid(canonicalScheme.value());

    LegacySchemeRegistry::registerURLSchemeAsAllowingServiceWorkerClients(canonicalScheme.value());

    for (auto& pool : WebProcessPool::allProcessPools())
        pool->sendToAllProcesses(Messages::WebProcess::RegisterURLSchemeAsWebExtension(urlScheme));
}

bool WebExtensionMatchPattern::isWebExtensionURL(const URL& url)
{
    if (!url.isValid())
        return false;
    return extensionSchemes().contains(url.protocol().toString());
}

RefPtr<WebExtensionMatchPattern> WebExtensionMatchPattern::getOrCreate(StringImpl* pattern)
{
    ASSERT(!isEmpty(pattern));

    return patternCache().ensure(pattern, [&] {
        RefPtr<API::Error> error;
        return create(pattern, error);
    }).iterator->value;
}

RefPtr<WebExtensionMatchPattern> WebExtensionMatchPattern::getOrCreate(StringImpl* scheme, StringImpl* host, StringImpl* path)
{
    RefPtr<StringImpl> resolvedScheme = !isEmpty(scheme) ? RefPtr { scheme } : "*"_si;
    RefPtr<StringImpl> resolvedHost = !isEmpty(host) || equalLettersIgnoringASCIICase(scheme, "file"_s) ? RefPtr { host } : "*"_si;
    RefPtr<StringImpl> resolvedPath = !isEmpty(path) ? RefPtr { path } : "/*"_si;

    RefPtr<StringImpl> pattern = makeString(resolvedScheme, "://"_s, resolvedHost, resolvedPath);

    return patternCache().ensure(pattern, [&] {
        RefPtr<API::Error> error;
        return create(resolvedScheme, resolvedHost, resolvedPath, error);
    }).iterator->value;
}

RefPtr<WebExtensionMatchPattern> WebExtensionMatchPattern::getOrCreate(const URL& url, OptionSet<CreateOptions> options)
{
    ASSERT(!url.isEmpty());

    RefPtr<StringImpl> scheme = "*"_si;
    if (options.contains(CreateOptions::MatchExactScheme) || !url.protocolIsInHTTPFamily())
        scheme = url.protocol().toString();

    RefPtr<StringImpl> host = url.host().toString();
    if (options.contains(CreateOptions::MatchSubdomains) && !isEmpty(host))
        host = makeString("*."_s, host);

    RefPtr<StringImpl> path = "/*"_si;
    if (!options.contains(CreateOptions::MatchAllPaths))
        path = url.hasPath() ? url.path().toString() : "/"_si;

    return getOrCreate(scheme, host, path);
}

Ref<WebExtensionMatchPattern> WebExtensionMatchPattern::allURLsMatchPattern()
{
    return getOrCreate(StringImpl::create(allURLsPattern).ptr()).releaseNonNull();
}

Ref<WebExtensionMatchPattern> WebExtensionMatchPattern::allHostsAndSchemesMatchPattern()
{
    return getOrCreate(StringImpl::create(allHostsAndSchemesPattern).ptr()).releaseNonNull();
}

bool WebExtensionMatchPattern::patternsMatchAllHosts(const MatchPatternSet& patterns)
{
    for (auto& pattern : patterns) {
        if (pattern->matchesAllHosts())
            return true;
    }

    return false;
}

Ref<API::Error> WebExtensionMatchPattern::createError(Error error, StringImpl* localizedDescription)
{
    return API::Error::create({ "WKWebExtensionMatchPatternErrorDomain"_si, static_cast<int>(error), { }, localizedDescription });
}

WebExtensionMatchPattern::WebExtensionMatchPattern(StringImpl* patternString, RefPtr<API::Error>& outError)
{
    ASSERT(patternString);

    ASSERT(!m_valid);
    ASSERT(!m_hash);

    outError = nullptr;

    m_matchesAllURLs = equal(patternString, allURLsPattern);
    if (m_matchesAllURLs) {
        m_valid = true;
        m_hash = WTF::hash(patternString);
        return;
    }

    UserContentURLPattern pattern { patternString };

    if (!isEmpty(pattern.scheme()) && !isValidScheme(pattern.scheme())) {
        outError = createError(Error::InvalidScheme, WEB_UI_FORMAT_STRING("\"%s\" cannot be parsed because the scheme \"%s\" is invalid.", "WKWebExtensionMatchPatternErrorInvalidScheme description for invalid scheme in the pattern", utf8(patternString), utf8(pattern.scheme())));
        return;
    }

    if (!pattern.isValid()) {
        switch (pattern.error()) {
        case UserContentURLPattern::Error::None:
        case UserContentURLPattern::Error::Invalid:
            ASSERT_NOT_REACHED();
            break;

        case UserContentURLPattern::Error::MissingScheme:
            outError = createError(Error::InvalidScheme, WEB_UI_FORMAT_STRING("\"%s\" cannot be parsed because it doesn't have a scheme.", "WKWebExtensionMatchPatternErrorInvalidScheme description for missing scheme in the pattern", utf8(patternString)));
            break;

        case UserContentURLPattern::Error::MissingHost:
            outError = createError(Error::InvalidHost, WEB_UI_FORMAT_STRING("\"%s\" cannot be parsed because it doesn't have a host.", "WKWebExtensionMatchPatternErrorInvalidHost description for missing host in the pattern", utf8(patternString)));
            break;

        case UserContentURLPattern::Error::InvalidHost:
            outError = createError(Error::InvalidHost, WEB_UI_FORMAT_STRING("\"%s\" cannot be parsed because the host \"%s\" is invalid.", "WKWebExtensionMatchPatternErrorInvalidHost description for invalid host in the pattern", utf8(patternString), utf8(pattern.host())));
            break;

        case UserContentURLPattern::Error::MissingPath:
            outError = createError(Error::InvalidPath, WEB_UI_FORMAT_STRING("\"%s\" cannot be parsed because it doesn't have a path.", "WKWebExtensionMatchPatternErrorInvalidPath description for missing path in the pattern", utf8(patternString)));
            break;
        }

        return;
    }

    m_pattern = WTF::move(pattern);
    m_valid = true;
    m_hash = WTF::hash(patternString);
}

WebExtensionMatchPattern::WebExtensionMatchPattern(StringImpl* scheme, StringImpl* host, StringImpl* path, RefPtr<API::Error>& outError)
{
    ASSERT(scheme);
    ASSERT(host);
    ASSERT(path);

    ASSERT(!m_valid);
    ASSERT(!m_hash);
    ASSERT(!m_matchesAllURLs);

    outError = nullptr;

    if (!isValidScheme(scheme)) {
        outError = createError(Error::InvalidScheme, WEB_UI_FORMAT_STRING("Scheme \"%s\" is invalid.", "WKWebExtensionMatchPatternErrorInvalidScheme description for invalid scheme", utf8(scheme)));
        return;
    }

    UserContentURLPattern pattern { scheme, host, path };

    if (!pattern.isValid()) {
        switch (pattern.error()) {
        case UserContentURLPattern::Error::None:
        case UserContentURLPattern::Error::Invalid:
            ASSERT_NOT_REACHED();
            break;

        case UserContentURLPattern::Error::MissingScheme:
            outError = createError(Error::InvalidScheme, WEB_UI_FORMAT_STRING("Scheme \"%s\" is invalid.", "WKWebExtensionMatchPatternErrorInvalidScheme description for missing scheme", utf8(scheme)));
            break;

        case UserContentURLPattern::Error::MissingHost:
        case UserContentURLPattern::Error::InvalidHost:
            outError = createError(Error::InvalidHost, WEB_UI_FORMAT_STRING("Host \"%s\" is invalid.", "WKWebExtensionMatchPatternErrorInvalidHost description for invalid or missing host", utf8(host)));
            break;

        case UserContentURLPattern::Error::MissingPath:
            outError = createError(Error::InvalidPath, WEB_UI_FORMAT_STRING("Path \"%s\" is invalid.", "WKWebExtensionMatchPatternErrorInvalidPath description for missing path", utf8(path)));
            break;
        }

        return;
    }

    m_pattern = WTF::move(pattern);
    m_valid = true;
    m_hash = WTF::hash(string());
}

bool WebExtensionMatchPattern::isSupported() const
{
    return isValid() && (m_matchesAllURLs || supportedSchemes().contains(scheme()) || equal(scheme(), "file"_s));
}

bool WebExtensionMatchPattern::operator==(const WebExtensionMatchPattern& other) const
{
    return this == &other || (m_valid == other.m_valid && m_matchesAllURLs == other.m_matchesAllURLs && m_hash == other.m_hash && m_pattern == other.m_pattern);
}

RefPtr<StringImpl> WebExtensionMatchPattern::scheme() const
{
    if (!isValid() || matchesAllURLs())
        return nullString();
    return pattern().scheme();
}

RefPtr<StringImpl> WebExtensionMatchPattern::host() const
{
    if (!isValid() || matchesAllURLs())
        return nullString();
    return pattern().originalHost();
}

RefPtr<StringImpl> WebExtensionMatchPattern::path() const
{
    if (!isValid() || matchesAllURLs())
        return nullString();
    return pattern().path();
}

bool WebExtensionMatchPattern::hostIsPublicSuffix() const
{
    auto host = pattern().host();
    if (startsWith(host, "*."_s))
        host = substring(host, 2);

    return WebCore::PublicSuffixStore::singleton().isPublicSuffix(host);
}

RefPtr<StringImpl> WebExtensionMatchPattern::stringWithScheme(StringImpl* differentScheme) const
{
    if (!isValid())
        return nullString();

    ASSERT(!m_matchesAllURLs || (m_matchesAllURLs && isEmpty(differentScheme)));

    if (m_matchesAllURLs)
        return allURLsPattern;
    return makeString(isEmpty(differentScheme) ? scheme() : RefPtr { differentScheme }, "://"_s, host(), path());
}

Vector<RefPtr<StringImpl>> WebExtensionMatchPattern::expandedStrings(OptionSet<Options> options) const
{
    if (!isValid())
        return { };

    if (m_matchesAllURLs) {
        auto strings = compactMap(supportedSchemes(), [&](auto& scheme) -> std::optional<RefPtr<StringImpl>> {
            if (scheme == "*"_s)
                return std::nullopt;
            return makeString(scheme, "://*/*"_s);
        });

        if (options.contains(Options::AllowFileScheme))
            strings.append("file://*/*"_s);

        return strings;
    }

    return { string() };
}

bool WebExtensionMatchPattern::matchesAllHosts() const
{
    return isValid() && (m_matchesAllURLs || pattern().matchAllHosts());
}

bool WebExtensionMatchPattern::isValidScheme(RefPtr<StringImpl> scheme)
{
    return !isEmpty(scheme) && validSchemes().contains(scheme);
}

bool WebExtensionMatchPattern::matchesURL(const URL& urlToMatch, OptionSet<Options> options) const
{
    ASSERT(!options.contains(Options::MatchBidirectionally));

    if (!isValid() || !urlToMatch.isValid())
        return false;

    if (m_matchesAllURLs) {
        if (urlToMatch.protocolIsFile())
            return options.contains(Options::AllowFileScheme);
        return supportedSchemes().contains(urlToMatch.protocol().toString());
    }

    if (!options.contains(Options::IgnoreSchemes) && !pattern().matchesScheme(urlToMatch))
        return false;

    if (!pattern().matchesHost(urlToMatch))
        return false;

    if (!options.contains(Options::IgnorePaths) && !pattern().matchesPath(urlToMatch))
        return false;

    return true;
}

bool WebExtensionMatchPattern::matchesPattern(const WebExtensionMatchPattern& patternToMatch, OptionSet<Options> options) const
{
    if (!isValid())
        return false;

    if (*this == patternToMatch)
        return true;

    auto compareAllURLs = [&options](const WebExtensionMatchPattern& a, const WebExtensionMatchPattern& b) {
        if (!a.matchesAllURLs())
            return false;
        if (b.matchesAllURLs() || supportedSchemes().contains(b.scheme()))
            return true;
        return options.contains(Options::AllowFileScheme) && equal(b.scheme(), "file"_s);
    };

    if (compareAllURLs(*this, patternToMatch))
        return true;

    if (options.contains(Options::MatchBidirectionally) && compareAllURLs(patternToMatch, *this))
        return true;

    // If we get here, and either pattern matchesAllURLs, one of the patterns has an unsupported, but parsable, scheme.
    // Bail now, since scheme, host, and path are nil for matchesAllURLs patterns and can't be used by the component matches.
    // For example: matching "<all_urls>" against "ftp://*/*" will not return true above, but should return false here.
    if (m_matchesAllURLs || patternToMatch.matchesAllURLs())
        return false;

    return schemeMatches(patternToMatch, options) && hostMatches(patternToMatch, options) && pathMatches(patternToMatch, options);
}

bool WebExtensionMatchPattern::schemeMatches(const WebExtensionMatchPattern& other, OptionSet<Options> options) const
{
    if (options.contains(Options::IgnoreSchemes))
        return true;

    if (pattern().matchesScheme(other.pattern()))
        return true;

    if (options.contains(Options::MatchBidirectionally) && other.pattern().matchesScheme(pattern()))
        return true;

    return false;
}

bool WebExtensionMatchPattern::hostMatches(const WebExtensionMatchPattern& other, OptionSet<Options> options) const
{
    if (pattern().matchesHost(other.pattern()))
        return true;

    if (options.contains(Options::MatchBidirectionally) && other.pattern().matchesHost(pattern()))
        return true;

    return false;
}

bool WebExtensionMatchPattern::pathMatches(const WebExtensionMatchPattern& other, OptionSet<Options> options) const
{
    if (options.contains(Options::IgnorePaths))
        return true;

    if (pattern().matchesPath(other.pattern()))
        return true;

    if (options.contains(Options::MatchBidirectionally) && other.pattern().matchesPath(pattern()))
        return true;

    return false;
}

HashSet<RefPtr<StringImpl>> toStrings(const WebExtensionMatchPattern::MatchPatternSet& matchPatterns)
{
    HashSet<RefPtr<StringImpl>> stringsToReturn;
    stringsToReturn.reserveInitialCapacity(matchPatterns.size());

    for (auto& pattern : matchPatterns)
        stringsToReturn.add(pattern.get().string());

    return stringsToReturn;
}

WebExtensionMatchPattern::MatchPatternSet toPatterns(const HashSet<RefPtr<StringImpl>>& domains)
{
    WebExtensionMatchPattern::MatchPatternSet matchPatterns;
    matchPatterns.reserveInitialCapacity(domains.size());

    for (auto& domain : domains)
        matchPatterns.add(*WebExtensionMatchPattern::getOrCreate(domain));

    return matchPatterns;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
