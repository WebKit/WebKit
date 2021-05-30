/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#if ENABLE(AVF_CAPTIONS)

#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class TextTrack;
class InbandTextTrackPrivate;

struct PlatformTextTrackData {
    enum class TrackKind : uint8_t {
        Subtitle = 0,
        Caption = 1,
        Description = 2,
        Chapter = 3,
        MetaData = 4,
        Forced = 5,
    };
    enum class TrackType : uint8_t {
        InBand = 0,
        OutOfBand = 1,
        Script = 2
    };
    enum class TrackMode : uint8_t {
        Disabled,
        Hidden,
        Showing
    };

    PlatformTextTrackData() = default;
    PlatformTextTrackData(const String& label, const String& language, const String& url, TrackMode mode, TrackKind kind, TrackType type, int uniqueId, bool isDefault)
        : m_label(label)
        , m_language(language)
        , m_url(url)
        , m_mode(mode)
        , m_kind(kind)
        , m_type(type)
        , m_uniqueId(uniqueId)
        , m_isDefault(isDefault)
    {
    }

    String m_label;
    String m_language;
    String m_url;
    TrackMode m_mode;
    TrackKind m_kind;
    TrackType m_type;
    int m_uniqueId;
    bool m_isDefault;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PlatformTextTrackData> decode(Decoder&);
};

template <class Decoder>
std::optional<PlatformTextTrackData> PlatformTextTrackData::decode(Decoder& decoder)
{
    std::optional<String> label;
    decoder >> label;
    if (!label)
        return std::nullopt;

    std::optional<String> language;
    decoder >> language;
    if (!language)
        return std::nullopt;

    std::optional<String> url;
    decoder >> url;
    if (!url)
        return std::nullopt;

    std::optional<TrackMode> mode;
    decoder >> mode;
    if (!mode)
        return std::nullopt;

    std::optional<TrackKind> kind;
    decoder >> kind;
    if (!kind)
        return std::nullopt;

    std::optional<TrackType> type;
    decoder >> type;
    if (!type)
        return std::nullopt;

    std::optional<int> uniqueId;
    decoder >> uniqueId;
    if (!uniqueId)
        return std::nullopt;

    std::optional<bool> isDefault;
    decoder >> isDefault;
    if (!isDefault)
        return std::nullopt;

    PlatformTextTrackData data = {
        WTFMove(*label),
        WTFMove(*language),
        WTFMove(*url),
        WTFMove(*mode),
        WTFMove(*kind),
        WTFMove(*type),
        WTFMove(*uniqueId),
        WTFMove(*isDefault),
    };

    return data;
}

template<class Encoder>
void PlatformTextTrackData::encode(Encoder& encoder) const
{
    encoder << m_label;
    encoder << m_language;
    encoder << m_url;
    encoder << m_mode;
    encoder << m_kind;
    encoder << m_type;
    encoder << m_uniqueId;
    encoder << m_isDefault;
}

class PlatformTextTrackClient {
public:
    virtual ~PlatformTextTrackClient() = default;
    
    virtual TextTrack* publicTrack() = 0;
    virtual InbandTextTrackPrivate* privateTrack() { return 0; }
};

class PlatformTextTrack : public RefCounted<PlatformTextTrack> {
public:
    static Ref<PlatformTextTrack> create(PlatformTextTrackClient* client, const String& label, const String& language, PlatformTextTrackData::TrackMode mode, PlatformTextTrackData::TrackKind kind, PlatformTextTrackData::TrackType type, int uniqueId)
    {
        return adoptRef(*new PlatformTextTrack(client, label, language, String(), mode, kind, type, uniqueId, false));
    }

    static Ref<PlatformTextTrack> createOutOfBand(const String& label, const String& language, const String& url, PlatformTextTrackData::TrackMode mode, PlatformTextTrackData::TrackKind kind, int uniqueId, bool isDefault)
    {
        return adoptRef(*new PlatformTextTrack(nullptr, label, language, url, mode, kind, PlatformTextTrackData::TrackType::OutOfBand, uniqueId, isDefault));
    }
    
    static Ref<PlatformTextTrack> create(PlatformTextTrackData&& data)
    {
        return adoptRef(*new PlatformTextTrack(WTFMove(data)));
    }

    virtual ~PlatformTextTrack() = default;
    
    PlatformTextTrackData::TrackType type() const { return m_trackData.m_type; }
    PlatformTextTrackData::TrackKind kind() const { return m_trackData.m_kind; }
    PlatformTextTrackData::TrackMode mode() const { return m_trackData.m_mode; }
    const String& label() const { return m_trackData.m_label; }
    const String& language() const { return m_trackData.m_language; }
    const String& url() const { return m_trackData.m_url; }
    int uniqueId() const { return m_trackData.m_uniqueId; }
    bool isDefault() const { return m_trackData.m_isDefault; }
    PlatformTextTrackClient* client() const { return m_client; }
    
    PlatformTextTrackData data() const { return m_trackData; }

protected:
    PlatformTextTrack(PlatformTextTrackClient* client, const String& label, const String& language, const String& url, PlatformTextTrackData::TrackMode mode, PlatformTextTrackData::TrackKind kind, PlatformTextTrackData::TrackType type, int uniqueId, bool isDefault)
        : m_client(client)
    {
        m_trackData = {
            label,
            language,
            url,
            mode,
            kind,
            type,
            uniqueId,
            isDefault,
        };
    }
    
    PlatformTextTrack(PlatformTextTrackData&& data)
        : m_trackData(WTFMove(data))
    {
    }

    PlatformTextTrackData m_trackData;
    PlatformTextTrackClient* m_client;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::PlatformTextTrackData::TrackKind> {
    using values = EnumValues<
        WebCore::PlatformTextTrackData::TrackKind,
        WebCore::PlatformTextTrackData::TrackKind::Subtitle,
        WebCore::PlatformTextTrackData::TrackKind::Caption,
        WebCore::PlatformTextTrackData::TrackKind::Description,
        WebCore::PlatformTextTrackData::TrackKind::Chapter,
        WebCore::PlatformTextTrackData::TrackKind::MetaData,
        WebCore::PlatformTextTrackData::TrackKind::Forced
    >;
};

template<> struct EnumTraits<WebCore::PlatformTextTrackData::TrackType> {
    using values = EnumValues<
        WebCore::PlatformTextTrackData::TrackType,
        WebCore::PlatformTextTrackData::TrackType::InBand,
        WebCore::PlatformTextTrackData::TrackType::OutOfBand,
        WebCore::PlatformTextTrackData::TrackType::Script
    >;
};

template<> struct EnumTraits<WebCore::PlatformTextTrackData::TrackMode> {
    using values = EnumValues<
        WebCore::PlatformTextTrackData::TrackMode,
        WebCore::PlatformTextTrackData::TrackMode::Disabled,
        WebCore::PlatformTextTrackData::TrackMode::Hidden,
        WebCore::PlatformTextTrackData::TrackMode::Showing
    >;
};

} // namespace WTF

#endif // ENABLE(AVF_CAPTIONS)


