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
#import "_WKRemoteObjectInterfaceInternal.h"

#import <objc/runtime.h>
#import <wtf/HashMap.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/CString.h>

extern "C"
const char *_protocol_getMethodTypeEncoding(Protocol *p, SEL sel, BOOL isRequiredMethod, BOOL isInstanceMethod);

@interface NSMethodSignature ()
- (NSMethodSignature *)_signatureForBlockAtArgumentIndex:(NSInteger)idx;
- (Class)_classForObjectAtArgumentIndex:(NSInteger)idx;
- (NSString *)_typeString;
@end

struct MethodInfo {
    Vector<HashSet<CFTypeRef>> allowedArgumentClasses;
    RetainPtr<NSInvocation> invocation;

    Vector<HashSet<CFTypeRef>> allowedReplyClasses;
    RetainPtr<NSInvocation> replyInvocation;
};

@implementation _WKRemoteObjectInterface {
    RetainPtr<NSString> _identifier;

    HashMap<SEL, MethodInfo> _methods;
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

static void initializeMethod(MethodInfo& methodInfo, Protocol *protocol, SEL selector, NSMethodSignature *methodSignature, bool forReplyBlock)
{
    if (forReplyBlock)
        methodInfo.replyInvocation = [NSInvocation invocationWithMethodSignature:methodSignature];
    else {
        const char* types = methodArgumentTypeEncodingForSelector(protocol, selector);
        NSMethodSignature *signature = [NSMethodSignature signatureWithObjCTypes:types];
        methodInfo.invocation = [NSInvocation invocationWithMethodSignature:signature];
    }

    auto& allowedClasses = forReplyBlock ? methodInfo.allowedReplyClasses : methodInfo.allowedArgumentClasses;
    NSUInteger firstArgument = forReplyBlock ? 1 : 2;
    NSUInteger argumentCount = methodSignature.numberOfArguments;

    bool foundBlock = false;
    for (NSUInteger i = firstArgument; i < argumentCount; ++i) {
        auto argumentType = span([methodSignature getArgumentTypeAtIndex:i]);

        if (argumentType.empty() || argumentType.front() != '@') {
            // This is a non-object type; we won't allow any classes to be decoded for it.
            allowedClasses.append({ });
            continue;
        }

        if (argumentType.size() > 1 && argumentType[1] == '?') {
            if (forReplyBlock)
                [NSException raise:NSInvalidArgumentException format:@"Blocks as arguments to the reply block of method (%s / %s) are not allowed", protocol_getName(protocol), sel_getName(selector)];

            if (foundBlock)
                [NSException raise:NSInvalidArgumentException format:@"Only one reply block is allowed per method (%s / %s)", protocol_getName(protocol), sel_getName(selector)];
            foundBlock = true;
            NSMethodSignature *blockSignature = [methodSignature _signatureForBlockAtArgumentIndex:i];
            ASSERT(blockSignature._typeString);

            initializeMethod(methodInfo, protocol, selector, blockSignature, true);
        }

        Class objectClass = [methodSignature _classForObjectAtArgumentIndex:i];
        if (!objectClass) {
            allowedClasses.append({ });
            continue;
        }

        allowedClasses.append({ (__bridge CFTypeRef)objectClass });
    }
}

static void initializeMethods(_WKRemoteObjectInterface *interface, Protocol *protocol, bool requiredMethods)
{
    auto methods = protocol_copyMethodDescriptionListSpan(protocol, requiredMethods, true);

    for (auto& method : methods.span()) {
        SEL selector = method.name;

        ASSERT(!interface->_methods.contains(selector));
        MethodInfo& methodInfo = interface->_methods.add(selector, MethodInfo()).iterator->value;

        const char* methodTypeEncoding = _protocol_getMethodTypeEncoding(protocol, selector, requiredMethods, true);
        if (!methodTypeEncoding)
            [NSException raise:NSInvalidArgumentException format:@"Could not find method type encoding for method \"%s\"", sel_getName(selector)];

        NSMethodSignature *methodSignature = [NSMethodSignature signatureWithObjCTypes:methodTypeEncoding];

        initializeMethod(methodInfo, protocol, selector, methodSignature, false);
    }
}

static void initializeMethods(_WKRemoteObjectInterface *interface, Protocol *protocol)
{
    auto conformingProtocols = protocol_copyProtocolListSpan(protocol);
    for (auto* conformingProtocol : conformingProtocols.span()) {
        if (conformingProtocol == @protocol(NSObject))
            continue;
        initializeMethods(interface, conformingProtocol);
    }

    initializeMethods(interface, protocol, true);
    initializeMethods(interface, protocol, false);
}

- (id)initWithProtocol:(Protocol *)protocol identifier:(NSString *)identifier
{
    if (!(self = [super init]))
        return nil;

    _protocol = protocol;
    _identifier = adoptNS([identifier copy]);

    initializeMethods(self, _protocol);

    return self;
}

+ (instancetype)remoteObjectInterfaceWithProtocol:(Protocol *)protocol
{
    return adoptNS([[self alloc] initWithProtocol:protocol identifier:NSStringFromProtocol(protocol)]).autorelease();
}

- (NSString *)identifier
{
    return _identifier.get();
}

- (NSString *)debugDescription
{
    auto result = adoptNS([[NSMutableString alloc] initWithFormat:@"<%@: %p; protocol = \"%@\"; identifier = \"%@\"\n", NSStringFromClass(self.class), self, _identifier.get(), NSStringFromProtocol(_protocol)]);

    auto descriptionForClasses = [](auto& allowedClasses) {
        auto result = adoptNS([[NSMutableString alloc] initWithString:@"["]);
        bool needsComma = false;

        auto descriptionForArgument = [](auto& allowedArgumentClasses) {
            auto result = adoptNS([[NSMutableString alloc] initWithString:@"{"]);

            auto orderedArgumentClasses = copyToVector(allowedArgumentClasses);
            std::sort(orderedArgumentClasses.begin(), orderedArgumentClasses.end(), [](CFTypeRef a, CFTypeRef b) {
                return CString(class_getName((__bridge Class)a)) < CString(class_getName((__bridge Class)b));
            });

            bool needsComma = false;
            for (auto& argumentClass : orderedArgumentClasses) {
                if (needsComma)
                    [result appendString:@", "];

                [result appendFormat:@"%s", class_getName((__bridge Class)argumentClass)];
                needsComma = true;
            }

            [result appendString:@"}"];
            return result.autorelease();
        };

        for (auto& argumentClasses : allowedClasses) {
            if (needsComma)
                [result appendString:@", "];

            [result appendString:descriptionForArgument(argumentClasses)];
            needsComma = true;
        }

        [result appendString:@"]"];
        return result.autorelease();
    };

    for (auto& selectorAndMethod : _methods) {
        [result appendFormat:@" selector = %s\n  argument classes = %@\n", sel_getName(selectorAndMethod.key), descriptionForClasses(selectorAndMethod.value.allowedArgumentClasses)];

        if (auto& replyInvocation = selectorAndMethod.value.replyInvocation)
            [result appendFormat:@"  reply block = (argument '%@') %@\n", [replyInvocation methodSignature]._typeString, descriptionForClasses(selectorAndMethod.value.allowedReplyClasses)];
    }

    [result appendString:@">\n"];
    return result.autorelease();
}

static HashSet<CFTypeRef>& classesForSelectorArgument(_WKRemoteObjectInterface *interface, SEL selector, NSUInteger argumentIndex, bool replyBlock)
{
    auto it = interface->_methods.find(selector);
    if (it == interface->_methods.end())
        [NSException raise:NSInvalidArgumentException format:@"Interface does not contain selector \"%s\"", sel_getName(selector)];

    MethodInfo& methodInfo = it->value;
    if (replyBlock) {
        if (!methodInfo.replyInvocation)
            [NSException raise:NSInvalidArgumentException format:@"Selector \"%s\" does not have a reply block", sel_getName(selector)];

        if (argumentIndex >= methodInfo.allowedReplyClasses.size())
            [NSException raise:NSInvalidArgumentException format:@"Argument index %ld is out of range for reply block of selector \"%s\"", (unsigned long)argumentIndex, sel_getName(selector)];

        return methodInfo.allowedReplyClasses[argumentIndex];
    }

    if (argumentIndex >= methodInfo.allowedArgumentClasses.size())
        [NSException raise:NSInvalidArgumentException format:@"Argument index %ld is out of range for selector \"%s\"", (unsigned long)argumentIndex, sel_getName(selector)];

    return methodInfo.allowedArgumentClasses[argumentIndex];
}

- (NSSet *)classesForSelector:(SEL)selector argumentIndex:(NSUInteger)argumentIndex ofReply:(BOOL)ofReply
{
    auto result = adoptNS([[NSMutableSet alloc] init]);

    for (auto allowedClass : classesForSelectorArgument(self, selector, argumentIndex, ofReply))
        [result addObject:(__bridge Class)allowedClass];

    return result.autorelease();
}

- (void)setClasses:(NSSet *)classes forSelector:(SEL)selector argumentIndex:(NSUInteger)argumentIndex ofReply:(BOOL)ofReply
{
    HashSet<CFTypeRef> allowedClasses;
    for (Class allowedClass in classes)
        allowedClasses.add((__bridge CFTypeRef)allowedClass);

    classesForSelectorArgument(self, selector, argumentIndex, ofReply) = WTFMove(allowedClasses);
}

- (NSSet *)classesForSelector:(SEL)selector argumentIndex:(NSUInteger)argumentIndex
{
    return [self classesForSelector:selector argumentIndex:argumentIndex ofReply:NO];
}

- (void)setClasses:(NSSet *)classes forSelector:(SEL)selector argumentIndex:(NSUInteger)argumentIndex
{
    [self setClasses:classes forSelector:selector argumentIndex:argumentIndex ofReply:NO];
}

- (NSInvocation *)_invocationForSelector:(SEL)selector
{
    auto it = _methods.find(selector);
    if (it  == _methods.end())
        return nil;

    return adoptNS([it->value.invocation copy]).autorelease();
}

- (NSInvocation *)_invocationForReplyBlockOfSelector:(SEL)selector
{
    auto it = _methods.find(selector);
    if (it  == _methods.end())
        return nil;

    return adoptNS([it->value.replyInvocation copy]).autorelease();
}

- (const Vector<HashSet<CFTypeRef>>&)_allowedArgumentClassesForSelector:(SEL)selector
{
    ASSERT(_methods.contains(selector));

    return _methods.find(selector)->value.allowedArgumentClasses;
}

- (const Vector<HashSet<CFTypeRef>>&)_allowedArgumentClassesForReplyBlockOfSelector:(SEL)selector
{
    ASSERT(_methods.contains(selector));

    return _methods.find(selector)->value.allowedReplyClasses;
}

@end
