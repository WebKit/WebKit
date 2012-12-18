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

#include "TestDelegate.h"
#include "TestInterfaces.h"
#include "WebAccessibilityController.h"
#include "WebEventSender.h"
#include "WebTestDelegate.h"

using WebKit::WebContextMenuData;
using WebKit::WebFrame;
using WebKit::WebGamepads;
using WebKit::WebString;
using WebKit::WebVector;
using WebKit::WebView;

namespace WebTestRunner {

class WebTestInterfaces::Internal : public TestDelegate {
public:
    Internal();
    virtual ~Internal();

    TestInterfaces* testInterfaces() { return &m_interfaces; }
    void setDelegate(WebTestDelegate*);
    WebAccessibilityController* accessibilityController() { return &m_accessibilityController; }
    WebEventSender* eventSender() { return &m_eventSender; }
    WebTestRunner* testRunner() { return m_testRunner; }
    void setTestRunner(WebTestRunner* testRunner) { m_testRunner = testRunner; }

    // TestDelegate implementation.
    virtual void clearContextMenuData();
    virtual void clearEditCommand();
    virtual void fillSpellingSuggestionList(const WebString& word, WebVector<WebString>* suggestions);
    virtual void setEditCommand(const std::string& name, const std::string& value);
    virtual WebContextMenuData* lastContextMenuData() const;
    virtual void setGamepadData(const WebGamepads&);
    virtual void printMessage(const std::string& message);
    virtual void postTask(WebTask*);
    virtual void postDelayedTask(WebTask*, long long ms);
    virtual WebString registerIsolatedFileSystem(const WebVector<WebString>& absoluteFilenames);
    virtual long long getCurrentTimeInMillisecond();
    virtual WebKit::WebString getAbsoluteWebStringFromUTF8Path(const std::string& path);

private:
    TestInterfaces m_interfaces;
    WebAccessibilityController m_accessibilityController;
    WebEventSender m_eventSender;
    WebTestRunner* m_testRunner;
    WebTestDelegate* m_delegate;
};

WebTestInterfaces::Internal::Internal()
    : m_accessibilityController(m_interfaces.accessibilityController())
    , m_eventSender(m_interfaces.eventSender())
    , m_testRunner(0)
    , m_delegate(0)
{
}

WebTestInterfaces::Internal::~Internal()
{
}

void WebTestInterfaces::Internal::setDelegate(WebTestDelegate* delegate)
{
    if (delegate) {
        m_delegate = delegate;
        m_interfaces.setDelegate(this);
    } else {
        m_delegate = 0;
        m_interfaces.setDelegate(0);
    }
}

void WebTestInterfaces::Internal::clearContextMenuData()
{
    m_delegate->clearContextMenuData();
}

void WebTestInterfaces::Internal::clearEditCommand()
{
    m_delegate->clearEditCommand();
}

void WebTestInterfaces::Internal::fillSpellingSuggestionList(const WebString& word, WebVector<WebString>* suggestions)
{
    m_delegate->fillSpellingSuggestionList(word, suggestions);
}

void WebTestInterfaces::Internal::setEditCommand(const std::string& name, const std::string& value)
{
    m_delegate->setEditCommand(name, value);
}

WebContextMenuData* WebTestInterfaces::Internal::lastContextMenuData() const
{
    return m_delegate->lastContextMenuData();
}

void WebTestInterfaces::Internal::setGamepadData(const WebGamepads& pads)
{
    m_delegate->setGamepadData(pads);
}

void WebTestInterfaces::Internal::printMessage(const std::string& message)
{
    m_delegate->printMessage(message);
}

void WebTestInterfaces::Internal::postTask(WebTask* task)
{
    m_delegate->postTask(task);
}

void WebTestInterfaces::Internal::postDelayedTask(WebTask* task, long long ms)
{
    m_delegate->postDelayedTask(task, ms);
}

WebString WebTestInterfaces::Internal::registerIsolatedFileSystem(const WebVector<WebString>& absoluteFilenames)
{
    return m_delegate->registerIsolatedFileSystem(absoluteFilenames);
}

long long WebTestInterfaces::Internal::getCurrentTimeInMillisecond()
{
    return m_delegate->getCurrentTimeInMillisecond();
}

WebKit::WebString WebTestInterfaces::Internal::getAbsoluteWebStringFromUTF8Path(const std::string& path)
{
    return m_delegate->getAbsoluteWebStringFromUTF8Path(path);
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
    m_internal->testInterfaces()->setWebView(webView);
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

void WebTestInterfaces::setTestRunner(WebTestRunner* testRunner)
{
    m_internal->setTestRunner(testRunner);
}

}
