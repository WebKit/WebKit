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
#import "WKNSObjectExtras.h"

#if WK_API_ENABLED

#import "WKBackForwardListItemInternal.h"
#import "WebBackForwardListItem.h"
#import <wtf/RefPtr.h>

using namespace WebKit;

@interface WKObject : NSObject

- (id)initWithAPIObject:(WebKit::APIObject&)object;

@end

@implementation WKObject {
    RefPtr<APIObject> _object;
}

- (id)initWithAPIObject:(APIObject&)object
{
    if (!(self = [super init]))
        return nil;

    _object = &object;

    return self;
}

#pragma mark - NSObject protocol implementation

- (BOOL)isEqual:(id)object
{
    if ([object class] != [self class])
        return NO;

    return _object == ((WKObject *)object)->_object;
}

- (NSUInteger)hash
{
    return (NSUInteger)_object.get();
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@: %p; object = %p; type = %d>", NSStringFromClass([self class]), self, _object.get(), _object->type()];
}

@end

#pragma mark -

@implementation NSObject (WKExtras)

+ (id)_web_objectWithAPIObject:(APIObject*)object
{
    if (!object)
        return nil;

    switch (object->type()) {
    case APIObject::TypeBackForwardListItem:
        return [[[WKBackForwardListItem alloc] _initWithItem:*static_cast<WebBackForwardListItem*>(object)] autorelease];

    default:
        return [[[WKObject alloc] initWithAPIObject:*object] autorelease];
    }
}

@end

#endif // WK_API_ENABLED
