/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO_TRACK)

#include "TextTrackCue.h"

namespace WebCore {

class TextTrackCueList : public RefCounted<TextTrackCueList> {
public:
    static Ref<TextTrackCueList> create();

    unsigned length() const;
    TextTrackCue* item(unsigned index) const;
    TextTrackCue* getCueById(const String&) const;

    unsigned cueIndex(TextTrackCue&) const;

    void add(Ref<TextTrackCue>&&);
    void remove(TextTrackCue&);
    void updateCueIndex(TextTrackCue&);

    void clear();

    TextTrackCueList& activeCues();

private:
    TextTrackCueList() = default;

    Vector<RefPtr<TextTrackCue>> m_vector;
    RefPtr<TextTrackCueList> m_activeCues;
};

inline Ref<TextTrackCueList> TextTrackCueList::create()
{
    return adoptRef(*new TextTrackCueList);
}

inline unsigned TextTrackCueList::length() const
{
    return m_vector.size();
}

} // namespace WebCore

#endif // ENABLE(VIDEO_TRACK)
