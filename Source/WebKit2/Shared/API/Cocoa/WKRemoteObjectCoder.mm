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

#import "MutableArray.h"
#import "MutableDictionary.h"
#import "WebData.h"
#import "WebNumber.h"
#import "WebString.h"
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>
#import <wtf/TemporaryChange.h>
#import <wtf/text/CString.h>

#if WK_API_ENABLED

const char* const classNameKey = "$class";
const char* const objectStreamKey = "$objectStream";

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
    MutableArray* _objectStream;

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

    RefPtr<MutableArray> objectStream = MutableArray::create();
    encoder->_objectStream = objectStream.get();

    encoder->_rootDictionary->set(objectStreamKey, objectStream.release());
}

static void encodeToObjectStream(WKRemoteObjectEncoder *encoder, double value)
{
    ensureObjectStream(encoder);

    encoder->_objectStream->append(WebDouble::create(value));
}

static void encodeToObjectStream(WKRemoteObjectEncoder *encoder, int value)
{
    ensureObjectStream(encoder);

    encoder->_objectStream->append(WebUInt64::create(value));
}

static void encodeToObjectStream(WKRemoteObjectEncoder *encoder, id value)
{
    ensureObjectStream(encoder);

    encoder->_objectStream->append(createEncodedObject(encoder, value));
}

static void encodeInvocation(WKRemoteObjectEncoder *encoder, NSInvocation *invocation)
{
    NSMethodSignature *methodSignature = invocation.methodSignature;
    [encoder encodeObject:methodSignature._typeString forKey:@"typeString"];
    [encoder encodeObject:NSStringFromSelector(invocation.selector) forKey:@"selector"];

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

            encodeToObjectStream(encoder, value);
            break;
        }

        // int
        case 'i': {
            int value;
            [invocation getArgument:&value atIndex:i];

            encodeToObjectStream(encoder, value);
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
    
    if ([object isKindOfClass:[NSInvocation class]]) {
        // We have to special case NSInvocation since we don't want to encode the target.
        encodeInvocation(encoder, object);
        return;
    }

    if (![object conformsToProtocol:@protocol(NSSecureCoding)])
        [NSException raise:NSInvalidArgumentException format:@"%@ does not conform to NSSecureCoding", object];

    if (class_isMetaClass(object_getClass(object)))
        [NSException raise:NSInvalidArgumentException format:@"Class objects may not be encoded"];

    Class objectClass = [object classForCoder];
    if (!objectClass)
        [NSException raise:NSInvalidArgumentException format:@"-classForCoder returned nil for %@", object];

    encoder->_currentDictionary->set(classNameKey, WebString::create(class_getName(objectClass)));
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
        encodeToObjectStream(self, *static_cast<const int*>(address));
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
    _currentDictionary->set(escapeKey(key), WebData::create(bytes, length));
}

- (void)encodeBool:(BOOL)value forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), WebBoolean::create(value));
}

- (void)encodeInt64:(int64_t)value forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), WebUInt64::create(value));
}

- (void)encodeDouble:(double)value forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), WebDouble::create(value));
}

@end

@implementation WKRemoteObjectDecoder {
    RetainPtr<WKRemoteObjectInterface> _interface;

    const ImmutableDictionary* _rootDictionary;
    const ImmutableDictionary* _currentDictionary;

    NSSet *_allowedClasses;
}

- (id)initWithInterface:(WKRemoteObjectInterface *)interface rootObjectDictionary:(const WebKit::ImmutableDictionary*)rootObjectDictionary
{
    if (!(self = [super init]))
        return nil;

    _interface = interface;

    _rootDictionary = rootObjectDictionary;
    _currentDictionary = _rootDictionary;

    return self;
}

- (BOOL)allowsKeyedCoding
{
    return YES;
}

- (BOOL)containsValueForKey:(NSString *)key
{
    return _currentDictionary->map().contains(escapeKey(key));
}

- (const uint8_t *)decodeBytesForKey:(NSString *)key returnedLength:(NSUInteger *)length
{
    WebData* data = _currentDictionary->get<WebData>(escapeKey(key));
    if (!data || !data->size())
        return nullptr;

    *length = data->size();
    return data->bytes();
}

- (BOOL)requiresSecureCoding
{
    return YES;
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

static NSInvocation *decodeInvocation(WKRemoteObjectDecoder *decoder)
{
    // FIXME: Implement this.
    return nil;
}

static id decodeObject(WKRemoteObjectDecoder *decoder)
{
    WebString* classNameString = decoder->_currentDictionary->get<WebString>(classNameKey);
    if (!classNameString)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Class name missing"];

    CString className = classNameString->string().utf8();

    Class objectClass = objc_lookUpClass(className.data());
    if (!objectClass)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Class \"%s\" does not exist", className.data()];

    validateClass(decoder, objectClass);

    if (objectClass == [NSInvocation class])
        return decodeInvocation(decoder);

    RetainPtr<id> result = [objectClass allocWithZone:decoder.zone];
    if (!result)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Class \"%s\" returned nil from +alloc while being decoded", className.data()];

    result = [result initWithCoder:decoder];
    if (!result)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Object of class \"%s\" returned nil from -initWithCoder: while being decoded", className.data()];

    result = [result awakeAfterUsingCoder:decoder];
    if (!result)
        [NSException raise:NSInvalidUnarchiveOperationException format:@"Object of class \"%s\" returned nil from -awakeAfterUsingCoder: while being decoded", className.data()];

    return [result.leakRef() autorelease];
}

static id decodeObject(WKRemoteObjectDecoder *decoder, const ImmutableDictionary* dictionary)
{
    if (!dictionary)
        return nil;

    TemporaryChange<const ImmutableDictionary*> dictionaryChange(decoder->_currentDictionary, dictionary);

    return decodeObject(decoder);
}

- (id)decodeObjectOfClasses:(NSSet *)classes forKey:(NSString *)key
{
    TemporaryChange<NSSet *> allowedClassesChange(_allowedClasses, classes);

    return decodeObject(self, _currentDictionary->get<ImmutableDictionary>(escapeKey(key)));
}

- (NSSet *)allowedClasses
{
    return _allowedClasses;
}

@end

#endif // WK_API_ENABLED
