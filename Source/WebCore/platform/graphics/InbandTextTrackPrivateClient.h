/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#ifndef InbandTextTrackPrivateClient_h
#define InbandTextTrackPrivateClient_h

#include "Color.h"
#include "TrackPrivateBase.h"
#include <wtf/MediaTime.h>

#if ENABLE(DATACUE_VALUE)
#include "SerializedPlatformRepresentation.h"
#endif

#if ENABLE(VIDEO_TRACK)

namespace WebCore {

class InbandTextTrackPrivate;
class ISOWebVTTCue;

class GenericCueData : public RefCounted<GenericCueData> {
public:

    static PassRefPtr<GenericCueData> create() { return adoptRef(new GenericCueData()); }
    virtual ~GenericCueData() { }

    MediaTime startTime() const { return m_startTime; }
    void setStartTime(const MediaTime& startTime) { m_startTime = startTime; }

    MediaTime endTime() const { return m_endTime; }
    void setEndTime(const MediaTime& endTime) { m_endTime = endTime; }

    String id() const { return m_id; }
    void setId(String id) { m_id = id; }

    String content() const { return m_content; }
    void setContent(String content) { m_content = content; }

    double line() const { return m_line; }
    void setLine(double line) { m_line = line; }

    double position() const { return m_position; }
    void setPosition(double position) { m_position = position; }

    double size() const { return m_size; }
    void setSize(double size) { m_size = size; }

    enum Alignment {
        None,
        Start,
        Middle,
        End
    };
    Alignment align() const { return m_align; }
    void setAlign(Alignment align) { m_align = align; }

    String fontName() const { return m_fontName; }
    void setFontName(String fontName) { m_fontName = fontName; }

    double baseFontSize() const { return m_baseFontSize; }
    void setBaseFontSize(double baseFontSize) { m_baseFontSize = baseFontSize; }

    double relativeFontSize() const { return m_relativeFontSize; }
    void setRelativeFontSize(double relativeFontSize) { m_relativeFontSize = relativeFontSize; }

    Color foregroundColor() const { return m_foregroundColor; }
    void setForegroundColor(RGBA32 color) { m_foregroundColor.setRGB(color); }

    Color backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(RGBA32 color) { m_backgroundColor.setRGB(color); }
    
    Color highlightColor() const { return m_highlightColor; }
    void setHighlightColor(RGBA32 color) { m_highlightColor.setRGB(color); }
    
    enum Status {
        Uninitialized,
        Partial,
        Complete,
    };
    Status status() { return m_status; }
    void setStatus(Status status) { m_status = status; }

    bool doesExtendCueData(const GenericCueData&) const;

private:
    GenericCueData()
        : m_line(-1)
        , m_position(-1)
        , m_size(-1)
        , m_align(None)
        , m_baseFontSize(0)
        , m_relativeFontSize(0)
        , m_status(Uninitialized)
    {
    }

    MediaTime m_startTime;
    MediaTime m_endTime;
    String m_id;
    String m_content;
    double m_line;
    double m_position;
    double m_size;
    Alignment m_align;
    String m_fontName;
    double m_baseFontSize;
    double m_relativeFontSize;
    Color m_foregroundColor;
    Color m_backgroundColor;
    Color m_highlightColor;
    Status m_status;
};

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
    virtual ~InbandTextTrackPrivateClient() { }

    virtual void addDataCue(InbandTextTrackPrivate*, const MediaTime& start, const MediaTime& end, const void*, unsigned) = 0;

#if ENABLE(DATACUE_VALUE)
    virtual void addDataCue(InbandTextTrackPrivate*, const MediaTime& start, const MediaTime& end, PassRefPtr<SerializedPlatformRepresentation>, const String&) = 0;
    virtual void updateDataCue(InbandTextTrackPrivate*, const MediaTime& start, const MediaTime& end, PassRefPtr<SerializedPlatformRepresentation>) = 0;
    virtual void removeDataCue(InbandTextTrackPrivate*, const MediaTime& start, const MediaTime& end, PassRefPtr<SerializedPlatformRepresentation>) = 0;
#endif

    virtual void addGenericCue(InbandTextTrackPrivate*, PassRefPtr<GenericCueData>) = 0;
    virtual void updateGenericCue(InbandTextTrackPrivate*, GenericCueData*) = 0;
    virtual void removeGenericCue(InbandTextTrackPrivate*, GenericCueData*) = 0;

    virtual void parseWebVTTFileHeader(InbandTextTrackPrivate*, String) { ASSERT_NOT_REACHED(); }
    virtual void parseWebVTTCueData(InbandTextTrackPrivate*, const char* data, unsigned length) = 0;
    virtual void parseWebVTTCueData(InbandTextTrackPrivate*, const ISOWebVTTCue&) = 0;
};

} // namespace WebCore

#endif
#endif
