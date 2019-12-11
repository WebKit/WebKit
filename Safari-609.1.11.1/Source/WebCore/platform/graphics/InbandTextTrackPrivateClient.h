/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_TRACK)

#include "Color.h"
#include "TrackPrivateBase.h"
#include <wtf/JSONValues.h>
#include <wtf/MediaTime.h>

#if ENABLE(DATACUE_VALUE)
#include "SerializedPlatformRepresentation.h"
#endif

namespace WebCore {

class InbandTextTrackPrivate;
class ISOWebVTTCue;

class GenericCueData : public RefCounted<GenericCueData> {
public:
    static Ref<GenericCueData> create() { return adoptRef(*new GenericCueData); }

    MediaTime startTime() const { return m_startTime; }
    void setStartTime(const MediaTime& startTime) { m_startTime = startTime; }

    MediaTime endTime() const { return m_endTime; }
    void setEndTime(const MediaTime& endTime) { m_endTime = endTime; }

    const String& id() const { return m_id; }
    void setId(const String& id) { m_id = id; }

    const String& content() const { return m_content; }
    void setContent(const String& content) { m_content = content; }

    double line() const { return m_line; }
    void setLine(double line) { m_line = line; }

    double position() const { return m_position; }
    void setPosition(double position) { m_position = position; }

    double size() const { return m_size; }
    void setSize(double size) { m_size = size; }

    enum Alignment { None, Start, Middle, End };
    Alignment align() const { return m_align; }
    void setAlign(Alignment align) { m_align = align; }

    const String& fontName() const { return m_fontName; }
    void setFontName(const String& fontName) { m_fontName = fontName; }

    double baseFontSize() const { return m_baseFontSize; }
    void setBaseFontSize(double baseFontSize) { m_baseFontSize = baseFontSize; }

    double relativeFontSize() const { return m_relativeFontSize; }
    void setRelativeFontSize(double relativeFontSize) { m_relativeFontSize = relativeFontSize; }

    const Color& foregroundColor() const { return m_foregroundColor; }
    void setForegroundColor(const Color& color) { m_foregroundColor = color; }

    const Color& backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const Color& color) { m_backgroundColor = color; }

    const Color& highlightColor() const { return m_highlightColor; }
    void setHighlightColor(const Color& color) { m_highlightColor = color; }

    enum Status { Uninitialized, Partial, Complete };
    Status status() { return m_status; }
    void setStatus(Status status) { m_status = status; }

    bool doesExtendCueData(const GenericCueData&) const;

    String toJSONString() const;

private:
    GenericCueData() = default;

    MediaTime m_startTime;
    MediaTime m_endTime;
    String m_id;
    String m_content;
    double m_line { -1 };
    double m_position { -1 };
    double m_size { -1 };
    Alignment m_align { None };
    String m_fontName;
    double m_baseFontSize { 0 };
    double m_relativeFontSize { 0 };
    Color m_foregroundColor;
    Color m_backgroundColor;
    Color m_highlightColor;
    Status m_status { Uninitialized };
};

inline String GenericCueData::toJSONString() const
{
    auto object = JSON::Object::create();

#if !LOG_DISABLED
    object->setString("text"_s, m_content);
#endif
    object->setDouble("start"_s, m_startTime.toDouble());
    object->setDouble("end"_s, m_endTime.toDouble());

    const char* status;
    switch (m_status) {
    case GenericCueData::Uninitialized:
        status = "Uninitialized";
        break;
    case GenericCueData::Partial:
        status = "Partial";
        break;
    case GenericCueData::Complete:
        status = "Complete";
        break;
    }
    object->setString("status", status);

    if (!m_id.isEmpty())
        object->setString("id", m_id);

    if (m_line > 0)
        object->setDouble("line"_s, m_line);

    if (m_size > 0)
        object->setDouble("size"_s, m_size);

    if (m_position > 0)
        object->setDouble("position"_s, m_position);

    if (m_align != None) {
        const char* align;
        switch (m_align) {
        case GenericCueData::Start:
            align = "Start";
            break;
        case GenericCueData::Middle:
            align = "Middle";
            break;
        case GenericCueData::End:
            align = "End";
            break;
        case GenericCueData::None:
            align = "None";
            break;
        }
        object->setString("align"_s, align);
    }

    if (m_foregroundColor.isValid())
        object->setString("foregroundColor"_s, m_foregroundColor.serialized());

    if (m_backgroundColor.isValid())
        object->setString("backgroundColor"_s, m_backgroundColor.serialized());

    if (m_highlightColor.isValid())
        object->setString("highlightColor"_s, m_highlightColor.serialized());

    if (m_baseFontSize)
        object->setDouble("baseFontSize"_s, m_baseFontSize);

    if (m_relativeFontSize)
        object->setDouble("relativeFontSize"_s, m_relativeFontSize);

    if (!m_fontName.isEmpty())
        object->setString("font"_s, m_fontName);

    return object->toJSONString();
}

inline bool GenericCueData::doesExtendCueData(const GenericCueData& other) const
{
    if (m_relativeFontSize != other.m_relativeFontSize)
        return false;
    if (m_baseFontSize != other.m_baseFontSize)
        return false;
    if (m_position != other.m_position)
        return false;
    if (m_line != other.m_line)
        return false;
    if (m_size != other.m_size)
        return false;
    if (m_align != other.m_align)
        return false;
    if (m_foregroundColor != other.m_foregroundColor)
        return false;
    if (m_backgroundColor != other.m_backgroundColor)
        return false;
    if (m_highlightColor != other.m_highlightColor)
        return false;
    if (m_fontName != other.m_fontName)
        return false;
    if (m_id != other.m_id)
        return false;
    if (m_content != other.m_content)
        return false;
    
    return true;
}
    
class InbandTextTrackPrivateClient : public TrackPrivateBaseClient {
public:
    virtual ~InbandTextTrackPrivateClient() = default;

    virtual void addDataCue(const MediaTime& start, const MediaTime& end, const void*, unsigned) = 0;

#if ENABLE(DATACUE_VALUE)
    virtual void addDataCue(const MediaTime& start, const MediaTime& end, Ref<SerializedPlatformRepresentation>&&, const String&) = 0;
    virtual void updateDataCue(const MediaTime& start, const MediaTime& end, SerializedPlatformRepresentation&) = 0;
    virtual void removeDataCue(const MediaTime& start, const MediaTime& end, SerializedPlatformRepresentation&) = 0;
#endif

    virtual void addGenericCue(GenericCueData&) = 0;
    virtual void updateGenericCue(GenericCueData&) = 0;
    virtual void removeGenericCue(GenericCueData&) = 0;

    virtual void parseWebVTTFileHeader(String&&) { ASSERT_NOT_REACHED(); }
    virtual void parseWebVTTCueData(const char* data, unsigned length) = 0;
    virtual void parseWebVTTCueData(const ISOWebVTTCue&) = 0;
};

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::GenericCueData> {
    static String toString(const WebCore::GenericCueData& cue)
    {
        return cue.toJSONString();
    }
};

}

#endif
