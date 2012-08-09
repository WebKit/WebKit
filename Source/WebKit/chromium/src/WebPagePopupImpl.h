/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPagePopupImpl_h
#define WebPagePopupImpl_h

#if ENABLE(PAGE_POPUP)

#include "PagePopup.h"
#include "PageWidgetDelegate.h"
#include "WebPagePopup.h"
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class Page;
class PagePopupClient;
class PlatformKeyboardEvent;
}

namespace WebKit {

class PagePopupChromeClient;
class WebViewImpl;

class WebPagePopupImpl : public WebPagePopup,
                         public PageWidgetEventHandler,
                         public WebCore::PagePopup,
                         public RefCounted<WebPagePopupImpl> {
    WTF_MAKE_NONCOPYABLE(WebPagePopupImpl);
    WTF_MAKE_FAST_ALLOCATED;

public:
    virtual ~WebPagePopupImpl();
    bool init(WebViewImpl*, WebCore::PagePopupClient*, const WebCore::IntRect& originBoundsInRootView);
    bool handleKeyEvent(const WebCore::PlatformKeyboardEvent&);
    void closePopup();
    WebWidgetClient* widgetClient() const { return m_widgetClient; }
    bool hasSamePopupClient(WebPagePopupImpl* other) { return other && m_popupClient == other->m_popupClient; }

private:
    // WebWidget functions
    virtual WebSize size() OVERRIDE;
    virtual void animate(double) OVERRIDE;
    virtual void setCompositorSurfaceReady() OVERRIDE;
    virtual void composite(bool) OVERRIDE;
    virtual void layout() OVERRIDE;
    virtual void paint(WebCanvas*, const WebRect&) OVERRIDE;
    virtual void resize(const WebSize&) OVERRIDE;
    virtual void close() OVERRIDE;
    virtual bool handleInputEvent(const WebInputEvent&) OVERRIDE;
    virtual void setFocus(bool) OVERRIDE;

    // PageWidgetEventHandler functions
    virtual bool handleKeyEvent(const WebKeyboardEvent&) OVERRIDE;
    virtual bool handleCharEvent(const WebKeyboardEvent&) OVERRIDE;
#if ENABLE(GESTURE_EVENTS)
    virtual bool handleGestureEvent(const WebGestureEvent&) OVERRIDE;
#endif

    explicit WebPagePopupImpl(WebWidgetClient*);
    bool initPage();
    void reposition(const WebSize&);

    WebWidgetClient* m_widgetClient;
    WebRect m_windowRectInScreen;
    WebCore::IntRect m_originBoundsInRootView;
    WebViewImpl* m_webView;
    OwnPtr<WebCore::Page> m_page;
    OwnPtr<PagePopupChromeClient> m_chromeClient;
    WebCore::PagePopupClient* m_popupClient;

    friend class WebPagePopup;
    friend class PagePopupChromeClient;
};

} // namespace WebKit
#endif // ENABLE(PAGE_POPUP)
#endif // WebPagePopupImpl_h
