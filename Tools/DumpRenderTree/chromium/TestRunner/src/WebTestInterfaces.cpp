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
#include "WebTestInterfaces.h"

#include "TestInterfaces.h"
#include "WebAccessibilityController.h"
#include "WebEventSender.h"
#include "WebTestDelegate.h"
#include "WebTestRunner.h"

using WebKit::WebContextMenuData;
using WebKit::WebFrame;
using WebKit::WebGamepads;
using WebKit::WebString;
using WebKit::WebVector;
using WebKit::WebView;

namespace WebTestRunner {

class WebTestInterfaces::Internal {
public:
    Internal();
    virtual ~Internal();

    TestInterfaces* testInterfaces() { return &m_interfaces; }
    void setDelegate(WebTestDelegate*);
    void setWebView(WebView*);
    void setTestIsRunning(bool);
    WebView* webView() const { return m_webView; }
    WebAccessibilityController* accessibilityController() { return &m_accessibilityController; }
    WebEventSender* eventSender() { return &m_eventSender; }
    WebTestRunner* testRunner() { return &m_testRunner; }

private:
    TestInterfaces m_interfaces;
    bool m_testIsRunning;
    WebView* m_webView;
    WebAccessibilityController m_accessibilityController;
    WebEventSender m_eventSender;
    WebTestRunner m_testRunner;
};

WebTestInterfaces::Internal::Internal()
    : m_testIsRunning(false)
    , m_webView(0)
    , m_accessibilityController(m_interfaces.accessibilityController())
    , m_eventSender(m_interfaces.eventSender())
    , m_testRunner(m_interfaces.testRunner())
{
}

WebTestInterfaces::Internal::~Internal()
{
}

void WebTestInterfaces::Internal::setDelegate(WebTestDelegate* delegate)
{
    m_interfaces.setDelegate(delegate);
}

void WebTestInterfaces::Internal::setWebView(WebView* webView)
{
    m_webView = webView;
    m_interfaces.setWebView(webView);
}

void WebTestInterfaces::Internal::setTestIsRunning(bool running)
{
    m_interfaces.setTestIsRunning(running);
}

WebTestInterfaces::WebTestInterfaces()
{
    m_internal = new Internal;
}

WebTestInterfaces::~WebTestInterfaces()
{
    delete m_internal;
}

void WebTestInterfaces::setWebView(WebView* webView)
{
    m_internal->setWebView(webView);
}

void WebTestInterfaces::setDelegate(WebTestDelegate* delegate)
{
    m_internal->setDelegate(delegate);
}

void WebTestInterfaces::bindTo(WebFrame* frame)
{
    m_internal->testInterfaces()->bindTo(frame);
}

void WebTestInterfaces::resetAll()
{
    m_internal->testInterfaces()->resetAll();
}

void WebTestInterfaces::setTestIsRunning(bool running)
{
    m_internal->setTestIsRunning(running);
}

WebView* WebTestInterfaces::webView() const
{
    return m_internal->webView();
}

WebAccessibilityController* WebTestInterfaces::accessibilityController()
{
    return m_internal->accessibilityController();
}

WebEventSender* WebTestInterfaces::eventSender()
{
    return m_internal->eventSender();
}

WebTestRunner* WebTestInterfaces::testRunner()
{
    return m_internal->testRunner();
}

}
