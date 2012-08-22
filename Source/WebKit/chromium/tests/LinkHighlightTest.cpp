/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "LinkHighlight.h"

#include "FrameTestHelpers.h"
#include "IntRect.h"
#include "Node.h"
#include "URLTestHelpers.h"
#include "WebFrame.h"
#include "WebViewImpl.h"
#include <gtest/gtest.h>
#include <public/WebCompositor.h>
#include <public/WebContentLayer.h>
#include <public/WebFloatPoint.h>
#include <public/WebSize.h>
#include <wtf/PassOwnPtr.h>

using namespace WebKit;
using namespace WebCore;

namespace {

#if ENABLE(GESTURE_EVENTS)
TEST(LinkHighlightTest, verifyWebViewImplIntegration)
{
    WebCompositor::initialize(0);

    const std::string baseURL("http://www.test.com/");
    const std::string fileName("test_touch_link_highlight.html");

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8("test_touch_link_highlight.html"));
    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(baseURL + fileName, true));
    int pageWidth = 640;
    int pageHeight = 480;
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->layout();

    // The coordinates below are linked to absolute positions in the referenced .html file.
    IntPoint touchEventLocation(20, 20);
    Node* touchNode = webViewImpl->bestTouchLinkNode(touchEventLocation);
    ASSERT_TRUE(touchNode);

    touchEventLocation = IntPoint(20, 40);
    EXPECT_FALSE(webViewImpl->bestTouchLinkNode(touchEventLocation));

    touchEventLocation = IntPoint(20, 20);
    // Shouldn't crash.

    webViewImpl->enableTouchHighlight(touchEventLocation);
    EXPECT_TRUE(webViewImpl->linkHighlight());
    EXPECT_TRUE(webViewImpl->linkHighlight()->contentLayer());
    EXPECT_TRUE(webViewImpl->linkHighlight()->clipLayer());

    // Find a target inside a scrollable div

    touchEventLocation = IntPoint(20, 100);
    webViewImpl->enableTouchHighlight(touchEventLocation);
    ASSERT_TRUE(webViewImpl->linkHighlight());

    webViewImpl->close();
    WebCompositor::shutdown();
}
#endif

} // namespace
