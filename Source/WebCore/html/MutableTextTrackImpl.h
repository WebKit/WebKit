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

#ifndef MutableTextTrackImpl_h
#define MutableTextTrackImpl_h

#if ENABLE(VIDEO_TRACK)

#include "CueLoader.h"
#include "TextTrackPrivate.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CueLoaderClient;
class TextTrack;
class TextTrackCue;
class TextTrackCueList;

class MutableTextTrackImpl : public TextTrackPrivateInterface, public CueLoader {
public:
    static PassOwnPtr<MutableTextTrackImpl> create(const String& kind, const String& label, const String& language)
    {
        return adoptPtr(new MutableTextTrackImpl(kind, label, language));
    }
    virtual ~MutableTextTrackImpl();

    // TextTrackPrivateInterface methods
    virtual PassRefPtr<TextTrackCueList> cues() const;
    virtual PassRefPtr<TextTrackCueList> activeCues() const;
    virtual void addCue(TextTrackCue*);
    virtual void removeCue(TextTrackCue*);

    // CueLoader interface
    virtual void newCuesLoaded();
    virtual void fetchNewestCues(Vector<TextTrackCue*>&);

private:
    MutableTextTrackImpl(const String& kind, const String& label, const String& language);

    RefPtr<TextTrackCueList> m_cues;
    Vector<TextTrackCue*> m_newestCues;
};

} // namespace WebCore

#endif
#endif
