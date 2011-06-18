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

#ifndef TextTrackPrivate_h
#define TextTrackPrivate_h

#if ENABLE(VIDEO_TRACK)

#include "TextTrack.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class CueLoaderClient;
class TextTrackCueList;

class TextTrackPrivateInterface {
    WTF_MAKE_NONCOPYABLE(TextTrackPrivateInterface); WTF_MAKE_FAST_ALLOCATED;
public:
    TextTrackPrivateInterface(const String& kind, const String& label, const String& language)
        : m_kind(kind)
        , m_label(label)
        , m_language(language)
        , m_readyState(TextTrack::NONE)
        , m_mode(TextTrack::SHOWING)
    {
    }
    virtual ~TextTrackPrivateInterface() { }

    virtual String kind() const { return m_kind; }
    virtual String label() const { return m_label; }
    virtual String language() const { return m_language; }

    virtual TextTrack::ReadyState readyState() const { return m_readyState; }

    virtual TextTrack::Mode mode() const { return m_mode; }
    virtual void setMode(TextTrack::Mode mode) { m_mode = mode; }

    virtual PassRefPtr<TextTrackCueList> cues() const = 0;
    virtual PassRefPtr<TextTrackCueList> activeCues() const = 0;

    // This method should only be overridden by tracks that load cue data from a URL.
    virtual void load(const String&) { }

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
