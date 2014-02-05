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
#import "WKRemoteObjectInterface.h"

#if WK_API_ENABLED

#import <objc/runtime.h>
#import <wtf/HashMap.h>
#import <wtf/Vector.h>
#import <wtf/RetainPtr.h>

extern "C"
const char *_protocol_getMethodTypeEncoding(Protocol *p, SEL sel, BOOL isRequiredMethod, BOOL isInstanceMethod);

@interface NSMethodSignature (Details)
- (Class)_classForObjectAtArgumentIndex:(NSInteger)idx;
@end

@implementation WKRemoteObjectInterface {
    HashMap<SEL, Vector<RetainPtr<NSSet>>> _allowedArgumentClasses;
}

static bool isContainerClass(Class objectClass)
{
    // FIXME: Add more classes here if needed.
    static NSSet *containerClasses = [[NSSet alloc] initWithObjects:[NSArray class], [NSDictionary class], nil];

    return [containerClasses containsObject:objectClass];
}

static NSSet *propertyListClasses()
{
    // FIXME: Add more property list classes if needed.
    static NSSet *propertyListClasses = [[NSSet alloc] initWithObjects:[NSArray class], [NSDictionary class], [NSNull class], [NSNumber class], [NSString class], nil];

    return propertyListClasses;
}

static Vector<RetainPtr<NSSet>> allowedArgumentClassesForMethod(NSMethodSignature *methodSignature)
{
    Vector<RetainPtr<NSSet>> result;

    NSSet *emptySet = [NSSet set];

    // We ignore self and _cmd.
    NSUInteger argumentCount = methodSignature.numberOfArguments - 2;

    result.reserveInitialCapacity(argumentCount);

    for (NSUInteger i = 0; i < argumentCount; ++i) {
        const char* type = [methodSignature getArgumentTypeAtIndex:i + 2];

        if (*type != '@') {
            // This is a non-object type; we won't allow any classes to be decoded for it.
            result.uncheckedAppend(emptySet);
            continue;
        }

        Class objectClass = [methodSignature _classForObjectAtArgumentIndex:i + 2];
        if (!objectClass) {
            result.uncheckedAppend(emptySet);
            continue;
        }

        if (isContainerClass(objectClass)) {
            // For container classes, we allow all simple property list classes.
            result.uncheckedAppend(propertyListClasses());
            continue;
        }

        result.uncheckedAppend([NSSet setWithObject:objectClass]);
    }

    return result;
}

static void initializeAllowedArgumentClasses(WKRemoteObjectInterface *interface, bool requiredMethods)
{
    unsigned methodCount;
    struct objc_method_description *methods = protocol_copyMethodDescriptionList(interface->_protocol, requiredMethods, true, &methodCount);

    for (unsigned i = 0; i < methodCount; ++i) {
        SEL selector = methods[i].name;

        const char* types = _protocol_getMethodTypeEncoding(interface->_protocol, selector, requiredMethods, true);
        if (!types)
            [NSException raise:NSInvalidArgumentException format:@"Could not find method type encoding for method \"%s\"", sel_getName(selector)];

        NSMethodSignature *methodSignature = [NSMethodSignature signatureWithObjCTypes:types];
        interface->_allowedArgumentClasses.set(selector, allowedArgumentClassesForMethod(methodSignature));
    }

    free(methods);
}

static void initializeAllowedArgumentClasses(WKRemoteObjectInterface *interface)
{
    initializeAllowedArgumentClasses(interface, true);
    initializeAllowedArgumentClasses(interface, false);
}

- (id)initWithProtocol:(Protocol *)protocol identifier:(NSString *)identifier
{
    if (!(self = [super init]))
        return nil;

    _protocol = protocol;
    _identifier = [identifier copy];

    initializeAllowedArgumentClasses(self);

    return self;
}

+ (instancetype)remoteObjectInterfaceWithProtocol:(Protocol *)protocol
{
    return [[[self alloc] initWithProtocol:protocol identifier:NSStringFromProtocol(protocol)] autorelease];
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

- (NSMethodSignature *)_methodSignatureForSelector:(SEL)selector
{
    const char* types = methodArgumentTypeEncodingForSelector(_protocol, selector);
    if (!types)
        return nil;

    return [NSMethodSignature signatureWithObjCTypes:types];
}

- (const Vector<RetainPtr<NSSet>>&)_allowedArgumentClassesForSelector:(SEL)selector
{
    auto it = _allowedArgumentClasses.find(selector);
    ASSERT(it != _allowedArgumentClasses.end());

    return it->value;
}

@end

#endif // WK_API_ENABLED
