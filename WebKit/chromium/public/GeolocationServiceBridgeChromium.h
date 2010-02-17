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

#ifndef GeolocationServiceBridgeChromium_h
#define GeolocationServiceBridgeChromium_h

namespace WebCore {
class GeolocationServiceBridge;
class GeolocationServiceChromium;
}

namespace WebKit {

class WebString;
class WebURL;

// Provides a WebKit API called by the embedder.
class WebGeolocationServiceBridge {
public:
    virtual void setIsAllowed(bool allowed) = 0;
    virtual void setLastPosition(double latitude, double longitude, bool providesAltitude, double altitude, double accuracy, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed, long long timestamp) = 0;
    virtual void setLastError(int errorCode, const WebString& message) = 0;
};

// Provides an embedder API called by WebKit.
class WebGeolocationServiceInterface {
public:
    virtual void requestPermissionForFrame(int bridgeId, const WebURL& url) = 0;
    virtual void startUpdating(int bridgeId, bool hasHighAccuracy) = 0;
    virtual void stopUpdating(int bridgeId) = 0;
    virtual void suspend(int bridgeId) = 0;
    virtual void resume(int bridgeId) = 0;

    // Attaches the GeolocationBridge to the embedder and returns its id, which
    // should be used on subsequent calls for the methods above.
    virtual int attachBridge(WebKit::WebGeolocationServiceBridge* geolocationServiceBridge) = 0;

    // Dettaches the GeolocationService from the embedder.
    virtual void dettachBridge(int bridgeId) = 0;
};

WebCore::GeolocationServiceBridge* createGeolocationServiceBridgeImpl(WebCore::GeolocationServiceChromium*);

} // namespace WebKit

#endif // GeolocationServiceBridgeChromium_h
