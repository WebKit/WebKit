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

#ifndef TestInterfaces_h
#define TestInterfaces_h

#include <memory>
#include <vector>

#if defined(WIN32)
#include "WebTestThemeEngineWin.h"
#elif defined(__APPLE__)
#include "WebTestThemeEngineMac.h"
#endif

namespace WebKit {
class WebFrame;
class WebThemeEngine;
class WebURL;
class WebView;
}

namespace WebTestRunner {

class AccessibilityController;
class EventSender;
class GamepadController;
class TestRunner;
class TextInputController;
class WebTestDelegate;
class WebTestProxyBase;

class TestInterfaces {
public:
    TestInterfaces();
    ~TestInterfaces();

    void setWebView(WebKit::WebView*, WebTestProxyBase*);
    void setDelegate(WebTestDelegate*);
    void bindTo(WebKit::WebFrame*);
    void resetAll();
    void setTestIsRunning(bool);
    void configureForTestWithURL(const WebKit::WebURL&, bool generatePixels);

    void windowOpened(WebTestProxyBase*);
    void windowClosed(WebTestProxyBase*);

    AccessibilityController* accessibilityController();
    EventSender* eventSender();
    TestRunner* testRunner();
    WebKit::WebView* webView();
    WebTestDelegate* delegate();
    WebTestProxyBase* proxy();
    const std::vector<WebTestProxyBase*>& windowList();
    WebKit::WebThemeEngine* themeEngine();

private:
    std::auto_ptr<AccessibilityController> m_accessibilityController;
    std::auto_ptr<EventSender> m_eventSender;
    std::auto_ptr<GamepadController> m_gamepadController;
    std::auto_ptr<TextInputController> m_textInputController;
    std::auto_ptr<TestRunner> m_testRunner;
    WebKit::WebView* m_webView;
    WebTestDelegate* m_delegate;
    WebTestProxyBase* m_proxy;

    std::vector<WebTestProxyBase*> m_windowList;
#if !defined(USE_DEFAULT_RENDER_THEME)
#if defined(WIN32)
    std::auto_ptr<WebTestThemeEngineWin> m_themeEngine;
#elif defined(__APPLE__)
    std::auto_ptr<WebTestThemeEngineMac> m_themeEngine;
#endif
#endif
};

}

#endif // TestInterfaces_h
