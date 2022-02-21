/*
 * Copyright (C) 2011, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO)

#include "HTMLElement.h"
#include "TextTrackCue.h"
#include "VTTRegion.h"
#include <wtf/TypeCasts.h>

namespace WebCore {

class DocumentFragment;
class HTMLDivElement;
class HTMLSpanElement;
class ScriptExecutionContext;
class VTTCue;
class VTTScanner;
class WebVTTCueData;

// ----------------------------

class VTTCueBox : public TextTrackCueBox {
    WTF_MAKE_ISO_ALLOCATED(VTTCueBox);
public:
    static Ref<VTTCueBox> create(Document&, VTTCue&);

    void applyCSSProperties(const IntSize&) override;

    void setFontSizeFromCaptionUserPrefs(int fontSize) { m_fontSizeFromCaptionUserPrefs = fontSize; }

protected:
    VTTCueBox(Document&, VTTCue&);

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    int fontSizeFromCaptionUserPrefs() const { return m_fontSizeFromCaptionUserPrefs; }

private:
    WeakPtr<VTTCue> m_cue;
    int m_fontSizeFromCaptionUserPrefs;
};

// ----------------------------

class VTTCue : public TextTrackCue {
    WTF_MAKE_ISO_ALLOCATED(VTTCue);
public:
    static Ref<VTTCue> create(Document&, double start, double end, String&& content);
    static Ref<VTTCue> create(Document&, const WebVTTCueData&);

    virtual ~VTTCue();

    enum AutoKeyword { Auto };
    using LineAndPositionSetting = std::variant<double, AutoKeyword>;

    void setTrack(TextTrack*);

    const String& vertical() const;
    ExceptionOr<void> setVertical(const String&);

    bool snapToLines() const { return m_snapToLines; }
    void setSnapToLines(bool);

    LineAndPositionSetting line() const;
    virtual ExceptionOr<void> setLine(const LineAndPositionSetting&);

    const String& lineAlign() const;
    ExceptionOr<void> setLineAlign(const String&);

    LineAndPositionSetting position() const;
    virtual ExceptionOr<void> setPosition(const LineAndPositionSetting&);

    const String& positionAlign() const;
    ExceptionOr<void> setPositionAlign(const String&);

    int size() const { return m_cueSize; }
    ExceptionOr<void> setSize(int);

    const String& align() const;
    ExceptionOr<void> setAlign(const String&);

    const String& text() const { return m_content; }
    void setText(const String&);

    const String& cueSettings() const { return m_settings; }
    void setCueSettings(const String&);

    RefPtr<DocumentFragment> getCueAsHTML() final;
    RefPtr<DocumentFragment> createCueRenderingTree();

    void notifyRegionWhenRemovingDisplayTree(bool);

    VTTRegion* region();
    void setRegion(VTTRegion*);

    const String& regionId();

    void setIsActive(bool) override;

    bool hasDisplayTree() const { return m_displayTree; }
    RefPtr<TextTrackCueBox> getDisplayTree(const IntSize& videoSize, int fontSize) final;
    HTMLSpanElement& element() const { return *m_cueHighlightBox; }

    void updateDisplayTree(const MediaTime&) final;
    void removeDisplayTree() final;
    void markFutureAndPastNodes(ContainerNode*, const MediaTime&, const MediaTime&);

    int calculateComputedLinePosition() const;
    std::pair<double, double> getPositionCoordinates() const;

    using DisplayPosition = std::pair<std::optional<double>, std::optional<double>>;
    const DisplayPosition& getCSSPosition() const { return m_displayPosition; };

    CSSValueID getCSSAlignment() const;
    int getCSSSize() const;
    CSSValueID getCSSWritingDirection() const;
    CSSValueID getCSSWritingMode() const;

    enum WritingDirection {
        Horizontal,
        VerticalGrowingLeft,
        VerticalGrowingRight,
        NumberOfWritingDirections
    };
    WritingDirection getWritingDirection() const { return m_writingDirection; }

    enum CueAlignment {
        Start,
        Center,
        End,
        Left,
        Right,
        NumberOfAlignments
    };
    CueAlignment getAlignment() const { return m_cueAlignment; }

    enum CueLignAlignment {
        LignAlignmentStart,
        LignAlignmentCenter,
        LignAlignmentEnd,
        NumberOfCueLineAlignments
    };

    enum CuePositionAlignment {
        PositionAlignmentLignLeft,
        PositionAlignmentLignCenter,
        PositionAlignmentLignRight,
        PositionAlignmentLignAuto,
        NumberOfCuePositionAlignments
    };

    void recalculateStyles() final { m_displayTreeShouldChange = true; }
    void setFontSize(int, const IntSize&, bool important) override;

    CueType cueType() const override { return WebVTT; }
    bool isRenderable() const final { return !m_content.isEmpty(); }

    void didChange() final;

    double calculateComputedTextPosition() const;

protected:
    VTTCue(Document&, const MediaTime& start, const MediaTime& end, String&& content);

    bool cueContentsMatch(const TextTrackCue&) const override;

    virtual Ref<VTTCueBox> createDisplayTree();
    VTTCueBox& displayTreeInternal();

    void toJSON(JSON::Object&) const override;

private:
    VTTCue(Document&, const WebVTTCueData&);

    void initialize();
    void createWebVTTNodeTree();

    void parseSettings(const String&);

    void determineTextDirection();
    void calculateDisplayParameters();

    enum CueSetting {
        None,
        Vertical,
        Line,
        Position,
        Size,
        Align,
        Region
    };
    CueSetting settingName(VTTScanner&);

    String m_content;
    String m_settings;
    std::optional<double> m_linePosition;
    std::optional<double> m_computedLinePosition;
    std::optional<double> m_textPosition;
    int m_cueSize { 100 };

    WritingDirection m_writingDirection { Horizontal };
    CueAlignment m_cueAlignment { Center };

    RefPtr<VTTRegion> m_region;
    String m_parsedRegionId;

    RefPtr<DocumentFragment> m_webVTTNodeTree;
    RefPtr<HTMLSpanElement> m_cueHighlightBox;
    RefPtr<HTMLDivElement> m_cueBackdropBox;
    RefPtr<VTTCueBox> m_displayTree;

    CSSValueID m_displayDirection { CSSValueLtr };
    int m_displaySize { 0 };
    DisplayPosition m_displayPosition;

    MediaTime m_originalStartTime;

    int m_fontSize { 0 };
    bool m_fontSizeIsImportant { false };

    bool m_snapToLines : 1;
    bool m_displayTreeShouldChange : 1;
    bool m_notifyRegion : 1;

    CuePositionAlignment m_positionAlignment { PositionAlignmentLignAuto };
    CueLignAlignment m_lineAlignment { LignAlignmentStart };
};

} // namespace WebCore

namespace WTF {

template<> struct LogArgument<WebCore::VTTCue> : LogArgument<WebCore::TextTrackCue> { };

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::VTTCue)
static bool isType(const WebCore::TextTrackCue& cue) { return cue.cueType() == WebCore::TextTrackCue::WebVTT || cue.cueType() == WebCore::TextTrackCue::ConvertedToWebVTT; }
SPECIALIZE_TYPE_TRAITS_END()

#endif
