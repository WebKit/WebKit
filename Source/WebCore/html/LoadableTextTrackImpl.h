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

#ifndef LoadableTextTrackImpl_h
#define LoadableTextTrackImpl_h

#if ENABLE(VIDEO_TRACK)

#include "CueLoader.h"
#include "CueParser.h"
#include "TextTrackPrivate.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class TextTrack;
class TextTrackCue;
class TextTrackCueList;

class LoadableTextTrackImpl : public TextTrackPrivateInterface, public CueLoader {
public:
    static PassOwnPtr<LoadableTextTrackImpl> create(const String& kind, const String& label, const String& language, bool isDefault)
    {
        return adoptPtr(new LoadableTextTrackImpl(kind, label, language, isDefault));
    }
    virtual ~LoadableTextTrackImpl();

    // TextTrackPrivateInterface methods
    virtual PassRefPtr<TextTrackCueList> cues() const;
    virtual PassRefPtr<TextTrackCueList> activeCues() const;
    virtual void load(const String&);

    // CueLoader interface
    virtual void newCuesLoaded();
    virtual void fetchNewestCues(Vector<TextTrackCue*>&);

private:
    LoadableTextTrackImpl(const String& kind, const String& label, const String& language, bool isDefault);

    bool m_isDefault;

    CueParser m_parser;
    RefPtr<TextTrackCueList> m_cues;
};

} // namespace WebCore

#endif
#endif
