/*
 * Copyright (C) 2011, 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2012-2014 Apple Inc.  All rights reserved.
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

#ifndef VTTCue_h
#define VTTCue_h

#if ENABLE(VIDEO_TRACK)

#include "EventTarget.h"
#include "HTMLElement.h"
#include "TextTrackCue.h"
#include <wtf/RefCounted.h>

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
public:
    static PassRefPtr<VTTCueBox> create(Document&, VTTCue&);

    VTTCue* getCue() const;
    virtual void applyCSSProperties(const IntSize& videoSize);

    static const AtomicString& vttCueBoxShadowPseudoId();
    virtual void setFontSizeFromCaptionUserPrefs(int fontSize) { m_fontSizeFromCaptionUserPrefs = fontSize; }

protected:
    VTTCueBox(Document&, VTTCue&);

    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;

    VTTCue& m_cue;
    int m_fontSizeFromCaptionUserPrefs;
};

// ----------------------------

class VTTCue : public TextTrackCue {
public:
    static PassRefPtr<VTTCue> create(ScriptExecutionContext& context, double start, double end, const String& content)
    {
        return create(context, MediaTime::createWithDouble(start), MediaTime::createWithDouble(end), content);
    }

    static PassRefPtr<VTTCue> create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, const String& content)
    {
        return adoptRef(new VTTCue(context, start, end, content));
    }

    static PassRefPtr<VTTCue> create(ScriptExecutionContext&, const WebVTTCueData&);

    static const AtomicString& cueBackdropShadowPseudoId();

    virtual ~VTTCue();

    const String& vertical() const;
    void setVertical(const String&, ExceptionCode&);

    bool snapToLines() const { return m_snapToLines; }
    void setSnapToLines(bool);

    double line() const { return m_linePosition; }
    virtual void setLine(double, ExceptionCode&);

    double position() const { return m_textPosition; }
    virtual void setPosition(double, ExceptionCode&);

    int size() const { return m_cueSize; }
    virtual void setSize(int, ExceptionCode&);

    const String& align() const;
    void setAlign(const String&, ExceptionCode&);

    const String& text() const { return m_content; }
    void setText(const String&);

    const String& cueSettings() const { return m_settings; }
    void setCueSettings(const String&);

    PassRefPtr<DocumentFragment> getCueAsHTML();
    PassRefPtr<DocumentFragment> createCueRenderingTree();

#if ENABLE(WEBVTT_REGIONS)
    const String& regionId() const { return m_regionId; }
    void setRegionId(const String&);
    void notifyRegionWhenRemovingDisplayTree(bool);
#endif

    virtual void setIsActive(bool);

    bool hasDisplayTree() const { return m_displayTree; }
    VTTCueBox* getDisplayTree(const IntSize& videoSize, int fontSize);
    HTMLSpanElement* element() const { return m_cueHighlightBox.get(); }

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
        Middle,
        End,
        Left,
        Right,
        NumberOfAlignments
    };
    CueAlignment getAlignment() const { return m_cueAlignment; }

    virtual void setFontSize(int, const IntSize&, bool important);

    virtual bool isEqual(const TextTrackCue&, CueMatchRules) const override;
    virtual bool cueContentsMatch(const TextTrackCue&) const override;
    virtual bool doesExtendCue(const TextTrackCue&) const override;

    virtual CueType cueType() const { return WebVTT; }
    virtual bool isRenderable() const override final { return true; }

    virtual void didChange() override;

protected:
    VTTCue(ScriptExecutionContext&, const MediaTime& start, const MediaTime& end, const String& content);
    VTTCue(ScriptExecutionContext&, const WebVTTCueData&);

    virtual PassRefPtr<VTTCueBox> createDisplayTree();
    VTTCueBox* displayTreeInternal();

private:
    void initialize(ScriptExecutionContext&);
    void createWebVTTNodeTree();
    void copyWebVTTNodeToDOMTree(ContainerNode* WebVTTNode, ContainerNode* root);

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
#if ENABLE(WEBVTT_REGIONS)
        RegionId
#endif
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
#if ENABLE(WEBVTT_REGIONS)
    String m_regionId;
#endif

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

#endif
#endif
