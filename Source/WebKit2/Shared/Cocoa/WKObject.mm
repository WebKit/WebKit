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
#import "objcSPI.h"
#import <wtf/ObjcRuntimeExtras.h>

@implementation WKObject {
    Class _isa;
    BOOL _hasInitializedTarget;
    NSObject *_target;
}

+ (Class)class
{
    return self;
}

static inline void initializeTargetIfNeeded(WKObject *self)
{
    if (self->_hasInitializedTarget)
        return;

    self->_hasInitializedTarget = YES;
    self->_target = [self _web_createTarget];
}

// MARK: Methods used by the Objective-C runtime

- (id)forwardingTargetForSelector:(SEL)selector
{
    initializeTargetIfNeeded(self);

    return _target;
}

- (BOOL)allowsWeakReference
{
    return !_objc_rootIsDeallocating(self);
}

- (BOOL)retainWeakReference
{
    return _objc_rootTryRetain(self);
}

// MARK: NSObject protocol implementation

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

    return _target ? [_target hash] : reinterpret_cast<NSUInteger>(self);
}

- (Class)superclass
{
    initializeTargetIfNeeded(self);

    return _target ? [_target superclass] : class_getSuperclass(object_getClass(self));
}

- (Class)class
{
    initializeTargetIfNeeded(self);

    return _target ? [_target class] : object_getClass(self);
}

- (instancetype)self
{
    return self;
}

- (id)performSelector:(SEL)selector
{
    return selector ? wtfObjcMsgSend<id>(self, selector) : nil;
}

- (id)performSelector:(SEL)selector withObject:(id)object
{
    return selector ? wtfObjcMsgSend<id>(self, selector, object) : nil;
}

- (id)performSelector:(SEL)selector withObject:(id)object1 withObject:(id)object2
{
    return selector ? wtfObjcMsgSend<id>(self, selector, object1, object2) : nil;
}

- (BOOL)isProxy
{
    return NO;
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

    return [_target respondsToSelector:selector] || (selector && class_respondsToSelector(object_getClass(self), selector));
}

+ (BOOL)conformsToProtocol:(Protocol *)protocol
{
    if (!protocol)
        return NO;

    for (Class cls = self; cls; cls = class_getSuperclass(cls)) {
        if (class_conformsToProtocol(cls, protocol))
            return YES;
    }

    return NO;
}

- (BOOL)conformsToProtocol:(Protocol *)protocol
{
    initializeTargetIfNeeded(self);

    if ([_target conformsToProtocol:protocol])
        return YES;

    if (!protocol)
        return NO;

    for (Class cls = object_getClass(self); cls; cls = class_getSuperclass(cls)) {
        if (class_conformsToProtocol(cls, protocol))
            return YES;
    }

    return NO;
}

- (NSString *)description
{
    initializeTargetIfNeeded(self);

    return _target ? [_target description] : [NSString stringWithFormat:@"<%s %p>", class_getName(object_getClass(self)), self];
}

- (NSString *)debugDescription
{
    initializeTargetIfNeeded(self);

    return _target ? [_target debugDescription] : [self description];
}

- (instancetype)retain
{
    return _objc_rootRetain(self);
}

- (oneway void)release
{
    if (_objc_rootReleaseWasZero(self)) {
        static_cast<API::Object*>(object_getIndexedIvars(self))->~Object();
        [_target release];
        _objc_rootDealloc(self);
    }
}

- (instancetype)autorelease
{
    return _objc_rootAutorelease(self);
}

- (NSUInteger)retainCount
{
    return _objc_rootRetainCount(self);
}

- (NSZone *)zone
{
    return NSDefaultMallocZone();
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
