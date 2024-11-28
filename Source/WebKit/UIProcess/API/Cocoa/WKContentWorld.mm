/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WKContentWorldInternal.h"

#import "_WKUserContentWorldInternal.h"
#import <WebCore/WebCoreObjCExtras.h>

static void checkContentWorldOptions(API::ContentWorld& world, _WKContentWorldConfiguration *configuration)
{
    if (world.allowAccessToClosedShadowRoots() != (configuration && configuration.allowAccessToClosedShadowRoots))
        [NSException raise:NSInternalInconsistencyException format:@"The value of allowAccessToClosedShadowRoots does not match the existing world"];
    if (world.allowAutofill() != (configuration && configuration.allowAutofill))
        [NSException raise:NSInternalInconsistencyException format:@"The value of allowAutofill does not match the existing world"];
    if (world.allowElementUserInfo() != (configuration && configuration.allowElementUserInfo))
        [NSException raise:NSInternalInconsistencyException format:@"The value of allowElementUserInfo does not match the existing world"];
    if (world.disableLegacyBuiltinOverrides() != (configuration && configuration.disableLegacyBuiltinOverrides))
        [NSException raise:NSInternalInconsistencyException format:@"The value of disableLegacyBuiltinOverrides does not match the existing world"];
}

@implementation WKContentWorld

WK_OBJECT_DISABLE_DISABLE_KVC_IVAR_ACCESS;

+ (WKContentWorld *)pageWorld
{
    return wrapper(API::ContentWorld::pageContentWorld());
}

+ (WKContentWorld *)defaultClientWorld
{
    return wrapper(API::ContentWorld::defaultClientWorld());
}

+ (WKContentWorld *)worldWithName:(NSString *)name
{
    Ref world = API::ContentWorld::sharedWorldWithName(name);
    checkContentWorldOptions(world, nil);
    return wrapper(WTFMove(world)).autorelease();
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKContentWorld.class, self))
        return;

    _contentWorld->~ContentWorld();

    [super dealloc];
}

- (NSString *)name
{
    if (_contentWorld->name().isNull())
        return nil;

    return _contentWorld->name();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_contentWorld;
}

@end

@implementation WKContentWorld (WKPrivate)

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (_WKUserContentWorld *)_userContentWorld
{
    return adoptNS([[_WKUserContentWorld alloc] _initWithContentWorld:self]).autorelease();
}
ALLOW_DEPRECATED_DECLARATIONS_END

+ (WKContentWorld *)_worldWithConfiguration:(_WKContentWorldConfiguration *)configuration
{
    OptionSet<WebKit::ContentWorldOption> optionSet;
    if (configuration.allowAccessToClosedShadowRoots)
        optionSet.add(WebKit::ContentWorldOption::AllowAccessToClosedShadowRoots);
    if (configuration.allowAutofill)
        optionSet.add(WebKit::ContentWorldOption::AllowAutofill);
    if (configuration.allowElementUserInfo)
        optionSet.add(WebKit::ContentWorldOption::AllowElementUserInfo);
    if (configuration.disableLegacyBuiltinOverrides)
        optionSet.add(WebKit::ContentWorldOption::DisableLegacyBuiltinOverrides);
    Ref world = API::ContentWorld::sharedWorldWithName(configuration.name, optionSet);
    checkContentWorldOptions(world, configuration);
    return wrapper(WTFMove(world)).autorelease();
}

@end
