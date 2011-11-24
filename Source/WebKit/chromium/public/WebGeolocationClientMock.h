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

#ifndef WebGeolocationClientMock_h
#define WebGeolocationClientMock_h

#include "WebGeolocationClient.h"
#include "platform/WebCommon.h"
#include "platform/WebPrivateOwnPtr.h"

namespace WebCore {
class GeolocationClientMock;
}

namespace WebKit {
class WebGeolocationPosition;
class WebString;

class WebGeolocationClientMock : public WebGeolocationClient {
public:
    WEBKIT_EXPORT static WebGeolocationClientMock* create();
    ~WebGeolocationClientMock() { reset(); }

    WEBKIT_EXPORT void setPosition(double latitude, double longitude, double accuracy);
    WEBKIT_EXPORT void setError(int errorCode, const WebString& message);
    WEBKIT_EXPORT void setPermission(bool);
    WEBKIT_EXPORT int numberOfPendingPermissionRequests() const;
    WEBKIT_EXPORT void resetMock();

    virtual void startUpdating();
    virtual void stopUpdating();
    virtual void setEnableHighAccuracy(bool);

    virtual void geolocationDestroyed();
    virtual void setController(WebGeolocationController*);

    virtual void requestPermission(const WebGeolocationPermissionRequest&);
    virtual void cancelPermissionRequest(const WebGeolocationPermissionRequest&);

    virtual bool lastPosition(WebGeolocationPosition& webPosition);

private:
    WebGeolocationClientMock();
    WEBKIT_EXPORT void reset();

    WebPrivateOwnPtr<WebCore::GeolocationClientMock> m_clientMock;
};
}

#endif // WebGeolocationClientMock_h
