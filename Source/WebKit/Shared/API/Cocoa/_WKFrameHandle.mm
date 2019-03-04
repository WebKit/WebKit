/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "_WKFrameHandleInternal.h"

@implementation _WKFrameHandle {
    API::ObjectStorage<API::FrameHandle> _frameHandle;
}

- (void)dealloc
{
    _frameHandle->~FrameHandle();

    [super dealloc];
}

- (BOOL)isEqual:(id)object
{
    if (object == self)
        return YES;

    if (![object isKindOfClass:[_WKFrameHandle self]])
        return NO;

    return _frameHandle->frameID() == ((_WKFrameHandle *)object)->_frameHandle->frameID();
}

- (NSUInteger)hash
{
    return _frameHandle->frameID();
}

- (uint64_t)_frameID
{
    return _frameHandle->frameID();
}

#pragma mark NSCopying protocol implementation

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

#pragma mark NSSecureCoding protocol implementation

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (id)initWithCoder:(NSCoder *)decoder
{
    if (!(self = [super init]))
        return nil;

    NSNumber *frameID = [decoder decodeObjectOfClass:[NSNumber self] forKey:@"frameID"];
    if (![frameID isKindOfClass:[NSNumber self]]) {
        [self release];
        return nil;
    }

    API::Object::constructInWrapper<API::FrameHandle>(self, frameID.unsignedLongLongValue, false);

    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:@(_frameHandle->frameID()) forKey:@"frameID"];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_frameHandle;
}

@end
