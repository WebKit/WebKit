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

#import "config.h"
#import "WebCoreMotionManager.h"

#import "SoftLinking.h"
#import "WebCoreObjCExtras.h"
#import <CoreLocation/CoreLocation.h>
#import <objc/objc-runtime.h>
#import <wtf/MathExtras.h>
#import <wtf/PassRefPtr.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)

#import "WebCoreThreadRun.h"
#import <CoreMotion/CoreMotion.h>

// Get CoreLocation classes
SOFT_LINK_FRAMEWORK(CoreLocation)

SOFT_LINK_CLASS(CoreLocation, CLLocationManager)
SOFT_LINK_CLASS(CoreLocation, CLHeading)

// Get CoreMotion classes
SOFT_LINK_FRAMEWORK(CoreMotion)

SOFT_LINK_CLASS(CoreMotion, CMMotionManager)
SOFT_LINK_CLASS(CoreMotion, CMAccelerometerData)
SOFT_LINK_CLASS(CoreMotion, CMDeviceMotion)

static const double kGravity = 9.80665;

@interface WebCoreMotionManager(Private)

- (void)initializeOnMainThread;
- (void)checkClientStatus;
- (void)update;
- (void)sendAccelerometerData:(CMAccelerometerData *)newAcceleration;
- (void)sendMotionData:(CMDeviceMotion *)newMotion withHeading:(CLHeading *)newHeading;

@end

@implementation WebCoreMotionManager

+ (WebCoreMotionManager *)sharedManager
{
    DEPRECATED_DEFINE_STATIC_LOCAL(RetainPtr<WebCoreMotionManager>, sharedMotionManager, ([[WebCoreMotionManager alloc] init]));
    return sharedMotionManager.get();
}

- (id)init
{
    self = [super init];
    if (self)
        [self performSelectorOnMainThread:@selector(initializeOnMainThread) withObject:nil waitUntilDone:NO];
    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainThread([WebCoreMotionManager class], self))
        return;

    ASSERT(!m_updateTimer);

    if (m_headingAvailable)
        [m_locationManager stopUpdatingHeading];
    [m_locationManager release];

    if (m_gyroAvailable)
        [m_motionManager stopDeviceMotionUpdates];
    else
        [m_motionManager stopAccelerometerUpdates];
    [m_motionManager release];

    [super dealloc];
}

- (void)addMotionClient:(WebCore::DeviceMotionClientIOS *)client
{
    m_deviceMotionClients.add(client);
    [self checkClientStatus];
}

- (void)removeMotionClient:(WebCore::DeviceMotionClientIOS *)client
{
    m_deviceMotionClients.remove(client);
    [self checkClientStatus];
}

- (void)addOrientationClient:(WebCore::DeviceOrientationClientIOS *)client
{
    m_deviceOrientationClients.add(client);
    [self checkClientStatus];
}

- (void)removeOrientationClient:(WebCore::DeviceOrientationClientIOS *)client
{
    m_deviceOrientationClients.remove(client);
    [self checkClientStatus];
}

- (BOOL)gyroAvailable
{
    return m_gyroAvailable;
}

- (BOOL)headingAvailable
{
    return m_headingAvailable;
}

- (void)initializeOnMainThread
{
    ASSERT(!WebThreadIsCurrent());

#define CMMotionManager getCMMotionManagerClass()
    m_motionManager = [[CMMotionManager alloc] init];
#undef CMMotionManager

    m_gyroAvailable = m_motionManager.deviceMotionAvailable;

    if (m_gyroAvailable)
        m_motionManager.deviceMotionUpdateInterval = kMotionUpdateInterval;
    else
        m_motionManager.accelerometerUpdateInterval = kMotionUpdateInterval;

#define CLLocationManager getCLLocationManagerClass()
    m_locationManager = [[CLLocationManager alloc] init];
    m_headingAvailable = [CLLocationManager headingAvailable];
#undef CLLocationManager

    [self checkClientStatus];
}

- (void)checkClientStatus
{
    if (!pthread_main_np()) {
        [self performSelectorOnMainThread:_cmd withObject:nil waitUntilDone:NO];
        return;
    }

    // Since this method executes on the main thread, it should always be called
    // after the initializeOnMainThread method has run, and hence there should
    // be no chance that m_motionManager has not been created.
    ASSERT(m_motionManager);

    if (m_deviceMotionClients.size() || m_deviceOrientationClients.size()) {
        if (m_gyroAvailable)
            [m_motionManager startDeviceMotionUpdates];
        else
            [m_motionManager startAccelerometerUpdates];

        if (m_headingAvailable)
            [m_locationManager startUpdatingHeading];

        if (!m_updateTimer) {
            m_updateTimer = [[NSTimer scheduledTimerWithTimeInterval:kMotionUpdateInterval
                                                              target:self
                                                            selector:@selector(update)
                                                            userInfo:nil
                                                             repeats:YES] retain];
        }
    } else {
        NSTimer *timer = m_updateTimer;
        m_updateTimer = nil;
        [timer invalidate];
        [timer release];

        if (m_gyroAvailable)
            [m_motionManager stopDeviceMotionUpdates];
        else
            [m_motionManager stopAccelerometerUpdates];

        if (m_headingAvailable)
            [m_locationManager stopUpdatingHeading];
    }
}

- (void)update
{
    // It's extremely unlikely that an update happens without an active
    // motion or location manager, but we should guard for this case just in case.
    if (!m_motionManager || !m_locationManager)
        return;
    
    // We should, however, guard for the case where the managers return nil data.
    CMDeviceMotion *deviceMotion = m_motionManager.deviceMotion;
    if (m_gyroAvailable && deviceMotion)
        [self sendMotionData:deviceMotion withHeading:m_locationManager.heading];
    else {
        if (CMAccelerometerData *accelerometerData = m_motionManager.accelerometerData)
            [self sendAccelerometerData:accelerometerData];
    }
}

- (void)sendAccelerometerData:(CMAccelerometerData *)newAcceleration
{
    WebThreadRun(^{
        CMAcceleration accel = newAcceleration.acceleration;

        Vector<WebCore::DeviceMotionClientIOS*> clients;
        copyToVector(m_deviceMotionClients, clients);
        for (size_t i = 0; i < clients.size(); ++i)
            clients[i]->motionChanged(0, 0, 0, accel.x * kGravity, accel.y * kGravity, accel.z * kGravity, 0, 0, 0);
    });
}

- (void)sendMotionData:(CMDeviceMotion *)newMotion withHeading:(CLHeading *)newHeading
{
    WebThreadRun(^{
        // Acceleration is user + gravity
        CMAcceleration userAccel = newMotion.userAcceleration;
        CMAcceleration gravity = newMotion.gravity;
        CMAcceleration totalAccel;
        totalAccel.x = userAccel.x + gravity.x;
        totalAccel.y = userAccel.y + gravity.y;
        totalAccel.z = userAccel.z + gravity.z;

        CMRotationRate rotationRate = newMotion.rotationRate;

        Vector<WebCore::DeviceMotionClientIOS*> motionClients;
        copyToVector(m_deviceMotionClients, motionClients);
        for (size_t i = 0; i < motionClients.size(); ++i) {
            motionClients[i]->motionChanged(userAccel.x * kGravity, userAccel.y * kGravity, userAccel.z * kGravity,
                                            totalAccel.x * kGravity, totalAccel.y * kGravity, totalAccel.z * kGravity,
                                            rad2deg(rotationRate.x), rad2deg(rotationRate.y), rad2deg(rotationRate.z));
        }

        CMAttitude* attitude = newMotion.attitude;

        Vector<WebCore::DeviceOrientationClientIOS*> orientationClients;
        copyToVector(m_deviceOrientationClients, orientationClients);

        // Note that the W3C DeviceOrientation specification labels the X axis
        // as passing between the sides of the device, and the Y axis as passing
        // from top to bottom of the device. Conventional Tait-Bryan Euler angles
        // have the X axis pointing forward (the heading of the aircraft, for
        // example), which is what you would "roll" around, and what the W3C says
        // is the Y axis. Unfortunately CoreMotion doesn't normalize in the
        // same order as the W3C requests, so our beta is [-90, 90] and gamma is
        // [-180, 180]. For most practical uses this is acceptable.
        // See <rdar://problem/9414459> Normalize our DeviceOrientation beta/gamma per spec

        // Rotation around the Z axis (pointing up, normalized to [0, 360] deg).
        double alpha = rad2deg(attitude.yaw > 0 ? attitude.yaw : (M_PI * 2 + attitude.yaw));
        // Rotation around the X axis (side to side).
        double beta = rad2deg(attitude.pitch);
        // Rotation around the Y axis (top to bottom).
        double gamma = rad2deg(attitude.roll);

        double heading = (m_headingAvailable && newHeading) ? newHeading.magneticHeading : 0;
        double headingAccuracy = (m_headingAvailable && newHeading) ? newHeading.headingAccuracy : -1;

        for (size_t i = 0; i < orientationClients.size(); ++i)
            orientationClients[i]->orientationChanged(alpha, beta, gamma, heading, headingAccuracy);
    });
}

@end

#endif
