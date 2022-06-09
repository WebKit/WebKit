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
#include <WebCore/IntRect.h>
#include <WebCore/LengthBox.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace IPC {
class Connection;
class Decoder;
}

namespace WebCore {
class IntRect;
class Element;
class GraphicsLayer;
class HTMLVideoElement;
}

namespace WebKit {

class WebPage;

class WebFullScreenManager : public RefCounted<WebFullScreenManager> {
public:
    static Ref<WebFullScreenManager> create(WebPage*);
    virtual ~WebFullScreenManager();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&);

    bool supportsFullScreen(bool withKeyboard);
    void enterFullScreenForElement(WebCore::Element*);
    void exitFullScreenForElement(WebCore::Element*);

    void willEnterFullScreen();
    void didEnterFullScreen();
    void willExitFullScreen();
    void didExitFullScreen();

    WebCore::Element* element();

    void videoControlsManagerDidChange();

protected:
    WebFullScreenManager(WebPage*);

    void setPIPStandbyElement(WebCore::HTMLVideoElement*);

    void setAnimatingFullScreen(bool);
    void requestEnterFullScreen();
    void requestExitFullScreen();
    void saveScrollPosition();
    void restoreScrollPosition();
    void setFullscreenInsets(const WebCore::FloatBoxExtent&);
    void setFullscreenAutoHideDuration(Seconds);
    void setFullscreenControlsHidden(bool);

    void didReceiveWebFullScreenManagerMessage(IPC::Connection&, IPC::Decoder&);

    WebCore::IntRect m_initialFrame;
    WebCore::IntRect m_finalFrame;
    WebCore::IntPoint m_scrollPosition;
    float m_topContentInset { 0 };
    RefPtr<WebPage> m_page;
    RefPtr<WebCore::Element> m_element;
#if ENABLE(VIDEO)
    RefPtr<WebCore::HTMLVideoElement> m_pipStandbyElement;
#endif

private:
    void close();
    bool m_closing { false };
};

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
