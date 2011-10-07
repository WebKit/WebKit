/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextTrack_h
#define TextTrack_h

#if ENABLE(VIDEO_TRACK)

#include "ExceptionCode.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class TextTrack;
class TextTrackCueList;

class TextTrackClient {
public:
    virtual ~TextTrackClient() { }
    virtual void textTrackReadyStateChanged(TextTrack*) { }
    virtual void textTrackModeChanged(TextTrack*) { }
    virtual void textTrackCreated(TextTrack*) { }
};

class TextTrack : public RefCounted<TextTrack> {
public:
    static PassRefPtr<TextTrack> create(const String& kind, const String& label, const String& language)
    {
        return adoptRef(new TextTrack(kind, label, language));
    }
    virtual ~TextTrack();

    String kind() const;
    String label() const;
    String language() const;

    enum ReadyState { None, Loading, Loaded, Error };
    ReadyState readyState() const;

    enum Mode { Off = 0, Hidden = 1, Showing = 2 };
    Mode mode() const;
    void setMode(unsigned short, ExceptionCode&);

    PassRefPtr<TextTrackCueList> cues() const;
    PassRefPtr<TextTrackCueList> activeCues() const;

    void readyStateChanged();
    void modeChanged();

protected:
    TextTrack(const String& kind, const String& label, const String& language);

    void setReadyState(ReadyState);

    RefPtr<TextTrackCueList> m_cues;

private:
    String m_kind;
    String m_label;
    String m_language;
    TextTrack::ReadyState m_readyState;
    TextTrack::Mode m_mode;

};

} // namespace WebCore

#endif
#endif
