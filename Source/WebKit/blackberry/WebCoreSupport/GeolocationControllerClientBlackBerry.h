/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef GeolocationControllerClientBlackBerry_h
#define GeolocationControllerClientBlackBerry_h

#include <BlackBerryPlatformGeoTracker.h>
#include <BlackBerryPlatformGeoTrackerListener.h>
#include <GeolocationClient.h>
#include <GeolocationPosition.h>

namespace BlackBerry {
namespace WebKit {
class WebPagePrivate;
}
}

namespace WebCore {

class GeolocationControllerClientBlackBerry : public GeolocationClient, public BlackBerry::Platform::GeoTrackerListener {
public:
    GeolocationControllerClientBlackBerry(BlackBerry::WebKit::WebPagePrivate*);

    virtual void geolocationDestroyed();
    virtual void startUpdating();
    virtual void stopUpdating();
    virtual GeolocationPosition* lastPosition();
    virtual void setEnableHighAccuracy(bool);
    virtual void requestPermission(Geolocation*);
    virtual void cancelPermissionRequest(Geolocation*);

    virtual void onLocationUpdate(double timestamp, double latitude, double longitude, double accuracy, double altitude, bool altitudeValid, double altitudeAccuracy,
                                  bool altitudeAccuracyValid, double speed, bool speedValid, double heading, bool headingValid);
    virtual void onLocationError(const char* error);
    virtual void onPermission(void* context, bool isAllowed);
    BlackBerry::Platform::GeoTracker* tracker() const { return m_tracker; }

private:
    BlackBerry::WebKit::WebPagePrivate* m_webPagePrivate;
    BlackBerry::Platform::GeoTracker* m_tracker;
    RefPtr<GeolocationPosition> m_lastPosition;
    bool m_accuracy;
};
}

#endif // GeolocationControllerClientBlackBerry_h
