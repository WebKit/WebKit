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

#import "DeviceOrientationClientIOS.h"

#import "WebCoreMotionManager.h"

#if PLATFORM(IOS)

namespace WebCore {

DeviceOrientationClientIOS::DeviceOrientationClientIOS()
    : DeviceOrientationClient()
    , m_motionManager(nullptr)
    , m_updating(0)
{
}

DeviceOrientationClientIOS::~DeviceOrientationClientIOS()
{
}

void DeviceOrientationClientIOS::setController(DeviceOrientationController* controller)
{
    m_controller = controller;
}

void DeviceOrientationClientIOS::startUpdating()
{
    m_updating = true;

    if (!m_motionManager)
        m_motionManager = [WebCoreMotionManager sharedManager];

    [m_motionManager addOrientationClient:this];
}

void DeviceOrientationClientIOS::stopUpdating()
{
    m_updating = false;

    // Remove ourselves as the orientation client so we won't get updates.
    [m_motionManager removeOrientationClient:this];
}

DeviceOrientationData* DeviceOrientationClientIOS::lastOrientation() const
{
    return m_currentDeviceOrientation.get();
}

void DeviceOrientationClientIOS::deviceOrientationControllerDestroyed()
{
    [m_motionManager removeOrientationClient:this];
}
    
void DeviceOrientationClientIOS::orientationChanged(double alpha, double beta, double gamma, double compassHeading, double compassAccuracy)
{
    if (!m_updating)
        return;

#if PLATFORM(IOS_SIMULATOR)
    UNUSED_PARAM(alpha);
    UNUSED_PARAM(beta);
    UNUSED_PARAM(gamma);
    UNUSED_PARAM(compassHeading);
    UNUSED_PARAM(compassAccuracy);
    m_currentDeviceOrientation = DeviceOrientationData::create(false, 0,
                                                           false, 0,
                                                           false, 0,
                                                           false, 0,
                                                           false, 0);
#else
    m_currentDeviceOrientation = DeviceOrientationData::create(true, alpha,
                                                           true, beta,
                                                           true, gamma,
                                                           true, compassHeading,
                                                           true, compassAccuracy);
#endif
    m_controller->didChangeDeviceOrientation(m_currentDeviceOrientation.get());
}

} // namespace WebCore

#endif // PLATFORM(IOS)
