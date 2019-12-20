/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#include "InbandTextTrackPrivateClient.h"
#include "TextTrack.h"

namespace WebCore {

class InbandTextTrack : public TextTrack, private InbandTextTrackPrivateClient {
    WTF_MAKE_ISO_ALLOCATED(InbandTextTrack);
public:
    static Ref<InbandTextTrack> create(ScriptExecutionContext&, TextTrackClient&, InbandTextTrackPrivate&);
    virtual ~InbandTextTrack();

    bool isClosedCaptions() const override;
    bool isSDH() const override;
    bool containsOnlyForcedSubtitles() const override;
    bool isMainProgramContent() const override;
    bool isEasyToRead() const override;
    void setMode(Mode) override;
    size_t inbandTrackIndex();

    AtomString inBandMetadataTrackDispatchType() const override;

    void setPrivate(InbandTextTrackPrivate&);
    void setMediaElement(WeakPtr<HTMLMediaElement>) override;

protected:
    InbandTextTrack(ScriptExecutionContext&, TextTrackClient&, InbandTextTrackPrivate&);

    void setModeInternal(Mode);
    void updateKindFromPrivate();

    Ref<InbandTextTrackPrivate> m_private;

private:
    bool isInband() const final { return true; }
    void idChanged(const AtomString&) override;
    void labelChanged(const AtomString&) override;
    void languageChanged(const AtomString&) override;
    void willRemove() override;

    void addDataCue(const MediaTime&, const MediaTime&, const void*, unsigned) override { ASSERT_NOT_REACHED(); }

#if ENABLE(DATACUE_VALUE)
    void addDataCue(const MediaTime&, const MediaTime&, Ref<SerializedPlatformRepresentation>&&, const String&) override { ASSERT_NOT_REACHED(); }
    void updateDataCue(const MediaTime&, const MediaTime&, SerializedPlatformRepresentation&) override  { ASSERT_NOT_REACHED(); }
    void removeDataCue(const MediaTime&, const MediaTime&, SerializedPlatformRepresentation&) override  { ASSERT_NOT_REACHED(); }
#endif

    void addGenericCue(GenericCueData&) override { ASSERT_NOT_REACHED(); }
    void updateGenericCue(GenericCueData&) override { ASSERT_NOT_REACHED(); }
    void removeGenericCue(GenericCueData&) override { ASSERT_NOT_REACHED(); }

    void parseWebVTTFileHeader(String&&) override { ASSERT_NOT_REACHED(); }
    void parseWebVTTCueData(const char*, unsigned) override { ASSERT_NOT_REACHED(); }
    void parseWebVTTCueData(const ISOWebVTTCue&) override { ASSERT_NOT_REACHED(); }

    MediaTime startTimeVariance() const override;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::InbandTextTrack)
    static bool isType(const WebCore::TextTrack& track) { return track.isInband(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO_TRACK)
