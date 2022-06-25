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

#if ENABLE(GPU_PROCESS)

#include <WebCore/InbandTextTrackPrivate.h>
#include <WebCore/TrackPrivateBase.h>
#include <wtf/MediaTime.h>

namespace WebKit {

struct TextTrackPrivateRemoteConfiguration {
    AtomString trackId;
    AtomString label;
    AtomString language;
    AtomString inBandMetadataTrackDispatchType;
    MediaTime startTimeVariance { MediaTime::zeroTime() };
    int trackIndex;

    WebCore::InbandTextTrackPrivate::CueFormat cueFormat { WebCore::InbandTextTrackPrivate::CueFormat::Generic };
    WebCore::InbandTextTrackPrivate::Kind kind { WebCore::InbandTextTrackPrivate::Kind::None };

    bool isClosedCaptions { false };
    bool isSDH { false };
    bool containsOnlyForcedSubtitles { false };
    bool isMainProgramContent { true };
    bool isEasyToRead { false };
    bool isDefault { false };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << trackId;
        encoder << label;
        encoder << language;
        encoder << inBandMetadataTrackDispatchType;
        encoder << startTimeVariance;
        encoder << trackIndex;
        encoder << cueFormat;
        encoder << kind;
        encoder << isClosedCaptions;
        encoder << isSDH;
        encoder << containsOnlyForcedSubtitles;
        encoder << isMainProgramContent;
        encoder << isEasyToRead;
        encoder << isDefault;
    }

    template <class Decoder>
    static std::optional<TextTrackPrivateRemoteConfiguration> decode(Decoder& decoder)
    {
        std::optional<AtomString> trackId;
        decoder >> trackId;
        if (!trackId)
            return std::nullopt;

        std::optional<AtomString> label;
        decoder >> label;
        if (!label)
            return std::nullopt;

        std::optional<AtomString> language;
        decoder >> language;
        if (!language)
            return std::nullopt;

        std::optional<AtomString> inBandMetadataTrackDispatchType;
        decoder >> inBandMetadataTrackDispatchType;
        if (!inBandMetadataTrackDispatchType)
            return std::nullopt;

        std::optional<MediaTime> startTimeVariance;
        decoder >> startTimeVariance;
        if (!startTimeVariance)
            return std::nullopt;

        std::optional<int> trackIndex;
        decoder >> trackIndex;
        if (!trackIndex)
            return std::nullopt;

        std::optional<WebCore::InbandTextTrackPrivate::CueFormat> cueFormat;
        decoder >> cueFormat;
        if (!cueFormat)
            return std::nullopt;

        std::optional<WebCore::InbandTextTrackPrivate::Kind> kind;
        decoder >> kind;
        if (!kind)
            return std::nullopt;

        std::optional<bool> isClosedCaptions;
        decoder >> isClosedCaptions;
        if (!isClosedCaptions)
            return std::nullopt;

        std::optional<bool> isSDH;
        decoder >> isSDH;
        if (!isSDH)
            return std::nullopt;

        std::optional<bool> containsOnlyForcedSubtitles;
        decoder >> containsOnlyForcedSubtitles;
        if (!containsOnlyForcedSubtitles)
            return std::nullopt;

        std::optional<bool> isMainProgramContent;
        decoder >> isMainProgramContent;
        if (!isMainProgramContent)
            return std::nullopt;

        std::optional<bool> isEasyToRead;
        decoder >> isEasyToRead;
        if (!isEasyToRead)
            return std::nullopt;

        std::optional<bool> isDefault;
        decoder >> isDefault;
        if (!isDefault)
            return std::nullopt;

        return {{
            WTFMove(*trackId),
            WTFMove(*label),
            WTFMove(*language),
            WTFMove(*inBandMetadataTrackDispatchType),
            WTFMove(*startTimeVariance),
            *trackIndex,
            *cueFormat,
            *kind,
            *isClosedCaptions,
            *isSDH,
            *containsOnlyForcedSubtitles,
            *isMainProgramContent,
            *isEasyToRead,
            *isDefault,
        }};
    }
};


} // namespace WebKit

#endif
