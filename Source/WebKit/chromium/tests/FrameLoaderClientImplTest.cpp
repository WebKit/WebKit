/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
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

#include "FrameLoaderClientImpl.h"

#include "FrameLoader.h"
#include "KURL.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebView.h"

#include <gtest/gtest.h>
#include <wtf/text/WTFString.h>

using namespace WebKit;

namespace {

class TestWebFrameClient : public WebFrameClient {
public:
    WebString userAgentOverride(WebFrame* frame, const WebURL& url) OVERRIDE
    {
        if (m_userAgentOverride.isEmpty())
            return WebString();

        return m_userAgentOverride;
    }

    void setUserAgentOverride(const WebString& userAgent)
    {
        m_userAgentOverride = userAgent;
    }

private:
    WebString m_userAgentOverride;
};

class FrameLoaderClientImplTest : public testing::Test {
public:
    void SetUp()
    {
        m_webView = WebView::create(0);
        m_webView->initializeMainFrame(&m_webFrameClient);
        WebFrameImpl* frame = static_cast<WebFrameImpl*>(m_webView->mainFrame());
        m_frameLoaderClientImpl = static_cast<FrameLoaderClientImpl*>(frame->frame()->loader()->client());
    }

    void TearDown()
    {
        m_webView->close();
    }

    void setUserAgentOverride(const WebString& userAgent)
    {
        return m_webFrameClient.setUserAgentOverride(userAgent);
    }

    const WebString userAgent()
    {
        // The test always returns the same user agent, regardless of the URL passed in.
        WebCore::KURL dummyURL(WebCore::ParsedURLString, "about:blank");
        WTF::CString userAgent = m_frameLoaderClientImpl->userAgent(dummyURL).utf8();
        return WebString::fromUTF8(userAgent.data(), userAgent.length());
    }

protected:
    TestWebFrameClient m_webFrameClient;
    FrameLoaderClientImpl* m_frameLoaderClientImpl;
    WebView* m_webView;
};

TEST_F(FrameLoaderClientImplTest, UserAgentOverride)
{
    const WebString defaultUserAgent = userAgent();
    const WebString override = WebString::fromUTF8("dummy override");

    // Override the user agent and make sure we get it back.
    setUserAgentOverride(override);
    EXPECT_TRUE(override.equals(userAgent()));

    // Remove the override and make sure we get the original back.
    setUserAgentOverride(WebString());
    EXPECT_TRUE(defaultUserAgent.equals(userAgent()));
}

} // namespace
