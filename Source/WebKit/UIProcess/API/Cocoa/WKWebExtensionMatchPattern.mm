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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WKWebExtensionMatchPatternInternal.h"

#import "APIError.h"
#import "WebExtensionMatchPattern.h"
#import <wtf/URLParser.h>

#if ENABLE(WK_WEB_EXTENSIONS)
static NSString * const stringCodingKey = @"string";
#endif

NSErrorDomain const WKWebExtensionMatchPatternErrorDomain = @"WKWebExtensionMatchPatternErrorDomain";

@implementation WKWebExtensionMatchPattern

+ (BOOL)supportsSecureCoding
{
    return YES;
}

#if ENABLE(WK_WEB_EXTENSIONS)

WK_OBJECT_DEALLOC_IMPL_ON_MAIN_THREAD(WKWebExtensionMatchPattern, WebExtensionMatchPattern, _webExtensionMatchPattern);

+ (void)registerCustomURLScheme:(NSString *)urlScheme
{
    NSParameterAssert([urlScheme isKindOfClass:NSString.class]);
    NSAssert1(WTF::URLParser::maybeCanonicalizeScheme(String(urlScheme)), @"Invalid parameter: '%@' is not a valid URL scheme", urlScheme);

    WebKit::WebExtensionMatchPattern::registerCustomURLScheme(urlScheme);
}

+ (instancetype)allURLsMatchPattern
{
    return WebKit::wrapper(WebKit::WebExtensionMatchPattern::allURLsMatchPattern()).autorelease();
}

+ (instancetype)allHostsAndSchemesMatchPattern
{
    return WebKit::wrapper(WebKit::WebExtensionMatchPattern::allHostsAndSchemesMatchPattern()).autorelease();
}

+ (instancetype)matchPatternWithString:(NSString *)patternString
{
    NSParameterAssert([patternString isKindOfClass:NSString.class]);

    return WebKit::wrapper(WebKit::WebExtensionMatchPattern::getOrCreate(patternString)).autorelease();
}

+ (instancetype)matchPatternWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path
{
    NSParameterAssert([scheme isKindOfClass:NSString.class]);
    NSParameterAssert([host isKindOfClass:NSString.class]);
    NSParameterAssert([path isKindOfClass:NSString.class]);

    return WebKit::wrapper(WebKit::WebExtensionMatchPattern::getOrCreate(scheme, host, path)).autorelease();
}

- (instancetype)initWithString:(NSString *)string error:(NSError **)error
{
    NSParameterAssert([string isKindOfClass:NSString.class]);

    if (!error) {
        // Balance the destructor in dealloc with the empty constructor.
        API::Object::constructInWrapper<WebKit::WebExtensionMatchPattern>(self);

        // Use the pattern cache instead for better performance, since an error isn't needed.
        return WebKit::wrapper(WebKit::WebExtensionMatchPattern::getOrCreate(string)).autorelease();
    }

    RefPtr<API::Error> internalError;
    API::Object::constructInWrapper<WebKit::WebExtensionMatchPattern>(self, string, internalError);

    if (error)
        *error = internalError ? static_cast<NSError *>(internalError->platformError()) : nil;

    return _webExtensionMatchPattern->isValid() ? self : nil;
}

- (instancetype)initWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path error:(NSError **)error
{
    NSParameterAssert([scheme isKindOfClass:NSString.class]);
    NSParameterAssert([host isKindOfClass:NSString.class]);
    NSParameterAssert([path isKindOfClass:NSString.class]);

    if (!error) {
        // Balance the destructor in dealloc with the empty constructor.
        API::Object::constructInWrapper<WebKit::WebExtensionMatchPattern>(self);

        // Use the pattern cache instead for better performance, since an error isn't needed.
        return WebKit::wrapper(WebKit::WebExtensionMatchPattern::getOrCreate(scheme, host, path)).autorelease();
    }

    RefPtr<API::Error> internalError;
    API::Object::constructInWrapper<WebKit::WebExtensionMatchPattern>(self, scheme, host, path, internalError);

    if (error)
        *error = internalError ? static_cast<NSError *>(internalError->platformError()) : nil;

    return _webExtensionMatchPattern->isValid() ? self : nil;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    NSParameterAssert([coder isKindOfClass:NSCoder.class]);

    return [self initWithString:[coder decodeObjectOfClass:[NSString class] forKey:stringCodingKey] error:nullptr];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    NSParameterAssert([coder isKindOfClass:NSCoder.class]);

    [coder encodeObject:self.string forKey:stringCodingKey];
}

- (instancetype)copyWithZone:(NSZone *)zone
{
    // WKWebExtensionMatchPattern is immutable.
    return self;
}

- (NSUInteger)hash
{
    return self.string.hash;
}

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    WKWebExtensionMatchPattern *other = dynamic_objc_cast<WKWebExtensionMatchPattern>(object);
    if (!other)
        return NO;

    return *_webExtensionMatchPattern == *other->_webExtensionMatchPattern;
}

- (NSString *)description
{
    return self.string;
}

- (NSString *)debugDescription
{
    if (self.matchesAllURLs)
        return [NSString stringWithFormat:@"<%@: %p; matchesAllURLs = YES>", NSStringFromClass(self.class), self];
    return [NSString stringWithFormat:@"<%@: %p; scheme = %@; host = %@; path = %@>", NSStringFromClass(self.class), self, self.scheme, self.host, self.path];
}

- (NSString *)scheme
{
    Ref pattern = *_webExtensionMatchPattern;
    if (pattern->scheme().isNull())
        return nil;
    return pattern->scheme().createNSString().autorelease();
}

- (NSString *)host
{
    Ref pattern = *_webExtensionMatchPattern;
    if (pattern->host().isNull())
        return nil;
    return pattern->host().createNSString().autorelease();
}

- (NSString *)path
{
    Ref pattern = *_webExtensionMatchPattern;
    if (pattern->path().isNull())
        return nil;
    return pattern->path().createNSString().autorelease();
}

- (NSString *)string
{
    return protect(*_webExtensionMatchPattern)->string().createNSString().autorelease();
}

- (BOOL)matchesAllURLs
{
    return _webExtensionMatchPattern->matchesAllURLs();
}

- (BOOL)matchesAllHosts
{
    return protect(*_webExtensionMatchPattern)->matchesAllHosts();
}

- (BOOL)matchesURL:(NSURL *)urlToMatch
{
    return [self matchesURL:urlToMatch options:0];
}

static OptionSet<WebKit::WebExtensionMatchPattern::Options> toImpl(WKWebExtensionMatchPatternOptions options)
{
    OptionSet<WebKit::WebExtensionMatchPattern::Options> result;

    if (options & WKWebExtensionMatchPatternOptionsIgnoreSchemes)
        result.add(WebKit::WebExtensionMatchPattern::Options::IgnoreSchemes);

    if (options & WKWebExtensionMatchPatternOptionsIgnorePaths)
        result.add(WebKit::WebExtensionMatchPattern::Options::IgnorePaths);

    if (options & WKWebExtensionMatchPatternOptionsMatchBidirectionally)
        result.add(WebKit::WebExtensionMatchPattern::Options::MatchBidirectionally);

    return result;
}

- (BOOL)matchesURL:(NSURL *)urlToMatch options:(WKWebExtensionMatchPatternOptions)options
{
    NSAssert(!(options & WKWebExtensionMatchPatternOptionsMatchBidirectionally), @"Invalid parameter: WKWebExtensionMatchPatternOptionsMatchBidirectionally is not valid when matching a URL");

    if (!urlToMatch)
        return NO;

    NSParameterAssert([urlToMatch isKindOfClass:NSURL.class]);

    return protect(*_webExtensionMatchPattern)->matchesURL(urlToMatch, toImpl(options));
}

- (BOOL)matchesPattern:(WKWebExtensionMatchPattern *)patternToMatch
{
    return [self matchesPattern:patternToMatch options:0];
}

- (BOOL)matchesPattern:(WKWebExtensionMatchPattern *)patternToMatch options:(WKWebExtensionMatchPatternOptions)options
{
    if (!patternToMatch)
        return NO;

    NSParameterAssert([patternToMatch isKindOfClass:WKWebExtensionMatchPattern.class]);

    return protect(*_webExtensionMatchPattern)->matchesPattern(protect(*patternToMatch->_webExtensionMatchPattern), toImpl(options));
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtensionMatchPattern;
}

- (WebKit::WebExtensionMatchPattern&)_webExtensionMatchPattern
{
    return *_webExtensionMatchPattern;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

+ (void)registerCustomURLScheme:(NSString *)urlScheme
{
}

+ (instancetype)allURLsMatchPattern
{
    return nil;
}

+ (instancetype)allHostsAndSchemesMatchPattern
{
    return nil;
}

+ (instancetype)matchPatternWithString:(NSString *)patternString
{
    return nil;
}

+ (instancetype)matchPatternWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path
{
    return nil;
}

- (instancetype)initWithString:(NSString *)string error:(NSError **)error
{
    if (error)
        *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
    return nil;
}

- (instancetype)initWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path error:(NSError **)error
{
    if (error)
        *error = [NSError errorWithDomain:NSCocoaErrorDomain code:NSFeatureUnsupportedError userInfo:nil];
    return nil;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    return [self initWithString:@"" error:nullptr];
}

- (id)copyWithZone:(NSZone *)zone
{
    return nil;
}

- (NSString *)scheme
{
    return nil;
}

- (NSString *)host
{
    return nil;
}

- (NSString *)path
{
    return nil;
}

- (NSString *)string
{
    return nil;
}

- (BOOL)matchesAllURLs
{
    return NO;
}

- (BOOL)matchesAllHosts
{
    return NO;
}

- (BOOL)matchesURL:(NSURL *)urlToMatch
{
    return NO;
}

- (BOOL)matchesURL:(NSURL *)urlToMatch options:(WKWebExtensionMatchPatternOptions)options
{
    return NO;
}

- (BOOL)matchesPattern:(WKWebExtensionMatchPattern *)patternToMatch
{
    return NO;
}

- (BOOL)matchesPattern:(WKWebExtensionMatchPattern *)patternToMatch options:(WKWebExtensionMatchPatternOptions)options
{
    return NO;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
