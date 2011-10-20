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

#ifndef TextTrackCueIndex_h
#define TextTrackCueIndex_h

#if ENABLE(VIDEO_TRACK)

#include "TextTrackLoader.h"
#include <wtf/HashSet.h>

namespace WebCore {

class TextTrackCue;
class TextTrackCueList;

class TextTrackCueSet {
public:
    TextTrackCueSet() { }
    ~TextTrackCueSet() { }
    TextTrackCueSet difference(const TextTrackCueSet&) const;
    TextTrackCueSet unionSet(const TextTrackCueSet&) const;
    void add(const TextTrackCue&);
    bool contains(const TextTrackCue&) const;
    void remove(const TextTrackCue&);
    bool isEmpty() const;
    int size() const;
private:
    HashSet<TextTrackCue*> m_set;
};

class TextTrackCueIndex : public TextTrackLoaderClient {
public:
    // TextTrackLoaderClient methods.
    void fetchNewCuesFromLoader(TextTrackLoader*);
    void removeCuesFromIndex(const TextTrackCueList*);

    // Returns set of cues visible at a time in seconds.
    TextTrackCueSet visibleCuesAtTime(double) const;
    void add(TextTrackCue*);
    void remove(TextTrackCue*);
};

} // namespace WebCore

#endif
#endif
