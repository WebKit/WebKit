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

void WebDeviceOrientationUpdateProvider::startUpdating(WebCore::MotionManagerClient& client)
{
    if (m_clients.isEmpty() && m_page)
        m_page->send(Messages::WebDeviceOrientationUpdateProviderProxy::StartUpdatingDeviceOrientation());
    m_clients.add(&client);
}

void WebDeviceOrientationUpdateProvider::stopUpdating(WebCore::MotionManagerClient& client)
{
    if (m_clients.isEmpty())
        return;
    m_clients.remove(&client);
    if (m_clients.isEmpty() && m_page)
        m_page->send(Messages::WebDeviceOrientationUpdateProviderProxy::StopUpdatingDeviceOrientation());
}

void WebDeviceOrientationUpdateProvider::deviceOrientationChanged(double alpha, double beta, double gamma, double compassHeading, double compassAccuracy)
{
    for (auto* client : copyToVector(m_clients))
        client->orientationChanged(alpha, beta, gamma, compassHeading, compassAccuracy);
}

}

#endif // PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
