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

#include "AccessibilityController.h"
#include "GamepadController.h"
#include "TextInputController.h"
#include "platform/WebString.h"

#include <wtf/OwnPtr.h>

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebView;

class TestInterfaces::Internal {
public:
    Internal();
    ~Internal();

    void setWebView(WebView*);
    void setDelegate(TestDelegate*);
    void bindTo(WebFrame*);
    void resetAll();

    AccessibilityController* accessibilityController() { return m_accessibilityController.get(); }

private:
    OwnPtr<AccessibilityController> m_accessibilityController;
    OwnPtr<GamepadController> m_gamepadController;
    OwnPtr<TextInputController> m_textInputController;
};

TestInterfaces::Internal::Internal()
{
    m_accessibilityController = adoptPtr(new AccessibilityController());
    m_gamepadController = adoptPtr(new GamepadController());
    m_textInputController = adoptPtr(new TextInputController());
}

TestInterfaces::Internal::~Internal()
{
    m_accessibilityController->setWebView(0);
    // m_gamepadController doesn't depend on WebView.
    m_textInputController->setWebView(0);

    // m_accessibilityController doesn't depend on TestDelegate.
    m_gamepadController->setDelegate(0);
    // m_textInputController doesn't depend on TestDelegate.
}

void TestInterfaces::Internal::setWebView(WebView* webView)
{
    m_accessibilityController->setWebView(webView);
    // m_gamepadController doesn't depend on WebView.
    m_textInputController->setWebView(webView);
}

void TestInterfaces::Internal::setDelegate(TestDelegate* delegate)
{
    // m_accessibilityController doesn't depend on TestDelegate.
    m_gamepadController->setDelegate(delegate);
    // m_textInputController doesn't depend on TestDelegate.
}

void TestInterfaces::Internal::bindTo(WebFrame* frame)
{
    m_accessibilityController->bindToJavascript(frame, WebString::fromUTF8("accessibilityController"));
    m_gamepadController->bindToJavascript(frame, WebString::fromUTF8("gamepadController"));
    m_textInputController->bindToJavascript(frame, WebString::fromUTF8("textInputController"));
}

void TestInterfaces::Internal::resetAll()
{
    m_accessibilityController->reset();
    m_gamepadController->reset();
    // m_textInputController doesn't have any state to reset.
}

TestInterfaces::TestInterfaces()
    : m_internal(new TestInterfaces::Internal())
{
}

TestInterfaces::~TestInterfaces()
{
    delete m_internal;
}

void TestInterfaces::setWebView(WebView* webView)
{
    m_internal->setWebView(webView);
}

void TestInterfaces::setDelegate(TestDelegate* delegate)
{
    m_internal->setDelegate(delegate);
}

void TestInterfaces::bindTo(WebFrame* frame)
{
    m_internal->bindTo(frame);
}

void TestInterfaces::resetAll()
{
    m_internal->resetAll();
}

AccessibilityController* TestInterfaces::accessibilityController()
{
    return m_internal->accessibilityController();
}
