/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MultipleTrackList_h
#define MultipleTrackList_h

#if ENABLE(MEDIA_STREAM) || ENABLE(VIDEO_TRACK)

#include "TrackList.h"
#include <wtf/Vector.h>

namespace WebCore {

typedef Vector<bool> EnabledTracks;

class MultipleTrackList : public TrackList {
public:
    static PassRefPtr<MultipleTrackList> create(const TrackVector&, const EnabledTracks&);
    virtual ~MultipleTrackList();

    bool isEnabled(unsigned long index, ExceptionCode&) const;
    void enable(unsigned long index, ExceptionCode&);
    void disable(unsigned long index, ExceptionCode&);

    virtual void clear();

    // EventTarget implementation.
    virtual MultipleTrackList* toMultipleTrackList();

private:
    MultipleTrackList(const TrackVector&, const EnabledTracks&);

    Vector<bool> m_isEnabled;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) || ENABLE(VIDEO_TRACK)

#endif // MultipleTrackList_h
