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
#include <WebCore/MediaDescription.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct MediaDescriptionInfo {
    MediaDescriptionInfo(const AtomString& codec, bool isVideo, bool isAudio, bool isText)
        : m_codec(codec)
        , m_isVideo(isVideo)
        , m_isAudio(isAudio)
        , m_isText(isText)
    {
    }

    MediaDescriptionInfo(const WebCore::MediaDescription& description)
        : m_codec(description.codec())
        , m_isVideo(description.isVideo())
        , m_isAudio(description.isAudio())
        , m_isText(description.isText())
    {
    }

    AtomString m_codec;
    bool m_isVideo { false };
    bool m_isAudio { false };
    bool m_isText { false };

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << m_codec;
        encoder << m_isVideo;
        encoder << m_isAudio;
        encoder << m_isText;
    }

    template <class Decoder>
    static std::optional<MediaDescriptionInfo> decode(Decoder& decoder)
    {
        std::optional<AtomString> codec;
        decoder >> codec;
        if (!codec)
            return std::nullopt;

        std::optional<bool> isVideo;
        decoder >> isVideo;
        if (!isVideo)
            return std::nullopt;

        std::optional<bool> isAudio;
        decoder >> isAudio;
        if (!isAudio)
            return std::nullopt;

        std::optional<bool> isText;
        decoder >> isText;
        if (!isText)
            return std::nullopt;

        return {{
            WTFMove(*codec),
            *isVideo,
            *isAudio,
            *isText
        }};
    }
};

class RemoteMediaDescription : public WebCore::MediaDescription {
public:
    static Ref<RemoteMediaDescription> create(const MediaDescriptionInfo& descriptionInfo)
    {
        return adoptRef(*new RemoteMediaDescription(descriptionInfo));
    }

    virtual ~RemoteMediaDescription() = default;

    AtomString codec() const final { return m_codec; }
    bool isVideo() const final { return m_isVideo; }
    bool isAudio() const final { return m_isAudio; }
    bool isText() const final { return m_isText;}

private:
    RemoteMediaDescription(const MediaDescriptionInfo& descriptionInfo)
        : m_codec(descriptionInfo.m_codec)
        , m_isVideo(descriptionInfo.m_isVideo)
        , m_isAudio(descriptionInfo.m_isAudio)
        , m_isText(descriptionInfo.m_isText)
    {
    }

    AtomString m_codec;
    bool m_isVideo { false };
    bool m_isAudio { false };
    bool m_isText { false };
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(MEDIA_SOURCE)
