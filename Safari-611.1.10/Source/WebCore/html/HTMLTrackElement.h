/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "HTMLElement.h"
#include "LoadableTextTrack.h"

namespace WebCore {

class HTMLMediaElement;

class HTMLTrackElement final : public HTMLElement, public ActiveDOMObject, public TextTrackClient {
    WTF_MAKE_ISO_ALLOCATED(HTMLTrackElement);
public:
    static Ref<HTMLTrackElement> create(const QualifiedName&, Document&);

    const AtomString& kind();
    void setKind(const AtomString&);

    const AtomString& srclang() const;
    const AtomString& label() const;
    bool isDefault() const;

    enum ReadyState { NONE = 0, LOADING = 1, LOADED = 2, TRACK_ERROR = 3 };
    ReadyState readyState() const;
    void setReadyState(ReadyState);

    LoadableTextTrack& track();

    void scheduleLoad();

    enum LoadStatus { Failure, Success };
    void didCompleteLoad(LoadStatus);

    RefPtr<HTMLMediaElement> mediaElement() const;
    const AtomString& mediaElementCrossOriginAttribute() const;

private:
    HTMLTrackElement(const QualifiedName&, Document&);
    virtual ~HTMLTrackElement();

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    bool virtualHasPendingActivity() const final;

    void parseAttribute(const QualifiedName&, const AtomString&) final;

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) final;
    void removedFromAncestor(RemovalType, ContainerNode&) final;

    bool isURLAttribute(const Attribute&) const final;

    // EventTarget.
    void eventListenersDidChange() final;

    void loadTimerFired();

    // TextTrackClient
    void textTrackModeChanged(TextTrack&) final;
    void textTrackKindChanged(TextTrack&) final;
    void textTrackAddCues(TextTrack&, const TextTrackCueList&) final;
    void textTrackRemoveCues(TextTrack&, const TextTrackCueList&) final;
    void textTrackAddCue(TextTrack&, TextTrackCue&) final;
    void textTrackRemoveCue(TextTrack&, TextTrackCue&) final;

    bool canLoadURL(const URL&);

    RefPtr<LoadableTextTrack> m_track;
    Timer m_loadTimer;
    bool m_hasRelevantLoadEventsListener { false };
};

}

#endif
