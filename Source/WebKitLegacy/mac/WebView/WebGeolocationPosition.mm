/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#import "WebGeolocationPosition.h"

#import "WebGeolocationPositionInternal.h"
#import <WebCore/GeolocationPositionData.h>
#import <wtf/RefPtr.h>

using namespace WebCore;

@interface WebGeolocationPositionInternal : NSObject
{
@public
    GeolocationPositionData _position;
}

- (id)initWithCoreGeolocationPosition:(GeolocationPositionData&&)coreGeolocationPosition;
@end

@implementation WebGeolocationPositionInternal

- (id)initWithCoreGeolocationPosition:(GeolocationPositionData&&)coreGeolocationPosition
{
    self = [super init];
    if (!self)
        return nil;
    _position = WTFMove(coreGeolocationPosition);
    return self;
}

@end

@implementation WebGeolocationPosition

std::optional<GeolocationPositionData> core(WebGeolocationPosition *position)
{
    if (!position)
        return std::nullopt;
    return position->_internal->_position;
}

- (id)initWithTimestamp:(double)timestamp latitude:(double)latitude longitude:(double)longitude accuracy:(double)accuracy
{
    self = [super init];
    if (!self)
        return nil;
    _internal = [[WebGeolocationPositionInternal alloc] initWithCoreGeolocationPosition:GeolocationPositionData { timestamp, latitude, longitude, accuracy }];
    return self;
}

- (id)initWithGeolocationPosition:(GeolocationPositionData&&)coreGeolocationPosition
{
    self = [super init];
    if (!self)
        return nil;
    _internal = [[WebGeolocationPositionInternal alloc] initWithCoreGeolocationPosition:WTFMove(coreGeolocationPosition)];
    return self;
}

- (void)dealloc
{
    [_internal release];
    [super dealloc];
}

@end
