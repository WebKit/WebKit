/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#import <WebCore/ApplicationManifest.h>
#import <WebCore/ApplicationManifestParser.h>
#import <WebCore/Color.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

#if PLATFORM(MAC)
#import "AppKitSPI.h"
#endif

static OptionSet<WebCore::ApplicationManifest::Icon::Purpose> fromPurposes(NSArray<NSNumber *> *purposes)
{
    OptionSet<WebCore::ApplicationManifest::Icon::Purpose> purposeSet;
    for (NSNumber *purposeNumber in purposes) {
        auto purpose = static_cast<WebCore::ApplicationManifest::Icon::Purpose>(purposeNumber.integerValue);
        purposeSet.add(purpose);
    }

    return purposeSet;
}

static RetainPtr<NSArray<NSNumber *>> fromPurposes(OptionSet<WebCore::ApplicationManifest::Icon::Purpose> purposes)
{
    auto purposeArray = adoptNS([[NSMutableArray alloc] init]);
    for (auto purpose : purposes)
        [purposeArray addObject:[NSNumber numberWithUnsignedChar:static_cast<std::underlying_type<WebCore::ApplicationManifest::Icon::Purpose>::type>(purpose)]];
    return purposeArray;
}

static std::optional<WebCore::ApplicationManifest::Icon> makeVectorElement(const WebCore::ApplicationManifest::Icon*, id arrayElement)
{
    if (![arrayElement isKindOfClass: _WKApplicationManifestIcon.class])
        return std::nullopt;

    auto icon = dynamic_objc_cast<_WKApplicationManifestIcon>(arrayElement);
    if (!icon)
        return std::nullopt;

    return WebCore::ApplicationManifest::Icon {
        icon.src,
        makeVector<String>(icon.sizes),
        icon.type,
        fromPurposes(icon.purposes)
    };
}

@implementation _WKApplicationManifestIcon

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [self init]))
        return nil;

    _src = [[coder decodeObjectOfClass:[NSURL class] forKey:@"src"] copy];
    _sizes = [[coder decodeObjectOfClasses:[NSSet setWithArray:@[[NSArray class], [NSString class]]] forKey:@"sizes"] copy];
    _type = [[coder decodeObjectOfClass:[NSString class] forKey:@"type"] copy];
    _purposes = [[coder decodeObjectOfClasses:[NSSet setWithArray:@[[NSArray class], [NSNumber class]]] forKey:@"purposes"] copy];

    return self;
}

- (instancetype)initWithCoreIcon:(const WebCore::ApplicationManifest::Icon *)icon
{
    if (!(self = [[_WKApplicationManifestIcon alloc] init]))
        return nil;

    if (icon) {
        _src = [icon->src copy];
        _sizes = createNSArray(icon->sizes, [] (auto& size) -> NSString * {
            return size;
        }).leakRef();
        _type = [icon->type copy];
        _purposes = fromPurposes(icon->purposes).leakRef();
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:_src forKey:@"src"];
    [coder encodeObject:_sizes forKey:@"sizes"];
    [coder encodeObject:_type forKey:@"type"];
    [coder encodeObject:_purposes forKey:@"purposes"];
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKApplicationManifestIcon.class, self))
        return;

    [_src release];
    [_sizes release];
    [_type release];
    [_purposes release];

    [super dealloc];
}

@end

    
@implementation _WKApplicationManifest

#if ENABLE(APPLICATION_MANIFEST)

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder
{
    String name = [aDecoder decodeObjectOfClass:[NSString class] forKey:@"name"];
    String shortName = [aDecoder decodeObjectOfClass:[NSString class] forKey:@"short_name"];
    String description = [aDecoder decodeObjectOfClass:[NSString class] forKey:@"description"];
    URL scopeURL = [aDecoder decodeObjectOfClass:[NSURL class] forKey:@"scope"];
    NSInteger display = [aDecoder decodeIntegerForKey:@"display"];
    URL startURL = [aDecoder decodeObjectOfClass:[NSURL class] forKey:@"start_url"];
    URL manifestId = [aDecoder decodeObjectOfClass:[NSURL class] forKey:@"manifestId"];
    WebCore::CocoaColor *themeColor = [aDecoder decodeObjectOfClass:[WebCore::CocoaColor class] forKey:@"theme_color"];
    NSArray<_WKApplicationManifestIcon *> *icons = [aDecoder decodeObjectOfClasses:[NSSet setWithArray:@[[NSArray class], [_WKApplicationManifestIcon class]]] forKey:@"icons"];

    WebCore::ApplicationManifest coreApplicationManifest {
        WTFMove(name),
        WTFMove(shortName),
        WTFMove(description),
        WTFMove(scopeURL),
        static_cast<WebCore::ApplicationManifest::Display>(display),
        WTFMove(startURL),
        WTFMove(manifestId),
        WebCore::roundAndClampToSRGBALossy(themeColor.CGColor),
        makeVector<WebCore::ApplicationManifest::Icon>(icons),
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
    [aCoder encodeObject:self.manifestId forKey:@"manifestId"];
    [aCoder encodeObject:self.themeColor forKey:@"theme_color"];
    [aCoder encodeObject:self.icons forKey:@"icons"];
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

- (WebCore::CocoaColor *)themeColor
{
    return cocoaColor(_applicationManifest->applicationManifest().themeColor).autorelease();
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

- (NSArray<_WKApplicationManifestIcon *> *)icons
{
    return createNSArray(_applicationManifest->applicationManifest().icons, [] (auto& coreIcon) -> id {
        return adoptNS([[_WKApplicationManifestIcon alloc] initWithCoreIcon:&coreIcon]).autorelease();
    }).autorelease();
}

- (NSURL *)manifestId
{
    return _applicationManifest->applicationManifest().id;
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

- (WebCore::CocoaColor *)themeColor
{
    return nil;
}

- (_WKApplicationManifestDisplayMode)displayMode
{
    return _WKApplicationManifestDisplayModeBrowser;
}

- (NSArray<_WKApplicationManifestIcon *> *)icons
{
    return nil;
}

- (NSURL *)manifestId
{
    return nil;
}

#endif // ENABLE(APPLICATION_MANIFEST)

@end
