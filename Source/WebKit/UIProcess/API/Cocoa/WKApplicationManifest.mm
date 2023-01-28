/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "WKApplicationManifestInternal.h"

#import <WebCore/ColorCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

@interface _WKApplicationManifestIcon ()
- (instancetype)initWithCoreIcon:(const WebCore::ApplicationManifest::Icon *)icon;
@end

@implementation WKApplicationManifest

- (API::Object&)_apiObject
{
    return *_applicationManifest;
}

- (instancetype)initWithApplicationManifest:(Ref<API::ApplicationManifest>&&)applicationManifest
{
    self = [super init];
    if (!self)
        return nil;

    _applicationManifest = WTFMove(applicationManifest);

    return self;
}

static NSString *nullableNSString(const WTF::String& string)
{
    return string.isNull() ? nil : (NSString *)string;
}

- (NSString *)_name
{
    return nullableNSString(_applicationManifest->applicationManifest().name);
}

- (NSString *)_shortName
{
    return nullableNSString(_applicationManifest->applicationManifest().shortName);
}

- (NSString *)_applicationDescription
{
    return nullableNSString(_applicationManifest->applicationManifest().description);
}

- (NSURL *)_scope
{
    return _applicationManifest->applicationManifest().scope;
}

- (NSURL *)_startURL
{
    return _applicationManifest->applicationManifest().startURL;
}

- (WebCore::CocoaColor *)_themeColor
{
    return cocoaColor(_applicationManifest->applicationManifest().themeColor).autorelease();
}

- (_WKApplicationManifestDisplayMode)_displayMode
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

- (NSArray<_WKApplicationManifestIcon *> *)_icons
{
    return createNSArray(_applicationManifest->applicationManifest().icons, [] (auto& coreIcon) -> id {
        return adoptNS([[_WKApplicationManifestIcon alloc] initWithCoreIcon:&coreIcon]).autorelease();
    }).autorelease();
}

- (NSURL *)_manifestId
{
    return _applicationManifest->applicationManifest().id;
}


@end
