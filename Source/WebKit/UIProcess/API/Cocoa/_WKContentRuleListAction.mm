/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "_WKContentRuleListActionInternal.h"

#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/cocoa/VectorCocoa.h>

@implementation _WKContentRuleListAction

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKContentRuleListAction.class, self))
        return;

    _action->~ContentRuleListAction();
    
    [super dealloc];
}

- (BOOL)blockedLoad
{
#if ENABLE(CONTENT_EXTENSIONS)
    return _action->blockedLoad();
#else
    return NO;
#endif
}

- (BOOL)blockedCookies
{
#if ENABLE(CONTENT_EXTENSIONS)
    return _action->blockedCookies();
#else
    return NO;
#endif
}

- (BOOL)madeHTTPS
{
#if ENABLE(CONTENT_EXTENSIONS)
    return _action->madeHTTPS();
#else
    return NO;
#endif
}

- (BOOL)redirected
{
#if ENABLE(CONTENT_EXTENSIONS)
    return _action->redirected();
#else
    return NO;
#endif
}

- (BOOL)modifiedHeaders
{
#if ENABLE(CONTENT_EXTENSIONS)
    return _action->modifiedHeaders();
#else
    return NO;
#endif
}

- (NSArray<NSString *> *)notifications
{
#if ENABLE(CONTENT_EXTENSIONS)
    auto& vector = _action->notifications();
    if (vector.isEmpty())
        return nil;
    return createNSArray(vector).autorelease();
#else
    return nil;
#endif
}

- (API::Object&)_apiObject
{
    return *_action;
}

@end
