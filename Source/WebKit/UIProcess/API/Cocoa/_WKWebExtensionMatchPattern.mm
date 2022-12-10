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
#import "_WKWebExtensionMatchPatternInternal.h"

#import "WebExtensionMatchPattern.h"
#import <WebCore/WebCoreObjCExtras.h>

static NSString * const stringCodingKey = @"string";

NSErrorDomain const _WKWebExtensionMatchPatternErrorDomain = @"_WKWebExtensionMatchPatternErrorDomain";

@implementation _WKWebExtensionMatchPattern

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    NSParameterAssert(coder);

    return [self initWithString:[coder decodeObjectOfClass:[NSString class] forKey:stringCodingKey] error:nullptr];
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    NSParameterAssert(coder);

    [coder encodeObject:self.string forKey:stringCodingKey];
}

- (instancetype)copyWithZone:(NSZone *)zone
{
    // _WKWebExtensionMatchPattern is immutable.
    return self;
}

#if ENABLE(WK_WEB_EXTENSIONS)

+ (instancetype)allURLsMatchPattern
{
    return WebKit::wrapper(WebKit::WebExtensionMatchPattern::allURLsMatchPattern());
}

+ (instancetype)allHostsAndSchemesMatchPattern
{
    return WebKit::wrapper(WebKit::WebExtensionMatchPattern::allHostsAndSchemesMatchPattern());
}

+ (instancetype)matchPatternWithString:(NSString *)patternString
{
    NSParameterAssert(patternString);

    return WebKit::wrapper(WebKit::WebExtensionMatchPattern::getOrCreate(patternString));
}

+ (instancetype)matchPatternWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path
{
    NSParameterAssert(scheme);
    NSParameterAssert(host);
    NSParameterAssert(path);

    return WebKit::wrapper(WebKit::WebExtensionMatchPattern::getOrCreate(scheme, host, path));
}

- (instancetype)initWithString:(NSString *)string error:(NSError **)error
{
    NSParameterAssert(string);

    if (!error) {
        // Balance the destructor in dealloc with the empty constructor.
        API::Object::constructInWrapper<WebKit::WebExtensionMatchPattern>(self);

        // Use the pattern cache instead for better performance, since an error isn't needed.
        return WebKit::wrapper(WebKit::WebExtensionMatchPattern::getOrCreate(string));
    }

    API::Object::constructInWrapper<WebKit::WebExtensionMatchPattern>(self, string, error);

    return _webExtensionMatchPattern->isValid() ? self : nil;
}

- (instancetype)initWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path error:(NSError **)error
{
    NSParameterAssert(scheme);
    NSParameterAssert(host);
    NSParameterAssert(path);

    if (!error) {
        // Balance the destructor in dealloc with the empty constructor.
        API::Object::constructInWrapper<WebKit::WebExtensionMatchPattern>(self);

        // Use the pattern cache instead for better performance, since an error isn't needed.
        return WebKit::wrapper(WebKit::WebExtensionMatchPattern::getOrCreate(scheme, host, path));
    }

    API::Object::constructInWrapper<WebKit::WebExtensionMatchPattern>(self, scheme, host, path, error);

    return _webExtensionMatchPattern->isValid() ? self : nil;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKWebExtensionMatchPattern.class, self))
        return;

    _webExtensionMatchPattern->~WebExtensionMatchPattern();
}

- (NSUInteger)hash
{
    return self.string.hash;
}

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    _WKWebExtensionMatchPattern *other = dynamic_objc_cast<_WKWebExtensionMatchPattern>(object);
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
    return _webExtensionMatchPattern->scheme();
}

- (NSString *)host
{
    return _webExtensionMatchPattern->host();
}

- (NSString *)path
{
    return _webExtensionMatchPattern->path();
}

- (NSString *)string
{
    return _webExtensionMatchPattern->string();
}

- (BOOL)matchesAllURLs
{
    return _webExtensionMatchPattern->matchesAllURLs();
}

- (BOOL)matchesAllHosts
{
    return _webExtensionMatchPattern->matchesAllHosts();
}

- (BOOL)matchesURL:(NSURL *)urlToMatch
{
    return [self matchesURL:urlToMatch options:0];
}

static OptionSet<WebKit::WebExtensionMatchPattern::Options> toImpl(_WKWebExtensionMatchPatternOptions options)
{
    OptionSet<WebKit::WebExtensionMatchPattern::Options> result;

    if (options & _WKWebExtensionMatchPatternOptionsIgnoreSchemes)
        result.add(WebKit::WebExtensionMatchPattern::Options::IgnoreSchemes);

    if (options & _WKWebExtensionMatchPatternOptionsIgnorePaths)
        result.add(WebKit::WebExtensionMatchPattern::Options::IgnorePaths);

    if (options & _WKWebExtensionMatchPatternOptionsMatchBidirectionally)
        result.add(WebKit::WebExtensionMatchPattern::Options::MatchBidirectionally);

    return result;
}

- (BOOL)matchesURL:(NSURL *)urlToMatch options:(_WKWebExtensionMatchPatternOptions)options
{
    return _webExtensionMatchPattern->matchesURL(urlToMatch, toImpl(options));
}

- (BOOL)matchesPattern:(_WKWebExtensionMatchPattern *)patternToMatch
{
    return [self matchesPattern:patternToMatch options:0];
}

- (BOOL)matchesPattern:(_WKWebExtensionMatchPattern *)patternToMatch options:(_WKWebExtensionMatchPatternOptions)options
{
    if (!patternToMatch)
        return NO;
    return _webExtensionMatchPattern->matchesPattern(patternToMatch._webExtensionMatchPattern, toImpl(options));
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
    return nil;
}

- (instancetype)initWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path error:(NSError **)error
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

- (BOOL)matchesURL:(NSURL *)urlToMatch options:(_WKWebExtensionMatchPatternOptions)options
{
    return NO;
}

- (BOOL)matchesPattern:(_WKWebExtensionMatchPattern *)patternToMatch
{
    return NO;
}

- (BOOL)matchesPattern:(_WKWebExtensionMatchPattern *)patternToMatch options:(_WKWebExtensionMatchPatternOptions)options
{
    return NO;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
