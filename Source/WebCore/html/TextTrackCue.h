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

#include "TextTrack.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DocumentFragment;
class TextTrack;

class TextTrackCue : public RefCounted<TextTrackCue> {
public:
    static PassRefPtr<TextTrackCue> create(const String& id, double start, double end, const String& content, const String& settings, bool pauseOnExit)
    {
        return adoptRef(new TextTrackCue(id, start, end, content, settings, pauseOnExit));
    }
    
    enum Direction { Horizontal, VerticalGrowingLeft, VerticalGrowingRight };
    enum Alignment { Start, Middle, End };

    virtual ~TextTrackCue();

    TextTrack* track() const;
    void setTrack(TextTrack*);

    String id() const;
    double startTime() const;
    double endTime() const;
    bool pauseOnExit() const;
    
    String direction() const;
    bool snapToLines() const { return m_snapToLines; }
    int linePosition() const { return m_linePosition; }
    int textPosition() const { return m_textPosition; }
    int size() const { return m_cueSize; }
    String alignment() const;

    String getCueAsSource();
    PassRefPtr<DocumentFragment> getCueAsHTML();

    bool isActive();
    void setIsActive(bool);

private:
    TextTrackCue(const String& id, double start, double end, const String& content, const String& settings, bool pauseOnExit);

    void parseSettings(const String&);
    
    TextTrack* m_track;
    
    String m_id;
    double m_startTime;
    double m_endTime;
    String m_content;
    bool m_pauseOnExit;
    Direction m_writingDirection;
    bool m_snapToLines;
    int m_linePosition;
    int m_textPosition;
    int m_cueSize;
    Alignment m_cueAlignment;

    bool m_isActive;
};

} // namespace WebCore

#endif
#endif
