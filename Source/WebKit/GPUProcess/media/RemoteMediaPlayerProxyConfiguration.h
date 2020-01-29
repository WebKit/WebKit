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
#include <wtf/text/WTFString.h>

namespace WebKit {

struct RemoteMediaPlayerProxyConfiguration {
    String referrer;
    String userAgent;
    String sourceApplicationIdentifier;
    String networkInterfaceName;
    Vector<WebCore::ContentType> mediaContentTypesRequiringHardwareSupport;
    Vector<String> preferredAudioCharacteristics;
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
        encoder << logIdentifier;
        encoder << shouldUsePersistentCache;
        encoder << isVideo;
    }

    template <class Decoder>
    static Optional<RemoteMediaPlayerProxyConfiguration> decode(Decoder& decoder)
    {
        Optional<String> referrer;
        decoder >> referrer;
        if (!referrer)
            return WTF::nullopt;

        Optional<String> userAgent;
        decoder >> userAgent;
        if (!userAgent)
            return WTF::nullopt;

        Optional<String> sourceApplicationIdentifier;
        decoder >> sourceApplicationIdentifier;
        if (!sourceApplicationIdentifier)
            return WTF::nullopt;

        Optional<String> networkInterfaceName;
        decoder >> networkInterfaceName;
        if (!networkInterfaceName)
            return WTF::nullopt;

        Optional<Vector<WebCore::ContentType>> mediaContentTypesRequiringHardwareSupport;
        decoder >> mediaContentTypesRequiringHardwareSupport;
        if (!mediaContentTypesRequiringHardwareSupport)
            return WTF::nullopt;

        Optional<Vector<String>> preferredAudioCharacteristics;
        decoder >> preferredAudioCharacteristics;
        if (!preferredAudioCharacteristics)
            return WTF::nullopt;

        Optional<uint64_t> logIdentifier;
        decoder >> logIdentifier;
        if (!logIdentifier)
            return WTF::nullopt;

        Optional<bool> shouldUsePersistentCache;
        decoder >> shouldUsePersistentCache;
        if (!shouldUsePersistentCache)
            return WTF::nullopt;

        Optional<bool> isVideo;
        decoder >> isVideo;
        if (!isVideo)
            return WTF::nullopt;

        return {{
            WTFMove(*referrer),
            WTFMove(*userAgent),
            WTFMove(*sourceApplicationIdentifier),
            WTFMove(*networkInterfaceName),
            WTFMove(*mediaContentTypesRequiringHardwareSupport),
            WTFMove(*preferredAudioCharacteristics),
            *logIdentifier,
            *shouldUsePersistentCache,
            *isVideo,
        }};
    }
};

} // namespace WebKit

#endif
