/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cmath>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

#if PLATFORM(IOS_FAMILY)
OBJC_CLASS CLLocation;
#endif

namespace WebCore {

class GeolocationPosition {
public:
    GeolocationPosition() = default;

    GeolocationPosition(double timestamp, double latitude, double longitude, double accuracy)
        : timestamp(timestamp)
        , latitude(latitude)
        , longitude(longitude)
        , accuracy(accuracy)
    {
    }

#if PLATFORM(IOS_FAMILY)
    WEBCORE_EXPORT explicit GeolocationPosition(CLLocation*);
#endif

    double timestamp { std::numeric_limits<double>::quiet_NaN() };

    double latitude { std::numeric_limits<double>::quiet_NaN() };
    double longitude { std::numeric_limits<double>::quiet_NaN() };
    double accuracy { std::numeric_limits<double>::quiet_NaN() };

    std::optional<double> altitude;
    std::optional<double> altitudeAccuracy;
    std::optional<double> heading;
    std::optional<double> speed;
    std::optional<double> floorLevel;

    bool isValid() const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, GeolocationPosition&);
};

template<class Encoder>
void GeolocationPosition::encode(Encoder& encoder) const
{
    encoder << timestamp;
    encoder << latitude;
    encoder << longitude;
    encoder << accuracy;
    encoder << altitude;
    encoder << altitudeAccuracy;
    encoder << heading;
    encoder << speed;
    encoder << floorLevel;
}

template<class Decoder>
bool GeolocationPosition::decode(Decoder& decoder, GeolocationPosition& position)
{
    if (!decoder.decode(position.timestamp))
        return false;
    if (!decoder.decode(position.latitude))
        return false;
    if (!decoder.decode(position.longitude))
        return false;
    if (!decoder.decode(position.accuracy))
        return false;
    if (!decoder.decode(position.altitude))
        return false;
    if (!decoder.decode(position.altitudeAccuracy))
        return false;
    if (!decoder.decode(position.heading))
        return false;
    if (!decoder.decode(position.speed))
        return false;
    if (!decoder.decode(position.floorLevel))
        return false;

    return true;
}

inline bool GeolocationPosition::isValid() const
{
    return !std::isnan(timestamp) && !std::isnan(latitude) && !std::isnan(longitude) && !std::isnan(accuracy);
}

} // namespace WebCore
