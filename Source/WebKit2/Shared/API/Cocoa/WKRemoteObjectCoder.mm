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

#if WK_API_ENABLED

#import "APIArray.h"
#import "APIData.h"
#import "APINumber.h"
#import "APIString.h"
#import "MutableDictionary.h"
#import "_WKRemoteObjectInterfaceInternal.h"
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>
#import <wtf/TemporaryChange.h>
#import <wtf/text/CString.h>

static const char* const classNameKey = "$class";
static const char* const objectStreamKey = "$objectStream";

static NSString * const selectorKey = @"selector";
static NSString * const typeStringKey = @"typeString";

using namespace WebKit;

static PassRefPtr<ImmutableDictionary> createEncodedObject(WKRemoteObjectEncoder *, id);

@interface NSMethodSignature (Details)
- (NSString *)_typeString;
@end

@interface NSCoder (Details)
- (void)validateClassSupportsSecureCoding:(Class)objectClass;
@end

@implementation WKRemoteObjectEncoder {
    RefPtr<MutableDictionary> _rootDictionary;
    API::Array* _objectStream;

    MutableDictionary* _currentDictionary;
}

- (id)init
{
    if (!(self = [super init]))
        return nil;

    _rootDictionary = MutableDictionary::create();
    _currentDictionary = _rootDictionary.get();

    return self;
}

#if !ASSERT_DISABLED
- (void)dealloc
{
    ASSERT(_currentDictionary == _rootDictionary);

    [super dealloc];
}
#endif

- (ImmutableDictionary*)rootObjectDictionary
{
    return _rootDictionary.get();
}

static void ensureObjectStream(WKRemoteObjectEncoder *encoder)
{
    if (encoder->_objectStream)
        return;

    RefPtr<API::Array> objectStream = API::Array::create();
    encoder->_objectStream = objectStream.get();

    encoder->_rootDictionary->set(objectStreamKey, objectStream.release());
}

static void encodeToObjectStream(WKRemoteObjectEncoder *encoder, id value)
{
    ensureObjectStream(encoder);

    size_t position = encoder->_objectStream->size();
    encoder->_objectStream->elements().append(nullptr);

    RefPtr<ImmutableDictionary> encodedObject = createEncodedObject(encoder, value);
    ASSERT(!encoder->_objectStream->elements()[position]);
    encoder->_objectStream->elements()[position] = encodedObject.release();
}

static void encodeInvocation(WKRemoteObjectEncoder *encoder, NSInvocation *invocation)
{
    NSMethodSignature *methodSignature = invocation.methodSignature;
    [encoder encodeObject:methodSignature._typeString forKey:typeStringKey];
    [encoder encodeObject:NSStringFromSelector(invocation.selector) forKey:selectorKey];

    NSUInteger argumentCount = methodSignature.numberOfArguments;

    // The invocation should always have have self and _cmd arguments.
    ASSERT(argumentCount >= 2);

    // We ignore self and _cmd.
    for (NSUInteger i = 2; i < argumentCount; ++i) {
        const char* type = [methodSignature getArgumentTypeAtIndex:i];

        switch (*type) {
        // double
        case 'd': {
            double value;
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

        // char
        case 'c': {
            char value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // bool
        case 'B': {
            BOOL value;
            [invocation getArgument:&value atIndex:i];;

            encodeToObjectStream(encoder, @(value));
            break;
        }

        // NSInteger
        case 'q': {
            NSInteger value;
            [invocation getArgument:&value atIndex:i];;

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

        default:
            [NSException raise:NSInvalidArgumentException format:@"Unsupported invocation argument type '%s'", type];
        }
    }
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

    [object encodeWithCoder:encoder];
}

static PassRefPtr<ImmutableDictionary> createEncodedObject(WKRemoteObjectEncoder *encoder, id object)
{
    if (!object)
        return nil;

    RefPtr<MutableDictionary> dictionary = MutableDictionary::create();
    TemporaryChange<MutableDictionary*> dictionaryChange(encoder->_currentDictionary, dictionary.get());

    encodeObject(encoder, object);

    return dictionary;
}

- (void)encodeValueOfObjCType:(const char *)type at:(const void *)address
{
    switch (*type) {
    // int
    case 'i':
        encodeToObjectStream(self, @(*static_cast<const int*>(address)));
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

- (void)encodeInt64:(int64_t)value forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), API::UInt64::create(value));
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

    const ImmutableDictionary* _rootDictionary;
    const ImmutableDictionary* _currentDictionary;

    const API::Array* _objectStream;
    size_t _objectStreamPosition;

    NSSet *_allowedClasses;
}

- (id)initWithInterface:(_WKRemoteObjectInterface *)interface rootObjectDictionary:(const WebKit::ImmutableDictionary*)rootObjectDictionary
{
    if (!(self = [super init]))
        return nil;

    _interface = interface;

    _rootDictionary = rootObjectDictionary;
    _currentDictionary = _rootDictionary;

    _objectStream = _rootDictionary->get<API::Array>(objectStreamKey);

    return self;
}

- (void)decodeValueOfObjCType:(const char *)type at:(void *)data
{
    switch (*type) {
    // int
    case 'i':
        *static_cast<int*>(data) = [decodeObjectFromObjectStream(self, [NSSet setWithObject:[NSNumber class]]) intValue];
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

static id decodeObject(WKRemoteObjectDecoder *, const ImmutableDictionary*, NSSet *allowedClasses);

static id decodeObjectFromObjectStream(WKRemoteObjectDecoder *decoder, NSSet *allowedClasses)
{
    if (!decoder->_objectStream)
        return nil;

    if (decoder->_objectStreamPosition == decoder->_objectStream->size())
        return nil;

    const ImmutableDictionary* dictionary = decoder->_objectStream->at<ImmutableDictionary>(decoder->_objectStreamPosition++);

    return decodeObject(decoder, dictionary, allowedClasses);
}

static void checkIfClassIsAllowed(WKRemoteObjectDecoder *decoder, Class objectClass)
{
    NSSet *allowedClasses = decoder->_allowedClasses;

    // Check if the class or any of its superclasses are in the allowed classes set.
    for (Class cls = objectClass; cls; cls = class_getSuperclass(cls)) {
        if ([allowedClasses containsObject:cls])
            return;
    }

    [NSException raise:NSInvalidUnarchiveOperationException format:@"Object of class \"%@\" is not allowed. Allowed classes are \"%@\"", objectClass, allowedClasses];
}

static void validateClass(WKRemoteObjectDecoder *decoder, Class objectClass)
{
    ASSERT(objectClass);

    checkIfClassIsAllowed(decoder, objectClass);

    // NSInvocation doesn't support NSSecureCoding, but we allow it anyway.
    if (objectClass == [NSInvocation class])
        return;

    [decoder validateClassSupportsSecureCoding:objectClass];
}

static void decodeInvocationArguments(WKRemoteObjectDecoder *decoder, NSInvocation *invocation, const Vector<RetainPtr<NSSet>>& allowedArgumentClasses)
{
    NSMethodSignature *methodSignature = invocation.methodSignature;
    NSUInteger argumentCount = methodSignature.numberOfArguments;

    // The invocation should always have have self and _cmd arguments.
    ASSERT(argumentCount >= 2);

    // We ignore self and _cmd.
    for (NSUInteger i = 2; i < argumentCount; ++i) {
        const char* type = [methodSignature getArgumentTypeAtIndex:i];

        switch (*type) {
        // double
        case 'd': {
            double value = [decodeObjectFromObjectStream(decoder, [NSSet setWithObject:[NSNumber class]]) doubleValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // int
        case 'i': {
            int value = [decodeObjectFromObjectStream(decoder, [NSSet setWithObject:[NSNumber class]]) intValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // char
        case 'c': {
            char value = [decodeObjectFromObjectStream(decoder, [NSSet setWithObject:[NSNumber class]]) charValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // bool
        case 'B': {
            bool value = [decodeObjectFromObjectStream(decoder, [NSSet setWithObject:[NSNumber class]]) boolValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // NSInteger
        case 'q': {
            NSInteger value = [decodeObjectFromObjectStream(decoder, [NSSet setWithObject:[NSNumber class]]) integerValue];
            [invocation setArgument:&value atIndex:i];
            break;
        }

        // Objective-C object
        case '@': {
            NSSet *allowedClasses = allowedArgumentClasses[i - 2].get();

            id value = decodeObjectFromObjectStream(decoder, allowedClasses);
            [invocation setArgument:&value atIndex:i];

            // FIXME: Make sure the invocation doesn't outlive the value.
            break;
        }

        default:
            [NSException raise:NSInvalidArgumentException format:@"Unsupported invocation argument type '%s' for argument %zu", type, (unsigned long)i];
        }
    }
}

static NSInvocation *decodeInvocation(WKRemoteObjectDecoder *decoder)
{
    NSString *selectorString = [decoder decodeObjectOfClass:[NSString class] forKey:selectorKey];
    if (!selectorString)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Invocation had no selector"];

    SEL selector = NSSelectorFromString(selectorString);
    ASSERT(selector);

    NSMethodSignature *localMethodSignature = [decoder->_interface _methodSignatureForSelector:selector];
    if (!localMethodSignature)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Selector \"%@\" is not defined in the local interface", selectorString];

    NSString *typeSignature = [decoder decodeObjectOfClass:[NSString class] forKey:typeStringKey];
    if (!typeSignature)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Invocation had no type signature"];

    NSMethodSignature *remoteMethodSignature = [NSMethodSignature signatureWithObjCTypes:typeSignature.UTF8String];
    if (![localMethodSignature isEqual:remoteMethodSignature])
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Local and remote method signatures are not equal for method \"%@\"", selectorString];

    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:localMethodSignature];

    const auto& allowedClasses = [decoder->_interface _allowedArgumentClassesForSelector:selector];
    decodeInvocationArguments(decoder, invocation, allowedClasses);

    [invocation setArgument:&selector atIndex:1];
    return invocation;
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

    if (objectClass == [NSInvocation class])
        return decodeInvocation(decoder);

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

static id decodeObject(WKRemoteObjectDecoder *decoder, const ImmutableDictionary* dictionary, NSSet *allowedClasses)
{
    if (!dictionary)
        return nil;

    TemporaryChange<const ImmutableDictionary*> dictionaryChange(decoder->_currentDictionary, dictionary);

    // If no allowed classes were listed, just use the currently allowed classes.
    if (!allowedClasses)
        return decodeObject(decoder);

    TemporaryChange<NSSet *> allowedClassesChange(decoder->_allowedClasses, allowedClasses);
    return decodeObject(decoder);
}

- (BOOL)decodeBoolForKey:(NSString *)key
{
    const API::Boolean* value = _currentDictionary->get<API::Boolean>(escapeKey(key));
    if (!value)
        return false;
    return value->value();
}

- (int64_t)decodeInt64ForKey:(NSString *)key
{
    const API::UInt64* value = _currentDictionary->get<API::UInt64>(escapeKey(key));
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
    return decodeObject(self, _currentDictionary->get<ImmutableDictionary>(escapeKey(key)), classes);
}

- (NSSet *)allowedClasses
{
    return _allowedClasses;
}

@end

#endif // WK_API_ENABLED
