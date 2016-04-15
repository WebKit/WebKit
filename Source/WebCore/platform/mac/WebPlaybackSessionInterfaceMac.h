/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef WebPlaybackSessionInterfaceMac_h
#define WebPlaybackSessionInterfaceMac_h

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#include "HTMLMediaElementEnums.h"
#include "WebPlaybackSessionInterface.h"
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS WebPlaybackControlsManager;

namespace WebCore {
class IntRect;
class WebPlaybackSessionModel;

class WEBCORE_EXPORT WebPlaybackSessionInterfaceMac final
    : public WebPlaybackSessionInterface
    , public RefCounted<WebPlaybackSessionInterfaceMac> {
public:
    static Ref<WebPlaybackSessionInterfaceMac> create()
    {
        return adoptRef(*new WebPlaybackSessionInterfaceMac());
    }
    virtual ~WebPlaybackSessionInterfaceMac();
    WebPlaybackSessionModel* webPlaybackSessionModel() const { return m_playbackSessionModel; }
    WEBCORE_EXPORT void setWebPlaybackSessionModel(WebPlaybackSessionModel*);

    WEBCORE_EXPORT void resetMediaState() final { }
    WEBCORE_EXPORT void setDuration(double) final;
    WEBCORE_EXPORT void setCurrentTime(double /*currentTime*/, double /*anchorTime*/) final;
    WEBCORE_EXPORT void setBufferedTime(double) final { }
    WEBCORE_EXPORT void setRate(bool /*isPlaying*/, float /*playbackRate*/) final;
    WEBCORE_EXPORT void setSeekableRanges(const TimeRanges&) final;
    WEBCORE_EXPORT void setCanPlayFastReverse(bool) final { }
    WEBCORE_EXPORT void setAudioMediaSelectionOptions(const Vector<String>& /*options*/, uint64_t /*selectedIndex*/) final;
    WEBCORE_EXPORT void setLegibleMediaSelectionOptions(const Vector<String>& /*options*/, uint64_t /*selectedIndex*/) final;
    WEBCORE_EXPORT void setExternalPlayback(bool, ExternalPlaybackTargetType, String) final { }
    WEBCORE_EXPORT void setWirelessVideoPlaybackDisabled(bool) final { }
    WEBCORE_EXPORT void invalidate();
    WEBCORE_EXPORT void ensureControlsManager();
    WEBCORE_EXPORT WebPlaybackControlsManager *playBackControlsManager();

private:
    WebPlaybackSessionModel* m_playbackSessionModel { nullptr };
    RetainPtr<WebPlaybackControlsManager> m_playbackControlsManager;
};

}

#endif // PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#endif
