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
#include "URLTestHelpers.h"
#include "WebCache.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFrame.h"
#include "WebNode.h"
#include "WebNodeList.h"
#include "WebPrerendererClient.h"
#include "WebScriptSource.h"
#include "WebView.h"
#include "WebViewClient.h"

#include <gtest/gtest.h>
#include <list>
#include <public/WebPrerender.h>
#include <public/WebPrerenderingSupport.h>
#include <public/WebString.h>
#include <webkit/support/webkit_support.h>
#include <wtf/OwnPtr.h>

using namespace WebKit;
using WebKit::URLTestHelpers::toKURL;

namespace {

WebURL toWebURL(const char* url)
{
    return WebURL(toKURL(url));
}

class TestPrerendererClient : public WebPrerendererClient {
public:
    TestPrerendererClient() { }
    virtual ~TestPrerendererClient() { }

    void setExtraDataForNextPrerender(WebPrerender::ExtraData* extraData)
    {
        ASSERT(!m_extraData);
        m_extraData = adoptPtr(extraData);
    }

    WebPrerender releaseWebPrerender()
    {
        ASSERT(!m_webPrerenders.empty());
        WebPrerender retval(m_webPrerenders.front());
        m_webPrerenders.pop_front();
        return retval;
    }
    
    bool empty() const
    {
        return m_webPrerenders.empty();
    }

    void clear()
    {
        m_webPrerenders.clear();
    }

private:
    // From WebPrerendererClient:
    virtual void willAddPrerender(WebPrerender* prerender) OVERRIDE
    {
        prerender->setExtraData(m_extraData.leakPtr());

        ASSERT(!prerender->isNull());
        m_webPrerenders.push_back(*prerender);
    }

    OwnPtr<WebPrerender::ExtraData> m_extraData;
    std::list<WebPrerender> m_webPrerenders;
};

class TestPrerenderingSupport : public WebPrerenderingSupport {
public:
    TestPrerenderingSupport()
    {
        initialize(this);
    }

    virtual ~TestPrerenderingSupport()
    {
        shutdown();
    }

    void clear()
    {
        m_addedPrerenders.clear();
        m_canceledPrerenders.clear();
        m_abandonedPrerenders.clear();
    }

    size_t totalCount() const
    {
        return m_addedPrerenders.size() + m_canceledPrerenders.size() + m_abandonedPrerenders.size();
    }
    
    size_t addCount(const WebPrerender& prerender) const
    {
        return std::count_if(m_addedPrerenders.begin(), m_addedPrerenders.end(), std::bind1st(WebPrerenderEqual(), prerender));
    }

    size_t cancelCount(const WebPrerender& prerender) const
    {
        return std::count_if(m_canceledPrerenders.begin(), m_canceledPrerenders.end(), std::bind1st(WebPrerenderEqual(), prerender));
    }

    size_t abandonCount(const WebPrerender& prerender) const
    {
        return std::count_if(m_abandonedPrerenders.begin(), m_abandonedPrerenders.end(), std::bind1st(WebPrerenderEqual(), prerender));
    }

private:
    class WebPrerenderEqual : public std::binary_function<WebPrerender, WebPrerender, bool> {
    public:
        bool operator()(const WebPrerender& first, const WebPrerender& second) const
        {
            return first.toPrerender() == second.toPrerender();
        }
    };

    // From WebPrerenderingSupport:
    virtual void add(const WebPrerender& prerender) OVERRIDE
    {
        m_addedPrerenders.push_back(prerender);
    }

    virtual void cancel(const WebPrerender& prerender) OVERRIDE
    {
        m_canceledPrerenders.push_back(prerender);
    }

    virtual void abandon(const WebPrerender& prerender) OVERRIDE
    {
        m_abandonedPrerenders.push_back(prerender);
    }

    std::vector<WebPrerender> m_addedPrerenders;
    std::vector<WebPrerender> m_canceledPrerenders;
    std::vector<WebPrerender> m_abandonedPrerenders;
};

class PrerenderingTest : public testing::Test {
public:
    PrerenderingTest() : m_webView(0)
    {
    }

    ~PrerenderingTest()
    {
        webkit_support::UnregisterAllMockedURLs();
        if (m_webView)
            close();
    }
    
    void initialize(const char* baseURL, const char* fileName)
    {
        ASSERT(!m_webView);
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL), WebString::fromUTF8(fileName));
        const bool RunJavascript = true;
        m_webView = FrameTestHelpers::createWebView(RunJavascript);
        m_webView->setPrerendererClient(&m_prerendererClient);

        FrameTestHelpers::loadFrame(m_webView->mainFrame(), std::string(baseURL) + fileName);
        webkit_support::ServeAsynchronousMockedRequests();
    }

    void navigateAway()
    {
        FrameTestHelpers::loadFrame(m_webView->mainFrame(), "about:blank");
        //        webkit_support::RunAllPendingMessages();
    }

    void close()
    {
        ASSERT(m_webView);

        m_webView->mainFrame()->collectGarbage();

        m_webView->close();
        m_webView = 0;

        WebCache::clear();
    }

    WebElement console()
    {
        WebElement console = m_webView->mainFrame()->document().getElementById("console");
        ASSERT(console.nodeName() == "UL");
        return console;
    }

    unsigned consoleLength()
    {
        return console().childNodes().length() - 1;
    }

    std::string consoleAt(unsigned i)
    {
        ASSERT(consoleLength() > i);

        WebNode consoleListItem = console().childNodes().item(1 + i);
        ASSERT(consoleListItem.nodeName() == "LI");
        ASSERT(consoleListItem.hasChildNodes());

        WebNode textNode = consoleListItem.firstChild();
        ASSERT(textNode.nodeName() == "#text");
        
        return textNode.nodeValue().utf8().data();
    }

    void executeScript(const char* code)
    {
        m_webView->mainFrame()->executeScript(WebScriptSource(WebString::fromUTF8(code)));
    }

    TestPrerenderingSupport* prerenderingSupport()
    {
        return &m_prerenderingSupport;
    }

    TestPrerendererClient* prerendererClient()
    {
        return &m_prerendererClient;
    }

private:
    TestPrerenderingSupport m_prerenderingSupport;
    TestPrerendererClient m_prerendererClient;

    WebView* m_webView;
};

TEST_F(PrerenderingTest, SinglePrerender)
{
    initialize("http://www.foo.com/", "prerender/single_prerender.html");

    WebPrerender webPrerender = prerendererClient()->releaseWebPrerender();
    EXPECT_FALSE(webPrerender.isNull());
    EXPECT_EQ(toWebURL("http://prerender.com/"), webPrerender.url());

    EXPECT_EQ(1u, prerenderingSupport()->addCount(webPrerender));
    EXPECT_EQ(1u, prerenderingSupport()->totalCount());

    webPrerender.didStartPrerender();
    EXPECT_EQ(1u, consoleLength());
    EXPECT_EQ("webkitprerenderstart", consoleAt(0));

    webPrerender.didSendDOMContentLoadedForPrerender();
    EXPECT_EQ(2u, consoleLength());
    EXPECT_EQ("webkitprerenderdomcontentloaded", consoleAt(1));

    webPrerender.didSendLoadForPrerender();
    EXPECT_EQ(3u, consoleLength());
    EXPECT_EQ("webkitprerenderload", consoleAt(2));

    webPrerender.didStopPrerender();
    EXPECT_EQ(4u, consoleLength());
    EXPECT_EQ("webkitprerenderstop", consoleAt(3));
}

TEST_F(PrerenderingTest, CancelPrerender)
{
    initialize("http://www.foo.com/", "prerender/single_prerender.html");

    WebPrerender webPrerender = prerendererClient()->releaseWebPrerender();
    EXPECT_FALSE(webPrerender.isNull());

    EXPECT_EQ(1u, prerenderingSupport()->addCount(webPrerender));
    EXPECT_EQ(1u, prerenderingSupport()->totalCount());

    executeScript("removePrerender()");
    
    EXPECT_EQ(1u, prerenderingSupport()->cancelCount(webPrerender));
    EXPECT_EQ(2u, prerenderingSupport()->totalCount());
}

TEST_F(PrerenderingTest, AbandonPrerender)
{
    initialize("http://www.foo.com/", "prerender/single_prerender.html");

    WebPrerender webPrerender = prerendererClient()->releaseWebPrerender();
    EXPECT_FALSE(webPrerender.isNull());

    EXPECT_EQ(1u, prerenderingSupport()->addCount(webPrerender));
    EXPECT_EQ(1u, prerenderingSupport()->totalCount());

    navigateAway();
    
    EXPECT_EQ(1u, prerenderingSupport()->abandonCount(webPrerender));
    EXPECT_EQ(2u, prerenderingSupport()->totalCount());
}

TEST_F(PrerenderingTest, ExtraData)
{
    class TestExtraData : public WebPrerender::ExtraData {
    public:
        explicit TestExtraData(bool* alive) : m_alive(alive)
        {
            *alive = true;
        }

        virtual ~TestExtraData() { *m_alive = false; }

    private:
        bool* m_alive;
    };

    bool alive = false;
    {
        prerendererClient()->setExtraDataForNextPrerender(new TestExtraData(&alive));
        initialize("http://www.foo.com/", "prerender/single_prerender.html");
        EXPECT_TRUE(alive);

        WebPrerender webPrerender = prerendererClient()->releaseWebPrerender();

        executeScript("removePrerender()");
        close();
        prerenderingSupport()->clear();
    }
    EXPECT_FALSE(alive);
}

TEST_F(PrerenderingTest, TwoPrerenders)
{
    initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");
    
    WebPrerender firstPrerender = prerendererClient()->releaseWebPrerender();
    EXPECT_FALSE(firstPrerender.isNull());
    EXPECT_EQ(toWebURL("http://first-prerender.com/"), firstPrerender.url());

    WebPrerender secondPrerender = prerendererClient()->releaseWebPrerender();
    EXPECT_FALSE(firstPrerender.isNull());
    EXPECT_EQ(toWebURL("http://second-prerender.com/"), secondPrerender.url());

    EXPECT_EQ(1u, prerenderingSupport()->addCount(firstPrerender));
    EXPECT_EQ(1u, prerenderingSupport()->addCount(secondPrerender));
    EXPECT_EQ(2u, prerenderingSupport()->totalCount());

    firstPrerender.didStartPrerender();
    EXPECT_EQ(1u, consoleLength());
    EXPECT_EQ("first_webkitprerenderstart", consoleAt(0));

    secondPrerender.didStartPrerender();
    EXPECT_EQ(2u, consoleLength());
    EXPECT_EQ("second_webkitprerenderstart", consoleAt(1));
}

TEST_F(PrerenderingTest, TwoPrerendersRemovingFirstThenNavigating)
{
    initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");
    
    WebPrerender firstPrerender = prerendererClient()->releaseWebPrerender();
    WebPrerender secondPrerender = prerendererClient()->releaseWebPrerender();

    EXPECT_EQ(1u, prerenderingSupport()->addCount(firstPrerender));
    EXPECT_EQ(1u, prerenderingSupport()->addCount(secondPrerender));
    EXPECT_EQ(2u, prerenderingSupport()->totalCount());

    executeScript("removeFirstPrerender()");

    EXPECT_EQ(1u, prerenderingSupport()->cancelCount(firstPrerender));
    EXPECT_EQ(3u, prerenderingSupport()->totalCount());

    navigateAway();

    EXPECT_EQ(1u, prerenderingSupport()->abandonCount(secondPrerender));

    // FIXME: After we fix prerenders such that they don't send redundant events, assert that totalCount() == 4u.
}

TEST_F(PrerenderingTest, TwoPrerendersAddingThird)
{
    initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");
    
    WebPrerender firstPrerender = prerendererClient()->releaseWebPrerender();
    WebPrerender secondPrerender = prerendererClient()->releaseWebPrerender();

    EXPECT_EQ(1u, prerenderingSupport()->addCount(firstPrerender));
    EXPECT_EQ(1u, prerenderingSupport()->addCount(secondPrerender));
    EXPECT_EQ(2u, prerenderingSupport()->totalCount());

    executeScript("addThirdPrerender()");

    WebPrerender thirdPrerender = prerendererClient()->releaseWebPrerender();
    EXPECT_EQ(1u, prerenderingSupport()->addCount(thirdPrerender));
    EXPECT_EQ(3u, prerenderingSupport()->totalCount());
}

TEST_F(PrerenderingTest, ShortLivedClient)
{
    initialize("http://www.foo.com/", "prerender/single_prerender.html");

    WebPrerender webPrerender = prerendererClient()->releaseWebPrerender();
    EXPECT_FALSE(webPrerender.isNull());

    EXPECT_EQ(1u, prerenderingSupport()->addCount(webPrerender));
    EXPECT_EQ(1u, prerenderingSupport()->totalCount());

    navigateAway();
    close();

    // This test passes if this next line doesn't crash.
    webPrerender.didStartPrerender();
}

} // namespace
