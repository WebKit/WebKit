/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#ifndef WebViewEfl_h
#define WebViewEfl_h

#include "WebView.h"

class EwkView;

namespace WebKit {

#if ENABLE(TOUCH_EVENTS)
class EwkTouchEvent;
#endif

class WebViewEfl : public WebView
    {
public:
    void setEwkView(EwkView*);
    EwkView* ewkView() { return m_ewkView; }

    void paintToCairoSurface(cairo_surface_t*);
    void setThemePath(const String&);

#if ENABLE(TOUCH_EVENTS)
    void sendTouchEvent(EwkTouchEvent*);
#endif
    void sendMouseEvent(const Evas_Event_Mouse_Down*);
    void sendMouseEvent(const Evas_Event_Mouse_Up*);
    void sendMouseEvent(const Evas_Event_Mouse_Move*);

private:
    WebViewEfl(WebContext*, WebPageGroup*);

    void setCursor(const WebCore::Cursor&) override;
    PassRefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy*) override;
    void updateTextInputState() override;
    void handleDownloadRequest(DownloadProxy*) override;

#if ENABLE(CONTEXT_MENUS)
    PassRefPtr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy*) override;
#endif

#if ENABLE(FULLSCREEN_API)
    // WebFullScreenManagerProxyClient
    virtual void closeFullScreenManager() override final { }
    virtual bool isFullScreen() override final;
    virtual void enterFullScreen() override final;
    virtual void exitFullScreen() override final;
    virtual void beganEnterFullScreen(const WebCore::IntRect&, const WebCore::IntRect&) override final { }
    virtual void beganExitFullScreen(const WebCore::IntRect&, const WebCore::IntRect&) override final { }
#endif

private:
    EwkView* m_ewkView;
    bool m_hasRequestedFullScreen;

    friend class WebView;
};

} // namespace WebKit

#endif // WebViewEfl_h
