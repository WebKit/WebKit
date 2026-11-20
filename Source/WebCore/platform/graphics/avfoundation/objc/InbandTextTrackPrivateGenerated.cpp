/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InbandTextTrackPrivateGenerated.h"

#if ENABLE(VIDEO) && HAVE(AVCAPTIONTRANSCRIBER)

#include "InbandGenericCue.h"
#include "InbandTextTrackPrivateClient.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(InbandTextTrackPrivateGenerated);

static constexpr TrackID generatedTrackID = 1ULL << 62;

InbandTextTrackPrivateGenerated::InbandTextTrackPrivateGenerated()
    : InbandTextTrackPrivate(CueFormat::Generic)
{
}

TrackID InbandTextTrackPrivateGenerated::id() const
{
    return generatedTrackID;
}

void InbandTextTrackPrivateGenerated::setLanguage(const String& language)
{
    if (language == m_language)
        return;

    m_language = language;
    notifyMainThreadClient([&](auto& client) {
        client.languageChanged(m_language);
    });
}

void InbandTextTrackPrivateGenerated::addCues(Vector<GenericCueData>&& cues)
{
    for (auto& cueData : cues) {
        Ref cue = InbandGenericCue::create(WTF::move(cueData));
        notifyMainThreadClient([&cue](auto& client) {
            downcast<InbandTextTrackPrivateClient>(client).addGenericCue(cue);
        });
    }
}

}

#endif
