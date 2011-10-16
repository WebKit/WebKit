/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#ifndef CueLoader_h
#define CueLoader_h

#if ENABLE(VIDEO_TRACK)

#include <wtf/Vector.h>

namespace WebCore {

class CueLoader;
class TextTrackCue;
class TextTrackCueList;

// Listener to CueLoader.
class CueLoaderClient {
public:
    virtual ~CueLoaderClient() { }

    // Queries CueLoader for newest cues.
    virtual void fetchNewCuesFromLoader(CueLoader*) = 0;
    virtual void removeCuesFromIndex(const TextTrackCueList*) = 0;
};

class CueLoader {
public:
    virtual ~CueLoader() { }

    void setCueLoaderClient(CueLoaderClient*);

    // Informs client that new cues have been loaded.
    virtual void newCuesLoaded() = 0;

    // Transfers ownership of currently loaded cues.
    virtual void fetchNewestCues(Vector<TextTrackCue*>& cues) = 0;

protected:
    CueLoaderClient* m_client;
};

}

#endif
#endif
