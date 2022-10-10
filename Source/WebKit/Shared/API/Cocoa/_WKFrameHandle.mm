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

#import <WebCore/FrameIdentifier.h>
#import <WebCore/WebCoreObjCExtras.h>

@implementation _WKFrameHandle

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKFrameHandle.class, self))
        return;

    _frameHandle->~FrameHandle();

    [super dealloc];
}

- (BOOL)isEqual:(id)object
{
    if (object == self)
        return YES;

    if (![object isKindOfClass:[_WKFrameHandle class]])
        return NO;

    return _frameHandle->frameID() == ((_WKFrameHandle *)object)->_frameHandle->frameID();
}

- (NSUInteger)hash
{
    return _frameHandle->frameID().object().toUInt64();
}

- (uint64_t)frameID
{
    return _frameHandle->frameID().object().toUInt64();
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

    NSNumber *frameID = [decoder decodeObjectOfClass:[NSNumber class] forKey:@"frameID"];
    if (![frameID isKindOfClass:[NSNumber class]]) {
        [self release];
        return nil;
    }

    NSNumber *processID = [decoder decodeObjectOfClass:[NSNumber class] forKey:@"processID"];
    if (![processID isKindOfClass:[NSNumber class]]) {
        [self release];
        return nil;
    }

    API::Object::constructInWrapper<API::FrameHandle>(self, WebCore::FrameIdentifier {
        makeObjectIdentifier<WebCore::FrameIdentifierType>(frameID.unsignedLongLongValue),
        makeObjectIdentifier<WebCore::ProcessIdentifierType>(processID.unsignedLongLongValue)
    }, false);

    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder
{
    [coder encodeObject:@(self.frameID) forKey:@"frameID"];
    [coder encodeObject:@(_frameHandle->frameID().processIdentifier().toUInt64()) forKey:@"processID"];
}

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_frameHandle;
}

@end
