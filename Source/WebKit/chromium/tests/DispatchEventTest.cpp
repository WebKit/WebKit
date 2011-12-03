/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "FrameTestHelpers.h"
#include "WebDOMEvent.h"
#include "WebDOMEventListener.h"
#include "WebDOMMessageEvent.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFrame.h"
#include "WebSerializedScriptValue.h"
#include "WebView.h"
#include <gtest/gtest.h>
#include <utility>
#include <vector>
#include <webkit/support/webkit_support.h>

using namespace WebKit;

namespace {

class MockListener : public WebDOMEventListener {
public:
    MockListener() { }
    virtual ~MockListener() { }
    typedef std::pair<WebDOMEvent, WebDOMEvent::PhaseType> EventPhasePair;

    std::vector<EventPhasePair>& events()
    {
        return m_events;
    }

    virtual void handleEvent(const WebDOMEvent& event) OVERRIDE
    {
        m_events.push_back(EventPhasePair(event, event.eventPhase()));
    }
private:
    std::vector<EventPhasePair> m_events;
};

class DispatchEventTest : public testing::Test {
public:
    DispatchEventTest()
        : m_baseURL("http://www.test.com/"),
          m_view(0),
          m_frame(0)
    {
        FrameTestHelpers::registerMockedURLLoad(m_baseURL, "event_target.html");
        m_view = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "event_target.html");
        m_frame = m_view->mainFrame();
        m_document = m_frame->document();
        m_target = m_document.getElementById("event_target");
        m_targetParent = m_document.getElementById("targetParent");
     }

    virtual void TearDown()
    {
        m_view->close();
        webkit_support::UnregisterAllMockedURLs();
    }

protected:
    // Create, initialize, and return a MessageEvent.
    WebDOMEvent createMessageEvent()
    {
        WebDOMEvent event = m_document.createEvent("MessageEvent");
        WebDOMMessageEvent messageEvent = event.to<WebDOMMessageEvent>();
        messageEvent.initMessageEvent("message", false, false, WebSerializedScriptValue(), "origin", m_frame, "lastId");
        return messageEvent;
    }

    std::string m_baseURL;
    WebView* m_view;
    WebFrame* m_frame;
    WebDocument m_document;
    WebElement m_target;
    WebElement m_targetParent;
};

TEST_F(DispatchEventTest, BasicDispatch)
{
    MockListener listener;
    m_target.addEventListener("message", &listener, false);
    m_target.dispatchEvent(createMessageEvent());

    ASSERT_EQ(1u, listener.events().size());
    const WebDOMEvent& receivedEvent = listener.events().back().first;
    EXPECT_EQ("message", receivedEvent.type());
    EXPECT_EQ(false, receivedEvent.bubbles());
    EXPECT_EQ(false, receivedEvent.cancelable());
    EXPECT_EQ(true, receivedEvent.isMessageEvent());
    EXPECT_EQ(WebDOMEvent::AtTarget, listener.events().back().second);
    listener.events().clear();

    // Remove the event listener. Make sure we don't receive anything.
    m_target.removeEventListener("message", &listener, false);
    m_target.dispatchEvent(createMessageEvent());
    EXPECT_EQ(0u, listener.events().size());
}

TEST_F(DispatchEventTest, EventCapture)
{
    MockListener listener;
    MockListener capturer;
    m_target.addEventListener("message", &listener, false);
    m_targetParent.addEventListener("message", &capturer, true);
    m_target.dispatchEvent(createMessageEvent());

    // The message should be received by both listeners.
    ASSERT_EQ(1u, listener.events().size());
    ASSERT_EQ(1u, capturer.events().size());
    EXPECT_EQ(WebDOMEvent::AtTarget, listener.events().back().second);
    EXPECT_EQ(WebDOMEvent::CapturingPhase, capturer.events().back().second);
    listener.events().clear();
    capturer.events().clear();

    // Remove the capturing listener. Make sure the normal listener gets events
    // and the capturing one does not.
    m_targetParent.removeEventListener("message", &capturer, true);
    m_target.dispatchEvent(createMessageEvent());
    EXPECT_EQ(0u, capturer.events().size());
    ASSERT_EQ(1u, listener.events().size());
    EXPECT_EQ(WebDOMEvent::AtTarget, listener.events().back().second);
    listener.events().clear();
    capturer.events().clear();

    // Remove listener too and make sure nobody gets events.
    m_target.removeEventListener("message", &listener, false);
    m_target.dispatchEvent(createMessageEvent());
    EXPECT_EQ(0u, capturer.events().size());
    EXPECT_EQ(0u, listener.events().size());
}

}
