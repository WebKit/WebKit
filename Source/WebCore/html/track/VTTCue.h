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
class HTMLSpanElement;
class ScriptExecutionContext;
class VTTCue;
class VTTScanner;

// ----------------------------

class VTTCueBox : public HTMLElement {
public:
    static PassRefPtr<VTTCueBox> create(Document& document, VTTCue* cue)
    {
        return adoptRef(new VTTCueBox(document, cue));
    }

    VTTCue* getCue() const;
    virtual void applyCSSProperties(const IntSize& videoSize);

    static const AtomicString& vttCueBoxShadowPseudoId();

protected:
    VTTCueBox(Document&, VTTCue*);

    virtual RenderPtr<RenderElement> createElementRenderer(PassRef<RenderStyle>) override;

    VTTCue* m_cue;
};

// ----------------------------

class VTTCue : public TextTrackCue {
public:
    static PassRefPtr<VTTCue> create(ScriptExecutionContext& context, double start, double end, const String& content)
    {
        return adoptRef(new VTTCue(context, start, end, content));
    }

    virtual ~VTTCue();

    const String& vertical() const;
    void setVertical(const String&, ExceptionCode&);

    bool snapToLines() const { return m_snapToLines; }
    void setSnapToLines(bool);

    int line() const { return m_linePosition; }
    virtual void setLine(int, ExceptionCode&);

    int position() const { return m_textPosition; }
    virtual void setPosition(int, ExceptionCode&);

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
    VTTCueBox* getDisplayTree(const IntSize& videoSize);
    HTMLSpanElement* element() const { return m_cueBackgroundBox.get(); }

    void updateDisplayTree(double);
    void removeDisplayTree();
    void markFutureAndPastNodes(ContainerNode*, double, double);

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

    enum CueMatchRules {
        MatchAllFields,
        IgnoreDuration,
    };
    virtual bool isEqual(const VTTCue&, CueMatchRules) const;

    virtual CueType cueType() const { return WebVTT; }
    virtual bool isRenderable() const override final { return true; }

    virtual void didChange() override;

protected:
    VTTCue(ScriptExecutionContext&, double start, double end, const String& content);

    virtual PassRefPtr<VTTCueBox> createDisplayTree();
    VTTCueBox* displayTreeInternal();

private:
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
    int m_linePosition;
    int m_computedLinePosition;
    int m_textPosition;
    int m_cueSize;

    WritingDirection m_writingDirection;
    CueAlignment m_cueAlignment;

    RefPtr<DocumentFragment> m_webVTTNodeTree;

    bool m_snapToLines;

    RefPtr<HTMLSpanElement> m_cueBackgroundBox;

    bool m_displayTreeShouldChange;
    RefPtr<VTTCueBox> m_displayTree;

    CSSValueID m_displayDirection;

    int m_displaySize;

    std::pair<float, float> m_displayPosition;
#if ENABLE(WEBVTT_REGIONS)
    String m_regionId;
#endif
    bool m_notifyRegion;
};

VTTCue* toVTTCue(TextTrackCue*);
const VTTCue* toVTTCue(const TextTrackCue*);

} // namespace WebCore

#endif
#endif
