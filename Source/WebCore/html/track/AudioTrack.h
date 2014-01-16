/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2012, 2013 Apple Inc.  All rights reserved.
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

#ifndef AudioTrack_h
#define AudioTrack_h

#if ENABLE(VIDEO_TRACK)

#include "AudioTrackPrivate.h"
#include "ExceptionCode.h"
#include "TrackBase.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class AudioTrack;

class AudioTrackClient {
public:
    virtual ~AudioTrackClient() { }
    virtual void audioTrackEnabledChanged(AudioTrack*) = 0;
};

class AudioTrack : public TrackBase, public AudioTrackPrivateClient {
public:
    static PassRefPtr<AudioTrack> create(AudioTrackClient* client, PassRefPtr<AudioTrackPrivate> trackPrivate)
    {
        return adoptRef(new AudioTrack(client, trackPrivate));
    }
    virtual ~AudioTrack();

    static const AtomicString& alternativeKeyword();
    static const AtomicString& descriptionKeyword();
    static const AtomicString& mainKeyword();
    static const AtomicString& mainDescKeyword();
    static const AtomicString& translationKeyword();
    static const AtomicString& commentaryKeyword();
    virtual const AtomicString& defaultKindKeyword() const override { return emptyAtom; }

    virtual bool enabled() const override { return m_enabled; }
    virtual void setEnabled(const bool);

    virtual void clearClient() override { m_client = 0; }
    AudioTrackClient* client() const { return m_client; }

    size_t inbandTrackIndex();

protected:
    AudioTrack(AudioTrackClient*, PassRefPtr<AudioTrackPrivate>);

private:
    virtual bool isValidKind(const AtomicString&) const override;

    virtual void enabledChanged(AudioTrackPrivate*, bool) override;
    virtual void idChanged(TrackPrivateBase*, const String&) override;
    virtual void labelChanged(TrackPrivateBase*, const String&) override;
    virtual void languageChanged(TrackPrivateBase*, const String&) override;
    virtual void willRemove(TrackPrivateBase*) override;

    bool m_enabled;
    AudioTrackClient* m_client;

    RefPtr<AudioTrackPrivate> m_private;
};

inline AudioTrack* toAudioTrack(TrackBase* track)
{
    ASSERT_WITH_SECURITY_IMPLICATION(track->type() == TrackBase::AudioTrack);
    return static_cast<AudioTrack*>(track);
}

} // namespace WebCore

#endif
#endif
