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

#include "config.h"
#include "GeolocationServiceAndroid.h"

#include "GeolocationServiceBridge.h"
#include "Geoposition.h"
#include "PositionError.h"
#include "PositionOptions.h"

#include <wtf/CurrentTime.h>

using JSC::Bindings::getJNIEnv;
using namespace std;

namespace WebCore {

// GeolocationServiceAndroid is the Android implmentation of Geolocation
// service. Each object of this class owns an object of type
// GeolocationServiceBridge, which in turn owns a Java GeolocationService
// object. Therefore, there is a 1:1 mapping between Geolocation,
// GeolocationServiceAndroid, GeolocationServiceBridge and Java
// GeolocationService objects. In the case where multiple Geolocation objects
// exist simultaneously, the corresponsing Java GeolocationService objects all
// register with the platform location service. It is the platform service that
// handles making sure that updates are passed to all Geolocation objects.
PassOwnPtr<GeolocationService> GeolocationServiceAndroid::create(GeolocationServiceClient* client)
{
    return new GeolocationServiceAndroid(client);
}

GeolocationService::FactoryFunction* GeolocationService::s_factoryFunction = &GeolocationServiceAndroid::create;

GeolocationServiceAndroid::GeolocationServiceAndroid(GeolocationServiceClient* client)
    : GeolocationService(client)
    , m_timer(this, &GeolocationServiceAndroid::timerFired)
    , m_javaBridge(0)
{
}

bool GeolocationServiceAndroid::startUpdating(PositionOptions* options)
{
    // This method is called every time a new watch or one-shot position request
    // is started. If we already have a position or an error, call back
    // immediately.
    if (m_lastPosition || m_lastError) {
        ASSERT(m_javaBridge);
        m_timer.startOneShot(0);
    }

    // Lazilly create the Java object.
    bool haveJavaBridge = m_javaBridge;
    if (!haveJavaBridge)
        m_javaBridge.set(new GeolocationServiceBridge(this));
    ASSERT(m_javaBridge);

    // On Android, high power == GPS. Set whether to use GPS before we start the
    // implementation.
    ASSERT(options);
    if (options->enableHighAccuracy())
        m_javaBridge->setEnableGps(true);

    if (!haveJavaBridge)
        m_javaBridge->start();

    return true;
}

void GeolocationServiceAndroid::stopUpdating()
{
    // Called when the Geolocation object has no watches or one shots in
    // progress.
    m_javaBridge.clear();
    // Reset last position and error to make sure that we always try to get a
    // new position from the system service when a request is first made.
    m_lastPosition = 0;
    m_lastError = 0;
}

void GeolocationServiceAndroid::suspend()
{
    ASSERT(m_javaBridge);
    m_javaBridge->stop();
}

void GeolocationServiceAndroid::resume()
{
    ASSERT(m_javaBridge);
    m_javaBridge->start();
}

// Note that there is no guarantee that subsequent calls to this method offer a
// more accurate or updated position.
void GeolocationServiceAndroid::newPositionAvailable(PassRefPtr<Geoposition> position)
{
    ASSERT(position);
    if (!m_lastPosition
        || isPositionMovement(m_lastPosition.get(), position.get())
        || isPositionMoreAccurate(m_lastPosition.get(), position.get())
        || isPositionMoreTimely(m_lastPosition.get(), position.get())) {
        m_lastPosition = position;
        // Remove the last error.
        m_lastError = 0;
        positionChanged();
    }
}

void GeolocationServiceAndroid::newErrorAvailable(PassRefPtr<PositionError> error)
{
    ASSERT(error);
    // We leave the last position
    m_lastError = error;
    errorOccurred();
}

void GeolocationServiceAndroid::timerFired(Timer<GeolocationServiceAndroid>* timer)
{
    ASSERT(&m_timer == timer);
    ASSERT(m_lastPosition || m_lastError);
    if (m_lastPosition)
        positionChanged();
    else
        errorOccurred();
}

bool GeolocationServiceAndroid::isPositionMovement(Geoposition* position1, Geoposition* position2)
{
    ASSERT(position1 && position2);
    // For the small distances in which we are likely concerned, it's reasonable
    // to approximate the distance between the two positions as the sum of the
    // differences in latitude and longitude.
    double delta = fabs(position1->coords()->latitude() - position2->coords()->latitude())
        + fabs(position1->coords()->longitude() - position2->coords()->longitude());
    // Approximate conversion from degrees of arc to metres.
    delta *= 60 * 1852;
    // The threshold is when the distance between the two positions exceeds the
    // worse (larger) of the two accuracies.
    int maxAccuracy = max(position1->coords()->accuracy(), position2->coords()->accuracy());
    return delta > maxAccuracy;
}

bool GeolocationServiceAndroid::isPositionMoreAccurate(Geoposition* position1, Geoposition* position2)
{
    ASSERT(position1 && position2);
    return position2->coords()->accuracy() < position1->coords()->accuracy();
}

bool GeolocationServiceAndroid::isPositionMoreTimely(Geoposition* position1, Geoposition* position2)
{
    ASSERT(position1 && position2);
    DOMTimeStamp currentTime = convertSecondsToDOMTimeStamp(WTF::currentTime());
    DOMTimeStamp maximumAge = convertSecondsToDOMTimeStamp(10 * 60); // 10 minutes
    return currentTime - position1->timestamp() > maximumAge;
}

} // namespace WebCore
