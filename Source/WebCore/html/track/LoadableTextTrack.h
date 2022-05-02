/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#include "TextTrack.h"
#include "TextTrackLoader.h"

namespace WebCore {

class HTMLTrackElement;

class LoadableTextTrack final : public TextTrack, private TextTrackLoaderClient {
    WTF_MAKE_ISO_ALLOCATED(LoadableTextTrack);
public:
    static Ref<LoadableTextTrack> create(HTMLTrackElement&, const AtomString& kind, const AtomString& label, const AtomString& language);

    void scheduleLoad(const URL&);

    size_t trackElementIndex();
    HTMLTrackElement* trackElement() const { return m_trackElement; }
    void clearElement() { m_trackElement = nullptr; }

private:
    LoadableTextTrack(HTMLTrackElement&, const AtomString& kind, const AtomString& label, const AtomString& language);

    void newCuesAvailable(TextTrackLoader&) final;
    void cueLoadingCompleted(TextTrackLoader&, bool loadingFailed) final;
    void newRegionsAvailable(TextTrackLoader&) final;
    void newStyleSheetsAvailable(TextTrackLoader&) final;

    AtomString id() const final;
    bool isDefault() const final;

    void loadTimerFired();

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const override { return "LoadableTextTrack"; }
#endif

    HTMLTrackElement* m_trackElement;
    std::unique_ptr<TextTrackLoader> m_loader;
    URL m_url;
    bool m_loadPending { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::LoadableTextTrack)
    static bool isType(const WebCore::TextTrack& track) { return track.trackType() == WebCore::TextTrack::TrackElement; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO)
