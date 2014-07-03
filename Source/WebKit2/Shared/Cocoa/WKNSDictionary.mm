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
#import "WKNSDictionary.h"

#if WK_API_ENABLED

#import "WKNSArray.h"

using namespace WebKit;

@implementation WKNSDictionary {
    API::ObjectStorage<ImmutableDictionary> _dictionary;
}

- (void)dealloc
{
    _dictionary->~ImmutableDictionary();

    [super dealloc];
}

#pragma mark NSDictionary primitive methods

- (instancetype)initWithObjects:(const id [])objects forKeys:(const id <NSCopying> [])keys count:(NSUInteger)count
{
    ASSERT_NOT_REACHED();
    return [super initWithObjects:objects forKeys:keys count:count];
}

- (NSUInteger)count
{
    return _dictionary->size();
}

- (id)objectForKey:(id)key
{
    if (![key isKindOfClass:[NSString class]])
        return nil;

    bool exists;
    API::Object* value = _dictionary->get((NSString *)key, exists);
    if (!exists)
        return nil;

    return value ? value->wrapper() : [NSNull null];
}

- (NSEnumerator *)keyEnumerator
{
    return [wrapper(*_dictionary->keys()) objectEnumerator];
}

#pragma mark NSCopying protocol implementation

- (id)copyWithZone:(NSZone *)zone
{
    if (!_dictionary->isMutable())
        return [self retain];

    auto map = _dictionary->map();
    return ImmutableDictionary::create(WTF::move(map)).release().leakRef()->wrapper();
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_dictionary;
}

@end

#endif // WK_API_ENABLED
