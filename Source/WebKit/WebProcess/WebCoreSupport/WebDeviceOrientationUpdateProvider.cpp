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

#include "config.h"
#include "WebDeviceOrientationUpdateProvider.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)

#include "WebDeviceOrientationUpdateProviderMessages.h"
#include "WebDeviceOrientationUpdateProviderProxyMessages.h"
#include "WebPage.h"
#include "WebProcess.h"

#include <WebCore/MotionManagerClient.h>

namespace WebKit {

WebDeviceOrientationUpdateProvider::WebDeviceOrientationUpdateProvider(WebPage& page)
    : m_page(makeWeakPtr(page))
    , m_pageIdentifier(page.identifier())
{
    WebProcess::singleton().addMessageReceiver(Messages::WebDeviceOrientationUpdateProvider::messageReceiverName(), page.identifier(), *this);
}

WebDeviceOrientationUpdateProvider::~WebDeviceOrientationUpdateProvider()
{
    WebProcess::singleton().removeMessageReceiver(Messages::WebDeviceOrientationUpdateProvider::messageReceiverName(), m_pageIdentifier);
}

void WebDeviceOrientationUpdateProvider::startUpdatingDeviceOrientation(WebCore::MotionManagerClient& client)
{
    if (m_deviceOrientationClients.computesEmpty() && m_page)
        m_page->send(Messages::WebDeviceOrientationUpdateProviderProxy::StartUpdatingDeviceOrientation());
    m_deviceOrientationClients.add(client);
}

void WebDeviceOrientationUpdateProvider::stopUpdatingDeviceOrientation(WebCore::MotionManagerClient& client)
{
    if (m_deviceOrientationClients.computesEmpty())
        return;
    m_deviceOrientationClients.remove(client);
    if (m_deviceOrientationClients.computesEmpty() && m_page)
        m_page->send(Messages::WebDeviceOrientationUpdateProviderProxy::StopUpdatingDeviceOrientation());
}

void WebDeviceOrientationUpdateProvider::startUpdatingDeviceMotion(WebCore::MotionManagerClient& client)
{
    if (m_deviceMotionClients.computesEmpty() && m_page)
        m_page->send(Messages::WebDeviceOrientationUpdateProviderProxy::StartUpdatingDeviceMotion());
    m_deviceMotionClients.add(client);
}

void WebDeviceOrientationUpdateProvider::stopUpdatingDeviceMotion(WebCore::MotionManagerClient& client)
{
    if (m_deviceMotionClients.computesEmpty())
        return;
    m_deviceMotionClients.remove(client);
    if (m_deviceMotionClients.computesEmpty() && m_page)
        m_page->send(Messages::WebDeviceOrientationUpdateProviderProxy::StopUpdatingDeviceMotion());
}

void WebDeviceOrientationUpdateProvider::deviceOrientationChanged(double alpha, double beta, double gamma, double compassHeading, double compassAccuracy)
{
    Vector<WeakPtr<WebCore::MotionManagerClient>> clients;
    for (auto& client : m_deviceOrientationClients)
        clients.append(makeWeakPtr(&client));

    for (auto& client : clients) {
        if (client)
            client->orientationChanged(alpha, beta, gamma, compassHeading, compassAccuracy);
    }
}

void WebDeviceOrientationUpdateProvider::deviceMotionChanged(double xAcceleration, double yAcceleration, double zAcceleration, double xAccelerationIncludingGravity, double yAccelerationIncludingGravity, double zAccelerationIncludingGravity, double xRotationRate, double yRotationRate, double zRotationRate)
{
    Vector<WeakPtr<WebCore::MotionManagerClient>> clients;
    for (auto& client : m_deviceMotionClients)
        clients.append(makeWeakPtr(&client));
    
    for (auto& client : clients) {
        if (client)
            client->motionChanged(xAcceleration, yAcceleration, zAcceleration, xAccelerationIncludingGravity, yAccelerationIncludingGravity,  zAccelerationIncludingGravity, xRotationRate, yRotationRate, zRotationRate);
    }
}

}

#endif // PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
