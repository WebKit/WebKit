/*
 * Copyright (C) 2008 Holger Hans Peter Freyther <zecke@selfish.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef GeolocationClientGtk_h
#define GeolocationClientGtk_h

#if ENABLE(GEOLOCATION)

#include "GeolocationClient.h"
#include "GeolocationPosition.h"
#include <geoclue/geoclue-master.h>
#include <geoclue/geoclue-position.h>
#include <wtf/gobject/GRefPtr.h>

typedef struct _WebKitWebView WebKitWebView;

namespace WebCore {
class Geolocation;
}

namespace WebKit {

class GeolocationClient : public WebCore::GeolocationClient {
public:
    GeolocationClient(WebKitWebView*);

    void geolocationDestroyed();

    void startUpdating();
    void stopUpdating();

    void setEnableHighAccuracy(bool);
    WebCore::GeolocationPosition* lastPosition();

    void requestPermission(WebCore::Geolocation*);
    void cancelPermissionRequest(WebCore::Geolocation*);

    void positionChanged(GeocluePosition*, GeocluePositionFields, int, double, double, double, GeoclueAccuracy*);
    void errorOccured(const char*);

private:
    void updatePosition();

    WebKitWebView* m_webView;

    RefPtr<WebCore::GeolocationPosition> m_lastPosition;

    GRefPtr<GeoclueMasterClient> m_geoclueClient;
    GRefPtr<GeocluePosition> m_geocluePosition;

    double m_latitude;
    double m_longitude;
    double m_altitude;
    double m_accuracy;
    double m_altitudeAccuracy;
    int m_timestamp;

    bool m_enableHighAccuracy;
    bool m_isUpdating;
};

} // namespace WebKit

#endif // ENABLE(GEOLOCATION)

#endif // GeolocationClientGtk_h
