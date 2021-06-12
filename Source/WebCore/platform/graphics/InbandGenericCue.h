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

#if ENABLE(VIDEO)

#include "Color.h"
#include "InbandGenericCueIdentifier.h"
#include <wtf/MediaTime.h>

namespace WebCore {

struct GenericCueData {

    enum class Alignment : uint8_t { None, Start, Middle, End };
    enum class Status : uint8_t { Uninitialized, Partial, Complete };

    GenericCueData() = default;
    GenericCueData(InbandGenericCueIdentifier uniqueId, const MediaTime& startTime, const MediaTime& endTime, const String& id, const String& content, const String& fontName, double line, double position, double size, double baseFontSize, double relativeFontSize, const Color& foregroundColor, const Color& backgroundColor, const Color& highlightColor, GenericCueData::Alignment align, GenericCueData::Status status)
        : m_uniqueId(uniqueId)
        , m_startTime(startTime)
        , m_endTime(endTime)
        , m_id(id)
        , m_content(content)
        , m_fontName(fontName)
        , m_line(line)
        , m_position(position)
        , m_size(size)
        , m_baseFontSize(baseFontSize)
        , m_relativeFontSize(relativeFontSize)
        , m_foregroundColor(foregroundColor)
        , m_backgroundColor(backgroundColor)
        , m_highlightColor(highlightColor)
        , m_align(align)
        , m_status(status)
    {
        ASSERT(isValid());
    }

    bool isValid() const { return !!m_uniqueId; }
    bool equalNotConsideringTimesOrId(const GenericCueData&) const;

    InbandGenericCueIdentifier m_uniqueId;
    MediaTime m_startTime;
    MediaTime m_endTime;
    String m_id;
    String m_content;
    String m_fontName;
    double m_line { -1 };
    double m_position { -1 };
    double m_size { -1 };
    double m_baseFontSize { 0 };
    double m_relativeFontSize { 0 };
    Color m_foregroundColor;
    Color m_backgroundColor;
    Color m_highlightColor;
    Alignment m_align { Alignment::None };
    Status m_status { Status::Uninitialized };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<GenericCueData> decode(Decoder&);
};

template <class Decoder>
std::optional<GenericCueData> GenericCueData::decode(Decoder& decoder)
{
    std::optional<InbandGenericCueIdentifier> uniqueId;
    decoder >> uniqueId;
    if (!uniqueId)
        return std::nullopt;

    std::optional<MediaTime> startTime;
    decoder >> startTime;
    if (!startTime)
        return std::nullopt;

    std::optional<MediaTime> endTime;
    decoder >> endTime;
    if (!endTime)
        return std::nullopt;

    std::optional<String> identifier;
    decoder >> identifier;
    if (!identifier)
        return std::nullopt;

    std::optional<String> content;
    decoder >> content;
    if (!content)
        return std::nullopt;

    std::optional<String> fontName;
    decoder >> fontName;
    if (!fontName)
        return std::nullopt;

    std::optional<double> line;
    decoder >> line;
    if (!line)
        return std::nullopt;

    std::optional<double> position;
    decoder >> position;
    if (!position)
        return std::nullopt;

    std::optional<double> size;
    decoder >> size;
    if (!size)
        return std::nullopt;

    std::optional<double> baseFontSize;
    decoder >> baseFontSize;
    if (!baseFontSize)
        return std::nullopt;

    std::optional<double> relativeFontSize;
    decoder >> relativeFontSize;
    if (!relativeFontSize)
        return std::nullopt;

    std::optional<Color> foregroundColor;
    decoder >> foregroundColor;
    if (!foregroundColor)
        return std::nullopt;

    std::optional<Color> backgroundColor;
    decoder >> backgroundColor;
    if (!backgroundColor)
        return std::nullopt;

    std::optional<Color> highlightColor;
    decoder >> highlightColor;
    if (!highlightColor)
        return std::nullopt;

    std::optional<Alignment> alignment;
    decoder >> alignment;
    if (!alignment)
        return std::nullopt;

    std::optional<Status> status;
    decoder >> status;
    if (!status)
        return std::nullopt;

    GenericCueData data = {

        WTFMove(*uniqueId),

        WTFMove(*startTime),
        WTFMove(*endTime),

        WTFMove(*identifier),
        WTFMove(*content),
        WTFMove(*fontName),

        WTFMove(*line),
        WTFMove(*position),
        WTFMove(*size),
        WTFMove(*baseFontSize),
        WTFMove(*relativeFontSize),

        WTFMove(*foregroundColor),
        WTFMove(*backgroundColor),
        WTFMove(*highlightColor),

        WTFMove(*alignment),
        WTFMove(*status),

    };

    if (!data.isValid())
        return std::nullopt;

    return data;
}

template<class Encoder>
void GenericCueData::encode(Encoder& encoder) const
{
    encoder << m_uniqueId;
    encoder << m_startTime;
    encoder << m_endTime;
    encoder << m_id;
    encoder << m_content;
    encoder << m_fontName;
    encoder << m_line;
    encoder << m_position;
    encoder << m_size;
    encoder << m_baseFontSize;
    encoder << m_relativeFontSize;
    encoder << m_foregroundColor;
    encoder << m_backgroundColor;
    encoder << m_highlightColor;
    encoder << m_align;
    encoder << m_status;
}

class InbandGenericCue : public RefCounted<InbandGenericCue> {
public:
    static Ref<InbandGenericCue> create() { return adoptRef(*new InbandGenericCue); }
    static Ref<InbandGenericCue> create(GenericCueData&& cueData) { return adoptRef(*new InbandGenericCue(WTFMove(cueData))); }

    InbandGenericCueIdentifier uniqueId() const { return m_cueData.m_uniqueId; }

    MediaTime startTime() const { return m_cueData.m_startTime; }
    void setStartTime(const MediaTime& startTime) { m_cueData.m_startTime = startTime; }

    MediaTime endTime() const { return m_cueData.m_endTime; }
    void setEndTime(const MediaTime& endTime) { m_cueData.m_endTime = endTime; }

    const String& id() const { return m_cueData.m_id; }
    void setId(const String& id) { m_cueData.m_id = id; }

    const String& content() const { return m_cueData.m_content; }
    void setContent(const String& content) { m_cueData.m_content = content; }

    double line() const { return m_cueData.m_line; }
    void setLine(double line) { m_cueData.m_line = line; }

    double position() const { return m_cueData.m_position; }
    void setPosition(double position) { m_cueData.m_position = position; }

    double size() const { return m_cueData.m_size; }
    void setSize(double size) { m_cueData.m_size = size; }

    GenericCueData::Alignment align() const { return m_cueData.m_align; }
    void setAlign(GenericCueData::Alignment align) { m_cueData.m_align = align; }

    const String& fontName() const { return m_cueData.m_fontName; }
    void setFontName(const String& fontName) { m_cueData.m_fontName = fontName; }

    double baseFontSize() const { return m_cueData.m_baseFontSize; }
    void setBaseFontSize(double baseFontSize) { m_cueData.m_baseFontSize = baseFontSize; }

    double relativeFontSize() const { return m_cueData.m_relativeFontSize; }
    void setRelativeFontSize(double relativeFontSize) { m_cueData.m_relativeFontSize = relativeFontSize; }

    const Color& foregroundColor() const { return m_cueData.m_foregroundColor; }
    void setForegroundColor(const Color& color) { m_cueData.m_foregroundColor = color; }

    const Color& backgroundColor() const { return m_cueData.m_backgroundColor; }
    void setBackgroundColor(const Color& color) { m_cueData.m_backgroundColor = color; }

    const Color& highlightColor() const { return m_cueData.m_highlightColor; }
    void setHighlightColor(const Color& color) { m_cueData.m_highlightColor = color; }

    GenericCueData::Status status() { return m_cueData.m_status; }
    void setStatus(GenericCueData::Status status) { m_cueData.m_status = status; }

    bool doesExtendCueData(const InbandGenericCue& other) const { return m_cueData.equalNotConsideringTimesOrId(other.m_cueData); }

    String toJSONString() const;

    const GenericCueData& cueData() const { return m_cueData; }

private:
    InbandGenericCue();
    explicit InbandGenericCue(GenericCueData&& cueData)
        : m_cueData(WTFMove(cueData))
    {
        ASSERT(m_cueData.isValid());
    }

    GenericCueData m_cueData;
};

} // namespace WebCore

namespace WTF {

template<typename Type> struct LogArgument;
template <>
struct LogArgument<WebCore::InbandGenericCue> {
    static String toString(const WebCore::InbandGenericCue& cue)
    {
        return cue.toJSONString();
    }
};

template<> struct EnumTraits<WebCore::GenericCueData::Alignment> {
    using values = EnumValues<
        WebCore::GenericCueData::Alignment,
        WebCore::GenericCueData::Alignment::None,
        WebCore::GenericCueData::Alignment::Start,
        WebCore::GenericCueData::Alignment::Middle,
        WebCore::GenericCueData::Alignment::End
    >;
};

template<> struct EnumTraits<WebCore::GenericCueData::Status> {
    using values = EnumValues<
        WebCore::GenericCueData::Status,
        WebCore::GenericCueData::Status::Uninitialized,
        WebCore::GenericCueData::Status::Partial,
        WebCore::GenericCueData::Status::Complete
    >;
};

} // namespace WTF

#endif
