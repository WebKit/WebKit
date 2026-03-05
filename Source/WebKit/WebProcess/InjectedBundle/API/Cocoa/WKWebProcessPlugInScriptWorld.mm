/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#import "WKWebProcessPlugInScriptWorldInternal.h"

#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/AlignedStorage.h>

@implementation WKWebProcessPlugInScriptWorld {
    AlignedStorage<WebKit::InjectedBundleScriptWorld> _world;
}

+ (WKWebProcessPlugInScriptWorld *)world
{
    return WebKit::wrapper(WebKit::InjectedBundleScriptWorld::create(WebKit::ContentWorldIdentifier::generate())).autorelease();
}

+ (WKWebProcessPlugInScriptWorld *)normalWorld
{
    return WebKit::wrapper(WebKit::InjectedBundleScriptWorld::normalWorldSingleton());
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(WKWebProcessPlugInScriptWorld.class, self))
        return;
    SUPPRESS_UNCOUNTED_ARG _world->~InjectedBundleScriptWorld();
    [super dealloc];
}

- (void)clearWrappers
{
    protect(*_world)->clearWrappers();
}

- (void)makeAllShadowRootsOpen
{
    protect(*_world)->makeAllShadowRootsOpen();
}

- (void)exposeClosedShadowRootsForExtensions
{
    protect(*_world)->exposeClosedShadowRootsForExtensions();
}

- (void)disableOverrideBuiltinsBehavior
{
    protect(*_world)->disableOverrideBuiltinsBehavior();
}

- (void)allowJSHandleCreation
{
    protect(*_world)->setAllowJSHandleCreation();
}

- (NSString *)name
{
    return _world->name().createNSString().autorelease();
}

- (WebKit::InjectedBundleScriptWorld&)_scriptWorld
{
    return *_world;
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_world;
}

@end
