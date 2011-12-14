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

#ifndef WebGeolocationPosition_h
#define WebGeolocationPosition_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"

#if WEBKIT_IMPLEMENTATION
#include <wtf/PassRefPtr.h>
#endif

namespace WebCore { class GeolocationPosition; }

namespace WebKit {

class WebGeolocationPosition {
public:
    WebGeolocationPosition() {}
    WebGeolocationPosition(double timestamp, double latitude, double longitude, double accuracy, bool providesAltitude, double altitude, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed)
    {
        assign(timestamp, latitude, longitude, accuracy, providesAltitude, altitude, providesAltitudeAccuracy, altitudeAccuracy, providesHeading, heading, providesSpeed, speed);
    }
    WebGeolocationPosition(const WebGeolocationPosition& other) { assign(other); }
    ~WebGeolocationPosition() { reset(); }

    WEBKIT_API void assign(double timestamp, double latitude, double longitude, double accuracy, bool providesAltitude, double altitude, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed);
    WEBKIT_API void assign(const WebGeolocationPosition&);
    WEBKIT_API void reset();

#if WEBKIT_IMPLEMENTATION
    WebGeolocationPosition(WTF::PassRefPtr<WebCore::GeolocationPosition>);
    WebGeolocationPosition& operator=(WTF::PassRefPtr<WebCore::GeolocationPosition>);
    operator WTF::PassRefPtr<WebCore::GeolocationPosition>() const;
#endif

private:
    WebPrivatePtr<WebCore::GeolocationPosition> m_private;
};

} // namespace WebKit

#endif // WebGeolocationPosition_h
