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

#if WK_API_ENABLED

#import <objc/runtime.h>
#import <wtf/HashMap.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

extern "C"
const char *_protocol_getMethodTypeEncoding(Protocol *p, SEL sel, BOOL isRequiredMethod, BOOL isInstanceMethod);

@interface NSMethodSignature (WKDetails)
- (Class)_classForObjectAtArgumentIndex:(NSInteger)idx;
@end

struct MethodInfo {
    Vector<HashSet<Class>> allowedArgumentClasses;

    // FIXME: This should have allowed reply argument classes too.
};

@implementation _WKRemoteObjectInterface {
    RetainPtr<NSString> _identifier;

    HashMap<SEL, MethodInfo> _methods;
}

static bool isContainerClass(Class objectClass)
{
    // FIXME: Add more classes here if needed.
    static LazyNeverDestroyed<HashSet<Class>> containerClasses;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        containerClasses.construct(std::initializer_list<Class> { [NSArray class], [NSDictionary class] });
    });

    return containerClasses->contains(objectClass);
}

static HashSet<Class>& propertyListClasses()
{
    static LazyNeverDestroyed<HashSet<Class>> propertyListClasses;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        propertyListClasses.construct(std::initializer_list<Class> { [NSArray class], [NSDictionary class], [NSNumber class], [NSString class] });
    });

    return propertyListClasses;
}

static void initializeMethod(MethodInfo& methodInfo, NSMethodSignature *methodSignature, bool forReplyBlock)
{
    Vector<HashSet<Class>> allowedClasses;

    NSUInteger firstArgument = forReplyBlock ? 1 : 2;
    NSUInteger argumentCount = methodSignature.numberOfArguments;

    for (NSUInteger i = firstArgument; i < argumentCount; ++i) {
        const char* argumentType = [methodSignature getArgumentTypeAtIndex:i];

        if (*argumentType != '@') {
            // This is a non-object type; we won't allow any classes to be decoded for it.
            allowedClasses.uncheckedAppend({ });
            continue;
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

        allowedClasses.append({ objectClass });
    }

    methodInfo.allowedArgumentClasses = WTF::move(allowedClasses);
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

        initializeMethod(methodInfo, methodSignature, false);
    }

    free(methods);
}

static void initializeMethods(_WKRemoteObjectInterface *interface, Protocol *protocol)
{
    unsigned conformingProtocolCount;
    Protocol** conformingProtocols = protocol_copyProtocolList(interface->_protocol, &conformingProtocolCount);

    for (unsigned i = 0; i < conformingProtocolCount; ++i) {
        Protocol* conformingProtocol = conformingProtocols[i];

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

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; protocol = \"%@\"; identifier = \"%@\">", NSStringFromClass(self.class), self, _identifier.get(), NSStringFromProtocol(_protocol)];
}

static HashSet<Class>& classesForSelectorArgument(_WKRemoteObjectInterface *interface, SEL selector, NSUInteger argumentIndex)
{
    auto it = interface->_methods.find(selector);
    if (it == interface->_methods.end())
        [NSException raise:NSInvalidArgumentException format:@"Interface does not contain selector \"%s\"", sel_getName(selector)];

    MethodInfo& methodInfo = it->value;
    auto& allowedArgumentClasses = methodInfo.allowedArgumentClasses;

    if (argumentIndex >= allowedArgumentClasses.size())
        [NSException raise:NSInvalidArgumentException format:@"Argument index %ld is out of range for selector \"%s\"", (unsigned long)argumentIndex, sel_getName(selector)];

    return allowedArgumentClasses[argumentIndex];
}

- (NSSet *)classesForSelector:(SEL)selector argumentIndex:(NSUInteger)argumentIndex
{
    auto result = adoptNS([[NSMutableSet alloc] init]);

    for (auto allowedClass : classesForSelectorArgument(self, selector, argumentIndex))
        [result addObject:allowedClass];

    return result.autorelease();
}

- (void)setClasses:(NSSet *)classes forSelector:(SEL)selector argumentIndex:(NSUInteger)argumentIndex
{
    HashSet<Class> allowedClasses;
    for (Class allowedClass in classes)
        allowedClasses.add(allowedClass);

    classesForSelectorArgument(self, selector, argumentIndex) = WTF::move(allowedClasses);
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

- (const Vector<HashSet<Class>>&)_allowedArgumentClassesForSelector:(SEL)selector
{
    ASSERT(_methods.contains(selector));

    return _methods.find(selector)->value.allowedArgumentClasses;
}

@end

#endif // WK_API_ENABLED
