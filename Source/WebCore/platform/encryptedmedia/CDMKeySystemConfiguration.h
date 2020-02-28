/*
 * Copyright (C) 2017 Metrological Group B.V.
 * Copyright (C) 2017 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMMediaCapability.h"
#include "CDMRequirement.h"
#include "CDMSessionType.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct CDMKeySystemConfiguration {
    using KeysRequirement = CDMRequirement;

    String label;
    Vector<String> initDataTypes;
    Vector<CDMMediaCapability> audioCapabilities;
    Vector<CDMMediaCapability> videoCapabilities;
    CDMRequirement distinctiveIdentifier { CDMRequirement::Optional };
    CDMRequirement persistentState { CDMRequirement::Optional };
    Vector<CDMSessionType> sessionTypes;

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << label;
        encoder << initDataTypes;
        encoder << audioCapabilities;
        encoder << videoCapabilities;
        encoder << distinctiveIdentifier;
        encoder << persistentState;
        encoder << sessionTypes;
    }

    template <class Decoder>
    static Optional<CDMKeySystemConfiguration> decode(Decoder& decoder)
    {
        Optional<String> label;
        decoder >> label;
        if (!label)
            return WTF::nullopt;

        Optional<Vector<String>> initDataTypes;
        decoder >> initDataTypes;
        if (!initDataTypes)
            return WTF::nullopt;

        Optional<Vector<CDMMediaCapability>> audioCapabilities;
        decoder >> audioCapabilities;
        if (!audioCapabilities)
            return WTF::nullopt;

        Optional<Vector<CDMMediaCapability>> videoCapabilities;
        decoder >> videoCapabilities;
        if (!videoCapabilities)
            return WTF::nullopt;

        Optional<CDMRequirement> distinctiveIdentifier;
        decoder >> distinctiveIdentifier;
        if (!distinctiveIdentifier)
            return WTF::nullopt;
        
        Optional<CDMRequirement> persistentState;
        decoder >> persistentState;
        if (!persistentState)
            return WTF::nullopt;

        Optional<Vector<CDMSessionType>> sessionTypes;
        decoder >> sessionTypes;
        if (!sessionTypes)
            return WTF::nullopt;

        return {{
            WTFMove(*label),
            WTFMove(*initDataTypes),
            WTFMove(*audioCapabilities),
            WTFMove(*videoCapabilities),
            *distinctiveIdentifier,
            *persistentState,
            WTFMove(*sessionTypes),
        }};
    }
};

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
