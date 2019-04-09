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
#include "VTTCue.h"

namespace WebCore {

class GenericCueData;

// A "generic" cue is a non-WebVTT cue, so it is not positioned/sized with the WebVTT logic.
class TextTrackCueGeneric final : public VTTCue {
    WTF_MAKE_ISO_ALLOCATED_EXPORT(TextTrackCueGeneric, WEBCORE_EXPORT);
public:
    static Ref<TextTrackCueGeneric> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, const String& content)
    {
        return adoptRef(*new TextTrackCueGeneric(context, start, end, content));
    }

    ExceptionOr<void> setLine(double) final;
    ExceptionOr<void> setPosition(const LineAndPositionSetting&) final;

    bool useDefaultPosition() const { return m_useDefaultPosition; }

    double baseFontSizeRelativeToVideoHeight() const { return m_baseFontSizeRelativeToVideoHeight; }
    void setBaseFontSizeRelativeToVideoHeight(double size) { m_baseFontSizeRelativeToVideoHeight = size; }

    double fontSizeMultiplier() const { return m_fontSizeMultiplier; }
    void setFontSizeMultiplier(double size) { m_fontSizeMultiplier = size; }

    const String& fontName() const { return m_fontName; }
    void setFontName(const String& name) { m_fontName = name; }

    const Color& foregroundColor() const { return m_foregroundColor; }
    void setForegroundColor(const Color& color) { m_foregroundColor = color; }
    
    const Color& backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const Color& color) { m_backgroundColor = color; }
    
    const Color& highlightColor() const { return m_highlightColor; }
    void setHighlightColor(const Color& color) { m_highlightColor = color; }

    void setFontSize(int, const IntSize&, bool important) final;

    String toJSONString() const;

private:
    WEBCORE_EXPORT TextTrackCueGeneric(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, const String&);
    
    bool isOrderedBefore(const TextTrackCue*) const final;
    bool isPositionedAbove(const TextTrackCue*) const final;

    Ref<VTTCueBox> createDisplayTree() final;

    bool isEqual(const TextTrackCue&, CueMatchRules) const final;
    bool cueContentsMatch(const TextTrackCue&) const final;
    bool doesExtendCue(const TextTrackCue&) const final;

    CueType cueType() const final { return Generic; }

    Color m_foregroundColor;
    Color m_backgroundColor;
    Color m_highlightColor;
    double m_baseFontSizeRelativeToVideoHeight { 0 };
    double m_fontSizeMultiplier { 0 };
    String m_fontName;
    bool m_useDefaultPosition { true };
};

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::TextTrackCueGeneric> {
    static String toString(const WebCore::TextTrackCueGeneric& cue)
    {
        return cue.toJSONString();
    }
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::TextTrackCueGeneric)
static bool isType(const WebCore::TextTrackCue& cue) { return cue.cueType() == WebCore::TextTrackCue::Generic; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
