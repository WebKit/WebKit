/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "WebDeviceOrientationUpdateProviderProxy.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)

#import "MessageSenderInlines.h"
#import "WebDeviceOrientationUpdateProviderMessages.h"
#import "WebDeviceOrientationUpdateProviderProxyMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebCore/WebCoreMotionManager.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebDeviceOrientationUpdateProviderProxy);

WebDeviceOrientationUpdateProviderProxy::WebDeviceOrientationUpdateProviderProxy(WebPageProxy& page)
    : m_page(page)
{
    m_page->protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::WebDeviceOrientationUpdateProviderProxy::messageReceiverName(), m_page->webPageIDInMainFrameProcess(), *this);
}

WebDeviceOrientationUpdateProviderProxy::~WebDeviceOrientationUpdateProviderProxy()
{
    m_page->protectedLegacyMainFrameProcess()->removeMessageReceiver(Messages::WebDeviceOrientationUpdateProviderProxy::messageReceiverName(), m_page->webPageIDInMainFrameProcess());
}

void WebDeviceOrientationUpdateProviderProxy::startUpdatingDeviceOrientation()
{
    [[WebCoreMotionManager sharedManager] addOrientationClient:this];
}

void WebDeviceOrientationUpdateProviderProxy::stopUpdatingDeviceOrientation()
{
    [[WebCoreMotionManager sharedManager] removeOrientationClient:this];
}

void WebDeviceOrientationUpdateProviderProxy::startUpdatingDeviceMotion()
{
    [[WebCoreMotionManager sharedManager] addMotionClient:this];
}

void WebDeviceOrientationUpdateProviderProxy::stopUpdatingDeviceMotion()
{
    [[WebCoreMotionManager sharedManager] removeMotionClient:this];
}

void WebDeviceOrientationUpdateProviderProxy::orientationChanged(double alpha, double beta, double gamma, double compassHeading, double compassAccuracy)
{
    m_page->protectedLegacyMainFrameProcess()->send(Messages::WebDeviceOrientationUpdateProvider::DeviceOrientationChanged(alpha, beta, gamma, compassHeading, compassAccuracy), m_page->webPageIDInMainFrameProcess());
}

void WebDeviceOrientationUpdateProviderProxy::motionChanged(double xAcceleration, double yAcceleration, double zAcceleration, double xAccelerationIncludingGravity, double yAccelerationIncludingGravity, double zAccelerationIncludingGravity, std::optional<double> xRotationRate, std::optional<double> yRotationRate, std::optional<double> zRotationRate)
{
    m_page->protectedLegacyMainFrameProcess()->send(Messages::WebDeviceOrientationUpdateProvider::DeviceMotionChanged(xAcceleration, yAcceleration, zAcceleration, xAccelerationIncludingGravity, yAccelerationIncludingGravity, zAccelerationIncludingGravity, xRotationRate, yRotationRate, zRotationRate), m_page->webPageIDInMainFrameProcess());
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
