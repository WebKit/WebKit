/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(FULLSCREEN_API)

#include "MessageReceiver.h"
#include <wtf/CompletionHandler.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Seconds.h>
#include <wtf/Vector.h>

namespace WebCore {
class FloatSize;
class IntRect;

enum class ScreenOrientationType : uint8_t;

template <typename> class RectEdges;
using FloatBoxExtent = RectEdges<float>;
}

namespace WebKit {

class WebPageProxy;

class WebFullScreenManagerProxyClient {
public:
    virtual ~WebFullScreenManagerProxyClient() { }

    virtual void closeFullScreenManager() = 0;
    virtual bool isFullScreen() = 0;
#if PLATFORM(IOS_FAMILY)
    virtual void enterFullScreen(WebCore::FloatSize videoDimensions) = 0;
#else
    virtual void enterFullScreen() = 0;
#endif
    virtual void exitFullScreen() = 0;
    virtual void beganEnterFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame) = 0;
    virtual void beganExitFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame) = 0;

    virtual bool lockFullscreenOrientation(WebCore::ScreenOrientationType) { return false; }
    virtual void unlockFullscreenOrientation() { }
};

class WebFullScreenManagerProxy : public IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebFullScreenManagerProxy(WebPageProxy&, WebFullScreenManagerProxyClient&);
    virtual ~WebFullScreenManagerProxy();

    bool isFullScreen();
    bool blocksReturnToFullscreenFromPictureInPicture() const;
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    bool isVideoElementWithControls() const;
#endif
    void close();

    enum class FullscreenState : uint8_t {
        NotInFullscreen,
        EnteringFullscreen,
        InFullscreen,
        ExitingFullscreen,
    };
    FullscreenState fullscreenState() const { return m_fullscreenState; }

    void willEnterFullScreen();
    void didEnterFullScreen();
    void willExitFullScreen();
    void didExitFullScreen();
    void setAnimatingFullScreen(bool);
    void requestRestoreFullScreen();
    void requestExitFullScreen();
    void saveScrollPosition();
    void restoreScrollPosition();
    void setFullscreenInsets(const WebCore::FloatBoxExtent&);
    void setFullscreenAutoHideDuration(Seconds);
    void setFullscreenControlsHidden(bool);
    void closeWithCallback(CompletionHandler<void()>&&);
    bool lockFullscreenOrientation(WebCore::ScreenOrientationType);
    void unlockFullscreenOrientation();

private:
    void supportsFullScreen(bool withKeyboard, CompletionHandler<void(bool)>&&);
    void enterFullScreen(bool blocksReturnToFullscreenFromPictureInPicture, bool isVideoElementWithControls, WebCore::FloatSize videoDimensions);
    void exitFullScreen();
    void beganEnterFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame);
    void beganExitFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame);
    void callCloseCompletionHandlers();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

    WebPageProxy& m_page;
    WebFullScreenManagerProxyClient& m_client;
    FullscreenState m_fullscreenState { FullscreenState::NotInFullscreen };
    bool m_blocksReturnToFullscreenFromPictureInPicture { false };
#if HAVE(UIKIT_WEBKIT_INTERNALS)
    bool m_isVideoElementWithControls { false };
#endif
    Vector<CompletionHandler<void()>> m_closeCompletionHandlers;
};

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
