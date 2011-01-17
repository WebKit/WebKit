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

#ifndef WebGeolocationClient_h
#define WebGeolocationClient_h

namespace WebKit {
class WebGeolocationController;
class WebGeolocationPermissionRequest;
class WebGeolocationPosition;

class WebGeolocationClient {
public:
    virtual ~WebGeolocationClient() {}

    virtual void startUpdating() = 0;
    virtual void stopUpdating() = 0;
    virtual void setEnableHighAccuracy(bool) = 0;
    virtual void geolocationDestroyed() = 0;
    virtual bool lastPosition(WebGeolocationPosition&) = 0;

    virtual void requestPermission(const WebGeolocationPermissionRequest&) = 0;
    virtual void cancelPermissionRequest(const WebGeolocationPermissionRequest&) = 0;

    // The controller is valid until geolocationDestroyed() is invoked.
    // Ownership of the WebGeolocationController is transferred to the client.
    virtual void setController(WebGeolocationController*) = 0;
};

} // namespace WebKit

#endif // WebGeolocationClient_h
