/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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

#include "FullScreenMediaDetails.h"
#include "MessageReceiver.h"
#include <WebCore/BoxExtents.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/CheckedRef.h>
#include <wtf/CompletionHandler.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {
class FloatSize;
class IntRect;

enum class ScreenOrientationType : uint8_t;
}

namespace WebKit {

class RemotePageFullscreenManagerProxy;
class WebFullScreenManagerProxy;
class WebPageProxy;
class WebProcessProxy;
struct SharedPreferencesForWebProcess;

class WebFullScreenManagerProxyClient : public CanMakeCheckedPtr<WebFullScreenManagerProxyClient> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(WebFullScreenManagerProxyClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebFullScreenManagerProxyClient);
public:
    virtual ~WebFullScreenManagerProxyClient() { }

    virtual void closeFullScreenManager() = 0;
    virtual bool isFullScreen() = 0;
    virtual void enterFullScreen(WebCore::FloatSize mediaDimensions, CompletionHandler<void(bool)>&&) = 0;
#if ENABLE(QUICKLOOK_FULLSCREEN)
    virtual void updateImageSource() = 0;
#endif
    virtual void exitFullScreen(CompletionHandler<void()>&&) = 0;
    virtual void beganEnterFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame, CompletionHandler<void(bool)>&&) = 0;
    virtual void beganExitFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame, CompletionHandler<void()>&&) = 0;

    virtual bool lockFullscreenOrientation(WebCore::ScreenOrientationType) { return false; }
    virtual void unlockFullscreenOrientation() { }
};

class WebFullScreenManagerProxy : public IPC::MessageReceiver, public CanMakeCheckedPtr<WebFullScreenManagerProxy>, public RefCounted<WebFullScreenManagerProxy> {
    WTF_MAKE_TZONE_ALLOCATED(WebFullScreenManagerProxy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebFullScreenManagerProxy);
public:
    static Ref<WebFullScreenManagerProxy> create(WebPageProxy&, WebFullScreenManagerProxyClient&);
    virtual ~WebFullScreenManagerProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess(const IPC::Connection&) const;

    bool isFullScreen();
    bool blocksReturnToFullscreenFromPictureInPicture() const;
#if ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
    bool isVideoElement() const { return m_isVideoElement; }
#endif
#if ENABLE(QUICKLOOK_FULLSCREEN)
    bool isImageElement() const { return m_imageBuffer; }
    void prepareQuickLookImageURL(CompletionHandler<void(URL&&)>&&) const;
#endif // QUICKLOOK_FULLSCREEN
    void close();
    void detachFromClient();
    void attachToNewClient(WebFullScreenManagerProxyClient&);

    void enterFullScreenForOwnerElementsInOtherProcesses(WebCore::FrameIdentifier, CompletionHandler<void()>&&);
    void exitFullScreenInOtherProcesses(WebCore::FrameIdentifier, CompletionHandler<void()>&&);

    enum class FullscreenState : uint8_t {
        NotInFullscreen,
        EnteringFullscreen,
        InFullscreen,
        ExitingFullscreen,
    };
    FullscreenState fullscreenState() const { return m_fullscreenState; }
    void setAnimatingFullScreen(bool);
    void requestRestoreFullScreen(CompletionHandler<void(bool)>&&);
    void requestExitFullScreen();
    void setFullscreenInsets(const WebCore::FloatBoxExtent&);
    void setFullscreenAutoHideDuration(Seconds);
    void closeWithCallback(CompletionHandler<void()>&&);
    bool lockFullscreenOrientation(WebCore::ScreenOrientationType);
    void unlockFullscreenOrientation();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

private:
    WebFullScreenManagerProxy(WebPageProxy&, WebFullScreenManagerProxyClient&);

    Awaitable<bool> enterFullScreen(IPC::Connection&, WebCore::FrameIdentifier, bool blocksReturnToFullscreenFromPictureInPicture, FullScreenMediaDetails);
    void didEnterFullScreen(CompletionHandler<void(bool)>&&);
#if ENABLE(QUICKLOOK_FULLSCREEN)
    void updateImageSource(FullScreenMediaDetails&&);
#endif
    Awaitable<void> exitFullScreen();
    Awaitable<bool> beganEnterFullScreen(WebCore::IntRect initialFrame, WebCore::IntRect finalFrame);
    Awaitable<void> beganExitFullScreen(WebCore::FrameIdentifier, WebCore::IntRect initialFrame, WebCore::IntRect finalFrame);
    void callCloseCompletionHandlers();
    template<typename M> void sendToWebProcess(M&&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger; }
    uint64_t logIdentifier() const { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "WebFullScreenManagerProxy"_s; }
    WTFLogChannel& logChannel() const;
#endif

    WeakPtr<WebPageProxy> m_page;
    CheckedPtr<WebFullScreenManagerProxyClient> m_client;
    FullscreenState m_fullscreenState { FullscreenState::NotInFullscreen };
    bool m_blocksReturnToFullscreenFromPictureInPicture { false };
#if ENABLE(VIDEO_USES_ELEMENT_FULLSCREEN)
    bool m_isVideoElement { false };
#endif
#if ENABLE(QUICKLOOK_FULLSCREEN)
    String m_imageMIMEType;
    RefPtr<WebCore::SharedBuffer> m_imageBuffer;
#endif // QUICKLOOK_FULLSCREEN
    Vector<CompletionHandler<void()>> m_closeCompletionHandlers;
    WeakPtr<WebProcessProxy> m_fullScreenProcess;

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
