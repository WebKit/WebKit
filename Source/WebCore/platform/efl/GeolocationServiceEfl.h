/*
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2011 Samsung Electronics
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

#ifndef GeolocationServiceEfl_h
#define GeolocationServiceEfl_h

#if ENABLE(GEOLOCATION)
#include "GeolocationService.h"
#include "Geoposition.h"
#include "PositionError.h"
#include "RefPtr.h"

namespace WebCore {

class GeolocationServiceEfl : public GeolocationService {
public:
    static PassOwnPtr<GeolocationService> create(GeolocationServiceClient*);
    ~GeolocationServiceEfl();

    virtual bool startUpdating(PositionOptions*);
    virtual void stopUpdating();

    virtual void suspend();
    virtual void resume();

    virtual Geoposition* lastPosition() const;
    virtual PositionError* lastError() const;

private:
    GeolocationServiceEfl(GeolocationServiceClient*);

    RefPtr<Geoposition> m_lastPosition;
    RefPtr<PositionError> m_lastError;

};

}
#endif // ENABLE(GEOLOCATION)
#endif
