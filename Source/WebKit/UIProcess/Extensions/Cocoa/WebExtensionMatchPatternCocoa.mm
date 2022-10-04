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

#import "CocoaHelpers.h"
#import "_WKWebExtensionMatchPatternInternal.h"
#import <wtf/HashMap.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/text/StringHash.h>

namespace WebKit {

static NSString * const allURLsPattern = @"<all_urls>";
static NSString * const allHostsAndSchemesPattern = @"*://*/*";
static NSString * const patternFormat = @"%@://%@%@";

NSSet *WebExtensionMatchPattern::validSchemes()
{
    // FIXME: Add extension schemes to this set.
    static NSSet<NSString *> *schemes = [NSSet setWithObjects:@"*", @"http", @"https", @"file", @"ftp", nil];
    return schemes;
}

NSSet *WebExtensionMatchPattern::supportedSchemes()
{
    // FIXME: Add extension schemes to this set.
    static NSSet<NSString *> *schemes = [NSSet setWithObjects:@"*", @"http", @"https", nil];
    ASSERT([schemes isSubsetOfSet:validSchemes()]);
    return schemes;
}

static HashMap<String, RefPtr<WebExtensionMatchPattern>>& patternCache()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashMap<String, RefPtr<WebExtensionMatchPattern>>> cache;
    return cache;
}

RefPtr<WebExtensionMatchPattern> WebExtensionMatchPattern::getOrCreate(NSString *pattern)
{
    ASSERT(pattern);

    return patternCache().ensure(pattern, [&] {
        return create(pattern);
    }).iterator->value;
}

RefPtr<WebExtensionMatchPattern> WebExtensionMatchPattern::getOrCreate(NSString *scheme, NSString *host, NSString *path)
{
    ASSERT(scheme);
    ASSERT(host);
    ASSERT(path);

    NSString *pattern = [NSString stringWithFormat:patternFormat, scheme, host, path];

    return patternCache().ensure(pattern, [&] {
        return create(scheme, host, path);
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

WebExtensionMatchPattern::WebExtensionMatchPattern(NSString *pattern, NSError **outError)
{
    ASSERT(pattern);

    m_valid = parse(pattern, outError);
    m_hash = string().hash;
}

static inline NSError *error(_WKWebExtensionMatchPatternError code, NSString *debugDescription)
{
    return [NSError errorWithDomain:_WKWebExtensionMatchPatternErrorDomain code:code userInfo:@{ NSDebugDescriptionErrorKey: debugDescription }];
}

WebExtensionMatchPattern::WebExtensionMatchPattern(NSString *scheme, NSString *host, NSString *path, NSError **outError)
    : m_scheme(scheme)
    , m_host(host)
    , m_path(path)
{
    ASSERT(scheme);
    ASSERT(host);
    ASSERT(path);
    ASSERT(!m_matchesAllURLs);

    if (outError)
        *outError = nil;

    m_valid = true;

    auto markInvalid = [&](NSError *error) {
        if (outError)
            *outError = error;

        m_valid = false;
        m_scheme = nil;
        m_host = nil;
        m_path = nil;
        m_hash = 0;
    };

    if (!isValidScheme(scheme)) {
        markInvalid(error(_WKWebExtensionMatchPatternErrorInvalidScheme, [NSString stringWithFormat:@"Scheme \"%@\" is invalid.", scheme]));
        return;
    }

    if (!isValidHost(host)) {
        markInvalid(error(_WKWebExtensionMatchPatternErrorInvalidHost, [NSString stringWithFormat:@"Host \"%@\" is invalid.", host]));
        return;
    }

    if (!isValidPath(path)) {
        markInvalid(error(_WKWebExtensionMatchPatternErrorInvalidPath, [NSString stringWithFormat:@"Path \"%@\" is invalid.", path]));
        return;
    }

    m_hash = string().hash;
}

bool WebExtensionMatchPattern::isSupported() const
{
    return isValid() && (m_matchesAllURLs || [supportedSchemes() containsObject:m_scheme.get()]);
}

bool WebExtensionMatchPattern::operator==(const WebExtensionMatchPattern& other) const
{
    if (this == &other)
        return true;

    if (!isValid() && !other.isValid()) {
        ASSERT(!m_hash && !other.m_hash);
        return true;
    }

    if (m_matchesAllURLs && other.m_matchesAllURLs)
        return true;

    if (m_matchesAllURLs || other.m_matchesAllURLs)
        return false;

    if (![m_host isEqualToString:other.m_host.get()])
        return false;

    if (![m_scheme isEqualToString:other.m_scheme.get()])
        return false;

    if (![m_path isEqualToString:other.m_path.get()])
        return false;

    ASSERT(m_hash == other.m_hash);

    return true;
}

NSString *WebExtensionMatchPattern::stringWithScheme(NSString *differentScheme) const
{
    if (!isValid())
        return nil;

    ASSERT(!m_matchesAllURLs || (m_matchesAllURLs && !differentScheme));
    return m_matchesAllURLs ? allURLsPattern : [NSString stringWithFormat:patternFormat, differentScheme ?: m_scheme.get(), m_host.get(), m_path.get()];
}

NSArray *WebExtensionMatchPattern::expandedStrings() const
{
    if (!isValid())
        return @[ ];

    if (m_matchesAllURLs) {
        NSMutableArray<NSString *> *result = [NSMutableArray arrayWithCapacity:2];
        for (NSString *scheme in supportedSchemes()) {
            if ([scheme isEqualToString:@"*"])
                continue;
            [result addObject:[NSString stringWithFormat:patternFormat, scheme, @"*", @"/*"]];
        }

        return result;
    }

    if ([m_scheme isEqualToString:@"*"])
        return @[ stringWithScheme(@"http"), stringWithScheme(@"https") ];

    return @[ string() ];
}

bool WebExtensionMatchPattern::matchesAllHosts() const
{
    return isValid() && (m_matchesAllURLs || [m_host isEqualToString:@"*"]);
}

bool WebExtensionMatchPattern::isValidScheme(NSString *scheme)
{
    return [validSchemes() containsObject:scheme];
}

bool WebExtensionMatchPattern::isValidHost(NSString *host)
{
    return [host isEqualToString:@"*"] || ![host containsString:@"*"] || ([host hasPrefix:@"*."] && host.length > 2 && ![[host substringFromIndex:2] containsString:@"*"]);
}

bool WebExtensionMatchPattern::isValidPath(NSString *path)
{
    return [path hasPrefix:@"/"];
}

bool WebExtensionMatchPattern::parse(NSString *pattern, NSError **outError)
{
    // Documentation: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Match_patterns

    // <url-pattern> := <scheme>://<host><path>
    // <scheme> := '*' | 'http' | 'https' | 'file' | 'ftp'
    // <host> := '*' | '*.' <any char except '/' and '*'>+
    // <path> := '/' <any chars>

    if (outError)
        *outError = nil;

    m_matchesAllURLs = [pattern isEqualToString:allURLsPattern];
    if (m_matchesAllURLs)
        return true;

    NSRange schemeDelimiterRange = [pattern rangeOfString:@"://"];
    if (schemeDelimiterRange.location == NSNotFound || !schemeDelimiterRange.location) {
        if (outError)
            *outError = error(_WKWebExtensionMatchPatternErrorInvalidScheme, [NSString stringWithFormat:@"\"%@\" cannot be parsed because it doesn't have a scheme.", pattern]);
        return false;
    }

    NSString *scheme = [pattern substringToIndex:schemeDelimiterRange.location];
    if (!isValidScheme(scheme)) {
        if (outError)
            *outError = error(_WKWebExtensionMatchPatternErrorInvalidScheme, [NSString stringWithFormat:@"\"%@\" cannot be parsed because the scheme \"%@\" is invalid.", pattern, scheme]);
        return false;
    }

    NSString *hostAndPath = [pattern substringFromIndex:(schemeDelimiterRange.location + schemeDelimiterRange.length)];
    NSRange hostAndPathDelimitationRange = [hostAndPath rangeOfString:@"/"];
    if (hostAndPathDelimitationRange.location == NSNotFound) {
        if (outError)
            *outError = error(_WKWebExtensionMatchPatternErrorInvalidPath, [NSString stringWithFormat:@"\"%@\" cannot be parsed because it doesn't have a path.", pattern]);
        return false;
    }

    // Host is only required for non-file URL patterns.
    if (![scheme isEqualToString:@"file"] && !hostAndPathDelimitationRange.location) {
        if (outError)
            *outError = error(_WKWebExtensionMatchPatternErrorInvalidHost, [NSString stringWithFormat:@"\"%@\" cannot be parsed because it doesn't have a host.", pattern]);
        return false;
    }

    NSString *host = [hostAndPath substringToIndex:hostAndPathDelimitationRange.location];
    if (!isValidHost(host)) {
        if (outError)
            *outError = error(_WKWebExtensionMatchPatternErrorInvalidHost, [NSString stringWithFormat:@"\"%@\" cannot be parsed because the host \"%@\" is invalid.", pattern, host]);
        return false;
    }

    m_scheme = scheme;
    m_host = host;
    m_path = [hostAndPath substringFromIndex:hostAndPathDelimitationRange.location];

    return true;
}

bool WebExtensionMatchPattern::matchesURL(NSURL *urlToMatch, OptionSet<Options> options)
{
    if (!isValid() || !urlToMatch || !urlToMatch.scheme || !urlToMatch.path)
        return false;

    ASSERT(!options.contains(Options::MatchBidirectionally));

    if (m_matchesAllURLs)
        return [supportedSchemes() containsObject:urlToMatch.scheme];

    // If this is a file URL, the host can be nil. Pass empty string instead of nil to match our non-nil expectations.
    return schemeMatches(urlToMatch.scheme, options) && hostMatches(urlToMatch.host ?: @"", options) && pathMatches(urlToMatch.path, options);
}

bool WebExtensionMatchPattern::matchesPattern(const WebExtensionMatchPattern& patternToMatch, OptionSet<Options> options)
{
    if (!isValid())
        return false;

    if (*this == patternToMatch)
        return true;

    auto compareAllURLs = ^(const WebExtensionMatchPattern& a, const WebExtensionMatchPattern& b) {
        return a.matchesAllURLs() && (b.matchesAllURLs() || [supportedSchemes() containsObject:b.scheme()]);
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

    return schemeMatches(patternToMatch.scheme(), options) && hostMatches(patternToMatch.host(), options) && pathMatches(patternToMatch.path(), options);
}

bool WebExtensionMatchPattern::schemeMatches(NSString *schemeToMatch, OptionSet<Options> options)
{
    ASSERT(m_scheme);
    ASSERT(schemeToMatch);

    if (options.contains(Options::IgnoreSchemes))
        return true;

    if ([m_scheme isEqualToString:schemeToMatch])
        return true;

    auto compare = ^(NSString *a, NSString *b) {
        // If the scheme is *, then it matches either http or https, and not file, or ftp.
        return [a isEqualToString:@"*"] && [b hasPrefix:@"http"];
    };

    if (compare(m_scheme.get(), schemeToMatch))
        return true;

    if (options.contains(Options::MatchBidirectionally) && compare(schemeToMatch, m_scheme.get()))
        return true;

    return false;
}

bool WebExtensionMatchPattern::hostMatches(NSString *hostToMatch, OptionSet<Options> options)
{
    ASSERT(m_host);
    ASSERT(hostToMatch);

    if ([m_host isEqualToString:hostToMatch])
        return true;

    auto compare = ^(NSString *a, NSString *b) {
        // If the host is just *, then it matches any host.
        if ([a isEqualToString:@"*"])
            return true;

        if ([a hasPrefix:@"*."]) {
            // Matches "example.com" when the pattern is "*.example.com".
            NSString *domainSuffix = [a substringFromIndex:2];
            if ([b isEqualToString:domainSuffix])
                return true;

            // Matches "www.example.com" when the pattern is "*.example.com".
            // Does not match "the-example.com" or "www.the-example.com".
            domainSuffix = [a substringFromIndex:1];
            if ([b hasSuffix:domainSuffix])
                return true;
        }

        return false;
    };

    if (compare(m_host.get(), hostToMatch))
        return true;

    if (options.contains(Options::MatchBidirectionally) && compare(hostToMatch, m_host.get()))
        return true;

    return false;
}

bool WebExtensionMatchPattern::pathMatches(NSString *pathToMatch, OptionSet<Options> options)
{
    ASSERT(m_path);
    ASSERT(pathToMatch);

    if (options.contains(Options::IgnorePaths))
        return true;

    if ([m_path isEqualToString:pathToMatch])
        return true;

    auto compare = ^(NSString *a, NSString *b) {
        // Special case matches for any path to avoid the cost of NSRegularExpression.
        if ([a isEqualToString:@"/*"])
            return true;

        // If this is not a wildcard pattern, the paths should have been equal in the check above.
        if (![a containsString:@"*"])
            return false;

        // In the path section, each '*' matches 0 or more characters.
        NSString *regExpString = [NSString stringWithFormat:@"^%@$", [escapeCharactersInString(a, @"?+[(){}$|\\.") stringByReplacingOccurrencesOfString:@"*" withString:@".*"]];

        NSError *error;
        NSRegularExpression *regExp = [NSRegularExpression regularExpressionWithPattern:regExpString options:0 error:&error];
        if (error)
            return false;

        if ([regExp firstMatchInString:b options:0 range:NSMakeRange(0, b.length)])
            return true;
        return false;
    };

    if (compare(m_path.get(), pathToMatch))
        return true;

    if (options.contains(Options::MatchBidirectionally) && compare(pathToMatch, m_path.get()))
        return true;

    return false;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
