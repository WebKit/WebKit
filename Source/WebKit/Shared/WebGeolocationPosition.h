/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef WebGeolocationPosition_h
#define WebGeolocationPosition_h

#include "APIObject.h"
#include <WebCore/GeolocationPosition.h>
#include <wtf/RefPtr.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

class WebGeolocationPosition : public API::ObjectImpl<API::Object::Type::GeolocationPosition> {
public:
    static Ref<WebGeolocationPosition> create(WebCore::GeolocationPosition&&);

    virtual ~WebGeolocationPosition();

    double timestamp() const { return m_corePosition.timestamp; }
    double latitude() const { return m_corePosition.latitude; }
    double longitude() const { return m_corePosition.longitude; }
    double accuracy() const { return m_corePosition.accuracy; }
    Optional<double> altitude() const { return m_corePosition.altitude; }
    Optional<double> altitudeAccuracy() const { return m_corePosition.altitudeAccuracy; }
    Optional<double> heading() const { return m_corePosition.heading; }
    Optional<double> speed() const { return m_corePosition.speed; }

    const WebCore::GeolocationPosition& corePosition() const { return m_corePosition; }

private:
    explicit WebGeolocationPosition(WebCore::GeolocationPosition&& geolocationPosition)
        : m_corePosition(WTFMove(geolocationPosition))
    {
    }

    WebCore::GeolocationPosition m_corePosition;
};

} // namespace WebKit

#endif // WebGeolocationPosition_h
