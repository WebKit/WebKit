/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include <WebCore/ContentType.h>
#include <WebCore/PlatformTextTrack.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct RemoteMediaPlayerProxyConfiguration {
    String referrer;
    String userAgent;
    String sourceApplicationIdentifier;
    String networkInterfaceName;
    Vector<WebCore::ContentType> mediaContentTypesRequiringHardwareSupport;
    Vector<String> preferredAudioCharacteristics;
#if ENABLE(AVF_CAPTIONS)
    Vector<WebCore::PlatformTextTrackData> outOfBandTrackData;
#endif
    WebCore::SecurityOriginData documentSecurityOrigin;
    uint64_t logIdentifier { 0 };
    bool shouldUsePersistentCache { false };
    bool isVideo { 0 };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << referrer;
        encoder << userAgent;
        encoder << sourceApplicationIdentifier;
        encoder << networkInterfaceName;
        encoder << mediaContentTypesRequiringHardwareSupport;
        encoder << preferredAudioCharacteristics;
#if ENABLE(AVF_CAPTIONS)
        encoder << outOfBandTrackData;
#endif
        encoder << documentSecurityOrigin;
        encoder << logIdentifier;
        encoder << shouldUsePersistentCache;
        encoder << isVideo;
    }

    template <class Decoder>
    static std::optional<RemoteMediaPlayerProxyConfiguration> decode(Decoder& decoder)
    {
        std::optional<String> referrer;
        decoder >> referrer;
        if (!referrer)
            return std::nullopt;

        std::optional<String> userAgent;
        decoder >> userAgent;
        if (!userAgent)
            return std::nullopt;

        std::optional<String> sourceApplicationIdentifier;
        decoder >> sourceApplicationIdentifier;
        if (!sourceApplicationIdentifier)
            return std::nullopt;

        std::optional<String> networkInterfaceName;
        decoder >> networkInterfaceName;
        if (!networkInterfaceName)
            return std::nullopt;

        std::optional<Vector<WebCore::ContentType>> mediaContentTypesRequiringHardwareSupport;
        decoder >> mediaContentTypesRequiringHardwareSupport;
        if (!mediaContentTypesRequiringHardwareSupport)
            return std::nullopt;

        std::optional<Vector<String>> preferredAudioCharacteristics;
        decoder >> preferredAudioCharacteristics;
        if (!preferredAudioCharacteristics)
            return std::nullopt;

#if ENABLE(AVF_CAPTIONS)
        std::optional<Vector<WebCore::PlatformTextTrackData>> outOfBandTrackData;
        decoder >> outOfBandTrackData;
        if (!outOfBandTrackData)
            return std::nullopt;
#endif

        std::optional<WebCore::SecurityOriginData> documentSecurityOrigin;
        decoder >> documentSecurityOrigin;
        if (!documentSecurityOrigin)
            return std::nullopt;

        std::optional<uint64_t> logIdentifier;
        decoder >> logIdentifier;
        if (!logIdentifier)
            return std::nullopt;

        std::optional<bool> shouldUsePersistentCache;
        decoder >> shouldUsePersistentCache;
        if (!shouldUsePersistentCache)
            return std::nullopt;

        std::optional<bool> isVideo;
        decoder >> isVideo;
        if (!isVideo)
            return std::nullopt;

        return {{
            WTFMove(*referrer),
            WTFMove(*userAgent),
            WTFMove(*sourceApplicationIdentifier),
            WTFMove(*networkInterfaceName),
            WTFMove(*mediaContentTypesRequiringHardwareSupport),
            WTFMove(*preferredAudioCharacteristics),
#if ENABLE(AVF_CAPTIONS)
            WTFMove(*outOfBandTrackData),
#endif
            WTFMove(*documentSecurityOrigin),
            *logIdentifier,
            *shouldUsePersistentCache,
            *isVideo,
        }};
    }
};

} // namespace WebKit

#endif
