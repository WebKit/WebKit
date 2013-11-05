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
#import <wtf/TemporaryChange.h>

#if WK_API_ENABLED

using namespace WebKit;

@interface NSMethodSignature (Details)
- (NSString *)_typeString;
@end

@implementation WKRemoteObjectEncoder {
    RefPtr<MutableDictionary> _rootDictionary;
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

- (BOOL)allowsKeyedCoding
{
    return YES;
}

- (void)encodeObject:(id)object forKey:(NSString *)key
{
    if ([object isKindOfClass:[NSInvocation class]]) {
        // We have to special case NSInvocation since we don't want to encode the target.
        [self _encodeInvocation:object forKey:key];
        return;
    }

    if (![object conformsToProtocol:@protocol(NSSecureCoding)])
        [NSException raise:NSInvalidArgumentException format:@"%@ does not conform to NSSecureCoding", object];

    [self _encodeObjectForKey:key usingBlock:^{
        [object encodeWithCoder:self];
    }];
}

static NSString *escapeKey(NSString *key)
{
    if (key.length && [key characterAtIndex:0] == '$')
        return [@"$" stringByAppendingString:key];

    return key;
}

- (void)_encodeInvocation:(NSInvocation *)invocation forKey:(NSString *)key
{
    [self _encodeObjectForKey:key usingBlock:^{
        NSMethodSignature *methodSignature = invocation.methodSignature;
        [self encodeObject:methodSignature._typeString forKey:@"typeString"];
        [self encodeObject:NSStringFromSelector(invocation.selector) forKey:@"selector"];

        NSUInteger argumentCount = methodSignature.numberOfArguments;

        // The invocation should always have have self and _cmd arguments.
        ASSERT(argumentCount >= 2);

        RefPtr<MutableArray> arguments = MutableArray::create();

        // We ignore self and _cmd.
        for (NSUInteger i = 2; i < argumentCount; ++i) {
            const char* type = [methodSignature getArgumentTypeAtIndex:i];

            switch (*type) {
            // double
            case 'i': {
                int value;
                [invocation getArgument:&value atIndex:i];

                arguments->append(WebUInt64::create(value));
                break;
            }

            // int
            case 'd': {
                double value;
                [invocation getArgument:&value atIndex:i];

                arguments->append(WebDouble::create(value));
                break;
            }

            // Objective-C object
            case '@': {
                id value;
                [invocation getArgument:&value atIndex:i];

                arguments->append([self _encodedObjectUsingBlock:^{
                    [value encodeWithCoder:self];
                }]);
                break;
            }

            default:
                [NSException raise:NSInvalidArgumentException format:@"Unsupported invocation argument type '%s'", type];
            }
        }

        _currentDictionary->set("arguments", arguments.release());
    }];
}

- (void)encodeBytes:(const uint8_t *)bytes length:(NSUInteger)length forKey:(NSString *)key
{
    _currentDictionary->set(escapeKey(key), WebData::create(bytes, length));
}

- (RefPtr<MutableDictionary>)_encodedObjectUsingBlock:(void (^)())block
{
    RefPtr<MutableDictionary> dictionary = MutableDictionary::create();

    TemporaryChange<MutableDictionary*> dictionaryChange(_currentDictionary, dictionary.get());
    block();

    return dictionary;
}

- (void)_encodeObjectForKey:(NSString *)key usingBlock:(void (^)())block
{
    _currentDictionary->set(escapeKey(key), [self _encodedObjectUsingBlock:block]);
}

@end

#endif // WK_API_ENABLED
