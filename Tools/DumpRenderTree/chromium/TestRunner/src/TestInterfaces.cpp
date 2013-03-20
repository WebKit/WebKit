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

#include "config.h"
#include "TestInterfaces.h"

#include "AccessibilityControllerChromium.h"
#include "EventSender.h"
#include "GamepadController.h"
#include "TestRunner.h"
#include "TextInputController.h"
#include "WebCache.h"
#include "WebKit.h"
#include "WebRuntimeFeatures.h"
#include <public/WebString.h>
#include <public/WebURL.h>
#include <string>

using namespace WebKit;
using namespace std;

namespace WebTestRunner {

TestInterfaces::TestInterfaces()
    : m_accessibilityController(new AccessibilityController())
    , m_eventSender(new EventSender())
    , m_gamepadController(new GamepadController())
    , m_textInputController(new TextInputController())
    , m_testRunner(new TestRunner(this))
    , m_webView(0)
    , m_delegate(0)
{
    WebKit::setLayoutTestMode(true);

    WebRuntimeFeatures::enableDataTransferItems(true);
    WebRuntimeFeatures::enableDeviceMotion(false);
    WebRuntimeFeatures::enableGeolocation(true);
    WebRuntimeFeatures::enableIndexedDatabase(true);
    WebRuntimeFeatures::enableInputTypeDateTime(true);
    WebRuntimeFeatures::enableInputTypeDateTimeLocal(true);
    WebRuntimeFeatures::enableInputTypeMonth(true);
    WebRuntimeFeatures::enableInputTypeTime(true);
    WebRuntimeFeatures::enableInputTypeWeek(true);
    WebRuntimeFeatures::enableFileSystem(true);
    WebRuntimeFeatures::enableFontLoadEvents(true);
    WebRuntimeFeatures::enableJavaScriptI18NAPI(true);
    WebRuntimeFeatures::enableMediaSource(true);
    WebRuntimeFeatures::enableEncryptedMedia(true);
    WebRuntimeFeatures::enableMediaStream(true);
    WebRuntimeFeatures::enablePeerConnection(true);
    WebRuntimeFeatures::enableWebAudio(true);
    WebRuntimeFeatures::enableVideoTrack(true);
    WebRuntimeFeatures::enableGamepad(true);
    WebRuntimeFeatures::enableShadowDOM(true);
    WebRuntimeFeatures::enableCustomDOMElements(true);
    WebRuntimeFeatures::enableStyleScoped(true);
    WebRuntimeFeatures::enableScriptedSpeech(true);
    WebRuntimeFeatures::enableRequestAutocomplete(true);
    WebRuntimeFeatures::enableExperimentalContentSecurityPolicyFeatures(true);
    WebRuntimeFeatures::enableSeamlessIFrames(true);
    WebRuntimeFeatures::enableCanvasPath(true);

    resetAll();
}

TestInterfaces::~TestInterfaces()
{
    m_accessibilityController->setWebView(0);
    m_eventSender->setWebView(0);
    // m_gamepadController doesn't depend on WebView.
    m_textInputController->setWebView(0);
    m_testRunner->setWebView(0, 0);

    m_accessibilityController->setDelegate(0);
    m_eventSender->setDelegate(0);
    m_gamepadController->setDelegate(0);
    // m_textInputController doesn't depend on WebTestDelegate.
    m_testRunner->setDelegate(0);
}

void TestInterfaces::setWebView(WebView* webView, WebTestProxyBase* proxy)
{
    m_webView = webView;
    m_proxy = proxy;
    m_accessibilityController->setWebView(webView);
    m_eventSender->setWebView(webView);
    // m_gamepadController doesn't depend on WebView.
    m_textInputController->setWebView(webView);
    m_testRunner->setWebView(webView, proxy);
}

void TestInterfaces::setDelegate(WebTestDelegate* delegate)
{
    m_accessibilityController->setDelegate(delegate);
    m_eventSender->setDelegate(delegate);
    m_gamepadController->setDelegate(delegate);
    // m_textInputController doesn't depend on WebTestDelegate.
    m_testRunner->setDelegate(delegate);
    m_delegate = delegate;
}

void TestInterfaces::bindTo(WebFrame* frame)
{
    m_accessibilityController->bindToJavascript(frame, WebString::fromUTF8("accessibilityController"));
    m_eventSender->bindToJavascript(frame, WebString::fromUTF8("eventSender"));
    m_gamepadController->bindToJavascript(frame, WebString::fromUTF8("gamepadController"));
    m_textInputController->bindToJavascript(frame, WebString::fromUTF8("textInputController"));
    m_testRunner->bindToJavascript(frame, WebString::fromUTF8("testRunner"));
    m_testRunner->bindToJavascript(frame, WebString::fromUTF8("layoutTestController"));
}

void TestInterfaces::resetAll()
{
    m_accessibilityController->reset();
    m_eventSender->reset();
    m_gamepadController->reset();
    // m_textInputController doesn't have any state to reset.
    m_testRunner->reset();
    WebCache::clear();
}

void TestInterfaces::setTestIsRunning(bool running)
{
    m_testRunner->setTestIsRunning(running);
}

void TestInterfaces::configureForTestWithURL(const WebURL& testURL, bool generatePixels)
{
    string spec = GURL(testURL).spec();
    m_testRunner->setShouldGeneratePixelResults(generatePixels);
    if (spec.find("loading/") != string::npos)
        m_testRunner->setShouldDumpFrameLoadCallbacks(true);
    if (spec.find("/dumpAsText/") != string::npos) {
        m_testRunner->setShouldDumpAsText(true);
        m_testRunner->setShouldGeneratePixelResults(false);
    }
    if (spec.find("/inspector/") != string::npos)
        m_testRunner->showDevTools();
}

void TestInterfaces::windowOpened(WebTestProxyBase* proxy)
{
    m_windowList.push_back(proxy);
}

void TestInterfaces::windowClosed(WebTestProxyBase* proxy)
{
    vector<WebTestProxyBase*>::iterator pos = find(m_windowList.begin(), m_windowList.end(), proxy);
    if (pos == m_windowList.end()) {
        WEBKIT_ASSERT_NOT_REACHED();
        return;
    }
    m_windowList.erase(pos);
}

AccessibilityController* TestInterfaces::accessibilityController()
{
    return m_accessibilityController.get();
}

EventSender* TestInterfaces::eventSender()
{
    return m_eventSender.get();
}

TestRunner* TestInterfaces::testRunner()
{
    return m_testRunner.get();
}

WebView* TestInterfaces::webView()
{
    return m_webView;
}

WebTestDelegate* TestInterfaces::delegate()
{
    return m_delegate;
}

WebTestProxyBase* TestInterfaces::proxy()
{
    return m_proxy;
}

const vector<WebTestProxyBase*>& TestInterfaces::windowList()
{
    return m_windowList;
}

WebThemeEngine* TestInterfaces::themeEngine()
{
#if defined(USE_DEFAULT_RENDER_THEME) || !(defined(WIN32) || defined(__APPLE__))
    return 0;
#elif defined(WIN32)
    if (m_themeEngine.get())
        m_themeEngine.reset(new WebTestThemeEngineWin());
    return m_themeEngine.get();
#elif defined(__APPLE__)
    if (m_themeEngine.get())
        m_themeEngine.reset(new WebTestThemeEngineMac());
    return m_themeEngine.get();
#endif
}

}
