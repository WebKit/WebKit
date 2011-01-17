/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DeviceOrientationClientProxy.h"

#include "DeviceOrientation.h"
#include "WebDeviceOrientation.h"
#include "WebDeviceOrientationController.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {
class DeviceOrientationController;
}

namespace WebKit {

void DeviceOrientationClientProxy::setController(WebCore::DeviceOrientationController* c)
{
    if (!m_client) // FIXME: Get rid of these null checks once device orientation is enabled by default.
        return;
    m_client->setController(new WebDeviceOrientationController(c));
}

void DeviceOrientationClientProxy::startUpdating()
{
    if (!m_client)
        return;
    m_client->startUpdating();
}

void DeviceOrientationClientProxy::stopUpdating()
{
    if (!m_client)
        return;
    m_client->stopUpdating();
}

WebCore::DeviceOrientation* DeviceOrientationClientProxy::lastOrientation() const
{
    if (!m_client)
        return 0;

    // Cache the DeviceOrientation pointer so its reference count does not drop to zero upon return.
    m_lastOrientation = m_client->lastOrientation();

    return m_lastOrientation.get();
}

void DeviceOrientationClientProxy::deviceOrientationControllerDestroyed()
{
    // Our lifetime is bound to the WebViewImpl.
}

} // namespace WebKit
