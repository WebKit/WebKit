/*
 * Copyright (C) 2008 Holger Hans Peter Freyther
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
 */

#ifndef GeolocationServiceGtk_h
#define GeolocationServiceGtk_h
#if ENABLE(GEOLOCATION)

#include "GeolocationService.h"
#include "Geoposition.h"
#include "PositionError.h"
#include "RefPtr.h"

#include <geoclue/geoclue-master.h>
#include <geoclue/geoclue-position.h>

namespace WebCore {
    class GeolocationServiceGtk : public GeolocationService {
    public:
        static GeolocationService* create(GeolocationServiceClient*);
        ~GeolocationServiceGtk();

        virtual bool startUpdating(PositionOptions*);
        virtual void stopUpdating();

        virtual void suspend();
        virtual void resume();

        Geoposition* lastPosition() const;
        PositionError* lastError() const;

    private:
        GeolocationServiceGtk(GeolocationServiceClient*);

        void setError(PositionError::ErrorCode, const char* message);
        void updatePosition();

        static void position_changed(GeocluePosition*, GeocluePositionFields, int, double, double, double, GeoclueAccuracy*, GeolocationServiceGtk*);
        static void getPositionCallback(GeocluePosition*, GeocluePositionFields, int, double, double, double, GeoclueAccuracy*, GError*, GeolocationServiceGtk*);

    private:
        RefPtr<Geoposition> m_lastPosition;
        RefPtr<PositionError> m_lastError;

        // state objects
        GeoclueMasterClient* m_geoclueClient;
        GeocluePosition* m_geocluePosition;

        // Error and Position state
        double m_latitude;
        double m_longitude;
        double m_altitude;
        double m_accuracy;
        double m_altitudeAccuracy;
        int m_timestamp;
    };
}

#endif // ENABLE(GEOLOCATION)
#endif
