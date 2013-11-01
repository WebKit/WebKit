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

#import "WKData.h"
#import "WKMutableDictionary.h"
#import "WKNumber.h"
#import "WKRetainPtr.h"
#import "WKStringCF.h"

#if WK_API_ENABLED

@interface NSMethodSignature (Details)
- (NSString *)_typeString;
@end

@implementation WKRemoteObjectEncoder {
    WKRetainPtr<WKMutableDictionaryRef> _rootDictionary;
    WKMutableDictionaryRef _currentDictionary;
}

- (id)init
{
    if (!(self = [super init]))
        return nil;

    _rootDictionary = adoptWK(WKMutableDictionaryCreate());
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

- (void)_encodeInvocation:(NSInvocation *)invocation forKey:(NSString *)key
{
    [self _encodeObjectForKey:key usingBlock:^{
        NSMethodSignature *methodSignature = invocation.methodSignature;
        [self encodeObject:methodSignature._typeString forKey:@"typeString"];
        [self encodeObject:NSStringFromSelector(invocation.selector) forKey:@"selector"];

        // FIXME: Encode arguments as well.
    }];
}

- (void)encodeBytes:(const uint8_t *)bytes length:(NSUInteger)length forKey:(NSString *)key
{
    auto data = adoptWK(WKDataCreate(bytes, length));
    auto keyString = adoptWK(WKStringCreateWithCFString((CFStringRef)key));

    WKDictionarySetItem(_currentDictionary, keyString.get(), data.get());
}

- (void)_encodeObjectForKey:(NSString *)key usingBlock:(void (^)())block
{
    auto dictionary = adoptWK(WKMutableDictionaryCreate());
    auto keyString = adoptWK(WKStringCreateWithCFString((CFStringRef)key));

    WKDictionarySetItem(_currentDictionary, keyString.get(), dictionary.get());

    WKMutableDictionaryRef previousDictionary = _currentDictionary;
    block();
    _currentDictionary = previousDictionary;
}

@end

#endif // WK_API_ENABLED
