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
#include "TextInputController.h"
#include <public/WebString.h>

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebView;

namespace WebTestRunner {

TestInterfaces::TestInterfaces()
{
    m_accessibilityController = adoptPtr(new AccessibilityController());
    m_eventSender = adoptPtr(new EventSender());
    m_gamepadController = adoptPtr(new GamepadController());
    m_textInputController = adoptPtr(new TextInputController());
}

TestInterfaces::~TestInterfaces()
{
    m_accessibilityController->setWebView(0);
    m_eventSender->setWebView(0);
    // m_gamepadController doesn't depend on WebView.
    m_textInputController->setWebView(0);

    m_accessibilityController->setDelegate(0);
    m_eventSender->setDelegate(0);
    m_gamepadController->setDelegate(0);
    // m_textInputController doesn't depend on TestDelegate.
}

void TestInterfaces::setWebView(WebView* webView)
{
    m_accessibilityController->setWebView(webView);
    m_eventSender->setWebView(webView);
    // m_gamepadController doesn't depend on WebView.
    m_textInputController->setWebView(webView);
}

void TestInterfaces::setDelegate(TestDelegate* delegate)
{
    m_accessibilityController->setDelegate(delegate);
    m_eventSender->setDelegate(delegate);
    m_gamepadController->setDelegate(delegate);
    // m_textInputController doesn't depend on TestDelegate.
}

void TestInterfaces::bindTo(WebFrame* frame)
{
    m_accessibilityController->bindToJavascript(frame, WebString::fromUTF8("accessibilityController"));
    m_eventSender->bindToJavascript(frame, WebString::fromUTF8("eventSender"));
    m_gamepadController->bindToJavascript(frame, WebString::fromUTF8("gamepadController"));
    m_textInputController->bindToJavascript(frame, WebString::fromUTF8("textInputController"));
}

void TestInterfaces::resetAll()
{
    m_accessibilityController->reset();
    m_eventSender->reset();
    m_gamepadController->reset();
    // m_textInputController doesn't have any state to reset.
}

AccessibilityController* TestInterfaces::accessibilityController()
{
    return m_accessibilityController.get();
}

EventSender* TestInterfaces::eventSender()
{
    return m_eventSender.get();
}

}
