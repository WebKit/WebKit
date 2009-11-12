/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebScriptWorld.h"

#import "WebScriptWorldInternal.h"
#import <WebCore/JSDOMBinding.h>
#import <WebCore/ScriptController.h>
#import <wtf/RefPtr.h>

using namespace WebCore;

@interface WebScriptWorldPrivate : NSObject {
@public
    RefPtr<DOMWrapperWorld> world;
}
@end

@implementation WebScriptWorldPrivate
@end

@implementation WebScriptWorld

- (id)initWithWorld:(PassRefPtr<DOMWrapperWorld>)world
{
    ASSERT_ARG(world, world);
    if (!world)
        return nil;

    self = [super init];
    if (!self)
        return nil;

    _private = [[WebScriptWorldPrivate alloc] init];
    _private->world = world;

    return self;
}

- (id)init
{
    return [self initWithWorld:ScriptController::createWorld()];
}

- (void)dealloc
{
    [_private release];
    _private = nil;
    [super dealloc];
}

+ (WebScriptWorld *)standardWorld
{
    static WebScriptWorld *world = [[WebScriptWorld alloc] initWithWorld:mainThreadNormalWorld()];
    return world;
}

+ (WebScriptWorld *)world
{
    return [[[self alloc] init] autorelease];
}

DOMWrapperWorld* core(WebScriptWorld *world)
{
    return world ? world->_private->world.get() : 0;
}

@end
