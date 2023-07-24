/*
 * Copyright (C) 2021 Sony Interactive Entertainment Inc.
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

#include "WebContext.h"
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKView.h>
#include <memory>
#include <toolkitten/Widget.h>

#if defined(USE_WPE_BACKEND_PLAYSTATION) && USE_WPE_BACKEND_PLAYSTATION
namespace WPEToolingBackends {
class HeadlessViewBackend;
}
#endif

class WebViewWindow final : public toolkitten::Widget {
public:
    struct Client {
        std::function<WebViewWindow*(WKPageConfigurationRef configuration)> createNewWindow;

        std::function<void(WebViewWindow*)> didUpdateTitle;
        std::function<void(WebViewWindow*)> didUpdateURL;
        std::function<void(WebViewWindow*)> didUpdateProgress;
    };

    static std::unique_ptr<WebViewWindow> create(Client&&, WKPageConfigurationRef);

    explicit WebViewWindow(WKPageConfigurationRef, Client&&);
    virtual ~WebViewWindow();

    void loadURL(const char* url);
    void goBack();
    void goForward();
    void reload();

    void toggleZoomFactor();

    void setSize(toolkitten::IntSize) override;
    bool onKeyUp(int32_t virtualKeyCode) override;
    bool onKeyDown(int32_t virtualKeyCode) override;
    bool onMouseMove(toolkitten::IntPoint) override;
    bool onMouseUp(toolkitten::IntPoint) override;
    bool onMouseDown(toolkitten::IntPoint) override;
    bool onWheelMove(toolkitten::IntPoint, toolkitten::IntPoint) override;

    const std::string& title() const { return m_title; }
    const std::string& url() const { return m_url; }
    double progress() const { return m_progress; }

    void setActive(bool);

private:
    WKPageRef page();

    void paintSelf(toolkitten::IntPoint position) override;
    void updateSelf() override;
    void updateSurface(toolkitten::IntRect);

    void updateTitle();
    void updateURL();
    void updateProgress();

    WKPageRef createNewPage(WKPageRef, WKPageConfigurationRef);

    WKRetainPtr<WKViewRef> m_view;
    std::shared_ptr<WebContext> m_context;
    WKRetainPtr<WKPreferencesRef> m_preferences;

#if defined(USE_WPE_BACKEND_PLAYSTATION) && USE_WPE_BACKEND_PLAYSTATION
    std::unique_ptr<WPEToolingBackends::HeadlessViewBackend> m_window;
#endif

    Client m_client;

    std::unique_ptr<unsigned char[]> m_surface;

    std::string m_title;
    std::string m_url;
    double m_progress { 0 };

    bool m_active { false };
};
