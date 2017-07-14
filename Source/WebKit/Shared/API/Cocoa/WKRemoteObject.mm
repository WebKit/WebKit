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
#import "WKRemoteObject.h"

#if WK_API_ENABLED

#import "_WKRemoteObjectInterface.h"
#import "_WKRemoteObjectRegistryInternal.h"
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>

@implementation WKRemoteObject {
    RetainPtr<_WKRemoteObjectRegistry> _objectRegistry;
    RetainPtr<_WKRemoteObjectInterface> _interface;
}

- (instancetype)_initWithObjectRegistry:(_WKRemoteObjectRegistry *)objectRegistry interface:(_WKRemoteObjectInterface *)interface
{
    if (!(self = [super init]))
        return nil;

    _objectRegistry = objectRegistry;
    _interface = interface;

    return self;
}

- (BOOL)conformsToProtocol:(Protocol *)protocol
{
    return [super conformsToProtocol:protocol] || protocol_conformsToProtocol([_interface protocol], protocol);
}

static const char* methodArgumentTypeEncodingForSelector(Protocol *protocol, SEL selector)
{
    // First look at required methods.
    struct objc_method_description method = protocol_getMethodDescription(protocol, selector, YES, YES);
    if (method.name)
        return method.types;

    // Then look at optional methods.
    method = protocol_getMethodDescription(protocol, selector, NO, YES);
    if (method.name)
        return method.types;

    return nullptr;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector
{
    if (!selector)
        return nil;

    Protocol *protocol = [_interface protocol];

    const char* types = methodArgumentTypeEncodingForSelector(protocol, selector);
    if (!types) {
        // We didn't find anything the protocol, fall back to the superclass.
        return [super methodSignatureForSelector:selector];
    }

    return [NSMethodSignature signatureWithObjCTypes:types];
}

- (void)forwardInvocation:(NSInvocation *)invocation
{
    [_objectRegistry _sendInvocation:invocation interface:_interface.get()];
}

@end

#endif // WK_API_ENABLED

