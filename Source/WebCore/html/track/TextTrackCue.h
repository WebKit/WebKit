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
#include "TextTrack.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DocumentFragment;
class ScriptExecutionContext;
class TextTrack;
class HTMLDivElement;

class TextTrackCue : public RefCounted<TextTrackCue>, public EventTarget {
public:
    static PassRefPtr<TextTrackCue> create(ScriptExecutionContext* context, const String& id, double start, double end, const String& content, const String& settings, bool pauseOnExit)
    {
        return adoptRef(new TextTrackCue(context, id, start, end, content, settings, pauseOnExit));
    }

    virtual ~TextTrackCue();

    TextTrack* track() const;
    void setTrack(PassRefPtr<TextTrack>);

    const String& id() const { return m_id; }
    void setId(const String&);

    double startTime() const { return m_startTime; }
    void setStartTime(double);

    double endTime() const { return m_endTime; }
    void setEndTime(double);

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

    int cueIndex();
    void invalidateCueIndex();

    PassRefPtr<DocumentFragment> getCueAsHTML();

    virtual bool dispatchEvent(PassRefPtr<Event>);
    bool dispatchEvent(PassRefPtr<Event>, ExceptionCode&);

    bool isActive();
    void setIsActive(bool);

    PassRefPtr<HTMLDivElement> getDisplayTree();

    virtual const AtomicString& interfaceName() const;
    virtual ScriptExecutionContext* scriptExecutionContext() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(enter);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(exit);

    using RefCounted<TextTrackCue>::ref;
    using RefCounted<TextTrackCue>::deref;

protected:
    virtual EventTargetData* eventTargetData();
    virtual EventTargetData* ensureEventTargetData();

private:
    TextTrackCue(ScriptExecutionContext*, const String& id, double start, double end, const String& content, const String& settings, bool pauseOnExit);

    void parseSettings(const String&);

    int calculateComputedLinePosition();
    void calculateDisplayParameters();

    std::pair<double, double> getPositionCoordinates();

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
    int m_linePosition;
    int m_computedLinePosition;
    int m_textPosition;
    int m_cueSize;
    int m_cueIndex;

    enum WritingDirection {
        Horizontal,
        VerticalGrowingLeft,
        VerticalGrowingRight,
        NumberOfWritingDirections
    };
    WritingDirection m_writingDirection;

    enum Alignment { Start, Middle, End };
    Alignment m_cueAlignment;

    RefPtr<DocumentFragment> m_documentFragment;
    RefPtr<TextTrack> m_track;

    EventTargetData m_eventTargetData;
    ScriptExecutionContext* m_scriptExecutionContext;

    bool m_isActive;
    bool m_pauseOnExit;
    bool m_snapToLines;

    bool m_displayTreeShouldChange;
    RefPtr<HTMLDivElement> m_displayTree;

    int m_displayDirection;

    int m_displayWritingModeMap[NumberOfWritingDirections];
    int m_displayWritingMode;

    int m_displaySize;
    int m_displayHeight;
    int m_displayWidth;

    double m_displayXPosition;
    double m_displayYPosition;
};

} // namespace WebCore

#endif
#endif
