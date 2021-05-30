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

#include "MediaImage.h"

#if ENABLE(MEDIA_SESSION)

namespace WebCore {

struct MediaMetadataInit {
    String title;
    String artist;
    String album;
#if ENABLE(MEDIA_SESSION_PLAYLIST)
    String trackIdentifier;
#endif
    Vector<MediaImage> artwork;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<MediaMetadataInit> decode(Decoder&);
};

template<class Encoder> inline void MediaMetadataInit::encode(Encoder& encoder) const
{
    encoder << title << artist << album
#if ENABLE(MEDIA_SESSION_PLAYLIST)
    << trackIdentifier
#endif
    << artwork;
}

template<class Decoder> inline std::optional<MediaMetadataInit> MediaMetadataInit::decode(Decoder& decoder)
{
    std::optional<String> title;
    decoder >> title;
    if (!title)
        return { };

    std::optional<String> artist;
    decoder >> artist;
    if (!artist)
        return { };

    std::optional<String> album;
    decoder >> album;
    if (!album)
        return { };

#if ENABLE(MEDIA_SESSION_PLAYLIST)
    std::optional<String> trackIdentifier;
    decoder >> trackIdentifier;
    if (!trackIdentifier)
        return { };
#endif

    std::optional<Vector<MediaImage>> artwork;
    decoder >> artwork;
    if (!artwork)
        return { };

    return { {
        WTFMove(*title),
        WTFMove(*artist),
        WTFMove(*album),
#if ENABLE(MEDIA_SESSION_PLAYLIST)
        WTFMove(*trackIdentifier),
#endif
        WTFMove(*artwork)
    } };
}

}

#endif // ENABLE(MEDIA_SESSION)
