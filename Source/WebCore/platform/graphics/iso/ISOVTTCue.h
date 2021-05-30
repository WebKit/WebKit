/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "ISOBox.h"
#include <wtf/MediaTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// 4 bytes : 4CC : identifier = 'vttc'
// 4 bytes : unsigned : length
// N bytes : CueSourceIDBox : box : optional
// N bytes : CueIDBox : box : optional
// N bytes : CueTimeBox : box : optional
// N bytes : CueSettingsBox : box : optional
// N bytes : CuePayloadBox : box : required

class WEBCORE_EXPORT ISOWebVTTCue final : public ISOBox {
public:
    ISOWebVTTCue(const MediaTime& presentationTime, const MediaTime& duration);
    ISOWebVTTCue(MediaTime&& presentationTime, MediaTime&& duration, String&& sourceID, String&& id, String&& originalStartTime, String&& settings, String&& cueText);

    static FourCC boxTypeName() { return "vttc"; }

    const MediaTime& presentationTime() const { return m_presentationTime; }
    const MediaTime& duration() const { return m_duration; }

    const String& sourceID() const { return m_sourceID; }
    const String& id() const { return m_identifier; }
    const String& originalStartTime() const { return m_originalStartTime; }
    const String& settings() const { return m_settings; }
    const String& cueText() const { return m_cueText; }

    String toJSONString() const;

    template<class Encoder>
    void encode(Encoder& encoder) const
    {
        encoder << m_presentationTime;
        encoder << m_duration;
        encoder << m_sourceID;
        encoder << m_identifier;
        encoder << m_originalStartTime;
        encoder << m_settings;
        encoder << m_cueText;
    }

    template <class Decoder>
    static std::optional<ISOWebVTTCue> decode(Decoder& decoder)
    {
        std::optional<MediaTime> presentationTime;
        decoder >> presentationTime;
        if (!presentationTime)
            return std::nullopt;

        std::optional<MediaTime> duration;
        decoder >> duration;
        if (!duration)
            return std::nullopt;

        std::optional<String> sourceID;
        decoder >> sourceID;
        if (!sourceID)
            return std::nullopt;

        std::optional<String> identifier;
        decoder >> identifier;
        if (!identifier)
            return std::nullopt;

        std::optional<String> originalStartTime;
        decoder >> originalStartTime;
        if (!originalStartTime)
            return std::nullopt;

        std::optional<String> settings;
        decoder >> settings;
        if (!settings)
            return std::nullopt;

        std::optional<String> cueText;
        decoder >> cueText;
        if (!cueText)
            return std::nullopt;

        return {{
            WTFMove(*presentationTime),
            WTFMove(*duration),
            WTFMove(*sourceID),
            WTFMove(*identifier),
            WTFMove(*originalStartTime),
            WTFMove(*settings),
            WTFMove(*cueText)
        }};
    }

private:
    bool parse(JSC::DataView&, unsigned& offset) override;

    MediaTime m_presentationTime;
    MediaTime m_duration;

    String m_sourceID;
    String m_identifier;
    String m_originalStartTime;
    String m_settings;
    String m_cueText;
};

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::ISOWebVTTCue> {
    static String toString(const WebCore::ISOWebVTTCue& cue)
    {
        return cue.toJSONString();
    }
};

} // namespace WTF
