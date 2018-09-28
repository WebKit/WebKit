/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#include "EventListener.h"
#include "HTMLMediaElementEnums.h"
#include "PlaybackSessionInterface.h"
#include "PlaybackSessionModel.h"
#include "Timer.h"
#include <functional>
#include <objc/objc.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS WebAVPlayerController;
OBJC_CLASS AVPlayerViewController;
OBJC_CLASS UIViewController;
OBJC_CLASS UIWindow;
OBJC_CLASS UIView;

namespace WebCore {
class IntRect;
class PlaybackSessionModel;
class WebPlaybackSessionChangeObserver;

class WEBCORE_EXPORT PlaybackSessionInterfaceAVKit
    : public PlaybackSessionInterface
    , public PlaybackSessionModelClient
    , public RefCounted<PlaybackSessionInterfaceAVKit> {

public:
    static Ref<PlaybackSessionInterfaceAVKit> create(PlaybackSessionModel& model)
    {
        return adoptRef(*new PlaybackSessionInterfaceAVKit(model));
    }
    virtual ~PlaybackSessionInterfaceAVKit();
    PlaybackSessionModel* playbackSessionModel() const { return m_playbackSessionModel; }

    // PlaybackSessionInterface
    WEBCORE_EXPORT void resetMediaState() override;

    // PlaybackSessionModelClient
    WEBCORE_EXPORT void durationChanged(double) override;
    WEBCORE_EXPORT void currentTimeChanged(double currentTime, double anchorTime) override;
    WEBCORE_EXPORT void bufferedTimeChanged(double) override;
    WEBCORE_EXPORT void rateChanged(bool isPlaying, float playbackRate) override;
    WEBCORE_EXPORT void seekableRangesChanged(const TimeRanges&, double lastModifiedTime, double liveUpdateInterval) override;
    WEBCORE_EXPORT void canPlayFastReverseChanged(bool) override;
    WEBCORE_EXPORT void audioMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex) override;
    WEBCORE_EXPORT void legibleMediaSelectionOptionsChanged(const Vector<MediaSelectionOption>& options, uint64_t selectedIndex) override;
    WEBCORE_EXPORT void externalPlaybackChanged(bool enabled, PlaybackSessionModel::ExternalPlaybackTargetType, const String& localizedDeviceName) override;
    WEBCORE_EXPORT void wirelessVideoPlaybackDisabledChanged(bool) override;
    WEBCORE_EXPORT void mutedChanged(bool) override;
    WEBCORE_EXPORT void volumeChanged(double) override;

    WEBCORE_EXPORT virtual void invalidate();

    WebAVPlayerController *playerController() const { return m_playerController.get(); }

protected:
    WEBCORE_EXPORT PlaybackSessionInterfaceAVKit(PlaybackSessionModel&);

    RetainPtr<WebAVPlayerController> m_playerController;
    PlaybackSessionModel* m_playbackSessionModel { nullptr };
};

}

#endif

