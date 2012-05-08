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

#include "FrameTestHelpers.h"
#include "WebDOMEvent.h"
#include "WebDOMEventListener.h"
#include "WebDOMMutationEvent.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFrame.h"
#include "WebScriptSource.h"
#include "WebView.h"
#include <gtest/gtest.h>
#include <webkit/support/webkit_support.h>

using namespace WebKit;

namespace {

class TestWebDOMEventListener : public WebDOMEventListener {
public:
    TestWebDOMEventListener() { }
    virtual ~TestWebDOMEventListener() { }

    virtual void handleEvent(const WebDOMEvent& event) OVERRIDE
    {
      m_events.push_back(event);
    }

    size_t eventCount() const { return m_events.size(); }

    WebDOMEvent eventAt(int index) const { return m_events.at(index); }

    void clearEvents() { m_events.clear(); }

private:
    std::vector<WebDOMEvent> m_events;
};

class WebDOMEventListenerTest : public testing::Test {
public:
    WebDOMEventListenerTest()
        : m_webView(0)
    {
    }

    virtual void OVERRIDE SetUp()
    {
        std::string baseURL("http://www.example.com/");
        std::string fileName("listener/mutation_event_listener.html");
        bool executeScript = true;
        FrameTestHelpers::registerMockedURLLoad(baseURL, fileName);
        m_webView = FrameTestHelpers::createWebViewAndLoad(baseURL + fileName, executeScript);
    }

    virtual void OVERRIDE TearDown()
    {
        m_webView->close();
        webkit_support::UnregisterAllMockedURLs();
    }

    WebFrame* mainFrame() const
    {
        return m_webView->mainFrame();
    }

    WebDocument document() const
    {
        return mainFrame()->document();
    }

    void executeScript(const char* code)
    {
        mainFrame()->executeScript(WebScriptSource(WebString::fromUTF8(code)));
    }


    static WebString GetNodeID(const WebNode& node)
    {
        if (node.nodeType() != WebNode::ElementNode)
            return WebString();
        WebElement element = node.toConst<WebElement>();
        return element.getAttribute("id");
    }

protected:
    WebView* m_webView;
};


// Tests that the right mutation events are fired when a node is added/removed.
// Note that the DOMSubtreeModified event is fairly vage, it only tells you
// something changed for the target node.
TEST_F(WebDOMEventListenerTest, NodeAddedRemovedMutationEvent)
{
    TestWebDOMEventListener eventListener;
    document().addEventListener("DOMSubtreeModified", &eventListener, false);

    // Test adding a node.
    executeScript("addElement('newNode')");
    ASSERT_EQ(1U, eventListener.eventCount());
    WebDOMEvent event = eventListener.eventAt(0);
    ASSERT_TRUE(event.isMutationEvent());
    // No need to check any of the MutationEvent, WebKit does not set any.
    EXPECT_EQ("DIV", event.target().nodeName());
    EXPECT_EQ("topDiv", GetNodeID(event.target()));
    eventListener.clearEvents();

    // Test removing a node.
    executeScript("removeNode('div1')");
    ASSERT_EQ(1U, eventListener.eventCount());
    event = eventListener.eventAt(0);
    ASSERT_TRUE(event.isMutationEvent());
    EXPECT_EQ("DIV", event.target().nodeName());
    EXPECT_EQ("topDiv", GetNodeID(event.target()));
}

// Tests the right mutation event is fired when a text node is modified.
TEST_F(WebDOMEventListenerTest, TextNodeModifiedMutationEvent)
{
    TestWebDOMEventListener eventListener;
    document().addEventListener("DOMSubtreeModified", &eventListener, false);
    executeScript("changeText('div2', 'Hello')");
    ASSERT_EQ(1U, eventListener.eventCount());
    WebDOMEvent event = eventListener.eventAt(0);
    ASSERT_TRUE(event.isMutationEvent());
    ASSERT_EQ(WebNode::TextNode, event.target().nodeType());
}

// Tests the right mutation events are fired when an attribute is added/removed.
TEST_F(WebDOMEventListenerTest, AttributeMutationEvent)
{
    TestWebDOMEventListener eventListener;
    document().addEventListener("DOMSubtreeModified", &eventListener, false);
    executeScript("document.getElementById('div2').setAttribute('myAttr',"
                  "'some value')");
    ASSERT_EQ(1U, eventListener.eventCount());
    WebDOMEvent event = eventListener.eventAt(0);
    ASSERT_TRUE(event.isMutationEvent());
    EXPECT_EQ("DIV", event.target().nodeName());
    EXPECT_EQ("div2", GetNodeID(event.target()));
    eventListener.clearEvents();

    executeScript("document.getElementById('div2').removeAttribute('myAttr')");
    ASSERT_EQ(1U, eventListener.eventCount());
    event = eventListener.eventAt(0);
    ASSERT_TRUE(event.isMutationEvent());
    EXPECT_EQ("DIV", event.target().nodeName());
    EXPECT_EQ("div2", GetNodeID(event.target()));
}

// Tests destroying WebDOMEventListener and triggering events, we shouldn't
// crash.
TEST_F(WebDOMEventListenerTest, FireEventDeletedListener)
{
    TestWebDOMEventListener* eventListener = new TestWebDOMEventListener();
    document().addEventListener("DOMSubtreeModified", eventListener, false);
    delete eventListener;
    executeScript("addElement('newNode')"); // That should fire an event.
}

// Tests registering several events on the same WebDOMEventListener and
// triggering events.
TEST_F(WebDOMEventListenerTest, SameListenerMultipleEvents)
{
    TestWebDOMEventListener eventListener;
    const WebString kDOMSubtreeModifiedType("DOMSubtreeModified");
    const WebString kDOMNodeRemovedType("DOMNodeRemoved");
    document().addEventListener(kDOMSubtreeModifiedType, &eventListener, false);
    WebElement div1Elem = document().getElementById("div1");
    div1Elem.addEventListener(kDOMNodeRemovedType, &eventListener, false);

    // Trigger a DOMSubtreeModified event by adding a node.
    executeScript("addElement('newNode')");
    ASSERT_EQ(1U, eventListener.eventCount());
    WebDOMEvent event = eventListener.eventAt(0);
    ASSERT_TRUE(event.isMutationEvent());
    EXPECT_EQ("DIV", event.target().nodeName());
    EXPECT_EQ("topDiv", GetNodeID(event.target()));
    eventListener.clearEvents();

    // Trigger for both event listener by removing the div1 node.
    executeScript("removeNode('div1')");
    ASSERT_EQ(2U, eventListener.eventCount());
    // Not sure if the order of the events is important. Assuming no specific
    // order.
    WebString type1 = eventListener.eventAt(0).type();
    WebString type2 = eventListener.eventAt(1).type();
    EXPECT_TRUE(type1 == kDOMSubtreeModifiedType || type1 == kDOMNodeRemovedType);
    EXPECT_TRUE(type2 == kDOMSubtreeModifiedType || type2 == kDOMNodeRemovedType);
    EXPECT_TRUE(type1 != type2);
}

// Tests removing event listeners.
TEST_F(WebDOMEventListenerTest, RemoveEventListener)
{
    TestWebDOMEventListener eventListener;
    const WebString kDOMSubtreeModifiedType("DOMSubtreeModified");
    // Adding twice the same listener for the same event, should be supported.
    document().addEventListener(kDOMSubtreeModifiedType, &eventListener, false);
    document().addEventListener(kDOMSubtreeModifiedType, &eventListener, false);

    // Add a node, that should trigger 2 events.
    executeScript("addElement('newNode')");
    EXPECT_EQ(2U, eventListener.eventCount());
    eventListener.clearEvents();

    // Remove one listener and trigger an event again.
    document().removeEventListener(
        kDOMSubtreeModifiedType, &eventListener, false);
    executeScript("addElement('newerNode')");
    EXPECT_EQ(1U, eventListener.eventCount());
    eventListener.clearEvents();

    // Remove the last listener and trigger yet another event.
    document().removeEventListener(
        kDOMSubtreeModifiedType, &eventListener, false);
    executeScript("addElement('newererNode')");
    EXPECT_EQ(0U, eventListener.eventCount());
}

} // namespace
