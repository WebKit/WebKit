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

#include "WebCoreArgumentCoders.h"
#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/IntRect.h>
#include <WebCore/LengthBox.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

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
}

namespace WebKit {

class WebPage;

class WebFullScreenManager final : public WebCore::EventListener {
public:
    static Ref<WebFullScreenManager> create(WebPage&);
    virtual ~WebFullScreenManager();

    void invalidate();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    bool supportsFullScreen(bool withKeyboard);
    void enterFullScreenForElement(WebCore::Element*, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void exitFullScreenForElement(WebCore::Element*);

    void willEnterFullScreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode = WebCore::HTMLMediaElementEnums::VideoFullscreenModeStandard);
    void didEnterFullScreen();
    void willExitFullScreen();
    void didExitFullScreen();

    void saveScrollPosition();
    void restoreScrollPosition();

    WebCore::Element* element();

    void videoControlsManagerDidChange();

    bool operator==(const WebCore::EventListener& listener) const final { return this == &listener; }

protected:
    WebFullScreenManager(WebPage&);

    void setPIPStandbyElement(WebCore::HTMLVideoElement*);

    void setAnimatingFullScreen(bool);
    void requestRestoreFullScreen(CompletionHandler<void(bool)>&&);
    void requestExitFullScreen();
    void setFullscreenInsets(const WebCore::FloatBoxExtent&);
    void setFullscreenAutoHideDuration(Seconds);

    void didReceiveWebFullScreenManagerMessage(IPC::Connection&, IPC::Decoder&);

    WebCore::IntRect m_initialFrame;
    WebCore::IntRect m_finalFrame;
    WebCore::IntPoint m_scrollPosition;
    float m_topContentInset { 0 };
    Ref<WebPage> m_page;
    RefPtr<WebCore::Element> m_element;
    WeakPtr<WebCore::Element, WebCore::WeakPtrImplWithEventTargetData> m_elementToRestore;
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
    const void* logIdentifier() const { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "WebFullScreenManager"_s; }
    WTFLogChannel& logChannel() const;
#endif

#if ENABLE(VIDEO)
    void scheduleTextRecognitionForMainVideo();
    void endTextRecognitionForMainVideoIfNeeded();
    void mainVideoElementTextRecognitionTimerFired();
    void updateMainVideoElement();
    void setMainVideoElement(RefPtr<WebCore::HTMLVideoElement>&&);

    WeakPtr<WebCore::HTMLVideoElement> m_mainVideoElement;
    RunLoop::Timer m_mainVideoElementTextRecognitionTimer;
    bool m_isPerformingTextRecognitionInMainVideo { false };
#endif // ENABLE(VIDEO)

    bool m_closing { false };
    bool m_inWindowFullScreenMode { false };
#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
#endif
};

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
