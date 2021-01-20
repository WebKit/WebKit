/*
 * Copyright (C) 2011, 2012 Apple Inc.  All rights reserved.
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

#if ENABLE(VIDEO)

#include "TrackListBase.h"

namespace WebCore {

class AudioTrack;

class AudioTrackList final : public TrackListBase {
public:
    static Ref<AudioTrackList> create(WeakPtr<HTMLMediaElement> owner, ScriptExecutionContext* context)
    {
        auto list = adoptRef(*new AudioTrackList(owner, context));
        list->suspendIfNeeded();
        return list;
    }
    virtual ~AudioTrackList();

    AudioTrack* getTrackById(const AtomString&) const;

    AudioTrack* item(unsigned index) const;
    AudioTrack* lastItem() const { return item(length() - 1); }
    void append(Ref<AudioTrack>&&);

    // EventTarget
    EventTargetInterface eventTargetInterface() const override;

private:
    AudioTrackList(WeakPtr<HTMLMediaElement>, ScriptExecutionContext*);

    const char* activeDOMObjectName() const final;
};
static_assert(sizeof(AudioTrackList) == sizeof(TrackListBase), "");

} // namespace WebCore

#endif // ENABLE(VIDEO)
