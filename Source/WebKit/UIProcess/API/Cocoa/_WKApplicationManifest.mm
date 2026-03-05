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
#import <wtf/RetainPtr.h>
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

static std::optional<WebCore::ApplicationManifest::Shortcut> makeVectorElement(const WebCore::ApplicationManifest::Shortcut*, id arrayElement)
{
    auto shortcut = dynamic_objc_cast<_WKApplicationManifestShortcut>(arrayElement);
    if (!shortcut)
        return std::nullopt;

    return WebCore::ApplicationManifest::Shortcut {
        shortcut.name,
        shortcut.url,
        makeVector<WebCore::ApplicationManifest::Icon>(shortcut.icons)
    };
}

@implementation _WKApplicationManifestIcon {
    RetainPtr<NSURL> _src;
    RetainPtr<NSArray<NSString *>> _sizes;
    RetainPtr<NSString> _type;
    RetainPtr<NSArray<NSNumber *>> _purposes;
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [self init]))
        return nil;

    _src = adoptNS([[coder decodeObjectOfClass:[NSURL class] forKey:@"src"] copy]);
    _sizes = adoptNS([[coder decodeObjectOfClasses:[NSSet setWithArray:@[[NSArray class], [NSString class]]] forKey:@"sizes"] copy]);
    _type = adoptNS([[coder decodeObjectOfClass:[NSString class] forKey:@"type"] copy]);
    _purposes = adoptNS([[coder decodeObjectOfClasses:[NSSet setWithArray:@[[NSArray class], [NSNumber class]]] forKey:@"purposes"] copy]);

    return self;
}

- (instancetype)initWithCoreIcon:(const WebCore::ApplicationManifest::Icon *)icon
{
    if (!(self = [super init]))
        return nil;

    if (icon) {
        _src = icon->src.createNSURL();
        _sizes = createNSArray(icon->sizes, [] (auto& size) {
            return size.createNSString();
        });
        _type = icon->type.createNSString();
        _purposes = fromPurposes(icon->purposes);
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:_src.get() forKey:@"src"];
    [coder encodeObject:_sizes.get() forKey:@"sizes"];
    [coder encodeObject:_type.get() forKey:@"type"];
    [coder encodeObject:_purposes.get() forKey:@"purposes"];
}

- (NSURL *)src
{
    return _src.get();
}

- (NSArray<NSString *> *)sizes
{
    return _sizes.get();
}

- (NSString *)type
{
    return _type.get();
}

- (NSArray<NSNumber *> *)purposes
{
    return _purposes.get();
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKApplicationManifestIcon.class, self))
        return;

    [super dealloc];
}

@end

@implementation _WKApplicationManifestShortcut {
    RetainPtr<NSString> _name;
    RetainPtr<NSURL> _url;
    RetainPtr<NSArray<_WKApplicationManifestIcon *>> _icons;
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    if (!(self = [self init]))
        return nil;

    _name = adoptNS([[coder decodeObjectOfClass:[NSString class] forKey:@"name"] copy]);
    _url = adoptNS([[coder decodeObjectOfClass:[NSURL class] forKey:@"url"] copy]);
    _icons = adoptNS([[coder decodeObjectOfClasses:[NSSet setWithArray:@[[NSArray class], [_WKApplicationManifestIcon class], [NSString class]]] forKey:@"icons"] copy]);

    return self;
}

- (NSString *)name
{
    return _name.get();
}

- (NSURL *)url
{
    return _url.get();
}

- (NSArray<_WKApplicationManifestIcon *> *)icons
{
    return _icons.get();
}

- (instancetype)initWithCoreShortcut:(const WebCore::ApplicationManifest::Shortcut *)shortcut
{
    if (!(self = [super init]))
        return nil;

    if (shortcut) {
        _name = shortcut->name.createNSString();
        _url = shortcut->url.createNSURL();
        _icons = createNSArray(shortcut->icons, [] (auto& icon) {
            return adoptNS([[_WKApplicationManifestIcon alloc] initWithCoreIcon:&icon]);
        });
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:_name.get() forKey:@"name"];
    [coder encodeObject:_url.get() forKey:@"url"];
    [coder encodeObject:_icons.get() forKey:@"icons"];
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKApplicationManifestShortcut.class, self))
        return;

    [super dealloc];
}

@end

@implementation _WKApplicationManifest

- (instancetype)initWithJSONData:(NSData *)jsonData manifestURL:(NSURL *)manifestURL documentURL:(NSURL *)documentURL
{
    if (!(self = [super init]))
        return nil;

    RetainPtr jsonString = adoptNS([[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding]);
    if (!jsonString)
        return nil;

    auto manifest = WebCore::ApplicationManifestParser::parseWithValidation(jsonString.get(), URL(manifestURL), URL(documentURL));
    if (!manifest)
        return nil;

    API::Object::constructInWrapper<API::ApplicationManifest>(self, *manifest);

    return self;
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder
{
    String rawJSON = [aDecoder decodeObjectOfClass:[NSString class] forKey:@"raw_json"];
    NSInteger dir = [aDecoder decodeIntegerForKey:@"dir"];
    // FIXME: <https://webkit.org/b/278619> Remove this assert after further manifest IPC hardening.
    RELEASE_ASSERT(dir >= 0 && dir <= 2);
    String lang = [aDecoder decodeObjectOfClass:[NSString class] forKey:@"lang"];
    String name = [aDecoder decodeObjectOfClass:[NSString class] forKey:@"name"];
    String shortName = [aDecoder decodeObjectOfClass:[NSString class] forKey:@"short_name"];
    String description = [aDecoder decodeObjectOfClass:[NSString class] forKey:@"description"];
    URL scopeURL = [aDecoder decodeObjectOfClass:[NSURL class] forKey:@"scope"];
    bool isDefaultScope = [aDecoder decodeBoolForKey:@"is_default_scope"];
    NSInteger display = [aDecoder decodeIntegerForKey:@"display"];
    NSInteger orientation = [aDecoder decodeIntegerForKey:@"orientation"];
    std::optional<WebCore::ScreenOrientationLockType> orientationValue = std::nullopt;
    if (orientation != NSNotFound)
        orientationValue = static_cast<WebCore::ScreenOrientationLockType>(orientation);
    URL manifestURL = [aDecoder decodeObjectOfClass:[NSURL class] forKey:@"manifest_url"];
    URL startURL = [aDecoder decodeObjectOfClass:[NSURL class] forKey:@"start_url"];
    URL manifestId = [aDecoder decodeObjectOfClass:[NSURL class] forKey:@"manifestId"];
    RetainPtr<WebCore::CocoaColor> backgroundColor = [aDecoder decodeObjectOfClass:[WebCore::CocoaColor class] forKey:@"background_color"];
    RetainPtr<WebCore::CocoaColor> themeColor = [aDecoder decodeObjectOfClass:[WebCore::CocoaColor class] forKey:@"theme_color"];
    RetainPtr<NSArray<NSString *>> categories = [aDecoder decodeObjectOfClasses:[NSSet setWithArray:@[[NSArray class], [NSString class]]] forKey:@"categories"];
    RetainPtr<NSArray<_WKApplicationManifestIcon *>> icons = [aDecoder decodeObjectOfClasses:[NSSet setWithArray:@[[NSArray class], [_WKApplicationManifestIcon class]]] forKey:@"icons"];
    RetainPtr<NSArray<_WKApplicationManifestIcon *>> shortcuts = [aDecoder decodeObjectOfClasses:[NSSet setWithArray:@[[NSArray class], [_WKApplicationManifestShortcut class], [_WKApplicationManifestIcon class]]] forKey:@"shortcuts"];

    WebCore::ApplicationManifest coreApplicationManifest {
        WTF::move(rawJSON),
        static_cast<WebCore::ApplicationManifest::Direction>(dir),
        WTF::move(lang),
        WTF::move(name),
        WTF::move(shortName),
        WTF::move(description),
        WTF::move(scopeURL),
        isDefaultScope,
        static_cast<WebCore::ApplicationManifest::Display>(display),
        WTF::move(orientationValue),
        WTF::move(manifestURL),
        WTF::move(startURL),
        WTF::move(manifestId),
        WebCore::roundAndClampToSRGBALossy(RetainPtr { backgroundColor.get().CGColor }.get()),
        WebCore::roundAndClampToSRGBALossy(RetainPtr { themeColor.get().CGColor }.get()),
        makeVector<String>(categories.get()),
        makeVector<WebCore::ApplicationManifest::Icon>(icons.get()),
        makeVector<WebCore::ApplicationManifest::Shortcut>(shortcuts.get()),
    };

    API::Object::constructInWrapper<API::ApplicationManifest>(self, WTF::move(coreApplicationManifest));

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKApplicationManifest.class, self))
        return;

    SUPPRESS_UNRETAINED_ARG _applicationManifest->~ApplicationManifest();

    [super dealloc];
}

- (void)encodeWithCoder:(NSCoder *)aCoder
{
    [aCoder encodeObject:self.rawJSON forKey:@"raw_json"];
    [aCoder encodeInteger:static_cast<NSInteger>(_applicationManifest->applicationManifest().dir) forKey:@"dir"];
    [aCoder encodeObject:self.lang forKey:@"lang"];
    [aCoder encodeObject:self.name forKey:@"name"];
    [aCoder encodeObject:self.shortName forKey:@"short_name"];
    [aCoder encodeObject:self.applicationDescription forKey:@"description"];
    [aCoder encodeObject:self.scope forKey:@"scope"];
    [aCoder encodeBool:self.isDefaultScope forKey:@"is_default_scope"];
    [aCoder encodeInteger:static_cast<NSInteger>(_applicationManifest->applicationManifest().display) forKey:@"display"];

    // If orientation has value, encode the integer value, otherwise encode NSNotFound.
    [aCoder encodeInteger:(_applicationManifest->applicationManifest().orientation.has_value() ? static_cast<NSInteger>(*_applicationManifest->applicationManifest().orientation) : NSNotFound) forKey:@"orientation"];

    [aCoder encodeObject:self.manifestURL forKey:@"manifest_url"];
    [aCoder encodeObject:self.startURL forKey:@"start_url"];
    [aCoder encodeObject:self.manifestId forKey:@"manifestId"];
    [aCoder encodeObject:self.backgroundColor forKey:@"background_color"];
    [aCoder encodeObject:self.themeColor forKey:@"theme_color"];
    [aCoder encodeObject:self.categories forKey:@"categories"];
    [aCoder encodeObject:self.icons forKey:@"icons"];
    [aCoder encodeObject:self.shortcuts forKey:@"shortcuts"];
}

+ (_WKApplicationManifest *)applicationManifestFromJSON:(NSString *)json manifestURL:(NSURL *)manifestURL documentURL:(NSURL *)documentURL
{
    auto manifest = WebCore::ApplicationManifestParser::parse(WTF::String(json), URL(manifestURL), URL(documentURL));
    return wrapper(API::ApplicationManifest::create(manifest)).autorelease();
}

- (API::Object&)_apiObject
{
    return *_applicationManifest;
}

static RetainPtr<NSString> nullableNSString(const WTF::String& string)
{
    return !string.isNull() ? string.createNSString() : nil;
}

- (NSString *)rawJSON
{
    return nullableNSString(_applicationManifest->applicationManifest().rawJSON).autorelease();
}

- (_WKApplicationManifestDirection)direction
{
    using enum WebCore::ApplicationManifest::Direction;

    switch (_applicationManifest->applicationManifest().dir) {
    case Auto:
        return _WKApplicationManifestDirectionAuto;
    case LTR:
        return _WKApplicationManifestDirectionLTR;
    case RTL:
        return _WKApplicationManifestDirectionRTL;
    }

    ASSERT_NOT_REACHED();
}

- (NSString *)lang
{
    return nullableNSString(_applicationManifest->applicationManifest().lang).autorelease();
}

- (NSString *)name
{
    return nullableNSString(_applicationManifest->applicationManifest().name).autorelease();
}

- (NSString *)shortName
{
    return nullableNSString(_applicationManifest->applicationManifest().shortName).autorelease();
}

- (NSString *)applicationDescription
{
    return nullableNSString(_applicationManifest->applicationManifest().description).autorelease();
}

- (NSURL *)scope
{
    return _applicationManifest->applicationManifest().scope.createNSURL().autorelease();
}

- (BOOL)isDefaultScope
{
    return _applicationManifest->applicationManifest().isDefaultScope;
}

- (NSURL *)manifestURL
{
    return _applicationManifest->applicationManifest().manifestURL.createNSURL().autorelease();
}

- (NSURL *)startURL
{
    return _applicationManifest->applicationManifest().startURL.createNSURL().autorelease();
}

- (WebCore::CocoaColor *)backgroundColor
{
    return cocoaColor(_applicationManifest->applicationManifest().backgroundColor).autorelease();
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

- (std::optional<_WKApplicationManifestOrientation>)orientation
{
    if (!_applicationManifest->applicationManifest().orientation.has_value())
        return std::nullopt;

    switch (*_applicationManifest->applicationManifest().orientation) {
    case WebCore::ScreenOrientationLockType::Any:
        return _WKApplicationManifestOrientationAny;
    case WebCore::ScreenOrientationLockType::Landscape:
        return _WKApplicationManifestOrientationLandscape;
    case WebCore::ScreenOrientationLockType::LandscapePrimary:
        return _WKApplicationManifestOrientationLandscapePrimary;
    case WebCore::ScreenOrientationLockType::LandscapeSecondary:
        return _WKApplicationManifestOrientationLandscapeSecondary;
    case WebCore::ScreenOrientationLockType::Natural:
        return _WKApplicationManifestOrientationNatural;
    case WebCore::ScreenOrientationLockType::Portrait:
        return _WKApplicationManifestOrientationPortrait;
    case WebCore::ScreenOrientationLockType::PortraitPrimary:
        return _WKApplicationManifestOrientationPortraitPrimary;
    case WebCore::ScreenOrientationLockType::PortraitSecondary:
        return _WKApplicationManifestOrientationPortraitSecondary;
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

- (NSArray<NSString *> *)categories
{
    return createNSArray(_applicationManifest->applicationManifest().categories, [] (auto& category) {
        return category.createNSString();
    }).autorelease();
}

- (NSArray<_WKApplicationManifestIcon *> *)icons
{
    return createNSArray(_applicationManifest->applicationManifest().icons, [] (auto& coreIcon) {
        return adoptNS([[_WKApplicationManifestIcon alloc] initWithCoreIcon:&coreIcon]);
    }).autorelease();
}

- (NSArray<_WKApplicationManifestShortcut *> *)shortcuts
{
    return createNSArray(_applicationManifest->applicationManifest().shortcuts, [] (auto& coreShortcut) {
        return adoptNS([[_WKApplicationManifestShortcut alloc] initWithCoreShortcut:&coreShortcut]);
    }).autorelease();
}

- (NSURL *)manifestId
{
    return _applicationManifest->applicationManifest().id.createNSURL().autorelease();
}

@end
