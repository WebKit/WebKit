/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WebGeolocationMockPrivate.h"

#import <WebCore/GeolocationServiceMock.h>
#import <WebCore/Geoposition.h>
#import <WebCore/PositionError.h>
#import <wtf/CurrentTime.h>


using namespace WebCore;
using namespace WTF;

@implementation WebGeolocationMock

+ (void)setPosition:(double)latitude:(double)longitude:(double)accuracy
{
    RefPtr<Coordinates> coordinates = Coordinates::create(latitude,
                                                          longitude,
                                                          false, 0.0,  // altitude
                                                          accuracy,
                                                          false, 0.0,  // altitudeAccuracy
                                                          false, 0.0,  // heading
                                                          false, 0.0);  // speed
    RefPtr<Geoposition> position = Geoposition::create(coordinates.release(), currentTime() * 1000.0);
    GeolocationServiceMock::setPosition(position.release());
}

+ (void)setError:(int)code:(NSString *)message
{
    PositionError::ErrorCode codeEnum = static_cast<PositionError::ErrorCode>(code);
    RefPtr<PositionError> error = PositionError::create(codeEnum, message);
    GeolocationServiceMock::setError(error.release());
}

@end
