/*
 * Copyright (C) 2015 Apple Inc. All Rights Reserved.
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

#if ENABLE(MEDIA_SESSION)

#include "MediaRemoteControls.h"
#include "MediaSessionMetadata.h"
#include <wtf/Function.h>
#include <wtf/HashSet.h>

namespace WebCore {

class Document;
class HTMLMediaElement;

class MediaSession final : public RefCounted<MediaSession> {
public:
    enum class Kind { Content, Transient, TransientSolo, Ambient };
    enum class State { Idle, Active, Interrupted };

    struct Metadata {
        String title;
        String artist;
        String album;
        String artwork;
    };

    static Ref<MediaSession> create(Document& document, Kind kind)
    {
        return adoptRef(*new MediaSession(document, kind));
    }

    ~MediaSession();

    Kind kind() const { return m_kind; }
    MediaRemoteControls* controls();

    WEBCORE_EXPORT State currentState() const { return m_currentState; }
    bool hasActiveMediaElements() const;

    void setMetadata(const Optional<Metadata>&);

    void deactivate();

    // Runs the media session invocation algorithm and returns true on success.
    bool invoke();

    void handleDuckInterruption();
    void handleIndefinitePauseInterruption();
    void handlePauseInterruption();
    void handleUnduckInterruption();
    void handleUnpauseInterruption();

    void togglePlayback();
    void skipToNextTrack();
    void skipToPreviousTrack();

    void controlIsEnabledDidChange();

private:
    friend class HTMLMediaElement;

    MediaSession(Document&, Kind);

    void addMediaElement(HTMLMediaElement&);
    void removeMediaElement(HTMLMediaElement&);

    void safelyIterateActiveMediaElements(const WTF::Function<void(HTMLMediaElement*)>&);
    void changeActiveMediaElements(const WTF::Function<void(void)>&);
    void addActiveMediaElement(HTMLMediaElement&);
    bool isMediaElementActive(HTMLMediaElement&);

    void releaseInternal();

    State m_currentState { State::Idle };
    HashSet<HTMLMediaElement*> m_participatingElements;
    HashSet<HTMLMediaElement*> m_activeParticipatingElements;
    HashSet<HTMLMediaElement*>* m_iteratedActiveParticipatingElements { nullptr };

    Document& m_document;
    const Kind m_kind;
    RefPtr<MediaRemoteControls> m_controls;
    MediaSessionMetadata m_metadata;
};

} // namespace WebCore

#endif /* ENABLE(MEDIA_SESSION) */
