/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L.
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

#include "MediaKeySessionType.h"
#include "MediaKeySystemMediaCapability.h"
#include "MediaKeysRequirement.h"
#include <WebCore/CDMKeySystemConfiguration.h>

namespace WebCore {

struct MediaKeySystemConfiguration {
    String label;
    Vector<AtomString> initDataTypes;
    Vector<MediaKeySystemMediaCapability> audioCapabilities;
    Vector<MediaKeySystemMediaCapability> videoCapabilities;
    MediaKeysRequirement distinctiveIdentifier { MediaKeysRequirement::Optional };
    MediaKeysRequirement persistentState { MediaKeysRequirement::Optional };
    std::optional<Vector<MediaKeySessionType>> sessionTypes;
};

inline CDMKeySystemConfiguration toPlatform(MediaKeySystemConfiguration&& value)
{
    return {
        WTF::move(value.label),
        WTF::move(value.initDataTypes),
        value.audioCapabilities.map([](auto capability) { return toPlatform(WTF::move(capability)); }),
        value.videoCapabilities.map([](auto capability) { return toPlatform(WTF::move(capability)); }),
        toPlatform(value.distinctiveIdentifier),
        toPlatform(value.persistentState),
        value.sessionTypes ? std::optional { value.sessionTypes->map([](auto type) { return toPlatform(type); }) } : std::nullopt,
    };
}

inline MediaKeySystemConfiguration fromPlatform(CDMKeySystemConfiguration&& value)
{
    return {
        WTF::move(value.label),
        WTF::move(value.initDataTypes),
        value.audioCapabilities.map([](auto capability) { return fromPlatform(WTF::move(capability)); }),
        value.videoCapabilities.map([](auto capability) { return fromPlatform(WTF::move(capability)); }),
        fromPlatform(value.distinctiveIdentifier),
        fromPlatform(value.persistentState),
        value.sessionTypes ? std::optional { value.sessionTypes->map([](auto type) { return fromPlatform(type); }) } : std::nullopt,
    };
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
