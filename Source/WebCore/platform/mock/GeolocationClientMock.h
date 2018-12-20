/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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

#pragma once

#include "GeolocationClient.h"
#include "GeolocationPosition.h"
#include "Timer.h"
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GeolocationController;

// FIXME: this should not be in WebCore. It should be moved to WebKit.
// Provides a mock object for the geolocation client.
class GeolocationClientMock : public GeolocationClient {
public:
    GeolocationClientMock();
    virtual ~GeolocationClientMock();

    void reset();
    void setController(GeolocationController*);

    void setPosition(GeolocationPosition&&);
    void setPositionUnavailableError(const String& errorMessage);
    void setPermission(bool allowed);
    int numberOfPendingPermissionRequests() const;

    // GeolocationClient
    void geolocationDestroyed() override;
    void startUpdating() override;
    void stopUpdating() override;
    void setEnableHighAccuracy(bool) override;
    Optional<GeolocationPosition> lastPosition() override;
    void requestPermission(Geolocation&) override;
    void cancelPermissionRequest(Geolocation&) override;

private:
    void asyncUpdateController();
    void controllerTimerFired();

    void asyncUpdatePermission();
    void permissionTimerFired();

    void clearError();

    GeolocationController* m_controller;
    Optional<GeolocationPosition> m_lastPosition;
    bool m_hasError;
    String m_errorMessage;
    Timer m_controllerTimer;
    Timer m_permissionTimer;
    bool m_isActive;

    enum PermissionState {
        PermissionStateUnset,
        PermissionStateAllowed,
        PermissionStateDenied,
    } m_permissionState;
    typedef WTF::HashSet<RefPtr<Geolocation> > GeolocationSet;
    GeolocationSet m_pendingPermission;
};

}
