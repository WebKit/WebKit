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

#include <WebCore/BoxExtents.h>
#include <WebCore/EventListener.h>
#include <WebCore/ExceptionOr.h>
#include <WebCore/FloatSize.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/IntRect.h>
#include <wtf/CompletionHandler.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

#if ENABLE(QUICKLOOK_FULLSCREEN)
#include "FullScreenMediaDetails.h"
#endif

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class IntRect;
class Element;
class WeakPtrImplWithEventTargetData;
class GraphicsLayer;
class HTMLVideoElement;
class RenderImage;
}

namespace WebKit {

class WebPage;
struct FullScreenMediaDetails;

class WebFullScreenManager final : public WebCore::EventListener {
public:
    static Ref<WebFullScreenManager> create(WebPage&);
    virtual ~WebFullScreenManager();

    void invalidate();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    bool supportsFullScreenForElement(const WebCore::Element&, bool withKeyboard);
    void enterFullScreenForElement(WebCore::Element&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, CompletionHandler<void(WebCore::ExceptionOr<void>)>&&, CompletionHandler<bool(bool)>&&);
#if ENABLE(QUICKLOOK_FULLSCREEN)
    void updateImageSource(WebCore::Element&);
#endif // ENABLE(QUICKLOOK_FULLSCREEN)
    void exitFullScreenForElement(WebCore::Element*, CompletionHandler<void()>&&);

    void didEnterFullScreen(CompletionHandler<bool(bool)>&&);
    void willExitFullScreen(CompletionHandler<void()>&&);
    void didExitFullScreen(CompletionHandler<void()>&&);

    void enterFullScreenForOwnerElements(WebCore::FrameIdentifier, CompletionHandler<void()>&&);
    void exitFullScreenInMainFrame(CompletionHandler<void()>&&);

    WebCore::Element* NODELETE element();

    void videoControlsManagerDidChange();

    bool operator==(const WebCore::EventListener& listener) const final { return this == &listener; }

protected:
    WebFullScreenManager(WebPage&);

    void setPIPStandbyElement(WebCore::HTMLVideoElement*);

    void willEnterFullScreen(WebCore::Element&, CompletionHandler<void(WebCore::ExceptionOr<void>)>&&, CompletionHandler<bool(bool)>&&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode = WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard);
    void setAnimatingFullScreen(bool);
    void requestRestoreFullScreen(CompletionHandler<void(bool)>&&);
    void requestExitFullScreen();
    void setFullscreenInsets(const WebCore::FloatBoxExtent&);
    void setFullscreenAutoHideDuration(Seconds);

    WebCore::IntRect m_initialFrame;
    WebCore::IntRect m_finalFrame;
    WebCore::IntPoint m_scrollPosition;
    const Ref<WebPage> m_page;
    RefPtr<WebCore::Element> m_element;
    WeakPtr<WebCore::Element, WebCore::WeakPtrImplWithEventTargetData> m_elementToRestore;
    std::optional<WebCore::FrameIdentifier> m_elementFrameIdentifier;
#if ENABLE(QUICKLOOK_FULLSCREEN)
    WebCore::FloatSize m_oldSize;
    double m_scaleFactor { 1 };
    double m_minEffectiveWidth { 0 };
#endif
#if ENABLE(VIDEO)
    RefPtr<WebCore::HTMLVideoElement> m_pipStandbyElement;
#endif

private:
    void close();

    void handleEvent(WebCore::ScriptExecutionContext&, WebCore::Event&) final;

    void setElement(WebCore::Element&);
    void clearElement();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger; }
    uint64_t logIdentifier() const { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "WebFullScreenManager"_s; }
    WTFLogChannel& NODELETE logChannel() const;
#endif

#if ENABLE(VIDEO)
#if ENABLE(IMAGE_ANALYSIS)
    void scheduleTextRecognitionForMainVideo();
    void endTextRecognitionForMainVideoIfNeeded();
    void mainVideoElementTextRecognitionTimerFired();
#endif
    void updateMainVideoElement();
    void setMainVideoElement(RefPtr<WebCore::HTMLVideoElement>&&);

    WeakPtr<WebCore::HTMLVideoElement> m_mainVideoElement;
#if ENABLE(IMAGE_ANALYSIS)
    RunLoop::Timer m_mainVideoElementTextRecognitionTimer;
    bool m_isPerformingTextRecognitionInMainVideo { false };
#endif
#endif // ENABLE(VIDEO)

    void performEnterFullScreen();

    // State stored by enterFullScreenForElement for performEnterFullScreen to use
    CompletionHandler<void(WebCore::ExceptionOr<void>)> m_pendingWillEnterCallback;
    CompletionHandler<bool(bool)> m_pendingDidEnterCallback;
    WebCore::HTMLMediaElementEnums::VideoFullscreenMode m_pendingMode { WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard };

#if ENABLE(QUICKLOOK_FULLSCREEN)
    enum class IsUpdating : bool { No, Yes };
    FullScreenMediaDetails getImageMediaDetails(CheckedPtr<WebCore::RenderImage>, IsUpdating);
    bool m_willUseQuickLookForFullscreen { false };

    // Timer for waiting on responsive image load
    RunLoop::Timer m_waitForLargerImageLoadTimer;
    enum class LargerImageLoadState  { NotWaiting, WaitingForLoad, TimedOut };
    LargerImageLoadState m_waitingForLargerImageLoad { LargerImageLoadState::NotWaiting };
    URL m_imageSourceBeforeViewportChange;

    void waitForLargerImageLoadTimerFired();

    std::optional<FullScreenMediaDetails> m_pendingImageMediaDetails;
#endif

    bool m_closing { false };
    bool m_inWindowFullScreenMode { false };
#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
