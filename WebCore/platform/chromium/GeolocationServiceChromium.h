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

#ifndef GeolocationServiceChromium_h
#define GeolocationServiceChromium_h

#include "Geolocation.h"
#include "GeolocationService.h"
#include "Geoposition.h"
#include "PlatformString.h"
#include "PositionError.h"

namespace WebCore {

// Provides an interface for GeolocationServiceChromium to call into the embedder.
class GeolocationServiceBridge {
public:
    virtual ~GeolocationServiceBridge();
    // Called by GeolocationServiceChromium.
    virtual bool startUpdating(PositionOptions*) = 0;
    virtual void stopUpdating() = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;

    // Called by the embedder, to identify this bridge.
    virtual int getBridgeId() const = 0;
    virtual void attachBridgeIfNeeded() = 0;
};

// This class extends GeolocationService, and uses GeolocationServiceBridge to
// call into the embedder, as well as provides a few extra methods so that the
// embedder can notify permission, position, error, etc.
class GeolocationServiceChromium : public GeolocationService {
public:
    explicit GeolocationServiceChromium(GeolocationServiceClient*);

    GeolocationServiceBridge* geolocationServiceBridge() const { return m_geolocationServiceBridge.get(); }
    void setIsAllowed(bool allowed);
    void setLastPosition(PassRefPtr<Geoposition>);
    void setLastError(int errorCode, const String& message);
    Frame* frame();

    // From GeolocationService.
    virtual bool startUpdating(PositionOptions*);
    virtual void stopUpdating();
    virtual void suspend();
    virtual void resume();
    virtual Geoposition* lastPosition() const;
    virtual PositionError* lastError() const;

private:
    Geolocation* m_geolocation;
    OwnPtr<GeolocationServiceBridge> m_geolocationServiceBridge;
    RefPtr<Geoposition> m_lastPosition;
    RefPtr<PositionError> m_lastError;
};

} // namespace WebCore

#endif // GeolocationServiceChromium_h
