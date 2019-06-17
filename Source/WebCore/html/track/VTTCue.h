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

#if ENABLE(VIDEO_TRACK)

#include "HTMLElement.h"
#include "TextTrackCue.h"
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

class VTTCueBox : public HTMLElement {
    WTF_MAKE_ISO_ALLOCATED(VTTCueBox);
public:
    static Ref<VTTCueBox> create(Document&, VTTCue&);

    VTTCue* getCue() const;
    virtual void applyCSSProperties(const IntSize& videoSize);

    static const AtomString& vttCueBoxShadowPseudoId();
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

class VTTCue : public TextTrackCue, public CanMakeWeakPtr<VTTCue> {
    WTF_MAKE_ISO_ALLOCATED(VTTCue);
public:
    static Ref<VTTCue> create(ScriptExecutionContext& context, double start, double end, const String& content)
    {
        return create(context, MediaTime::createWithDouble(start), MediaTime::createWithDouble(end), content);
    }

    static Ref<VTTCue> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, const String& content)
    {
        return adoptRef(*new VTTCue(context, start, end, content));
    }

    static Ref<VTTCue> create(ScriptExecutionContext&, const WebVTTCueData&);

    static const AtomString& cueBackdropShadowPseudoId();

    virtual ~VTTCue();

    enum AutoKeyword {
        Auto
    };
    
    using LineAndPositionSetting = Variant<double, AutoKeyword>;

    const String& vertical() const;
    ExceptionOr<void> setVertical(const String&);

    bool snapToLines() const { return m_snapToLines; }
    void setSnapToLines(bool);

    double line() const { return m_linePosition; }
    virtual ExceptionOr<void> setLine(double);

    LineAndPositionSetting position() const;
    virtual ExceptionOr<void> setPosition(const LineAndPositionSetting&);

    int size() const { return m_cueSize; }
    ExceptionOr<void> setSize(int);

    const String& align() const;
    ExceptionOr<void> setAlign(const String&);

    const String& text() const { return m_content; }
    void setText(const String&);

    const String& cueSettings() const { return m_settings; }
    void setCueSettings(const String&);

    RefPtr<DocumentFragment> getCueAsHTML();
    RefPtr<DocumentFragment> createCueRenderingTree();

    const String& regionId() const { return m_regionId; }
    void setRegionId(const String&);
    void notifyRegionWhenRemovingDisplayTree(bool);

    void setIsActive(bool) override;

    bool hasDisplayTree() const { return m_displayTree; }
    VTTCueBox& getDisplayTree(const IntSize& videoSize, int fontSize);
    HTMLSpanElement& element() const { return *m_cueHighlightBox; }

    void updateDisplayTree(const MediaTime&);
    void removeDisplayTree();
    void markFutureAndPastNodes(ContainerNode*, const MediaTime&, const MediaTime&);

    int calculateComputedLinePosition();
    std::pair<double, double> getPositionCoordinates() const;

    std::pair<double, double> getCSSPosition() const;

    CSSValueID getCSSAlignment() const;
    int getCSSSize() const;
    CSSValueID getCSSWritingDirection() const;
    CSSValueID getCSSWritingMode() const;

    enum WritingDirection {
        Horizontal = 0,
        VerticalGrowingLeft,
        VerticalGrowingRight,
        NumberOfWritingDirections
    };
    WritingDirection getWritingDirection() const { return m_writingDirection; }

    enum CueAlignment {
        Start = 0,
        Center,
        End,
        Left,
        Right,
        NumberOfAlignments
    };
    CueAlignment getAlignment() const { return m_cueAlignment; }

    virtual void setFontSize(int, const IntSize&, bool important);

    bool isEqual(const TextTrackCue&, CueMatchRules) const override;
    bool cueContentsMatch(const TextTrackCue&) const override;
    bool doesExtendCue(const TextTrackCue&) const override;

    CueType cueType() const override { return WebVTT; }
    bool isRenderable() const final { return true; }

    void didChange() override;

    String toJSONString() const;

    double calculateComputedTextPosition() const;

protected:
    VTTCue(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, const String& content);
    VTTCue(ScriptExecutionContext&, const WebVTTCueData&);

    virtual Ref<VTTCueBox> createDisplayTree();
    VTTCueBox& displayTreeInternal();

    void toJSON(JSON::Object&) const final;

private:
    void initialize(ScriptExecutionContext&);
    void createWebVTTNodeTree();
    void copyWebVTTNodeToDOMTree(ContainerNode* WebVTTNode, ContainerNode* root);

    void parseSettings(const String&);

    bool textPositionIsAuto() const;
    
    void determineTextDirection();
    void calculateDisplayParameters();

    enum CueSetting {
        None,
        Vertical,
        Line,
        Position,
        Size,
        Align,
        RegionId
    };
    CueSetting settingName(VTTScanner&);

    String m_content;
    String m_settings;
    double m_linePosition;
    double m_computedLinePosition;
    double m_textPosition;
    int m_cueSize;

    WritingDirection m_writingDirection;
    CueAlignment m_cueAlignment;
    String m_regionId;

    RefPtr<DocumentFragment> m_webVTTNodeTree;
    RefPtr<HTMLSpanElement> m_cueHighlightBox;
    RefPtr<HTMLDivElement> m_cueBackdropBox;
    RefPtr<VTTCueBox> m_displayTree;

    CSSValueID m_displayDirection;
    int m_displaySize;
    std::pair<float, float> m_displayPosition;

    MediaTime m_originalStartTime;

    bool m_snapToLines : 1;
    bool m_displayTreeShouldChange : 1;
    bool m_notifyRegion : 1;
};

VTTCue* toVTTCue(TextTrackCue*);
const VTTCue* toVTTCue(const TextTrackCue*);

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::VTTCue> {
    static String toString(const WebCore::VTTCue& cue)
    {
        return cue.toJSONString();
    }
};

} // namespace WTF

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::VTTCue)
    static bool isType(const WebCore::TextTrackCue& cue) { return cue.isRenderable(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
