/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "_WKApplicationManifestInternal.h"

#import "CocoaColor.h"
#import <WebCore/ApplicationManifest.h>
#import <WebCore/ApplicationManifestParser.h>
#import <WebCore/Color.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/WebCoreObjCExtras.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

#if PLATFORM(MAC)
#import "AppKitSPI.h"
#endif

@implementation _WKApplicationManifest

#if ENABLE(APPLICATION_MANIFEST)

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder
{
    NSString *name = [aDecoder decodeObjectOfClass:[NSString class] forKey:@"name"];
    NSString *shortName = [aDecoder decodeObjectOfClass:[NSString class] forKey:@"short_name"];
    NSString *description = [aDecoder decodeObjectOfClass:[NSString class] forKey:@"description"];
    NSURL *scopeURL = [aDecoder decodeObjectOfClass:[NSURL class] forKey:@"scope"];
    NSInteger display = [aDecoder decodeIntegerForKey:@"display"];
    NSURL *startURL = [aDecoder decodeObjectOfClass:[NSURL class] forKey:@"start_url"];
    CocoaColor *themeColor = [aDecoder decodeObjectOfClass:[CocoaColor class] forKey:@"theme_color"];

    WebCore::ApplicationManifest coreApplicationManifest {
        WTF::String(name),
        WTF::String(shortName),
        WTF::String(description),
        URL(scopeURL),
        static_cast<WebCore::ApplicationManifest::Display>(display),
        URL(startURL),
        WebCore::Color(themeColor.CGColor),
    };

    API::Object::constructInWrapper<API::ApplicationManifest>(self, WTFMove(coreApplicationManifest));

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKApplicationManifest.class, self))
        return;

    _applicationManifest->~ApplicationManifest();

    [super dealloc];
}

- (void)encodeWithCoder:(NSCoder *)aCoder
{
    [aCoder encodeObject:self.name forKey:@"name"];
    [aCoder encodeObject:self.shortName forKey:@"short_name"];
    [aCoder encodeObject:self.applicationDescription forKey:@"description"];
    [aCoder encodeObject:self.scope forKey:@"scope"];
    [aCoder encodeInteger:static_cast<NSInteger>(_applicationManifest->applicationManifest().display) forKey:@"display"];
    [aCoder encodeObject:self.startURL forKey:@"start_url"];
    [aCoder encodeObject:self.themeColor forKey:@"theme_color"];
}

+ (_WKApplicationManifest *)applicationManifestFromJSON:(NSString *)json manifestURL:(NSURL *)manifestURL documentURL:(NSURL *)documentURL
{
    auto manifest = WebCore::ApplicationManifestParser::parse(WTF::String(json), URL(manifestURL), URL(documentURL));
    return wrapper(API::ApplicationManifest::create(manifest));
}

- (API::Object&)_apiObject
{
    return *_applicationManifest;
}

static NSString *nullableNSString(const WTF::String& string)
{
    return string.isNull() ? nil : (NSString *)string;
}

- (NSString *)name
{
    return nullableNSString(_applicationManifest->applicationManifest().name);
}

- (NSString *)shortName
{
    return nullableNSString(_applicationManifest->applicationManifest().shortName);
}

- (NSString *)applicationDescription
{
    return nullableNSString(_applicationManifest->applicationManifest().description);
}

- (NSURL *)scope
{
    return _applicationManifest->applicationManifest().scope;
}

- (NSURL *)startURL
{
    return _applicationManifest->applicationManifest().startURL;
}

- (CocoaColor *)themeColor
{
    return WebCore::platformColor(_applicationManifest->applicationManifest().themeColor);
}

- (_WKApplicationManifestDisplayMode)displayMode
{
    switch (_applicationManifest->applicationManifest().display) {
    case WebCore::ApplicationManifest::Display::Browser:
        return _WKApplicationManifestDisplayModeBrowser;
    case WebCore::ApplicationManifest::Display::MinimalUI:
        return _WKApplicationManifestDisplayModeMinimalUI;
    case WebCore::ApplicationManifest::Display::Standalone:
        return _WKApplicationManifestDisplayModeStandalone;
    case WebCore::ApplicationManifest::Display::Fullscreen:
        return _WKApplicationManifestDisplayModeFullScreen;
    }

    ASSERT_NOT_REACHED();
}

#else // ENABLE(APPLICATION_MANIFEST)

+ (_WKApplicationManifest *)applicationManifestFromJSON:(NSString *)json manifestURL:(NSURL *)manifestURL documentURL:(NSURL *)documentURL
{
    UNUSED_PARAM(json);
    UNUSED_PARAM(manifestURL);
    UNUSED_PARAM(documentURL);
    RELEASE_ASSERT_NOT_REACHED();
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder
{
    UNUSED_PARAM(aDecoder);
    [self release];
    return nil;
}

- (void)encodeWithCoder:(NSCoder *)aCoder
{
    UNUSED_PARAM(aCoder);
}

- (NSString *)name
{
    return nil;
}

- (NSString *)shortName
{
    return nil;
}

- (NSString *)applicationDescription
{
    return nil;
}

- (NSURL *)scope
{
    return nil;
}

- (NSURL *)startURL
{
    return nil;
}

- (CocoaColor *)themeColor
{
    return nil;
}

- (_WKApplicationManifestDisplayMode)displayMode
{
    return _WKApplicationManifestDisplayModeBrowser;
}

#endif // ENABLE(APPLICATION_MANIFEST)

@end
