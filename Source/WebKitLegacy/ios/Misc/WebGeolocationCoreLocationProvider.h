/*
 * Copyright (C) 2008, 2009, 2010, 2012, 2014 Apple Inc. All Rights Reserved.
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

#if defined(__cplusplus)

#import <Foundation/Foundation.h>

namespace WebCore {
class GeolocationPositionData;
}

// WebGeolocationCoreLocationDelegate abstracts the location services of CoreLocation.
// All the results come back through the protocol GeolocationUpdateListener. Those callback can
// be done synchronously and asynchronously in responses to calls made on WebGeolocationCoreLocationDelegate.

// All calls to WebGeolocationCoreLocationDelegate must be on the main thread, all callbacks are done on the
// main thread.

@protocol WebGeolocationCoreLocationUpdateListener
- (void)geolocationAuthorizationGranted;
- (void)geolocationAuthorizationDenied;

- (void)positionChanged:(WebCore::GeolocationPositionData&&)position;
- (void)errorOccurred:(NSString *)errorMessage;
- (void)resetGeolocation;
@end


@interface WebGeolocationCoreLocationProvider : NSObject
- (id)initWithListener:(id<WebGeolocationCoreLocationUpdateListener>)listener;

- (void)requestGeolocationAuthorization;

- (void)start;
- (void)stop;

- (void)setEnableHighAccuracy:(BOOL)flag;
@end

#endif
