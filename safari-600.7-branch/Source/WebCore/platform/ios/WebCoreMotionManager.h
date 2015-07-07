/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
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

#ifndef WebCoreMotionManager_h
#define WebCoreMotionManager_h

#import "DeviceMotionClientIOS.h"
#import "DeviceOrientationClientIOS.h"
#import <CoreLocation/CoreLocation.h>
#import <wtf/HashCountedSet.h>

#if PLATFORM(IOS)

#import <CoreMotion/CoreMotion.h>

const float kMotionUpdateInterval = 0.05f;

@interface WebCoreMotionManager : NSObject {
    CMMotionManager* m_motionManager;
    CLLocationManager* m_locationManager;
    HashSet<WebCore::DeviceMotionClientIOS*> m_deviceMotionClients;
    HashSet<WebCore::DeviceOrientationClientIOS*> m_deviceOrientationClients;
    NSTimer* m_updateTimer;
    BOOL m_gyroAvailable;
    BOOL m_headingAvailable;
}

+ (WebCoreMotionManager *)sharedManager;
- (void)addMotionClient:(WebCore::DeviceMotionClientIOS *)client;
- (void)removeMotionClient:(WebCore::DeviceMotionClientIOS *)client;
- (void)addOrientationClient:(WebCore::DeviceOrientationClientIOS *)client;
- (void)removeOrientationClient:(WebCore::DeviceOrientationClientIOS *)client;
- (BOOL)gyroAvailable;
- (BOOL)headingAvailable;
@end

#endif

#endif // WebCoreMotionManager_h
