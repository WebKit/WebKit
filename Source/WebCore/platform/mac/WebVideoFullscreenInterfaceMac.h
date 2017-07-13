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

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)

#include "HTMLMediaElementEnums.h"
#include "WebPlaybackSessionInterfaceMac.h"
#include "WebPlaybackSessionModel.h"
#include "WebVideoFullscreenModel.h"
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS NSWindow;
OBJC_CLASS WebVideoFullscreenInterfaceMacObjC;

namespace WebCore {
class IntRect;
class FloatSize;
class WebPlaybackSessionInterfaceMac;
class WebVideoFullscreenChangeObserver;

class WEBCORE_EXPORT WebVideoFullscreenInterfaceMac
    : public WebVideoFullscreenModelClient
    , private WebPlaybackSessionModelClient
    , public RefCounted<WebVideoFullscreenInterfaceMac> {

public:
    static Ref<WebVideoFullscreenInterfaceMac> create(WebPlaybackSessionInterfaceMac& playbackSessionInterface)
    {
        return adoptRef(*new WebVideoFullscreenInterfaceMac(playbackSessionInterface));
    }
    virtual ~WebVideoFullscreenInterfaceMac();
    WebVideoFullscreenModel* webVideoFullscreenModel() const { return m_videoFullscreenModel; }
    WebPlaybackSessionModel* webPlaybackSessionModel() const { return m_playbackSessionInterface->webPlaybackSessionModel(); }
    WEBCORE_EXPORT void setWebVideoFullscreenModel(WebVideoFullscreenModel*);
    WebVideoFullscreenChangeObserver* webVideoFullscreenChangeObserver() const { return m_fullscreenChangeObserver; }
    WEBCORE_EXPORT void setWebVideoFullscreenChangeObserver(WebVideoFullscreenChangeObserver*);

    // WebPlaybackSessionModelClient
    WEBCORE_EXPORT void rateChanged(bool isPlaying, float playbackRate) override;
    WEBCORE_EXPORT void externalPlaybackChanged(bool  enabled, WebPlaybackSessionModel::ExternalPlaybackTargetType, const String& localizedDeviceName) override;

    // WebVideoFullscreenModelClient
    WEBCORE_EXPORT void hasVideoChanged(bool) final;
    WEBCORE_EXPORT void videoDimensionsChanged(const FloatSize&) final;

    WEBCORE_EXPORT void setupFullscreen(NSView& layerHostedView, const IntRect& initialRect, NSWindow *parentWindow, HTMLMediaElementEnums::VideoFullscreenMode, bool allowsPictureInPicturePlayback);
    WEBCORE_EXPORT void enterFullscreen();
    WEBCORE_EXPORT void exitFullscreen(const IntRect& finalRect, NSWindow *parentWindow);
    WEBCORE_EXPORT void exitFullscreenWithoutAnimationToMode(HTMLMediaElementEnums::VideoFullscreenMode);
    WEBCORE_EXPORT void cleanupFullscreen();
    WEBCORE_EXPORT void invalidate();
    void requestHideAndExitFullscreen() { }
    WEBCORE_EXPORT void preparedToReturnToInline(bool visible, const IntRect& inlineRect, NSWindow *parentWindow);
    WEBCORE_EXPORT void ensureControlsManager();

    HTMLMediaElementEnums::VideoFullscreenMode mode() const { return m_mode; }
    bool hasMode(HTMLMediaElementEnums::VideoFullscreenMode mode) const { return m_mode & mode; }
    bool isMode(HTMLMediaElementEnums::VideoFullscreenMode mode) const { return m_mode == mode; }
    void setMode(HTMLMediaElementEnums::VideoFullscreenMode);
    void clearMode(HTMLMediaElementEnums::VideoFullscreenMode);

    bool isPlayingVideoInEnhancedFullscreen() const;

    bool mayAutomaticallyShowVideoPictureInPicture() const { return false; }
    void applicationDidBecomeActive() { }

    WEBCORE_EXPORT WebVideoFullscreenInterfaceMacObjC *videoFullscreenInterfaceObjC();

private:
    WebVideoFullscreenInterfaceMac(WebPlaybackSessionInterfaceMac&);
    Ref<WebPlaybackSessionInterfaceMac> m_playbackSessionInterface;
    WebVideoFullscreenModel* m_videoFullscreenModel { nullptr };
    WebVideoFullscreenChangeObserver* m_fullscreenChangeObserver { nullptr };
    HTMLMediaElementEnums::VideoFullscreenMode m_mode { HTMLMediaElementEnums::VideoFullscreenModeNone };
    RetainPtr<WebVideoFullscreenInterfaceMacObjC> m_webVideoFullscreenInterfaceObjC;
};

}

#endif

