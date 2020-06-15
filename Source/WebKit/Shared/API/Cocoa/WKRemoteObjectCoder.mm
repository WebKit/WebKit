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
#import "WKRemoteObjectCoder.h"

#import "APIArray.h"
#import "APIData.h"
#import "APIDictionary.h"
#import "APINumber.h"
#import "APIString.h"
#import "NSInvocationSPI.h"
#import "_WKRemoteObjectInterfaceInternal.h"
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>
#import <wtf/SetForScope.h>
#import <wtf/text/CString.h>

static const char* const classNameKey = "$class";
static const char* const objectStreamKey = "$objectStream";
static const char* const stringKey = "$string";

static NSString * const selectorKey = @"selector";
static NSString * const typeStringKey = @"typeString";
static NSString * const isReplyBlockKey = @"isReplyBlock";

static RefPtr<API::Dictionary> createEncodedObject(WKRemoteObjectEncoder *, id);

@interface NSMethodSignature ()
- (NSString *)_typeString;
@end

@interface NSCoder ()
- (BOOL)validateClassSupportsSecureCoding:(Class)objectClass;
@end

@implementation WKRemoteObjectEncoder {
    RefPtr<API::Dictionary> _rootDictionary;
    API::Array* _objectStream;

    API::Dictionary* _currentDictionary;
}

- (id)init
{
    if (!(self = [super init]))
        return nil;

    _rootDictionary = API::Dictionary::create();
    _currentDictionary = _rootDictionary.get();

    return self;
}

#if ASSERT_ENABLED
- (void)dealloc
{
    ASSERT(_currentDictionary == _rootDictionary);

    [super dealloc];
}
#endif

- (API::Dictionary*)rootObjectDictionary
{
    return _rootDictionary.get();
}

static void ensureObjectStream(WKRemoteObjectEncoder *encoder)
{
    if (encoder->_objectStream)
        return;

    Ref<API::Array> objectStream = API::Array::create();
    encoder->_objectStream = objectStream.ptr();

    encoder->_rootDictionary->set(objectStreamKey, WTFMove(objectStream));
}

static void encodeToObjectStream(WKRemoteObjectEncoder *encoder, id value)
{
    ensureObjectStream(encoder);

    size_t position = encoder->_objectStream->size();
    encoder->_objectStream->elements().append(nullptr);

    auto encodedObject = createEncodedObject(encoder, value);
    ASSERT(!encoder->_objectStream->elements()[position]);
    encoder->_objectStream->elements()[position] = WTFMove(encodedObject);
}

static void encodeInvocationArguments(WKRemoteObjectEncoder *encoder, NSInvocation *invocation, NSUInteger firstArgument)
{
    NSMethodSignature *methodSignature = invocation.methodSignature;
    NSUInteger argumentCount = methodSignature.numberOfArguments;

    ASSERT(firstArgument <= argumentCount);

    for (NSUInteger i = firstArgument; i < argumentCount; ++i) {
        const char* type = [methodSignature getArgumentTypeAtIndex:i];

        switch (*type) {
        // double
        case 'd': {
            double value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // float
        case 'f': {
            float value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // short
        case 's': {
            short value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // unsigned short
        case 'S': {
            unsigned short value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // int
        case 'i': {
            int value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // unsigned
        case 'I': {
            unsigned value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // char
        case 'c': {
            char value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // unsigned char
        case 'C': {
            unsigned char value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // bool
        case 'B': {
            BOOL value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // long
        case 'l': {
            long value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // unsigned long
        case 'L': {
            unsigned long value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // long long
        case 'q': {
            long long value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // unsigned long long
        case 'Q': {
            unsigned long long value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // Objective-C object
        case '@': {
            id value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, value);
            break;
        }

        // struct
        case '{':
            if (!strcmp(type, @encode(NSRange))) {
                NSRange value;
                [invocation getArgument:&value atIndex:i];

                encodeToObjectStream(encoder, [NSValue valueWithRange:value]);
                break;
            } else if (!strcmp(type, @encode(CGSize))) {
                CGSize value;
                [invocation getArgument:&value atIndex:i];

                encodeToObjectStream(encoder, @(value.width));
                encodeToObjectStream(encoder, @(value.height));
                break;
            }
            FALLTHROUGH;

        default:
            [NSException raise:NSInvalidArgumentException format:@"Unsupported invocation argument type '%s'", type];
        }
    }
}

static void encodeInvocation(WKRemoteObjectEncoder *encoder, NSInvocation *invocation)
{
    NSMethodSignature *methodSignature = invocation.methodSignature;
    [encoder encodeObject:methodSignature._typeString forKey:typeStringKey];

    if ([invocation isKindOfClass:[NSBlockInvocation class]]) {
        [encoder encodeBool:YES forKey:isReplyBlockKey];
        encodeInvocationArguments(encoder, invocation, 1);
    } else {
        [encoder encodeObject:NSStringFromSelector(invocation.selector) forKey:selectorKey];
        encodeInvocationArguments(encoder, invocation, 2);
    }
}

static void encodeString(WKRemoteObjectEncoder *encoder, NSString *string)
{
    encoder->_currentDictionary->set(stringKey, API::String::create(string));
}

static void encodeObject(WKRemoteObjectEncoder *encoder, id object)
{
    ASSERT(object);

    if (![object conformsToProtocol:@protocol(NSSecureCoding)] && ![object isKindOfClass:[NSInvocation class]])
        [NSException raise:NSInvalidArgumentException format:@"%@ does not conform to NSSecureCoding", object];

    if (class_isMetaClass(object_getClass(object)))
        [NSException raise:NSInvalidArgumentException format:@"Class objects may not be encoded"];

    Class objectClass = [object classForCoder];
    if (!objectClass)
        [NSException raise:NSInvalidArgumentException format:@"-classForCoder returned nil for %@", object];

    encoder->_currentDictionary->set(classNameKey, API::String::create(class_getName(objectClass)));

    if ([object isKindOfClass:[NSInvocation class]]) {
        // We have to special case NSInvocation since we don't want to encode the target.
        encodeInvocation(encoder, object);
        return;
    }

    if (objectClass == [NSString class] || objectClass == [NSMutableString class]) {
        encodeString(encoder, object);
        return;
    }

    [object encodeWithCoder:encoder];
}

static RefPtr<API::Dictionary> createEncodedObject(WKRemoteObjectEncoder *encoder, id object)
{
    if (!object)
        return nil;

    Ref<API::Dictionary> dictionary = API::Dictionary::create();
    SetForScope<API::Dictionary*> dictionaryChange(encoder->_currentDictionary, dictionary.ptr());

    encodeObject(encoder, object);

    return WTFMove(dictionary);
}

- (void)encodeValueOfObjCType:(const char *)type at:(const void *)address
{
    switch (*type) {
    // double
    case 'd':
        encodeToObjectStream(self, @(*static_cast<const double*>(address)));
        break;

    // float
    case 'f':
        encodeToObjectStream(self, @(*static_cast<const float*>(address)));
        break;

    // short
    case 's':
        encodeToObjectStream(self, @(*static_cast<const short*>(address)));
        break;

    // unsigned short
    case 'S':
        encodeToObjectStream(self, @(*static_cast<const unsigned short*>(address)));
        break;

    // int
    case 'i':
        encodeToObjectStream(self, @(*static_cast<const int*>(address)));
        break;

    // unsigned
    case 'I':
        encodeToObjectStream(self, @(*static_cast<const unsigned*>(address)));
        break;

    // char
    case 'c':
        encodeToObjectStream(self, @(*static_cast<const char*>(address)));
        break;

    // unsigned char
    case 'C':
        encodeToObjectStream(self, @(*static_cast<const unsigned char*>(address)));
        break;

    // bool
    case 'B':
        encodeToObjectStream(self, @(*static_cast<const bool*>(address)));
        break;

    // long
    case 'l':
        encodeToObjectStream(self, @(*static_cast<const long*>(address)));
        break;

    // unsigned long
    case 'L':
        encodeToObjectStream(self, @(*static_cast<const unsigned long*>(address)));
        break;

    // long long
    case 'q':
        encodeToObjectStream(self, @(*static_cast<const long long*>(address)));
        break;

    // unsigned long long
    case 'Q':
        encodeToObjectStream(self, @(*static_cast<const unsigned long long*>(address)));
        break;

    // Objective-C object.
    case '@':
        encodeToObjectStream(self, *static_cast<const id*>(address));
        break;

    default:
        [NSException raise:NSInvalidArgumentException format:@"Unsupported type '%s'", type];
    }
}

- (BOOL)allowsKeyedCoding
{
    return YES;
}

static NSString *escapeKey(NSString *key)
{
    if (key.length && [key characterAtIndex:0] == '$')
        return [@"$" stringByAppendingString:key];

    return key;
}

- (void)encodeObject:(id)object forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), createEncodedObject(self, object));
}

- (void)encodeBytes:(const uint8_t *)bytes length:(NSUInteger)length forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), API::Data::create(bytes, length));
}

- (void)encodeBool:(BOOL)value forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), API::Boolean::create(value));
}

- (void)encodeInt:(int)value forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), API::UInt64::create(value));
}

- (void)encodeInt32:(int32_t)value forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), API::UInt64::create(value));
}

- (void)encodeInt64:(int64_t)value forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), API::UInt64::create(value));
}

- (void)encodeInteger:(NSInteger)intv forKey:(NSString *)key
{
    return [self encodeInt64:intv forKey:key];
}

- (void)encodeFloat:(float)value forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), API::Double::create(value));
}

- (void)encodeDouble:(double)value forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), API::Double::create(value));
}

- (BOOL)requiresSecureCoding
{
    return YES;
}

@end

@implementation WKRemoteObjectDecoder {
    RetainPtr<_WKRemoteObjectInterface> _interface;

    const API::Dictionary* _rootDictionary;
    const API::Dictionary* _currentDictionary;

    SEL _replyToSelector;

    const API::Array* _objectStream;
    size_t _objectStreamPosition;

    const HashSet<CFTypeRef>* _allowedClasses;
}

- (id)initWithInterface:(_WKRemoteObjectInterface *)interface rootObjectDictionary:(const API::Dictionary*)rootObjectDictionary replyToSelector:(SEL)replyToSelector
{
    if (!(self = [super init]))
        return nil;

    _interface = interface;

    _rootDictionary = rootObjectDictionary;
    _currentDictionary = _rootDictionary;

    _replyToSelector = replyToSelector;

    _objectStream = _rootDictionary->get<API::Array>(objectStreamKey);

    return self;
}

- (void)decodeValueOfObjCType:(const char *)type at:(void *)data
{
    switch (*type) {
    // double
    case 'd':
        *static_cast<double*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) doubleValue];
        break;

    // float
    case 'f':
        *static_cast<float*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) floatValue];
        break;

    // short
    case 's':
        *static_cast<short*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) shortValue];
        break;

    // unsigned short
    case 'S':
        *static_cast<unsigned short*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) unsignedShortValue];
        break;

    // int
    case 'i':
        *static_cast<int*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) intValue];
        break;

    // unsigned
    case 'I':
        *static_cast<unsigned*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) unsignedIntValue];
        break;

    // char
    case 'c':
        *static_cast<char*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) charValue];
        break;

    // unsigned char
    case 'C':
        *static_cast<unsigned char*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) unsignedCharValue];
        break;

    // bool
    case 'B':
        *static_cast<bool*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) boolValue];
        break;

    // long
    case 'l':
        *static_cast<long*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) longValue];
        break;

    // unsigned long
    case 'L':
        *static_cast<unsigned long*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) unsignedLongValue];
        break;

    // long long
    case 'q':
        *static_cast<long long*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) longLongValue];
        break;

    // unsigned long long
    case 'Q':
        *static_cast<unsigned long long*>(data) = [decodeObjectFromObjectStream(self, { (__bridge CFTypeRef)[NSNumber class] }) unsignedLongLongValue];
        break;

    default:
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Unsupported type '%s'", type];
    }
}

- (BOOL)allowsKeyedCoding
{
    return YES;
}

- (BOOL)containsValueForKey:(NSString *)key
{
    return _currentDictionary->map().contains(escapeKey(key));
}

- (id)decodeObjectForKey:(NSString *)key
{
    return [self decodeObjectOfClasses:nil forKey:key];
}

static id decodeObject(WKRemoteObjectDecoder *, const API::Dictionary*, const HashSet<CFTypeRef>& allowedClasses);

static id decodeObjectFromObjectStream(WKRemoteObjectDecoder *decoder, const HashSet<CFTypeRef>& allowedClasses)
{
    if (!decoder->_objectStream)
        return nil;

    if (decoder->_objectStreamPosition == decoder->_objectStream->size())
        return nil;

    const API::Dictionary* dictionary = decoder->_objectStream->at<API::Dictionary>(decoder->_objectStreamPosition++);

    return decodeObject(decoder, dictionary, allowedClasses);
}

static void checkIfClassIsAllowed(WKRemoteObjectDecoder *decoder, Class objectClass)
{
    auto* allowedClasses = decoder->_allowedClasses;
    if (!allowedClasses)
        return;

    if (allowedClasses->contains((__bridge CFTypeRef)objectClass))
        return;

    for (Class superclass = class_getSuperclass(objectClass); superclass; superclass = class_getSuperclass(superclass)) {
        if (allowedClasses->contains((__bridge CFTypeRef)superclass))
            return;
    }

    [NSException raise:NSInvalidUnarchiveOperationException format:@"Object of class \"%@\" is not allowed. Allowed classes are \"%@\"", objectClass, decoder.allowedClasses];
}

static void validateClass(WKRemoteObjectDecoder *decoder, Class objectClass)
{
    ASSERT(objectClass);

    checkIfClassIsAllowed(decoder, objectClass);

    // NSInvocation and NSBlockInvocation don't support NSSecureCoding, but we allow them anyway.
    if (objectClass == [NSInvocation class] || objectClass == [NSBlockInvocation class])
        return;

    if (![decoder validateClassSupportsSecureCoding:objectClass])
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Object of class \"%@\" does not support NSSecureCoding.", objectClass];
}

static void decodeInvocationArguments(WKRemoteObjectDecoder *decoder, NSInvocation *invocation, const Vector<HashSet<CFTypeRef>>& allowedArgumentClasses, NSUInteger firstArgument)
{
    NSMethodSignature *methodSignature = invocation.methodSignature;
    NSUInteger argumentCount = methodSignature.numberOfArguments;

    ASSERT(firstArgument <= argumentCount);

    for (NSUInteger i = firstArgument; i < argumentCount; ++i) {
        const char* type = [methodSignature getArgumentTypeAtIndex:i];

        switch (*type) {
        // double
        case 'd': {
            double value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) doubleValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // float
        case 'f': {
            float value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) floatValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // short
        case 's': {
            short value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) shortValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // unsigned short
        case 'S': {
            unsigned short value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) unsignedShortValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // int
        case 'i': {
            int value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) intValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // unsigned
        case 'I': {
            unsigned value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) unsignedIntValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // char
        case 'c': {
            char value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) charValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // unsigned char
        case 'C': {
            unsigned char value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) unsignedCharValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // bool
        case 'B': {
            bool value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) boolValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // long
        case 'l': {
            long value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) longValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // unsigned long
        case 'L': {
            unsigned long value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) unsignedLongValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // long long
        case 'q': {
            long long value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) longLongValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // unsigned long long
        case 'Q': {
            unsigned long long value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) unsignedLongLongValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }
            
        // Objective-C object
        case '@': {
            auto& allowedClasses = allowedArgumentClasses[i - firstArgument];

            id value = decodeObjectFromObjectStream(decoder, allowedClasses);
            [invocation setArgument:&value atIndex:i];

            // FIXME: Make sure the invocation doesn't outlive the value.
            break;
        }

        // struct
        case '{':
            if (!strcmp(type, @encode(NSRange))) {
                NSRange value = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSValue class] }) rangeValue];
                [invocation setArgument:&value atIndex:i];
                break;
            } else if (!strcmp(type, @encode(CGSize))) {
                CGSize value;
                value.width = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) doubleValue];
                value.height = [decodeObjectFromObjectStream(decoder, { (__bridge CFTypeRef)[NSNumber class] }) doubleValue];
                [invocation setArgument:&value atIndex:i];
                break;
            }
            FALLTHROUGH;

        default:
            [NSException raise:NSInvalidArgumentException format:@"Unsupported invocation argument type '%s' for argument %zu", type, (unsigned long)i];
        }
    }
}

static NSInvocation *decodeInvocation(WKRemoteObjectDecoder *decoder)
{
    SEL selector = nullptr;
    NSMethodSignature *localMethodSignature = nil;

    BOOL isReplyBlock = [decoder decodeBoolForKey:isReplyBlockKey];

    if (isReplyBlock) {
        if (!decoder->_replyToSelector)
            [NSException raise:NSInvalidUnarchiveOperationException format:@"%@: Received unknown reply block", decoder];

        localMethodSignature = [decoder->_interface _methodSignatureForReplyBlockOfSelector:decoder->_replyToSelector];
        if (!localMethodSignature)
            [NSException raise:NSInvalidUnarchiveOperationException format:@"Reply block for selector \"%s\" is not defined in the local interface", sel_getName(decoder->_replyToSelector)];
    } else {
        NSString *selectorString = [decoder decodeObjectOfClass:[NSString class] forKey:selectorKey];
        if (!selectorString)
            [NSException raise:NSInvalidUnarchiveOperationException format:@"Invocation had no selector"];

        selector = NSSelectorFromString(selectorString);
        ASSERT(selector);

        localMethodSignature = [decoder->_interface _methodSignatureForSelector:selector];
        if (!localMethodSignature)
            [NSException raise:NSInvalidUnarchiveOperationException format:@"Selector \"%@\" is not defined in the local interface", selectorString];
    }

    NSString *typeSignature = [decoder decodeObjectOfClass:[NSString class] forKey:typeStringKey];
    if (!typeSignature)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Invocation had no type signature"];

    NSMethodSignature *remoteMethodSignature = [NSMethodSignature signatureWithObjCTypes:typeSignature.UTF8String];
    if (![localMethodSignature isEqual:remoteMethodSignature])
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Local and remote method signatures are not equal for method \"%s\"", selector ? sel_getName(selector) : "(no selector)"];

    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:localMethodSignature];

    if (isReplyBlock) {
        const auto& allowedClasses = [decoder->_interface _allowedArgumentClassesForReplyBlockOfSelector:decoder->_replyToSelector];
        decodeInvocationArguments(decoder, invocation, allowedClasses, 1);
    } else {
        const auto& allowedClasses = [decoder->_interface _allowedArgumentClassesForSelector:selector];
        decodeInvocationArguments(decoder, invocation, allowedClasses, 2);

        [invocation setArgument:&selector atIndex:1];
    }

    return invocation;
}

static NSString *decodeString(WKRemoteObjectDecoder *decoder)
{
    API::String* string = decoder->_currentDictionary->get<API::String>(stringKey);
    if (!string)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"String missing"];

    return string->string();
}

static id decodeObject(WKRemoteObjectDecoder *decoder)
{
    API::String* classNameString = decoder->_currentDictionary->get<API::String>(classNameKey);
    if (!classNameString)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Class name missing"];

    CString className = classNameString->string().utf8();

    Class objectClass = objc_lookUpClass(className.data());
    if (!objectClass)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Class \"%s\" does not exist", className.data()];

    validateClass(decoder, objectClass);

    if (objectClass == [NSInvocation class] || objectClass == [NSBlockInvocation class])
        return decodeInvocation(decoder);

    if (objectClass == [NSString class])
        return decodeString(decoder);

    if (objectClass == [NSMutableString class])
        return [NSMutableString stringWithString:decodeString(decoder)];

    id result = [objectClass allocWithZone:decoder.zone];
    if (!result)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Class \"%s\" returned nil from +alloc while being decoded", className.data()];

    result = [result initWithCoder:decoder];
    if (!result)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Object of class \"%s\" returned nil from -initWithCoder: while being decoded", className.data()];

    result = [result awakeAfterUsingCoder:decoder];
    if (!result)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Object of class \"%s\" returned nil from -awakeAfterUsingCoder: while being decoded", className.data()];

    return [result autorelease];
}

static id decodeObject(WKRemoteObjectDecoder *decoder, const API::Dictionary* dictionary, const HashSet<CFTypeRef>& allowedClasses)
{
    if (!dictionary)
        return nil;

    SetForScope<const API::Dictionary*> dictionaryChange(decoder->_currentDictionary, dictionary);

    // If no allowed classes were listed, just use the currently allowed classes.
    if (allowedClasses.isEmpty())
        return decodeObject(decoder);

    SetForScope<const HashSet<CFTypeRef>*> allowedClassesChange(decoder->_allowedClasses, &allowedClasses);
    return decodeObject(decoder);
}

- (BOOL)decodeBoolForKey:(NSString *)key
{
    const API::Boolean* value = _currentDictionary->get<API::Boolean>(escapeKey(key));
    if (!value)
        return false;
    return value->value();
}

- (int)decodeIntForKey:(NSString *)key
{
    const API::UInt64* value = _currentDictionary->get<API::UInt64>(escapeKey(key));
    if (!value)
        return 0;
    return static_cast<int>(value->value());
}

- (int32_t)decodeInt32ForKey:(NSString *)key
{
    const API::UInt64* value = _currentDictionary->get<API::UInt64>(escapeKey(key));
    if (!value)
        return 0;
    return static_cast<int32_t>(value->value());
}

- (int64_t)decodeInt64ForKey:(NSString *)key
{
    const API::UInt64* value = _currentDictionary->get<API::UInt64>(escapeKey(key));
    if (!value)
        return 0;
    return value->value();
}

- (NSInteger)decodeIntegerForKey:(NSString *)key
{
    return [self decodeInt64ForKey:key];
}

- (float)decodeFloatForKey:(NSString *)key
{
    const API::Double* value = _currentDictionary->get<API::Double>(escapeKey(key));
    if (!value)
        return 0;
    return value->value();
}

- (double)decodeDoubleForKey:(NSString *)key
{
    const API::Double* value = _currentDictionary->get<API::Double>(escapeKey(key));
    if (!value)
        return 0;
    return value->value();
}

- (const uint8_t *)decodeBytesForKey:(NSString *)key returnedLength:(NSUInteger *)length
{
    auto* data = _currentDictionary->get<API::Data>(escapeKey(key));
    if (!data || !data->size()) {
        *length = 0;
        return nullptr;
    }

    *length = data->size();
    return data->bytes();
}

- (BOOL)requiresSecureCoding
{
    return YES;
}

- (id)decodeObjectOfClasses:(NSSet *)classes forKey:(NSString *)key
{
    HashSet<CFTypeRef> allowedClasses;
    for (Class allowedClass in classes)
        allowedClasses.add((__bridge CFTypeRef)allowedClass);

    return decodeObject(self, _currentDictionary->get<API::Dictionary>(escapeKey(key)), allowedClasses);
}

- (NSSet *)allowedClasses
{
    if (!_allowedClasses)
        return [NSSet set];

    auto result = adoptNS([[NSMutableSet alloc] init]);
    for (auto allowedClass : *_allowedClasses)
        [result addObject:(__bridge Class)allowedClass];

    return result.autorelease();
}

@end
