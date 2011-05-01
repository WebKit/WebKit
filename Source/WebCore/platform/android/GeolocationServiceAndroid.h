/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GeolocationServiceAndroid_h
#define GeolocationServiceAndroid_h

#include "GeolocationService.h"
#include "Timer.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

// The GeolocationServiceBridge is the bridge to the Java implementation of
// the Geolocation service. It is an implementation detail of
// GeolocationServiceAndroid.
class GeolocationServiceBridge;

class GeolocationServiceAndroid : public GeolocationService {
public:
    static PassOwnPtr<GeolocationService> create(GeolocationServiceClient*);

    virtual ~GeolocationServiceAndroid() { };

    virtual bool startUpdating(PositionOptions*);
    virtual void stopUpdating();

    virtual Geoposition* lastPosition() const { return m_lastPosition.get(); }
    virtual PositionError* lastError() const { return m_lastError.get(); }

    virtual void suspend();
    virtual void resume();

    // Android-specific
    void newPositionAvailable(PassRefPtr<Geoposition>);
    void newErrorAvailable(PassRefPtr<PositionError>);
    void timerFired(Timer<GeolocationServiceAndroid>* timer);

private:
    GeolocationServiceAndroid(GeolocationServiceClient*);

    static bool isPositionMovement(Geoposition* position1, Geoposition* position2);
    static bool isPositionMoreAccurate(Geoposition* position1, Geoposition* position2);
    static bool isPositionMoreTimely(Geoposition* position1, Geoposition* position2);

    Timer<GeolocationServiceAndroid> m_timer;
    RefPtr<Geoposition> m_lastPosition;
    RefPtr<PositionError> m_lastError;
    OwnPtr<GeolocationServiceBridge> m_javaBridge;
};

} // namespace WebCore

#endif // GeolocationServiceAndroid_h
