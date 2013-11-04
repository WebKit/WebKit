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
#import "WKNSArray.h"

#if WK_API_ENABLED

using namespace WebKit;

@implementation WKNSArray {
    std::aligned_storage<sizeof(ImmutableArray), std::alignment_of<ImmutableArray>::value>::type _array;
}

- (void)dealloc
{
    reinterpret_cast<ImmutableArray*>(&_array)->~ImmutableArray();

    [super dealloc];
}

#pragma mark NSArray primitive methods

- (NSUInteger)count
{
    return reinterpret_cast<ImmutableArray*>(&_array)->size();
}

- (id)objectAtIndex:(NSUInteger)i
{
    APIObject* object = reinterpret_cast<ImmutableArray*>(&_array)->at(i);
    return object ? object->wrapper() : [NSNull null];
}

#pragma mark NSCopying protocol implementation

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

#pragma mark WKObject protocol implementation

- (APIObject&)_apiObject
{
    return *reinterpret_cast<APIObject*>(&_array);
}

@end

#endif // WK_API_ENABLED
