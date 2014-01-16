/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef HTMLMediaSession_h
#define HTMLMediaSession_h

#include "MediaSession.h"

namespace WebCore {

class HTMLMediaElement;

class HTMLMediaSession : public MediaSession {
public:
    static std::unique_ptr<HTMLMediaSession> create(MediaSessionClient&);

    HTMLMediaSession(MediaSessionClient&);
    virtual ~HTMLMediaSession() { }

    void clientWillBeginPlayback();

    virtual void pauseSession() override;

    bool playbackPermitted(const HTMLMediaElement&) const;
    bool dataLoadingPermitted(const HTMLMediaElement&) const;
    bool fullscreenPermitted(const HTMLMediaElement&) const;
    bool pageAllowsDataLoading(const HTMLMediaElement&) const;
    bool pageAllowsPlaybackAfterResuming(const HTMLMediaElement&) const;
#if ENABLE(IOS_AIRPLAY)
    bool showingPlaybackTargetPickerPermitted(const HTMLMediaElement&) const;
#endif

    // Restrictions to modify default behaviors.
    enum BehaviorRestrictionFlags {
        NoRestrictions = 0,
        RequireUserGestureForLoad = 1 << 0,
        RequireUserGestureForRateChange = 1 << 1,
        RequireUserGestureForFullscreen = 1 << 2,
        RequirePageConsentToLoadMedia = 1 << 3,
        RequirePageConsentToResumeMedia = 1 << 4,
#if ENABLE(IOS_AIRPLAY)
        RequireUserGestureToShowPlaybackTargetPicker = 1 << 5,
#endif
    };
    typedef unsigned BehaviorRestrictions;

    void addBehaviorRestriction(BehaviorRestrictions);
    void removeBehaviorRestriction(BehaviorRestrictions);

private:
    BehaviorRestrictions m_restrictions;
};

}

#endif // MediaSession_h
