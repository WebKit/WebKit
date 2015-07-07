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
#import "WKObject.h"

#if WK_API_ENABLED

#import "APIObject.h"

@implementation WKObject {
    BOOL _hasInitializedTarget;
    NSObject *_target;
}

- (void)dealloc
{
    static_cast<API::Object*>(object_getIndexedIvars(self))->~Object();
    [_target release];

    [super dealloc];
}

static inline void initializeTargetIfNeeded(WKObject *self)
{
    if (self->_hasInitializedTarget)
        return;

    self->_hasInitializedTarget = YES;
    self->_target = [self _web_createTarget];
}

- (BOOL)isEqual:(id)object
{
    if (object == self)
        return YES;

    if (!object)
        return NO;

    initializeTargetIfNeeded(self);

    return [_target isEqual:object];
}

- (NSUInteger)hash
{
    initializeTargetIfNeeded(self);

    return _target ? [_target hash] : [super hash];
}

- (BOOL)isKindOfClass:(Class)aClass
{
    initializeTargetIfNeeded(self);

    return [_target isKindOfClass:aClass];
}

- (BOOL)isMemberOfClass:(Class)aClass
{
    initializeTargetIfNeeded(self);

    return [_target isMemberOfClass:aClass];
}

- (BOOL)respondsToSelector:(SEL)selector
{
    initializeTargetIfNeeded(self);

    return [_target respondsToSelector:selector] || [super respondsToSelector:selector];
}

- (BOOL)conformsToProtocol:(Protocol *)protocol
{
    initializeTargetIfNeeded(self);

    return [_target conformsToProtocol:protocol] || [super conformsToProtocol:protocol];
}

- (id)forwardingTargetForSelector:(SEL)selector
{
    initializeTargetIfNeeded(self);

    return _target;
}

- (NSString *)description
{
    initializeTargetIfNeeded(self);

    return _target ? [_target description] : [super description];
}

- (Class)classForCoder
{
    initializeTargetIfNeeded(self);

    return [_target classForCoder];
}

- (Class)classForKeyedArchiver
{
    initializeTargetIfNeeded(self);

    return [_target classForKeyedArchiver];
}

- (NSObject *)_web_createTarget
{
    return nil;
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *static_cast<API::Object*>(object_getIndexedIvars(self));
}

@end

#endif // WK_API_ENABLED
