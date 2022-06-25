/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)

#include "Decoder.h"
#include "Encoder.h"
#include "RemoteMediaDescription.h"
#include "TrackPrivateRemoteIdentifier.h"
#include <wtf/MediaTime.h>
#include <wtf/Vector.h>

namespace WebKit {

struct InitializationSegmentInfo {
    MediaTime duration;

    struct TrackInformation {
        MediaDescriptionInfo description;
        TrackPrivateRemoteIdentifier identifier;

        template<class Encoder>
        void encode(Encoder& encoder) const
        {
            encoder << description;
            encoder << identifier;
        }

        template <class Decoder>
        static std::optional<TrackInformation> decode(Decoder& decoder)
        {
            std::optional<MediaDescriptionInfo> mediaDescription;
            decoder >> mediaDescription;
            if (!mediaDescription)
                return std::nullopt;

            std::optional<TrackPrivateRemoteIdentifier> identifier;
            decoder >> identifier;
            if (!identifier)
                return std::nullopt;

            return {{
                WTFMove(*mediaDescription),
                WTFMove(*identifier)
            }};
        }
    };

    Vector<TrackInformation> audioTracks;
    Vector<TrackInformation> videoTracks;
    Vector<TrackInformation> textTracks;

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << duration;
        encoder << audioTracks;
        encoder << videoTracks;
        encoder << textTracks;
    }

    template <class Decoder>
    static std::optional<InitializationSegmentInfo> decode(Decoder& decoder)
    {
        std::optional<MediaTime> duration;
        decoder >> duration;
        if (!duration)
            return std::nullopt;

        std::optional<Vector<TrackInformation>> audioTracks;
        decoder >> audioTracks;
        if (!audioTracks)
            return std::nullopt;

        std::optional<Vector<TrackInformation>> videoTracks;
        decoder >> videoTracks;
        if (!videoTracks)
            return std::nullopt;

        std::optional<Vector<TrackInformation>> textTracks;
        decoder >> textTracks;
        if (!textTracks)
            return std::nullopt;

        return {{
            WTFMove(*duration),
            WTFMove(*audioTracks),
            WTFMove(*videoTracks),
            WTFMove(*textTracks)
        }};
    }
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
