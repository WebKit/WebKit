/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebGeolocationClientMock.h"

#include <wtf/CurrentTime.h>
#include "Geolocation.h"
#include "GeolocationClientMock.h"
#include "GeolocationError.h"
#include "GeolocationPosition.h"
#include "PositionError.h"
#include "WebGeolocationController.h"
#include "WebGeolocationError.h"
#include "WebGeolocationPermissionRequest.h"
#include "WebGeolocationPosition.h"

using namespace WebCore;

namespace WebKit {

WebGeolocationClientMock* WebGeolocationClientMock::create()
{
    return new WebGeolocationClientMock();
}

void WebGeolocationClientMock::setPosition(double latitude, double longitude, double accuracy)
{
    WebGeolocationPosition webPosition(currentTime(), latitude, longitude, accuracy,
                                       false, 0, false, 0, false, 0, false, 0);
    m_clientMock->setPosition(webPosition);
}

void WebGeolocationClientMock::setError(int errorCode, const WebString& message)
{
    WebGeolocationError::Error code;
    switch (errorCode) {
    case PositionError::PERMISSION_DENIED:
        code = WebGeolocationError::ErrorPermissionDenied;
        break;
    case PositionError::POSITION_UNAVAILABLE:
        code = WebGeolocationError::ErrorPositionUnavailable;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    WebGeolocationError webError(code, message);
    m_clientMock->setError(webError);
}

void WebGeolocationClientMock::setPermission(bool allowed)
{
    m_clientMock->setPermission(allowed);
}

int WebGeolocationClientMock::numberOfPendingPermissionRequests() const
{
    return m_clientMock->numberOfPendingPermissionRequests();
}

void WebGeolocationClientMock::resetMock()
{
    m_clientMock->reset();
}

void WebGeolocationClientMock::startUpdating()
{
    m_clientMock->startUpdating();
}

void WebGeolocationClientMock::stopUpdating()
{
    m_clientMock->stopUpdating();
}

void WebGeolocationClientMock::setEnableHighAccuracy(bool accuracy)
{
    m_clientMock->setEnableHighAccuracy(accuracy);
}

void WebGeolocationClientMock::geolocationDestroyed()
{
    m_clientMock->geolocationDestroyed();
}

void WebGeolocationClientMock::setController(WebGeolocationController* controller)
{
    m_clientMock->setController(controller->controller());
    delete controller;
}

void WebGeolocationClientMock::requestPermission(const WebGeolocationPermissionRequest& request)
{
    m_clientMock->requestPermission(request.geolocation());
}

void WebGeolocationClientMock::cancelPermissionRequest(const WebGeolocationPermissionRequest& request)
{
    m_clientMock->cancelPermissionRequest(request.geolocation());
}

bool WebGeolocationClientMock::lastPosition(WebGeolocationPosition& webPosition)
{
    RefPtr<GeolocationPosition> position = m_clientMock->lastPosition();
    if (!position)
        return false;

    webPosition = position.release();
    return true;
}

WebGeolocationClientMock::WebGeolocationClientMock()
{
    m_clientMock.reset(new GeolocationClientMock());
}

void WebGeolocationClientMock::reset()
{
    m_clientMock.reset(0);
}

} // WebKit
