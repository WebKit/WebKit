/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionMatchPattern.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "_WKWebExtensionMatchPatternInternal.h"
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/URLParser.h>
#import <wtf/text/StringHash.h>

namespace WebKit {

using namespace WTF;
using namespace WebCore;

static constexpr ASCIILiteral allURLsPattern = "<all_urls>"_s;
static constexpr ASCIILiteral allHostsAndSchemesPattern = "*://*/*"_s;

WebExtensionMatchPattern::URLSchemeSet& WebExtensionMatchPattern::validSchemes()
{
    static MainThreadNeverDestroyed<URLSchemeSet> schemes = std::initializer_list<String> { "*"_s, "http"_s, "https"_s, "file"_s, "ftp"_s, "webkit-extension"_s };
    return schemes;
}

WebExtensionMatchPattern::URLSchemeSet& WebExtensionMatchPattern::supportedSchemes()
{
    static MainThreadNeverDestroyed<URLSchemeSet> schemes = std::initializer_list<String> { "*"_s, "http"_s, "https"_s, "webkit-extension"_s };
    return schemes;
}

static HashMap<String, RefPtr<WebExtensionMatchPattern>>& patternCache()
{
    static MainThreadNeverDestroyed<HashMap<String, RefPtr<WebExtensionMatchPattern>>> cache;
    return cache;
}

void WebExtensionMatchPattern::registerCustomURLScheme(String urlScheme)
{
    auto canonicalScheme = URLParser::maybeCanonicalizeScheme(String(urlScheme));
    ASSERT(canonicalScheme);

    validSchemes().addVoid(canonicalScheme.value());
    supportedSchemes().addVoid(canonicalScheme.value());
}

RefPtr<WebExtensionMatchPattern> WebExtensionMatchPattern::getOrCreate(const String& pattern)
{
    ASSERT(!pattern.isEmpty());

    return patternCache().ensure(pattern, [&] {
        return create(pattern);
    }).iterator->value;
}

RefPtr<WebExtensionMatchPattern> WebExtensionMatchPattern::getOrCreate(const String& scheme, const String& host, const String& path)
{
    String resolvedScheme = !scheme.isEmpty() ? scheme : "*"_s;
    String resolvedHost = !host.isEmpty() ? host : "*"_s;
    String resolvedPath = !path.isEmpty() ? path : "/*"_s;

    String pattern = makeString(resolvedScheme, "://"_s, resolvedHost, resolvedPath);

    return patternCache().ensure(pattern, [&] {
        return create(resolvedScheme, resolvedHost, resolvedPath);
    }).iterator->value;
}

Ref<WebExtensionMatchPattern> WebExtensionMatchPattern::allURLsMatchPattern()
{
    return getOrCreate(allURLsPattern).releaseNonNull();
}

Ref<WebExtensionMatchPattern> WebExtensionMatchPattern::allHostsAndSchemesMatchPattern()
{
    return getOrCreate(allHostsAndSchemesPattern).releaseNonNull();
}

bool WebExtensionMatchPattern::patternsMatchAllHosts(HashSet<Ref<WebExtensionMatchPattern>>& patterns)
{
    for (auto& pattern : patterns) {
        if (pattern->matchesAllHosts())
            return true;
    }

    return false;
}

static inline NSError *error(_WKWebExtensionMatchPatternError code, NSString *debugDescription)
{
    return [NSError errorWithDomain:_WKWebExtensionMatchPatternErrorDomain code:code userInfo:@{ NSDebugDescriptionErrorKey: debugDescription }];
}

WebExtensionMatchPattern::WebExtensionMatchPattern(const String& patternString, NSError **outError)
{
    ASSERT(!patternString.isNull());

    ASSERT(!m_valid);
    ASSERT(!m_hash);

    if (outError)
        *outError = nil;

    m_matchesAllURLs = patternString == allURLsPattern;
    if (m_matchesAllURLs) {
        m_valid = true;
        m_hash = patternString.hash();
        return;
    }

    UserContentURLPattern pattern { patternString };

    if (!pattern.scheme().isEmpty() && !isValidScheme(pattern.scheme())) {
        if (outError)
            *outError = error(_WKWebExtensionMatchPatternErrorInvalidScheme, [NSString stringWithFormat:@"\"%@\" cannot be parsed because the scheme \"%@\" is invalid.", (NSString *)patternString, (NSString *)pattern.scheme()]);
        return;
    }

    if (!pattern.isValid()) {
        switch (pattern.error()) {
        case UserContentURLPattern::Error::None:
        case UserContentURLPattern::Error::Invalid:
            ASSERT_NOT_REACHED();
            break;

        case UserContentURLPattern::Error::MissingScheme:
            if (outError)
                *outError = error(_WKWebExtensionMatchPatternErrorInvalidScheme, [NSString stringWithFormat:@"\"%@\" cannot be parsed because it doesn't have a scheme.", (NSString *)patternString]);
            break;

        case UserContentURLPattern::Error::MissingHost:
            if (outError)
                *outError = error(_WKWebExtensionMatchPatternErrorInvalidHost, [NSString stringWithFormat:@"\"%@\" cannot be parsed because it doesn't have a host.", (NSString *)patternString]);
            break;

        case UserContentURLPattern::Error::InvalidHost:
            if (outError)
                *outError = error(_WKWebExtensionMatchPatternErrorInvalidHost, [NSString stringWithFormat:@"\"%@\" cannot be parsed because the host \"%@\" is invalid.", (NSString *)patternString, (NSString *)pattern.host()]);
            break;

        case UserContentURLPattern::Error::MissingPath:
            if (outError)
                *outError = error(_WKWebExtensionMatchPatternErrorInvalidPath, [NSString stringWithFormat:@"\"%@\" cannot be parsed because it doesn't have a path.", (NSString *)patternString]);
            break;
        }

        return;
    }

    m_pattern = WTFMove(pattern);
    m_valid = true;
    m_hash = patternString.hash();
}

WebExtensionMatchPattern::WebExtensionMatchPattern(const String& scheme, const String& host, const String& path, NSError **outError)
{
    ASSERT(!scheme.isNull());
    ASSERT(!host.isNull());
    ASSERT(!path.isNull());

    ASSERT(!m_valid);
    ASSERT(!m_hash);
    ASSERT(!m_matchesAllURLs);

    if (outError)
        *outError = nil;

    if (!isValidScheme(scheme)) {
        if (outError)
            *outError = error(_WKWebExtensionMatchPatternErrorInvalidScheme, [NSString stringWithFormat:@"Scheme \"%@\" is invalid.", (NSString *)scheme]);
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
            if (outError)
                *outError = error(_WKWebExtensionMatchPatternErrorInvalidScheme, [NSString stringWithFormat:@"Scheme \"%@\" is invalid.", (NSString *)scheme]);
            break;

        case UserContentURLPattern::Error::MissingHost:
        case UserContentURLPattern::Error::InvalidHost:
            if (outError)
                *outError = error(_WKWebExtensionMatchPatternErrorInvalidHost, [NSString stringWithFormat:@"Host \"%@\" is invalid.", (NSString *)host]);
            break;

        case UserContentURLPattern::Error::MissingPath:
            if (outError)
                *outError = error(_WKWebExtensionMatchPatternErrorInvalidPath, [NSString stringWithFormat:@"Path \"%@\" is invalid.", (NSString *)path]);
            break;
        }

        return;
    }

    m_pattern = WTFMove(pattern);
    m_valid = true;
    m_hash = string().hash();
}

bool WebExtensionMatchPattern::isSupported() const
{
    return isValid() && (m_matchesAllURLs || supportedSchemes().contains(scheme()));
}

bool WebExtensionMatchPattern::operator==(const WebExtensionMatchPattern& other) const
{
    return this == &other || (m_valid == other.m_valid && m_matchesAllURLs == other.m_matchesAllURLs && m_hash == other.m_hash && m_pattern == other.m_pattern);
}

String WebExtensionMatchPattern::scheme() const
{
    if (!isValid() || matchesAllURLs())
        return nullString();
    return pattern().scheme();
}

String WebExtensionMatchPattern::host() const
{
    if (!isValid() || matchesAllURLs())
        return nullString();
    return pattern().originalHost();
}

String WebExtensionMatchPattern::path() const
{
    if (!isValid() || matchesAllURLs())
        return nullString();
    return pattern().path();
}

String WebExtensionMatchPattern::stringWithScheme(const String& differentScheme) const
{
    if (!isValid())
        return nullString();

    ASSERT(!m_matchesAllURLs || (m_matchesAllURLs && differentScheme.isEmpty()));

    if (m_matchesAllURLs)
        return allURLsPattern;
    return makeString(differentScheme.isEmpty() ? scheme() : differentScheme, "://"_s, host(), path());
}

NSArray *WebExtensionMatchPattern::expandedStrings() const
{
    if (!isValid())
        return @[ ];

    if (m_matchesAllURLs) {
        NSMutableArray<NSString *> *result = [NSMutableArray arrayWithCapacity:2];

        for (auto& scheme : supportedSchemes()) {
            if (scheme == "*"_s)
                continue;
            [result addObject:(NSString *)makeString(scheme, "://*/*"_s)];
        }

        return [result copy];
    }

    return @[ (NSString *)string() ];
}

bool WebExtensionMatchPattern::matchesAllHosts() const
{
    return isValid() && (m_matchesAllURLs || pattern().matchAllHosts());
}

bool WebExtensionMatchPattern::isValidScheme(String scheme)
{
    return !scheme.isEmpty() && validSchemes().contains(scheme);
}

bool WebExtensionMatchPattern::matchesURL(const URL& urlToMatch, OptionSet<Options> options) const
{
    ASSERT(!options.contains(Options::MatchBidirectionally));

    if (!isValid() || !urlToMatch.isValid())
        return false;

    if (m_matchesAllURLs)
        return supportedSchemes().contains(urlToMatch.protocol().toString());

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

    auto compareAllURLs = [](const WebExtensionMatchPattern& a, const WebExtensionMatchPattern& b) {
        return a.matchesAllURLs() && (b.matchesAllURLs() || supportedSchemes().contains(b.scheme()));
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

HashSet<String> toStrings(const MatchPatternSet& matchPatterns)
{
    HashSet<String> stringsToReturn;
    stringsToReturn.reserveInitialCapacity(matchPatterns.size());

    for (auto& pattern : matchPatterns)
        stringsToReturn.add(pattern.get().string());

    return stringsToReturn;
}

MatchPatternSet toPatterns(HashSet<String>& domains)
{
    MatchPatternSet matchPatterns;
    matchPatterns.reserveInitialCapacity(domains.size());

    for (auto& domain : domains)
        matchPatterns.add(*WebExtensionMatchPattern::getOrCreate(domain));

    return matchPatterns;
}

NSSet *toAPI(MatchPatternSet& set)
{
    NSMutableSet *result = [[NSMutableSet alloc] initWithCapacity:set.size()];
    for (auto& element : set)
        [result addObject:element->wrapper()];

    return [result copy];
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
