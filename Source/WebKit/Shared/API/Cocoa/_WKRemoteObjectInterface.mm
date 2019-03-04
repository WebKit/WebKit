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
#import <wtf/Optional.h>
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

    struct ReplyInfo {
        NSUInteger replyPosition;
        CString replySignature;
        Vector<HashSet<CFTypeRef>> allowedReplyClasses;
    };
    Optional<ReplyInfo> replyInfo;
};

@implementation _WKRemoteObjectInterface {
    RetainPtr<NSString> _identifier;

    HashMap<SEL, MethodInfo> _methods;
}

static bool isContainerClass(Class objectClass)
{
    // FIXME: Add more classes here if needed.
    static Class arrayClass = [NSArray class];
    static Class dictionaryClass = [NSDictionary class];
    return objectClass == arrayClass || objectClass == dictionaryClass;
}

static HashSet<CFTypeRef>& propertyListClasses()
{
    static LazyNeverDestroyed<HashSet<CFTypeRef>> propertyListClasses;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        propertyListClasses.construct(std::initializer_list<CFTypeRef> {
            (__bridge CFTypeRef)[NSArray class], (__bridge CFTypeRef)[NSDictionary class],
            (__bridge CFTypeRef)[NSNumber class], (__bridge CFTypeRef)[NSString class]
        });
    });

    return propertyListClasses;
}

static void initializeMethod(MethodInfo& methodInfo, Protocol *protocol, SEL selector, NSMethodSignature *methodSignature, bool forReplyBlock)
{
    Vector<HashSet<CFTypeRef>> allowedClasses;

    NSUInteger firstArgument = forReplyBlock ? 1 : 2;
    NSUInteger argumentCount = methodSignature.numberOfArguments;

    bool foundBlock = false;
    for (NSUInteger i = firstArgument; i < argumentCount; ++i) {
        const char* argumentType = [methodSignature getArgumentTypeAtIndex:i];

        if (*argumentType != '@') {
            // This is a non-object type; we won't allow any classes to be decoded for it.
            allowedClasses.append({ });
            continue;
        }

        if (*(argumentType + 1) == '?') {
            if (forReplyBlock)
                [NSException raise:NSInvalidArgumentException format:@"Blocks as arguments to the reply block of method (%s / %s) are not allowed", protocol_getName(protocol), sel_getName(selector)];

            if (foundBlock)
                [NSException raise:NSInvalidArgumentException format:@"Only one reply block is allowed per method (%s / %s)", protocol_getName(protocol), sel_getName(selector)];
            foundBlock = true;
            NSMethodSignature *blockSignature = [methodSignature _signatureForBlockAtArgumentIndex:i];
            ASSERT(blockSignature._typeString);

            methodInfo.replyInfo = MethodInfo::ReplyInfo();
            methodInfo.replyInfo->replyPosition = i;
            methodInfo.replyInfo->replySignature = blockSignature._typeString.UTF8String;

            initializeMethod(methodInfo, protocol, selector, blockSignature, true);
        }

        Class objectClass = [methodSignature _classForObjectAtArgumentIndex:i];
        if (!objectClass) {
            allowedClasses.append({ });
            continue;
        }

        if (isContainerClass(objectClass)) {
            // For container classes, we allow all simple property list classes.
            allowedClasses.append(propertyListClasses());
            continue;
        }

        allowedClasses.append({ (__bridge CFTypeRef)objectClass });
    }

    if (forReplyBlock)
        methodInfo.replyInfo->allowedReplyClasses = WTFMove(allowedClasses);
    else
        methodInfo.allowedArgumentClasses = WTFMove(allowedClasses);
}

static void initializeMethods(_WKRemoteObjectInterface *interface, Protocol *protocol, bool requiredMethods)
{
    unsigned methodCount;
    struct objc_method_description *methods = protocol_copyMethodDescriptionList(protocol, requiredMethods, true, &methodCount);

    for (unsigned i = 0; i < methodCount; ++i) {
        SEL selector = methods[i].name;

        ASSERT(!interface->_methods.contains(selector));
        MethodInfo& methodInfo = interface->_methods.add(selector, MethodInfo()).iterator->value;

        const char* methodTypeEncoding = _protocol_getMethodTypeEncoding(protocol, selector, requiredMethods, true);
        if (!methodTypeEncoding)
            [NSException raise:NSInvalidArgumentException format:@"Could not find method type encoding for method \"%s\"", sel_getName(selector)];

        NSMethodSignature *methodSignature = [NSMethodSignature signatureWithObjCTypes:methodTypeEncoding];

        initializeMethod(methodInfo, protocol, selector, methodSignature, false);
    }

    free(methods);
}

static void initializeMethods(_WKRemoteObjectInterface *interface, Protocol *protocol)
{
    unsigned conformingProtocolCount;
    auto conformingProtocols = protocol_copyProtocolList(protocol, &conformingProtocolCount);

    for (unsigned i = 0; i < conformingProtocolCount; ++i) {
        auto conformingProtocol = conformingProtocols[i];
        if (conformingProtocol == @protocol(NSObject))
            continue;
        initializeMethods(interface, conformingProtocol);
    }

    free(conformingProtocols);

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
    return [[[self alloc] initWithProtocol:protocol identifier:NSStringFromProtocol(protocol)] autorelease];
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

        if (auto replyInfo = selectorAndMethod.value.replyInfo)
            [result appendFormat:@"  reply block = (argument #%lu '%s') %@\n", static_cast<unsigned long>(replyInfo->replyPosition), replyInfo->replySignature.data(), descriptionForClasses(replyInfo->allowedReplyClasses)];
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
        if (!methodInfo.replyInfo)
            [NSException raise:NSInvalidArgumentException format:@"Selector \"%s\" does not have a reply block", sel_getName(selector)];

        if (argumentIndex >= methodInfo.replyInfo->allowedReplyClasses.size())
            [NSException raise:NSInvalidArgumentException format:@"Argument index %ld is out of range for reply block of selector \"%s\"", (unsigned long)argumentIndex, sel_getName(selector)];

        return methodInfo.replyInfo->allowedReplyClasses[argumentIndex];
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
    if (!_methods.contains(selector))
        return nil;

    const char* types = methodArgumentTypeEncodingForSelector(_protocol, selector);
    if (!types)
        return nil;

    return [NSMethodSignature signatureWithObjCTypes:types];
}

- (NSMethodSignature *)_methodSignatureForReplyBlockOfSelector:(SEL)selector
{
    auto it = _methods.find(selector);
    if (it  == _methods.end())
        return nil;

    auto& methodInfo = it->value;
    if (!methodInfo.replyInfo)
        return nil;

    return [NSMethodSignature signatureWithObjCTypes:methodInfo.replyInfo->replySignature.data()];
}

- (const Vector<HashSet<CFTypeRef>>&)_allowedArgumentClassesForSelector:(SEL)selector
{
    ASSERT(_methods.contains(selector));

    return _methods.find(selector)->value.allowedArgumentClasses;
}

- (const Vector<HashSet<CFTypeRef>>&)_allowedArgumentClassesForReplyBlockOfSelector:(SEL)selector
{
    ASSERT(_methods.contains(selector));

    return _methods.find(selector)->value.replyInfo->allowedReplyClasses;
}

@end
