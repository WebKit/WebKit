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

#ifndef LoadableTextTrack_h
#define LoadableTextTrack_h

#if ENABLE(VIDEO_TRACK)

#include "CueLoader.h"
#include "CueParser.h"
#include "TextTrack.h"
#include "TextTrackCueList.h"
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class ScriptExecutionContext;
class TextTrack;
class TextTrackCue;

class LoadableTextTrack : public TextTrack, public CueParserClient, public CueLoader {
public:
    static PassRefPtr<LoadableTextTrack> create(const String& kind, const String& label, const String& language, bool isDefault)
    {
        return adoptRef(new LoadableTextTrack(kind, label, language, isDefault));
    }
    virtual ~LoadableTextTrack();

    void load(const String&, ScriptExecutionContext*);
    bool supportsType(const String&);

    virtual void newCuesParsed();
    virtual void trackLoadStarted();
    virtual void trackLoadError();
    virtual void trackLoadCompleted();

    virtual void newCuesLoaded();
    virtual void fetchNewestCues(Vector<TextTrackCue*>&);

private:
    LoadableTextTrack(const String& kind, const String& label, const String& language, bool isDefault);

    CueParser m_parser;

    bool m_isDefault;
};
} // namespace WebCore

#endif
#endif
