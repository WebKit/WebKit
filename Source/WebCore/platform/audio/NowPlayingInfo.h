/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "Image.h"
#include "MediaUniqueIdentifier.h"
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct NowPlayingInfoArtwork {
    String src;
    String mimeType;
    RefPtr<Image> image;

    bool operator==(const NowPlayingInfoArtwork& other) const
    {
        return src == other.src && mimeType == other.mimeType;
    }

    bool operator!=(const NowPlayingInfoArtwork& other) const
    {
        return !(*this == other);
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<NowPlayingInfoArtwork> decode(Decoder&);
};

template<class Encoder> inline void NowPlayingInfoArtwork::encode(Encoder& encoder) const
{
    // Encoder of RefPtr<Image> will automatically decode the image and convert it to a BitmapImage/ShareableBitmap.
    encoder << src << mimeType << image;
}

template<class Decoder> inline std::optional<NowPlayingInfoArtwork> NowPlayingInfoArtwork::decode(Decoder& decoder)
{
    auto src = decoder.template decode<String>();
    auto mimeType = decoder.template decode<String>();
    auto image = decoder.template decode<RefPtr<Image>>();

    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;

    return { { WTFMove(*src), WTFMove(*mimeType), WTFMove(*image) } };
}

struct NowPlayingInfo {
    String title;
    String artist;
    String album;
    String sourceApplicationIdentifier;
    double duration { 0 };
    double currentTime { 0 };
    double rate { 1.0 };
    bool supportsSeeking { false };
    MediaUniqueIdentifier uniqueIdentifier;
    bool isPlaying { false };
    bool allowsNowPlayingControlsVisibility { false };
    std::optional<NowPlayingInfoArtwork> artwork;

    bool operator==(const NowPlayingInfo& other) const
    {
        return title == other.title
            && artist == other.artist
            && album == other.album
            && sourceApplicationIdentifier == other.sourceApplicationIdentifier
            && duration == other.duration
            && currentTime == other.currentTime
            && rate == other.rate
            && supportsSeeking == other.supportsSeeking
            && uniqueIdentifier == other.uniqueIdentifier
            && isPlaying == other.isPlaying
            && allowsNowPlayingControlsVisibility == other.allowsNowPlayingControlsVisibility
            && artwork == other.artwork;
    }

    bool operator!=(const NowPlayingInfo& other) const
    {
        return !(*this == other);
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<NowPlayingInfo> decode(Decoder&);
};

template<class Encoder> inline void NowPlayingInfo::encode(Encoder& encoder) const
{
    encoder << title << artist << album << sourceApplicationIdentifier << duration << currentTime << rate << supportsSeeking << uniqueIdentifier << isPlaying << allowsNowPlayingControlsVisibility << artwork;
}

template<class Decoder> inline std::optional<NowPlayingInfo> NowPlayingInfo::decode(Decoder& decoder)
{
    String title;
    if (!decoder.decode(title))
        return { };

    String artist;
    if (!decoder.decode(artist))
        return { };

    String album;
    if (!decoder.decode(album))
        return { };

    String sourceApplicationIdentifier;
    if (!decoder.decode(sourceApplicationIdentifier))
        return { };

    double duration;
    if (!decoder.decode(duration))
        return { };

    double currentTime;
    if (!decoder.decode(currentTime))
        return { };

    double rate;
    if (!decoder.decode(rate))
        return { };

    bool supportsSeeking;
    if (!decoder.decode(supportsSeeking))
        return { };

    MediaUniqueIdentifier uniqueIdentifier;
    if (!decoder.decode(uniqueIdentifier))
        return { };

    bool isPlaying;
    if (!decoder.decode(isPlaying))
        return { };

    bool allowsNowPlayingControlsVisibility;
    if (!decoder.decode(allowsNowPlayingControlsVisibility))
        return { };

    std::optional<NowPlayingInfoArtwork> artwork;
    if (!decoder.decode(artwork))
        return { };

    return NowPlayingInfo { WTFMove(title), WTFMove(artist), WTFMove(album), WTFMove(sourceApplicationIdentifier), duration, currentTime, rate, supportsSeeking, uniqueIdentifier, isPlaying, allowsNowPlayingControlsVisibility, WTFMove(artwork) };
}

} // namespace WebCore
