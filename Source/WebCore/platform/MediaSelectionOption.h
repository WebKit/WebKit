/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/text/WTFString.h>

namespace WebCore {

struct MediaSelectionOption {
    enum class MediaType {
        Unknown,
        Audio,
        Subtitles,
        Captions,
        Metadata,
    };

    enum class LegibleType {
        Regular,
        LegibleOff,
        LegibleAuto,
    };

    MediaSelectionOption() = default;
    MediaSelectionOption(MediaType mediaType, const String& displayName, LegibleType legibleType)
        : mediaType { mediaType }
        , displayName { displayName }
        , legibleType { legibleType }
    {
    }

    MediaSelectionOption isolatedCopy() const & { return { mediaType, displayName.isolatedCopy(), legibleType }; }
    MediaSelectionOption isolatedCopy() && { return { mediaType, WTFMove(displayName).isolatedCopy(), legibleType }; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool WARN_UNUSED_RETURN decode(Decoder&, MediaSelectionOption&);

    MediaType mediaType { MediaType::Unknown };
    String displayName;
    LegibleType legibleType { LegibleType::Regular };
};

template<class Encoder> void MediaSelectionOption::encode(Encoder& encoder) const
{
    encoder << mediaType;
    encoder << displayName;
    encoder << legibleType;
}

template<class Decoder> bool MediaSelectionOption::decode(Decoder& decoder, MediaSelectionOption& option)
{
    return decoder.decode(option.mediaType)
        && decoder.decode(option.displayName)
        && decoder.decode(option.legibleType);
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::MediaSelectionOption::MediaType> {
    using values = EnumValues<
        WebCore::MediaSelectionOption::MediaType,
        WebCore::MediaSelectionOption::MediaType::Unknown,
        WebCore::MediaSelectionOption::MediaType::Audio,
        WebCore::MediaSelectionOption::MediaType::Subtitles,
        WebCore::MediaSelectionOption::MediaType::Captions,
        WebCore::MediaSelectionOption::MediaType::Metadata
    >;
};

template<> struct EnumTraits<WebCore::MediaSelectionOption::LegibleType> {
    using values = EnumValues<
        WebCore::MediaSelectionOption::LegibleType,
        WebCore::MediaSelectionOption::LegibleType::Regular,
        WebCore::MediaSelectionOption::LegibleType::LegibleOff,
        WebCore::MediaSelectionOption::LegibleType::LegibleAuto
    >;
};

} // namespace WTF
