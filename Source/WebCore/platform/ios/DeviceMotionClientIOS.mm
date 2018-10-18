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

#import "DeviceMotionClientIOS.h"

#import "WebCoreMotionManager.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)

namespace WebCore {

DeviceMotionClientIOS::DeviceMotionClientIOS()
    : DeviceMotionClient()
    , m_motionManager(nullptr)
    , m_updating(0)
{
}

DeviceMotionClientIOS::~DeviceMotionClientIOS()
{
}

void DeviceMotionClientIOS::setController(DeviceMotionController* controller)
{
    m_controller = controller;
}

void DeviceMotionClientIOS::startUpdating()
{
    m_updating = true;

    if (!m_motionManager)
        m_motionManager = [WebCoreMotionManager sharedManager];

    [m_motionManager addMotionClient:this];
}

void DeviceMotionClientIOS::stopUpdating()
{
    m_updating = false;

    // Remove ourselves as the motion client so we won't get updates.
    [m_motionManager removeMotionClient:this];
}

DeviceMotionData* DeviceMotionClientIOS::lastMotion() const
{
    return m_currentDeviceMotionData.get();
}

void DeviceMotionClientIOS::deviceMotionControllerDestroyed()
{
    [m_motionManager removeMotionClient:this];
}

void DeviceMotionClientIOS::motionChanged(double xAcceleration, double yAcceleration, double zAcceleration, double xAccelerationIncludingGravity, double yAccelerationIncludingGravity, double zAccelerationIncludingGravity, double xRotationRate, double yRotationRate, double zRotationRate)
{
    if (!m_updating)
        return;

#if PLATFORM(IOS_FAMILY_SIMULATOR)
    UNUSED_PARAM(xAcceleration);
    UNUSED_PARAM(yAcceleration);
    UNUSED_PARAM(zAcceleration);
    UNUSED_PARAM(xAccelerationIncludingGravity);
    UNUSED_PARAM(yAccelerationIncludingGravity);
    UNUSED_PARAM(zAccelerationIncludingGravity);
    UNUSED_PARAM(xRotationRate);
    UNUSED_PARAM(yRotationRate);
    UNUSED_PARAM(zRotationRate);

    auto accelerationIncludingGravity = DeviceMotionData::Acceleration::create();
    auto acceleration = DeviceMotionData::Acceleration::create();
    auto rotationRate = DeviceMotionData::RotationRate::create();
#else
    auto accelerationIncludingGravity = DeviceMotionData::Acceleration::create(xAccelerationIncludingGravity, yAccelerationIncludingGravity, zAccelerationIncludingGravity);

    RefPtr<DeviceMotionData::Acceleration> acceleration;
    RefPtr<DeviceMotionData::RotationRate> rotationRate;
    if ([m_motionManager gyroAvailable]) {
        acceleration = DeviceMotionData::Acceleration::create(xAcceleration, yAcceleration, zAcceleration);
        rotationRate = DeviceMotionData::RotationRate::create(xRotationRate, yRotationRate, zRotationRate);
    }
#endif // PLATFORM(IOS_FAMILY_SIMULATOR)

    m_currentDeviceMotionData = DeviceMotionData::create(WTFMove(acceleration), WTFMove(accelerationIncludingGravity), WTFMove(rotationRate), kMotionUpdateInterval);
    m_controller->didChangeDeviceMotion(m_currentDeviceMotionData.get());
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
