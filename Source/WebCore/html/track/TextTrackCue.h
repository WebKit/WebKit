/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#ifndef TextTrackCue_h
#define TextTrackCue_h

#if ENABLE(VIDEO_TRACK)

#include "EventTarget.h"
#include "HTMLElement.h"
#include "TextTrack.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DocumentFragment;
class HTMLDivElement;
class ScriptExecutionContext;
class TextTrack;
class TextTrackCue;

// ----------------------------

class TextTrackCueBox : public HTMLElement {
public:
    static PassRefPtr<TextTrackCueBox> create(Document* document, TextTrackCue* cue)
    {
        return adoptRef(new TextTrackCueBox(document, cue));
    }

    TextTrackCue* getCue() const;
    void applyCSSProperties();

    static const AtomicString& textTrackCueBoxShadowPseudoId();

private:
    TextTrackCueBox(Document*, TextTrackCue*);

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*) OVERRIDE;

    TextTrackCue* m_cue;
};

// ----------------------------

class TextTrackCue : public RefCounted<TextTrackCue>, public EventTarget {
public:
    static PassRefPtr<TextTrackCue> create(ScriptExecutionContext* context, double start, double end, const String& content)
    {
        return adoptRef(new TextTrackCue(context, start, end, content));
    }

    virtual ~TextTrackCue();

    static const AtomicString& pastNodesShadowPseudoId();
    static const AtomicString& futureNodesShadowPseudoId();

    TextTrack* track() const;
    void setTrack(TextTrack*);

    const String& id() const { return m_id; }
    void setId(const String&);

    double startTime() const { return m_startTime; }
    void setStartTime(double, ExceptionCode&);

    double endTime() const { return m_endTime; }
    void setEndTime(double, ExceptionCode&);

    bool pauseOnExit() const { return m_pauseOnExit; }
    void setPauseOnExit(bool);

    const String& vertical() const;
    void setVertical(const String&, ExceptionCode&);

    bool snapToLines() const { return m_snapToLines; }
    void setSnapToLines(bool);

    int line() const { return m_linePosition; }
    void setLine(int, ExceptionCode&);

    int position() const { return m_textPosition; }
    void setPosition(int, ExceptionCode&);

    int size() const { return m_cueSize; }
    void setSize(int, ExceptionCode&);

    const String& align() const;
    void setAlign(const String&, ExceptionCode&);

    const String& text() const { return m_content; }
    void setText(const String&);

    const String& cueSettings() const { return m_settings; }
    void setCueSettings(const String&);

    int cueIndex();
    void invalidateCueIndex();

    PassRefPtr<DocumentFragment> getCueAsHTML();
    void markNodesAsWebVTTNodes(Node*);

    virtual bool dispatchEvent(PassRefPtr<Event>);
    bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);

    bool isActive();
    void setIsActive(bool);

    PassRefPtr<TextTrackCueBox> getDisplayTree();
    void updateDisplayTree(float);
    void removeDisplayTree();

    int calculateComputedLinePosition();

    virtual const AtomicString& interfaceName() const;
    virtual ScriptExecutionContext* scriptExecutionContext() const;

    std::pair<double, double> getCSSPosition() const;
    int getCSSSize() const;
    int getCSSWritingMode() const;

    enum WritingDirection {
        Horizontal,
        VerticalGrowingLeft,
        VerticalGrowingRight,
        NumberOfWritingDirections
    };
    WritingDirection getWritingDirection() const { return m_writingDirection; }

    DEFINE_ATTRIBUTE_EVENT_LISTENER(enter);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(exit);

    using RefCounted<TextTrackCue>::ref;
    using RefCounted<TextTrackCue>::deref;

protected:
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

private:
    TextTrackCue(ScriptExecutionContext*, double start, double end, const String& content);

    std::pair<double, double> getPositionCoordinates() const;
    void parseSettings(const String&);

    void calculateDisplayParameters();

    void cueWillChange();
    void cueDidChange();

    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    enum CueSetting { None, Vertical, Line, Position, Size, Align };
    CueSetting settingName(const String&);

    String m_id;
    double m_startTime;
    double m_endTime;
    String m_content;
    String m_settings;
    int m_linePosition;
    int m_computedLinePosition;
    int m_textPosition;
    int m_cueSize;
    int m_cueIndex;

    WritingDirection m_writingDirection;

    enum Alignment { Start, Middle, End };
    Alignment m_cueAlignment;

    RefPtr<DocumentFragment> m_documentFragment;
    TextTrack* m_track;

    EventTargetData m_eventTargetData;
    ScriptExecutionContext* m_scriptExecutionContext;

    bool m_isActive;
    bool m_pauseOnExit;
    bool m_snapToLines;

    bool m_hasInnerTimestamps;
    RefPtr<HTMLDivElement> m_pastDocumentNodes;
    RefPtr<HTMLDivElement> m_futureDocumentNodes;

    bool m_displayTreeShouldChange;
    RefPtr<TextTrackCueBox> m_displayTree;

    int m_displayDirection;

    int m_displayWritingModeMap[NumberOfWritingDirections];
    int m_displayWritingMode;

    int m_displaySize;

    std::pair<float, float> m_displayPosition;
};

} // namespace WebCore

#endif
#endif
