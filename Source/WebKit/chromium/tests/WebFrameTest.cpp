/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "WebFrame.h"

#include "FloatRect.h"
#include "Frame.h"
#include "FrameTestHelpers.h"
#include "FrameView.h"
#include "Range.h"
#include "ResourceError.h"
#include "Settings.h"
#include "URLTestHelpers.h"
#include "WebDataSource.h"
#include "WebDocument.h"
#include "WebFindOptions.h"
#include "WebFormElement.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebHistoryItem.h"
#include "WebRange.h"
#include "WebScriptSource.h"
#include "WebSearchableFormData.h"
#include "WebSecurityOrigin.h"
#include "WebSecurityPolicy.h"
#include "WebSettings.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "v8.h"
#include <gtest/gtest.h>
#include <public/WebFloatRect.h>
#include <public/WebURLResponse.h>
#include <webkit/support/webkit_support.h>

using namespace WebKit;
using WebCore::FloatRect;
using WebCore::Range;
using WebKit::URLTestHelpers::toKURL;

namespace {

class WebFrameTest : public testing::Test {
public:
    WebFrameTest()
        : m_baseURL("http://www.test.com/"),
          m_chromeURL("chrome://")
    {
    }

    virtual void TearDown()
    {
        webkit_support::UnregisterAllMockedURLs();
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(fileName.c_str()));
    }

    void registerMockedChromeURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_chromeURL.c_str()), WebString::fromUTF8(fileName.c_str()));
    }

protected:
    std::string m_baseURL;
    std::string m_chromeURL;
};

TEST_F(WebFrameTest, ContentText)
{
    registerMockedHttpURLLoad("iframes_test.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    registerMockedHttpURLLoad("invisible_iframe.html");
    registerMockedHttpURLLoad("zero_sized_iframe.html");

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframes_test.html");

    // Now retrieve the frames text and test it only includes visible elements.
    std::string content = std::string(webView->mainFrame()->contentAsText(1024).utf8().data());
    EXPECT_NE(std::string::npos, content.find(" visible paragraph"));
    EXPECT_NE(std::string::npos, content.find(" visible iframe"));
    EXPECT_EQ(std::string::npos, content.find(" invisible pararaph"));
    EXPECT_EQ(std::string::npos, content.find(" invisible iframe"));
    EXPECT_EQ(std::string::npos, content.find("iframe with zero size"));

    webView->close();
}

TEST_F(WebFrameTest, FrameForEnteredContext)
{
    registerMockedHttpURLLoad("iframes_test.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    registerMockedHttpURLLoad("invisible_iframe.html");
    registerMockedHttpURLLoad("zero_sized_iframe.html");

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframes_test.html", true);

    v8::HandleScope scope;
    EXPECT_EQ(webView->mainFrame(),
              WebFrame::frameForContext(
                  webView->mainFrame()->mainWorldScriptContext()));
    EXPECT_EQ(webView->mainFrame()->firstChild(),
              WebFrame::frameForContext(
                  webView->mainFrame()->firstChild()->mainWorldScriptContext()));

    webView->close();
}

TEST_F(WebFrameTest, FormWithNullFrame)
{
    registerMockedHttpURLLoad("form.html");

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "form.html");

    WebVector<WebFormElement> forms;
    webView->mainFrame()->document().forms(forms);
    webView->close();

    EXPECT_EQ(forms.size(), 1U);

    // This test passes if this doesn't crash.
    WebSearchableFormData searchableDataForm(forms[0]);
}

TEST_F(WebFrameTest, ChromePageJavascript)
{
    registerMockedChromeURLLoad("history.html");
 
    // Pass true to enable JavaScript.
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_chromeURL + "history.html", true);

    // Try to run JS against the chrome-style URL.
    FrameTestHelpers::loadFrame(webView->mainFrame(), "javascript:document.body.appendChild(document.createTextNode('Clobbered'))");

    // Required to see any updates in contentAsText.
    webView->layout();

    // Now retrieve the frame's text and ensure it was modified by running javascript.
    std::string content = std::string(webView->mainFrame()->contentAsText(1024).utf8().data());
    EXPECT_NE(std::string::npos, content.find("Clobbered"));
}

TEST_F(WebFrameTest, ChromePageNoJavascript)
{
    registerMockedChromeURLLoad("history.html");

    /// Pass true to enable JavaScript.
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_chromeURL + "history.html", true);

    // Try to run JS against the chrome-style URL after prohibiting it.
    WebSecurityPolicy::registerURLSchemeAsNotAllowingJavascriptURLs("chrome");
    FrameTestHelpers::loadFrame(webView->mainFrame(), "javascript:document.body.appendChild(document.createTextNode('Clobbered'))");

    // Required to see any updates in contentAsText.
    webView->layout();

    // Now retrieve the frame's text and ensure it wasn't modified by running javascript.
    std::string content = std::string(webView->mainFrame()->contentAsText(1024).utf8().data());
    EXPECT_EQ(std::string::npos, content.find("Clobbered"));
}

TEST_F(WebFrameTest, DispatchMessageEventWithOriginCheck)
{
    registerMockedHttpURLLoad("postmessage_test.html");

    // Pass true to enable JavaScript.
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "postmessage_test.html", true);
    
    // Send a message with the correct origin.
    WebSecurityOrigin correctOrigin(WebSecurityOrigin::create(toKURL(m_baseURL)));
    WebDOMEvent event = webView->mainFrame()->document().createEvent("MessageEvent");
    WebDOMMessageEvent message = event.to<WebDOMMessageEvent>();
    WebSerializedScriptValue data(WebSerializedScriptValue::fromString("foo"));
    message.initMessageEvent("message", false, false, data, "http://origin.com", 0, "");
    webView->mainFrame()->dispatchMessageEventWithOriginCheck(correctOrigin, message);

    // Send another message with incorrect origin.
    WebSecurityOrigin incorrectOrigin(WebSecurityOrigin::create(toKURL(m_chromeURL)));
    webView->mainFrame()->dispatchMessageEventWithOriginCheck(incorrectOrigin, message);

    // Required to see any updates in contentAsText.
    webView->layout();

    // Verify that only the first addition is in the body of the page.
    std::string content = std::string(webView->mainFrame()->contentAsText(1024).utf8().data());
    EXPECT_NE(std::string::npos, content.find("Message 1."));
    EXPECT_EQ(std::string::npos, content.find("Message 2."));
}

#if ENABLE(VIEWPORT)

class FixedLayoutTestWebViewClient : public WebViewClient {
 public:
    virtual WebScreenInfo screenInfo() OVERRIDE { return m_screenInfo; }

    WebScreenInfo m_screenInfo;
};

TEST_F(WebFrameTest, DeviceScaleFactorUsesDefaultWithoutViewportTag)
{
    registerMockedHttpURLLoad("no_viewport_tag.html");

    int viewportWidth = 640;
    int viewportHeight = 480;

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 2;

    WebView* webView = static_cast<WebView*>(FrameTestHelpers::createWebViewAndLoad(m_baseURL + "no_viewport_tag.html", true, 0, &client));

    webView->settings()->setViewportEnabled(true);
    webView->enableFixedLayoutMode(true);
    webView->resize(WebSize(viewportWidth, viewportHeight));
    webView->layout();

    EXPECT_EQ(2, webView->deviceScaleFactor());

    // Device scale factor should be a component of page scale factor in fixed-layout, so a scale of 1 becomes 2.
    webView->setPageScaleFactorLimits(1, 2);
    EXPECT_EQ(2, webView->pageScaleFactor());

    // Force the layout to happen before leaving the test.
    webView->mainFrame()->contentAsText(1024).utf8();
}

TEST_F(WebFrameTest, FixedLayoutInitializeAtMinimumPageScale)
{
    registerMockedHttpURLLoad("fixed_layout.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(m_baseURL + "fixed_layout.html", true, 0, &client));
    webViewImpl->enableFixedLayoutMode(true);
    webViewImpl->settings()->setViewportEnabled(true);
    webViewImpl->resize(WebSize(viewportWidth, viewportHeight));

    int defaultFixedLayoutWidth = 980;
    float minimumPageScaleFactor = viewportWidth / (float) defaultFixedLayoutWidth;
    EXPECT_EQ(minimumPageScaleFactor, webViewImpl->pageScaleFactor());

    // Assume the user has pinch zoomed to page scale factor 2.
    float userPinchPageScaleFactor = 2;
    webViewImpl->setPageScaleFactorPreservingScrollOffset(userPinchPageScaleFactor);
    webViewImpl->mainFrameImpl()->frameView()->layout();

    // Make sure we don't reset to initial scale if the page continues to load.
    bool isNewNavigation;
    webViewImpl->didCommitLoad(&isNewNavigation, false);
    webViewImpl->didChangeContentsSize();
    EXPECT_EQ(userPinchPageScaleFactor, webViewImpl->pageScaleFactor());

    // Make sure we don't reset to initial scale if the viewport size changes.
    webViewImpl->resize(WebSize(viewportWidth, viewportHeight + 100));
    EXPECT_EQ(userPinchPageScaleFactor, webViewImpl->pageScaleFactor());
}

TEST_F(WebFrameTest, ScaleFactorShouldNotOscillate)
{
    registerMockedHttpURLLoad("scale_oscillate.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = static_cast<float>(1.325);
    int viewportWidth = 800;
    int viewportHeight = 1057;

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(m_baseURL + "scale_oscillate.html", true, 0, &client));
    webViewImpl->enableFixedLayoutMode(true);
    webViewImpl->settings()->setViewportEnabled(true);
    webViewImpl->resize(WebSize(viewportWidth, viewportHeight));
    webViewImpl->layout();
}

#endif

TEST_F(WebFrameTest, CanOverrideMaximumScaleFactor)
{
    registerMockedHttpURLLoad("no_scale_for_you.html");

    FixedLayoutTestWebViewClient client;
    client.m_screenInfo.deviceScaleFactor = 1;
    int viewportWidth = 640;
    int viewportHeight = 480;

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(m_baseURL + "no_scale_for_you.html", true, 0, &client));
    webViewImpl->enableFixedLayoutMode(true);
    webViewImpl->settings()->setViewportEnabled(true);
    webViewImpl->resize(WebSize(viewportWidth, viewportHeight));

    EXPECT_EQ(1.0f, webViewImpl->maximumPageScaleFactor());

    webViewImpl->setIgnoreViewportTagMaximumScale(true);

    EXPECT_EQ(4.0f, webViewImpl->maximumPageScaleFactor());
}

#if ENABLE(GESTURE_EVENTS)
void setScaleAndScrollAndLayout(WebKit::WebView* webView, WebPoint scroll, float scale)
{
    webView->setPageScaleFactor(scale, WebPoint(scroll.x, scroll.y));
    webView->layout();
}

TEST_F(WebFrameTest, DivAutoZoomParamsTest)
{
    registerMockedHttpURLLoad("get_scale_for_auto_zoom_into_div_test.html");

    int viewportWidth = 640;
    int viewportHeight = 480;
    WebKit::WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_scale_for_auto_zoom_into_div_test.html");
    webView->setDeviceScaleFactor(2.0f);
    webView->resize(WebSize(viewportWidth, viewportHeight));
    webView->setPageScaleFactorLimits(0.01f, 4);
    webView->enableFixedLayoutMode(true);
    webView->layout();

    WebRect wideDiv(200, 100, 400, 150);
    WebRect tallDiv(200, 300, 400, 800);
    WebRect doubleTapPointWide((wideDiv.x + 50) * webView->pageScaleFactor(),
        (wideDiv.y + 50) * webView->pageScaleFactor(), 0, 0);
    WebRect doubleTapPointTall((tallDiv.x + 50) * webView->pageScaleFactor(),
        (tallDiv.y + 50) * webView->pageScaleFactor(), 0, 0);
    float scale;
    WebPoint scroll;
    bool isAnchor;

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(webView);
    // Test double-tap zooming into wide div.
    webViewImpl->computeScaleAndScrollForHitRect(doubleTapPointWide, WebViewImpl::DoubleTap, scale, scroll, isAnchor);
    // The div should horizontally fill the screen (modulo margins), and
    // vertically centered (modulo integer rounding).
    EXPECT_NEAR(viewportWidth / (float) wideDiv.width, scale, 0.1);
    EXPECT_NEAR(wideDiv.x * scale, scroll.x, 20);
    int vScroll = (wideDiv.y + wideDiv.height / 2) * scale - (viewportHeight / 2);
    EXPECT_NEAR(vScroll, scroll.y, 1);
    EXPECT_FALSE(isAnchor);

    setScaleAndScrollAndLayout(webViewImpl, scroll, scale);

    // Test zoom out back to minimum scale.
    webViewImpl->computeScaleAndScrollForHitRect(doubleTapPointWide, WebViewImpl::DoubleTap, scale, scroll, isAnchor);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);
    EXPECT_TRUE(isAnchor);

    setScaleAndScrollAndLayout(webViewImpl, WebPoint(0, 0), scale);

    // Test double-tap zooming into tall div.
    webViewImpl->computeScaleAndScrollForHitRect(doubleTapPointTall, WebViewImpl::DoubleTap, scale, scroll, isAnchor);
    // The div should start at the top left of the viewport.
    EXPECT_NEAR(viewportWidth / (float) tallDiv.width, scale, 0.1);
    EXPECT_NEAR(tallDiv.x * scale, scroll.x, 20);
    EXPECT_NEAR(tallDiv.y * scale, scroll.y, 20);
    EXPECT_FALSE(isAnchor);

    // Test for Non-doubletap scaling
    // Test zooming into div.
    webViewImpl->computeScaleAndScrollForHitRect(WebRect(250, 250, 10, 10), WebViewImpl::FindInPage, scale, scroll, isAnchor);
    EXPECT_NEAR(viewportWidth / (float) wideDiv.width, scale, 0.1);
}

void simulateDoubleTap(WebViewImpl* webViewImpl, WebPoint& point, float& scale)
{
    WebPoint scaledPoint(static_cast<int>(point.x * webViewImpl->pageScaleFactor()),
        static_cast<int>(point.y * webViewImpl->pageScaleFactor()));
    webViewImpl->animateZoomAroundPoint(scaledPoint, WebViewImpl::DoubleTap);
    webViewImpl->mainFrameImpl()->frameView()->layout();
    scale = webViewImpl->pageScaleFactor();
}

TEST_F(WebFrameTest, DivAutoZoomMultipleDivsTest)
{
    registerMockedHttpURLLoad("get_multiple_divs_for_auto_zoom_test.html");

    int viewportWidth = 640;
    int viewportHeight = 480;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    WebKit::WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_multiple_divs_for_auto_zoom_test.html");
    webView->enableFixedLayoutMode(true);
    webView->resize(WebSize(viewportWidth, viewportHeight));
    webView->setPageScaleFactorLimits(1, 4);
    webView->layout();
    webView->setDeviceScaleFactor(1.5f);

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(webView);
    webViewImpl->shouldUseAnimateDoubleTapTimeZeroForTesting(true);

    WebRect topDiv(200, 100, 200, 150);
    WebRect bottomDiv(200, 300, 200, 150);
    WebPoint topPoint(topDiv.x + 50, topDiv.y + 50);
    WebPoint bottomPoint(bottomDiv.x + 50, bottomDiv.y + 50);
    float scale;
    setScaleAndScrollAndLayout(webViewImpl, WebPoint(0, 0), (webViewImpl->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);

    // Test double tap on two different divs
    // After first zoom, we should go back to minimum page scale with a second double tap.
    simulateDoubleTap(webViewImpl, topPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->deviceScaleFactor(), scale);
    simulateDoubleTap(webViewImpl, bottomPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);

    // If the user pinch zooms after double tap, a second double tap should zoom back to the div.
    simulateDoubleTap(webViewImpl, topPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->deviceScaleFactor(), scale);
    webViewImpl->applyScrollAndScale(WebSize(), 0.6f);
    simulateDoubleTap(webViewImpl, bottomPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->deviceScaleFactor(), scale);
}

TEST_F(WebFrameTest, DivAutoZoomScaleBoundsTest)
{
    registerMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

    int viewportWidth = 640;
    int viewportHeight = 480;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    WebKit::WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_scale_bounds_check_for_auto_zoom_test.html");
    webView->enableFixedLayoutMode(true);
    webView->resize(WebSize(viewportWidth, viewportHeight));
    webView->setPageScaleFactorLimits(1, 4);
    webView->layout();

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(webView);
    webViewImpl->shouldUseAnimateDoubleTapTimeZeroForTesting(true);
    float doubleTapZoomAlreadyLegibleScale = webViewImpl->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;

    WebRect div(200, 100, 200, 150);
    WebPoint doubleTapPoint(div.x + 50, div.y + 50);
    float scale;

    // Test double tap scale bounds.
    // minimumPageScale < doubleTapZoomAlreadyLegibleScale < deviceDpiScale
    webViewImpl->setDeviceScaleFactor(1.5f);
    setScaleAndScrollAndLayout(webViewImpl, WebPoint(0, 0), (webViewImpl->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->deviceScaleFactor(), scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->deviceScaleFactor(), scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewImpl->applyScrollAndScale(WebSize(), 1.1f);
    // deviceDpiScale < minimumPageScale < doubleTapZoomAlreadyLegibleScale
    webViewImpl->setDeviceScaleFactor(0.5f);
    setScaleAndScrollAndLayout(webViewImpl, WebPoint(0, 0), (webViewImpl->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewImpl->applyScrollAndScale(WebSize(), 1.1f);
    // minimumPageScale < deviceDpiScale < doubleTapZoomAlreadyLegibleScale
    webViewImpl->setDeviceScaleFactor(1.1f);
    setScaleAndScrollAndLayout(webViewImpl, WebPoint(0, 0), (webViewImpl->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);
}

#if ENABLE(TEXT_AUTOSIZING)
TEST_F(WebFrameTest, DivAutoZoomScaleFontScaleFactorTest)
{
    registerMockedHttpURLLoad("get_scale_bounds_check_for_auto_zoom_test.html");

    int viewportWidth = 640;
    int viewportHeight = 480;
    float doubleTapZoomAlreadyLegibleRatio = 1.2f;
    float textAutosizingFontScaleFactor = 1.13f;
    WebKit::WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_scale_bounds_check_for_auto_zoom_test.html");
    webView->enableFixedLayoutMode(true);
    webView->resize(WebSize(viewportWidth, viewportHeight));
    webView->setPageScaleFactorLimits(1, 4);
    webView->layout();

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(webView);
    webViewImpl->shouldUseAnimateDoubleTapTimeZeroForTesting(true);
    webViewImpl->page()->settings()->setTextAutosizingFontScaleFactor(textAutosizingFontScaleFactor);
    float doubleTapZoomAlreadyLegibleScale = webViewImpl->minimumPageScaleFactor() * doubleTapZoomAlreadyLegibleRatio;

    WebRect div(200, 100, 200, 150);
    WebPoint doubleTapPoint(div.x + 50, div.y + 50);
    float scale;

    // Test double tap scale bounds.
    // minimumPageScale < doubleTapZoomAlreadyLegibleScale < deviceDpiScale < deviceDpiScale * textAutosizingFontScaleFactor
    webViewImpl->setDeviceScaleFactor(1.5f);
    float legibleScale = webViewImpl->deviceScaleFactor() * textAutosizingFontScaleFactor;
    setScaleAndScrollAndLayout(webViewImpl, WebPoint(0, 0), (webViewImpl->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewImpl->applyScrollAndScale(WebSize(), 1.1f);
    // deviceDpiScale < deviceDpiScale * textAutosizingFontScaleFactor < minimumPageScale < doubleTapZoomAlreadyLegibleScale
    webViewImpl->setDeviceScaleFactor(0.5f);
    legibleScale = webViewImpl->deviceScaleFactor() * textAutosizingFontScaleFactor;
    setScaleAndScrollAndLayout(webViewImpl, WebPoint(0, 0), (webViewImpl->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewImpl->applyScrollAndScale(WebSize(), 1.1f);
    // minimumPageScale < deviceDpiScale < deviceDpiScale * textAutosizingFontScaleFactor < doubleTapZoomAlreadyLegibleScale
    webViewImpl->setDeviceScaleFactor(1.05f);
    legibleScale = webViewImpl->deviceScaleFactor() * textAutosizingFontScaleFactor;
    setScaleAndScrollAndLayout(webViewImpl, WebPoint(0, 0), (webViewImpl->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(doubleTapZoomAlreadyLegibleScale, scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);

    // Zoom in to reset double_tap_zoom_in_effect flag.
    webViewImpl->applyScrollAndScale(WebSize(), 1.1f);
    // minimumPageScale < deviceDpiScale < doubleTapZoomAlreadyLegibleScale < deviceDpiScale * textAutosizingFontScaleFactor
    webViewImpl->setDeviceScaleFactor(1.1f);
    legibleScale = webViewImpl->deviceScaleFactor() * textAutosizingFontScaleFactor;
    setScaleAndScrollAndLayout(webViewImpl, WebPoint(0, 0), (webViewImpl->minimumPageScaleFactor()) * (1 + doubleTapZoomAlreadyLegibleRatio) / 2);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(webViewImpl->minimumPageScaleFactor(), scale);
    simulateDoubleTap(webViewImpl, doubleTapPoint, scale);
    EXPECT_FLOAT_EQ(legibleScale, scale);
}
#endif

// This test depends on code that is compiled conditionally. We likely need to
// add the proper ifdef when re-enabling it. See
// https://bugs.webkit.org/show_bug.cgi?id=98558
TEST_F(WebFrameTest, DISABLED_DivScrollIntoEditableTest)
{
    registerMockedHttpURLLoad("get_scale_for_zoom_into_editable_test.html");

    int viewportWidth = 640;
    int viewportHeight = 480;
    float leftBoxRatio = 0.3f;
    int caretPadding = 10;
    int minReadableCaretHeight = 18;
    WebKit::WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "get_scale_for_zoom_into_editable_test.html");
    webView->enableFixedLayoutMode(true);
    webView->resize(WebSize(viewportWidth, viewportHeight));
    webView->setPageScaleFactorLimits(1, 10);
    webView->layout();
    webView->setDeviceScaleFactor(1.5f);
    webView->settings()->setAutoZoomFocusedNodeToLegibleScale(true);

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(webView);
    webViewImpl->shouldUseAnimateDoubleTapTimeZeroForTesting(true);

    WebRect editBoxWithText(200, 200, 250, 20);
    WebRect editBoxWithNoText(200, 250, 250, 20);

    // Test scrolling the focused node
    // The edit box is shorter and narrower than the viewport when legible.
    setScaleAndScrollAndLayout(webView, WebPoint(0, 0), 1);
    WebRect rect, caret;
    webViewImpl->selectionBounds(caret, rect);
    webView->scrollFocusedNodeIntoRect(rect);
    // The edit box should be left aligned with a margin for possible label.
    int hScroll = editBoxWithText.x * webView->pageScaleFactor() - leftBoxRatio * viewportWidth;
    EXPECT_EQ(hScroll, webView->mainFrame()->scrollOffset().width);
    int vScroll = editBoxWithText.y * webView->pageScaleFactor() - (viewportHeight - editBoxWithText.height * webView->pageScaleFactor()) / 2;
    EXPECT_EQ(vScroll, webView->mainFrame()->scrollOffset().height);
    EXPECT_FLOAT_EQ(webView->deviceScaleFactor() * minReadableCaretHeight / caret.height, webView->pageScaleFactor());

    // The edit box is wider than the viewport when legible.
    webView->setDeviceScaleFactor(4);
    setScaleAndScrollAndLayout(webView, WebPoint(0, 0), 1);
    webViewImpl->selectionBounds(caret, rect);
    webView->scrollFocusedNodeIntoRect(rect);
    // The caret should be right aligned since the caret would be offscreen when the edit box is left aligned.
    hScroll = (caret.x + caret.width) * webView->pageScaleFactor() + caretPadding - viewportWidth;
    EXPECT_EQ(hScroll, webView->mainFrame()->scrollOffset().width);
    EXPECT_FLOAT_EQ(webView->deviceScaleFactor() * minReadableCaretHeight / caret.height, webView->pageScaleFactor());

    setScaleAndScrollAndLayout(webView, WebPoint(0, 0), 1);
    // Move focus to edit box with text.
    webView->advanceFocus(false);
    webViewImpl->selectionBounds(caret, rect);
    webView->scrollFocusedNodeIntoRect(rect);
    // The edit box should be left aligned.
    hScroll = editBoxWithNoText.x * webView->pageScaleFactor();
    EXPECT_EQ(hScroll, webView->mainFrame()->scrollOffset().width);
    vScroll = editBoxWithNoText.y * webView->pageScaleFactor() - (viewportHeight - editBoxWithNoText.height * webView->pageScaleFactor()) / 2;
    EXPECT_EQ(vScroll, webView->mainFrame()->scrollOffset().height);
    EXPECT_FLOAT_EQ(webView->deviceScaleFactor() * minReadableCaretHeight / caret.height, webView->pageScaleFactor());

    // Move focus back to the first edit box.
    webView->advanceFocus(true);
    webViewImpl->selectionBounds(caret, rect);
    // The position should have stayed the same since this box was already on screen with the right scale.
    EXPECT_EQ(vScroll, webView->mainFrame()->scrollOffset().height);
    EXPECT_EQ(hScroll, webView->mainFrame()->scrollOffset().width);
}
#endif

class TestReloadDoesntRedirectWebFrameClient : public WebFrameClient {
public:
    virtual WebNavigationPolicy decidePolicyForNavigation(
        WebFrame*, const WebURLRequest&, WebNavigationType,
        const WebNode& originatingNode,
        WebNavigationPolicy defaultPolicy, bool isRedirect)
    {
        EXPECT_FALSE(isRedirect);
        return WebNavigationPolicyCurrentTab;
    }

    virtual WebURLError cancelledError(WebFrame*, const WebURLRequest& request)
    {
        // Return a dummy error so the DocumentLoader doesn't assert when
        // the reload cancels it.
        WebURLError webURLError;
        webURLError.domain = "";
        webURLError.reason = 1;
        webURLError.isCancellation = true;
        webURLError.unreachableURL = WebURL();
        return webURLError;
    }
};

TEST_F(WebFrameTest, ReloadDoesntSetRedirect)
{
    // Test for case in http://crbug.com/73104. Reloading a frame very quickly
    // would sometimes call decidePolicyForNavigation with isRedirect=true
    registerMockedHttpURLLoad("form.html");

    TestReloadDoesntRedirectWebFrameClient webFrameClient;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "form.html", false, &webFrameClient);

    webView->mainFrame()->reload(true);
    // start reload before request is delivered.
    webView->mainFrame()->reload(true);
    webkit_support::ServeAsynchronousMockedRequests();
}

TEST_F(WebFrameTest, ReloadWithOverrideURLPreservesState)
{
    const std::string firstURL = "find.html";
    const std::string secondURL = "form.html";
    const std::string thirdURL = "history.html";
    const float pageScaleFactor = 1.1684f;
    const int pageWidth = 640;
    const int pageHeight = 480;

    registerMockedHttpURLLoad(firstURL);
    registerMockedHttpURLLoad(secondURL);
    registerMockedHttpURLLoad(thirdURL);

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(m_baseURL + firstURL, true));
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->mainFrame()->setScrollOffset(WebSize(pageWidth / 4, pageHeight / 4));
    webViewImpl->setPageScaleFactorPreservingScrollOffset(pageScaleFactor);

    WebSize previousOffset = webViewImpl->mainFrame()->scrollOffset();
    float previousScale = webViewImpl->pageScaleFactor();

    // Reload the page using the cache.
    webViewImpl->mainFrame()->reloadWithOverrideURL(toKURL(m_baseURL + secondURL), false);
    webkit_support::ServeAsynchronousMockedRequests();
    ASSERT_EQ(previousOffset, webViewImpl->mainFrame()->scrollOffset());
    ASSERT_EQ(previousScale, webViewImpl->pageScaleFactor());

    // Reload the page while ignoring the cache.
    webViewImpl->mainFrame()->reloadWithOverrideURL(toKURL(m_baseURL + thirdURL), true);
    webkit_support::ServeAsynchronousMockedRequests();
    ASSERT_EQ(previousOffset, webViewImpl->mainFrame()->scrollOffset());
    ASSERT_EQ(previousScale, webViewImpl->pageScaleFactor());
}

TEST_F(WebFrameTest, IframeRedirect)
{
    registerMockedHttpURLLoad("iframe_redirect.html");
    registerMockedHttpURLLoad("visible_iframe.html");

    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframe_redirect.html", true);
    webkit_support::RunAllPendingMessages(); // Queue the iframe.
    webkit_support::ServeAsynchronousMockedRequests(); // Load the iframe.

    WebFrame* iframe = webView->findFrameByName(WebString::fromUTF8("ifr"));
    ASSERT_TRUE(iframe);
    WebDataSource* iframeDataSource = iframe->dataSource();
    ASSERT_TRUE(iframeDataSource);
    WebVector<WebURL> redirects;
    iframeDataSource->redirectChain(redirects);
    ASSERT_EQ(2U, redirects.size());
    EXPECT_EQ(toKURL("about:blank"), toKURL(redirects[0].spec().data()));
    EXPECT_EQ(toKURL("http://www.test.com/visible_iframe.html"), toKURL(redirects[1].spec().data()));

    webView->close();
}

TEST_F(WebFrameTest, ClearFocusedNodeTest)
{
    registerMockedHttpURLLoad("iframe_clear_focused_node_test.html");
    registerMockedHttpURLLoad("autofocus_input_field_iframe.html");

    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframe_clear_focused_node_test.html", true));

    // Clear the focused node.
    webViewImpl->clearFocusedNode();

    // Now retrieve the FocusedNode and test it should be null.
    EXPECT_EQ(0, webViewImpl->focusedWebCoreNode());

    webViewImpl->close();
}

// Implementation of WebFrameClient that tracks the v8 contexts that are created
// and destroyed for verification.
class ContextLifetimeTestWebFrameClient : public WebFrameClient {
public:
    struct Notification {
    public:
        Notification(WebFrame* frame, v8::Handle<v8::Context> context, int worldId)
            : frame(frame) ,
              context(v8::Persistent<v8::Context>::New(context)),
              worldId(worldId)
        {
        }

        ~Notification()
        {
            context.Dispose();
        }

        bool Equals(Notification* other)
        {
            return other && frame == other->frame && context == other->context && worldId == other->worldId;
        }

        WebFrame* frame;
        v8::Persistent<v8::Context> context;
        int worldId;
    };

    ~ContextLifetimeTestWebFrameClient()
    {
        reset();
    }

    void reset()
    {
        for (size_t i = 0; i < createNotifications.size(); ++i)
            delete createNotifications[i];

        for (size_t i = 0; i < releaseNotifications.size(); ++i)
            delete releaseNotifications[i];

        createNotifications.clear();
        releaseNotifications.clear();
    }

    std::vector<Notification*> createNotifications;
    std::vector<Notification*> releaseNotifications;

 private:
    virtual void didCreateScriptContext(WebFrame* frame, v8::Handle<v8::Context> context, int extensionGroup, int worldId) OVERRIDE
    {
        createNotifications.push_back(new Notification(frame, context, worldId));
    }

    virtual void willReleaseScriptContext(WebFrame* frame, v8::Handle<v8::Context> context, int worldId) OVERRIDE
    {
        releaseNotifications.push_back(new Notification(frame, context, worldId));
    }
};

TEST_F(WebFrameTest, ContextNotificationsLoadUnload)
{
    v8::HandleScope handleScope;

    registerMockedHttpURLLoad("context_notifications_test.html");
    registerMockedHttpURLLoad("context_notifications_test_frame.html");

    // Load a frame with an iframe, make sure we get the right create notifications.
    ContextLifetimeTestWebFrameClient webFrameClient;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    WebFrame* mainFrame = webView->mainFrame();
    WebFrame* childFrame = mainFrame->firstChild();

    ASSERT_EQ(2u, webFrameClient.createNotifications.size());
    EXPECT_EQ(0u, webFrameClient.releaseNotifications.size());

    ContextLifetimeTestWebFrameClient::Notification* firstCreateNotification = webFrameClient.createNotifications[0];
    ContextLifetimeTestWebFrameClient::Notification* secondCreateNotification = webFrameClient.createNotifications[1];

    EXPECT_EQ(mainFrame, firstCreateNotification->frame);
    EXPECT_EQ(mainFrame->mainWorldScriptContext(), firstCreateNotification->context);
    EXPECT_EQ(0, firstCreateNotification->worldId);

    EXPECT_EQ(childFrame, secondCreateNotification->frame);
    EXPECT_EQ(childFrame->mainWorldScriptContext(), secondCreateNotification->context);
    EXPECT_EQ(0, secondCreateNotification->worldId);

    // Close the view. We should get two release notifications that are exactly the same as the create ones, in reverse order.
    webView->close();

    ASSERT_EQ(2u, webFrameClient.releaseNotifications.size());
    ContextLifetimeTestWebFrameClient::Notification* firstReleaseNotification = webFrameClient.releaseNotifications[0];
    ContextLifetimeTestWebFrameClient::Notification* secondReleaseNotification = webFrameClient.releaseNotifications[1];

    ASSERT_TRUE(firstCreateNotification->Equals(secondReleaseNotification));
    ASSERT_TRUE(secondCreateNotification->Equals(firstReleaseNotification));
}

TEST_F(WebFrameTest, ContextNotificationsReload)
{
    v8::HandleScope handleScope;

    registerMockedHttpURLLoad("context_notifications_test.html");
    registerMockedHttpURLLoad("context_notifications_test_frame.html");

    ContextLifetimeTestWebFrameClient webFrameClient;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    // Refresh, we should get two release notifications and two more create notifications.
    webView->mainFrame()->reload(false);
    webkit_support::ServeAsynchronousMockedRequests();
    ASSERT_EQ(4u, webFrameClient.createNotifications.size());
    ASSERT_EQ(2u, webFrameClient.releaseNotifications.size());

    // The two release notifications we got should be exactly the same as the first two create notifications.
    for (size_t i = 0; i < webFrameClient.releaseNotifications.size(); ++i) {
      EXPECT_TRUE(webFrameClient.releaseNotifications[i]->Equals(
          webFrameClient.createNotifications[webFrameClient.createNotifications.size() - 3 - i]));
    }

    // The last two create notifications should be for the current frames and context.
    WebFrame* mainFrame = webView->mainFrame();
    WebFrame* childFrame = mainFrame->firstChild();
    ContextLifetimeTestWebFrameClient::Notification* firstRefreshNotification = webFrameClient.createNotifications[2];
    ContextLifetimeTestWebFrameClient::Notification* secondRefreshNotification = webFrameClient.createNotifications[3];

    EXPECT_EQ(mainFrame, firstRefreshNotification->frame);
    EXPECT_EQ(mainFrame->mainWorldScriptContext(), firstRefreshNotification->context);
    EXPECT_EQ(0, firstRefreshNotification->worldId);

    EXPECT_EQ(childFrame, secondRefreshNotification->frame);
    EXPECT_EQ(childFrame->mainWorldScriptContext(), secondRefreshNotification->context);
    EXPECT_EQ(0, secondRefreshNotification->worldId);

    webView->close();
}

TEST_F(WebFrameTest, ContextNotificationsIsolatedWorlds)
{
    v8::HandleScope handleScope;

    registerMockedHttpURLLoad("context_notifications_test.html");
    registerMockedHttpURLLoad("context_notifications_test_frame.html");

    ContextLifetimeTestWebFrameClient webFrameClient;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "context_notifications_test.html", true, &webFrameClient);

    // Add an isolated world.
    webFrameClient.reset();

    int isolatedWorldId = 42;
    WebScriptSource scriptSource("hi!");
    int numSources = 1;
    int extensionGroup = 0;
    webView->mainFrame()->executeScriptInIsolatedWorld(isolatedWorldId, &scriptSource, numSources, extensionGroup);

    // We should now have a new create notification.
    ASSERT_EQ(1u, webFrameClient.createNotifications.size());
    ContextLifetimeTestWebFrameClient::Notification* notification = webFrameClient.createNotifications[0];
    ASSERT_EQ(isolatedWorldId, notification->worldId);
    ASSERT_EQ(webView->mainFrame(), notification->frame);

    // We don't have an API to enumarate isolated worlds for a frame, but we can at least assert that the context we got is *not* the main world's context.
    ASSERT_NE(webView->mainFrame()->mainWorldScriptContext(), notification->context);

    webView->close();

    // We should have gotten three release notifications (one for each of the frames, plus one for the isolated context).
    ASSERT_EQ(3u, webFrameClient.releaseNotifications.size());

    // And one of them should be exactly the same as the create notification for the isolated context.
    int matchCount = 0;
    for (size_t i = 0; i < webFrameClient.releaseNotifications.size(); ++i) {
      if (webFrameClient.releaseNotifications[i]->Equals(webFrameClient.createNotifications[0]))
        ++matchCount;
    }
    EXPECT_EQ(1, matchCount);
}

TEST_F(WebFrameTest, FindInPage)
{
    registerMockedHttpURLLoad("find.html");
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find.html");
    WebFrame* frame = webView->mainFrame();
    const int findIdentifier = 12345;
    WebFindOptions options;

    // Find in a <div> element.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar1"), options, false, 0));
    frame->stopFinding(false);
    WebRange range = frame->selectionRange();
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_TRUE(frame->document().focusedNode().isNull());

    // Find in an <input> value.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar2"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_EQ(WebString::fromUTF8("INPUT"), frame->document().focusedNode().nodeName());

    // Find in a <textarea> content.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar3"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(5, range.startOffset());
    EXPECT_EQ(9, range.endOffset());
    EXPECT_EQ(WebString::fromUTF8("TEXTAREA"), frame->document().focusedNode().nodeName());

    // Find in a contentEditable element.
    EXPECT_TRUE(frame->find(findIdentifier, WebString::fromUTF8("bar4"), options, false, 0));
    // Confirm stopFinding(false) sets the selection on the found text.
    frame->stopFinding(false);
    range = frame->selectionRange();
    ASSERT_FALSE(range.isNull());
    EXPECT_EQ(0, range.startOffset());
    EXPECT_EQ(4, range.endOffset());
    // "bar4" is surrounded by <span>, but the focusable node should be the parent <div>.
    EXPECT_EQ(WebString::fromUTF8("DIV"), frame->document().focusedNode().nodeName());

    webView->close();
}

TEST_F(WebFrameTest, GetContentAsPlainText)
{
    WebView* webView = FrameTestHelpers::createWebViewAndLoad("about:blank", true);
    // We set the size because it impacts line wrapping, which changes the
    // resulting text value.
    webView->resize(WebSize(640, 480));
    WebFrame* frame = webView->mainFrame();

    // Generate a simple test case.
    const char simpleSource[] = "<div>Foo bar</div><div></div>baz";
    WebCore::KURL testURL = toKURL("about:blank");
    frame->loadHTMLString(simpleSource, testURL);
    webkit_support::RunAllPendingMessages();

    // Make sure it comes out OK.
    const std::string expected("Foo bar\nbaz");
    WebString text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ(expected, std::string(text.utf8().data()));

    // Try reading the same one with clipping of the text.
    const int length = 5;
    text = frame->contentAsText(length);
    EXPECT_EQ(expected.substr(0, length), std::string(text.utf8().data()));

    // Now do a new test with a subframe.
    const char outerFrameSource[] = "Hello<iframe></iframe> world";
    frame->loadHTMLString(outerFrameSource, testURL);
    webkit_support::RunAllPendingMessages();

    // Load something into the subframe.
    WebFrame* subframe = frame->findChildByExpression(WebString::fromUTF8("/html/body/iframe"));
    ASSERT_TRUE(subframe);
    subframe->loadHTMLString("sub<p>text", testURL);
    webkit_support::RunAllPendingMessages();

    text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ("Hello world\n\nsub\ntext", std::string(text.utf8().data()));

    // Get the frame text where the subframe separator falls on the boundary of
    // what we'll take. There used to be a crash in this case.
    text = frame->contentAsText(12);
    EXPECT_EQ("Hello world", std::string(text.utf8().data()));

    webView->close();
}

TEST_F(WebFrameTest, GetFullHtmlOfPage)
{
    WebView* webView = FrameTestHelpers::createWebViewAndLoad("about:blank", true);
    WebFrame* frame = webView->mainFrame();

    // Generate a simple test case.
    const char simpleSource[] = "<p>Hello</p><p>World</p>";
    WebCore::KURL testURL = toKURL("about:blank");
    frame->loadHTMLString(simpleSource, testURL);
    webkit_support::RunAllPendingMessages();

    WebString text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ("Hello\n\nWorld", std::string(text.utf8().data()));

    const std::string html = std::string(frame->contentAsMarkup().utf8().data());

    // Load again with the output html.
    frame->loadHTMLString(WebData(html.c_str(), html.length()), testURL);
    webkit_support::RunAllPendingMessages();

    EXPECT_EQ(html, std::string(frame->contentAsMarkup().utf8().data()));

    text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ("Hello\n\nWorld", std::string(text.utf8().data()));

    // Test selection check
    EXPECT_FALSE(frame->hasSelection());
    frame->executeCommand(WebString::fromUTF8("SelectAll"));
    EXPECT_TRUE(frame->hasSelection());
    frame->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_FALSE(frame->hasSelection());
    WebString selectionHtml = frame->selectionAsMarkup();
    EXPECT_TRUE(selectionHtml.isEmpty());
}

class TestExecuteScriptDuringDidCreateScriptContext : public WebFrameClient {
public:
    virtual void didCreateScriptContext(WebFrame* frame, v8::Handle<v8::Context> context, int extensionGroup, int worldId) OVERRIDE
    {
        frame->executeScript(WebScriptSource("window.history = 'replaced';"));
    }
};

TEST_F(WebFrameTest, ExecuteScriptDuringDidCreateScriptContext)
{
    registerMockedHttpURLLoad("hello_world.html");

    TestExecuteScriptDuringDidCreateScriptContext webFrameClient;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "hello_world.html", true, &webFrameClient);

    webView->mainFrame()->reload();
    webkit_support::ServeAsynchronousMockedRequests();
}

class TestDidCreateFrameWebFrameClient : public WebFrameClient {
public:
    TestDidCreateFrameWebFrameClient() : m_frameCount(0), m_parent(0)
    {
    }

    virtual void didCreateFrame(WebFrame* parent, WebFrame* child) 
    {
        m_frameCount++;
        if (!m_parent)
            m_parent = parent;
    }
    
    int m_frameCount;
    WebFrame* m_parent;
};

TEST_F(WebFrameTest, DidCreateFrame)
{
    registerMockedHttpURLLoad("iframes_test.html");
    registerMockedHttpURLLoad("visible_iframe.html");
    registerMockedHttpURLLoad("invisible_iframe.html");
    registerMockedHttpURLLoad("zero_sized_iframe.html");

    TestDidCreateFrameWebFrameClient webFrameClient;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "iframes_test.html", false, &webFrameClient);

    EXPECT_EQ(webFrameClient.m_frameCount, 3); 
    EXPECT_EQ(webFrameClient.m_parent, webView->mainFrame());
}

class FindUpdateWebFrameClient : public WebFrameClient {
public:
    FindUpdateWebFrameClient()
        : m_findResultsAreReady(false)
        , m_count(-1)
    {
    }

    virtual void reportFindInPageMatchCount(int, int count, bool finalUpdate) OVERRIDE
    {
        m_count = count;
        if (finalUpdate)
            m_findResultsAreReady = true;
    }

    bool findResultsAreReady() const { return m_findResultsAreReady; }
    int count() const { return m_count; }

private:
    bool m_findResultsAreReady;
    int m_count;
};

TEST_F(WebFrameTest, FindInPageMatchRects)
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find_in_page.html", true, &client);
    webView->resize(WebSize(640, 480));
    webView->layout();
    webkit_support::RunAllPendingMessages();

    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;
    static const int kNumResults = 16;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = static_cast<WebFrameImpl*>(webView->mainFrame());
    EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false, 0));

    mainFrame->resetMatchCount();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        frame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    webkit_support::RunAllPendingMessages();
    EXPECT_TRUE(client.findResultsAreReady());

    WebVector<WebFloatRect> webMatchRects;
    mainFrame->findMatchRects(webMatchRects);
    ASSERT_EQ(webMatchRects.size(), static_cast<size_t>(kNumResults));
    int rectsVersion = mainFrame->findMatchMarkersVersion();

    for (int resultIndex = 0; resultIndex < kNumResults; ++resultIndex) {
        FloatRect resultRect = static_cast<FloatRect>(webMatchRects[resultIndex]);

        // Select the match by the center of its rect.
        EXPECT_EQ(mainFrame->selectNearestFindMatch(resultRect.center(), 0), resultIndex + 1);

        // Check that the find result ordering matches with our expectations.
        Range* result = mainFrame->activeMatchFrame()->activeMatch();
        ASSERT_TRUE(result);
        result->setEnd(result->endContainer(), result->endOffset() + 3);
        EXPECT_EQ(result->text(), String::format("%s %02d", kFindString, resultIndex));

        // Verify that the expected match rect also matches the currently active match.
        // Compare the enclosing rects to prevent precision issues caused by CSS transforms.
        FloatRect activeMatch = mainFrame->activeFindMatchRect();
        EXPECT_EQ(enclosingIntRect(activeMatch), enclosingIntRect(resultRect));

        // The rects version should not have changed.
        EXPECT_EQ(mainFrame->findMatchMarkersVersion(), rectsVersion);
    }

    // All results after the first two ones should be below between them in find-in-page coordinates.
    // This is because results 2 to 9 are inside an iframe located between results 0 and 1. This applies to the fixed div too.
    EXPECT_TRUE(webMatchRects[0].y < webMatchRects[1].y);
    for (int resultIndex = 2; resultIndex < kNumResults; ++resultIndex) {
        EXPECT_TRUE(webMatchRects[0].y < webMatchRects[resultIndex].y);
        EXPECT_TRUE(webMatchRects[1].y > webMatchRects[resultIndex].y);
    }

    // Result 3 should be below both 2 and 4. This is caused by the CSS transform in the containing div.
    // If the transform doesn't work then 3 will be between 2 and 4.
    EXPECT_TRUE(webMatchRects[3].y > webMatchRects[2].y);
    EXPECT_TRUE(webMatchRects[3].y > webMatchRects[4].y);

    // Results 6, 7, 8 and 9 should be one below the other in that same order.
    // If overflow:scroll is not properly handled then result 8 would be below result 9 or
    // result 7 above result 6 depending on the scroll.
    EXPECT_TRUE(webMatchRects[6].y < webMatchRects[7].y);
    EXPECT_TRUE(webMatchRects[7].y < webMatchRects[8].y);
    EXPECT_TRUE(webMatchRects[8].y < webMatchRects[9].y);

    // Results 11, 12, 13 and 14 should be between results 10 and 15, as they are inside the table.
    EXPECT_TRUE(webMatchRects[11].y > webMatchRects[10].y);
    EXPECT_TRUE(webMatchRects[12].y > webMatchRects[10].y);
    EXPECT_TRUE(webMatchRects[13].y > webMatchRects[10].y);
    EXPECT_TRUE(webMatchRects[14].y > webMatchRects[10].y);
    EXPECT_TRUE(webMatchRects[11].y < webMatchRects[15].y);
    EXPECT_TRUE(webMatchRects[12].y < webMatchRects[15].y);
    EXPECT_TRUE(webMatchRects[13].y < webMatchRects[15].y);
    EXPECT_TRUE(webMatchRects[14].y < webMatchRects[15].y);

    // Result 11 should be above 12, 13 and 14 as it's in the table header.
    EXPECT_TRUE(webMatchRects[11].y < webMatchRects[12].y);
    EXPECT_TRUE(webMatchRects[11].y < webMatchRects[13].y);
    EXPECT_TRUE(webMatchRects[11].y < webMatchRects[14].y);

    // Result 11 should also be right to 12, 13 and 14 because of the colspan.
    EXPECT_TRUE(webMatchRects[11].x > webMatchRects[12].x);
    EXPECT_TRUE(webMatchRects[11].x > webMatchRects[13].x);
    EXPECT_TRUE(webMatchRects[11].x > webMatchRects[14].x);

    // Result 12 should be left to results 11, 13 and 14 in the table layout.
    EXPECT_TRUE(webMatchRects[12].x < webMatchRects[11].x);
    EXPECT_TRUE(webMatchRects[12].x < webMatchRects[13].x);
    EXPECT_TRUE(webMatchRects[12].x < webMatchRects[14].x);

    // Results 13, 12 and 14 should be one above the other in that order because of the rowspan
    // and vertical-align: middle by default.
    EXPECT_TRUE(webMatchRects[13].y < webMatchRects[12].y);
    EXPECT_TRUE(webMatchRects[12].y < webMatchRects[14].y);

    // Resizing should update the rects version.
    webView->resize(WebSize(800, 600));
    webkit_support::RunAllPendingMessages();
    EXPECT_TRUE(mainFrame->findMatchMarkersVersion() != rectsVersion);

    webView->close();
}

TEST_F(WebFrameTest, FindInPageSkipsHiddenFrames)
{
    registerMockedHttpURLLoad("find_in_hidden_frame.html");

    FindUpdateWebFrameClient client;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find_in_hidden_frame.html", true, &client);
    webView->resize(WebSize(640, 480));
    webView->layout();
    webkit_support::RunAllPendingMessages();

    static const char* kFindString = "hello";
    static const int kFindIdentifier = 12345;
    static const int kNumResults = 1;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = static_cast<WebFrameImpl*>(webView->mainFrame());
    EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false, 0));

    mainFrame->resetMatchCount();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        frame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    webkit_support::RunAllPendingMessages();
    EXPECT_TRUE(client.findResultsAreReady());
    EXPECT_EQ(kNumResults, client.count());

    webView->close();
}

TEST_F(WebFrameTest, FindOnDetachedFrame)
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find_in_page.html", true, &client);
    webView->resize(WebSize(640, 480));
    webView->layout();
    webkit_support::RunAllPendingMessages();

    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = static_cast<WebFrameImpl*>(webView->mainFrame());
    WebFrameImpl* secondFrame = static_cast<WebFrameImpl*>(mainFrame->traverseNext(false));
    RefPtr<WebCore::Frame> holdSecondFrame = secondFrame->frame();

    // Detach the frame before finding.
    EXPECT_TRUE(mainFrame->document().getElementById("frame").remove());

    EXPECT_TRUE(mainFrame->find(kFindIdentifier, searchText, options, false, 0));
    EXPECT_FALSE(secondFrame->find(kFindIdentifier, searchText, options, false, 0));

    webkit_support::RunAllPendingMessages();
    EXPECT_FALSE(client.findResultsAreReady());

    mainFrame->resetMatchCount();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        frame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    webkit_support::RunAllPendingMessages();
    EXPECT_TRUE(client.findResultsAreReady());

    holdSecondFrame.release();
    webView->close();
}

TEST_F(WebFrameTest, FindDetachFrameBeforeScopeStrings)
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find_in_page.html", true, &client);
    webView->resize(WebSize(640, 480));
    webView->layout();
    webkit_support::RunAllPendingMessages();

    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = static_cast<WebFrameImpl*>(webView->mainFrame());
    WebFrameImpl* secondFrame = static_cast<WebFrameImpl*>(mainFrame->traverseNext(false));
    RefPtr<WebCore::Frame> holdSecondFrame = secondFrame->frame();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        EXPECT_TRUE(frame->find(kFindIdentifier, searchText, options, false, 0));

    webkit_support::RunAllPendingMessages();
    EXPECT_FALSE(client.findResultsAreReady());

    // Detach the frame between finding and scoping.
    EXPECT_TRUE(mainFrame->document().getElementById("frame").remove());

    mainFrame->resetMatchCount();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        frame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    webkit_support::RunAllPendingMessages();
    EXPECT_TRUE(client.findResultsAreReady());

    holdSecondFrame.release();
    webView->close();
}

TEST_F(WebFrameTest, FindDetachFrameWhileScopingStrings)
{
    registerMockedHttpURLLoad("find_in_page.html");
    registerMockedHttpURLLoad("find_in_page_frame.html");

    FindUpdateWebFrameClient client;
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(m_baseURL + "find_in_page.html", true, &client);
    webView->resize(WebSize(640, 480));
    webView->layout();
    webkit_support::RunAllPendingMessages();

    static const char* kFindString = "result";
    static const int kFindIdentifier = 12345;

    WebFindOptions options;
    WebString searchText = WebString::fromUTF8(kFindString);
    WebFrameImpl* mainFrame = static_cast<WebFrameImpl*>(webView->mainFrame());
    WebFrameImpl* secondFrame = static_cast<WebFrameImpl*>(mainFrame->traverseNext(false));
    RefPtr<WebCore::Frame> holdSecondFrame = secondFrame->frame();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        EXPECT_TRUE(frame->find(kFindIdentifier, searchText, options, false, 0));

    webkit_support::RunAllPendingMessages();
    EXPECT_FALSE(client.findResultsAreReady());

    mainFrame->resetMatchCount();

    for (WebFrame* frame = mainFrame; frame; frame = frame->traverseNext(false))
        frame->scopeStringMatches(kFindIdentifier, searchText, options, true);

    // The first scopeStringMatches will have reset the state. Detach before it actually scopes.
    EXPECT_TRUE(mainFrame->document().getElementById("frame").remove());

    webkit_support::RunAllPendingMessages();
    EXPECT_TRUE(client.findResultsAreReady());

    holdSecondFrame.release();
    webView->close();
}

static WebView* createWebViewForTextSelection(const std::string& url)
{
    WebView* webView = FrameTestHelpers::createWebViewAndLoad(url, true);
    webView->settings()->setDefaultFontSize(12);
    webView->enableFixedLayoutMode(false);
    webView->resize(WebSize(640, 480));
    return webView;
}

static WebPoint topLeft(const WebRect& rect)
{
    return WebPoint(rect.x, rect.y);
}

static WebPoint bottomRightMinusOne(const WebRect& rect)
{
    // FIXME: If we don't subtract 1 from the x- and y-coordinates of the
    // selection bounds, selectRange() will select the *next* element. That's
    // strictly correct, as hit-testing checks the pixel to the lower-right of
    // the input coordinate, but it's a wart on the API.
    return WebPoint(rect.x + rect.width - 1, rect.y + rect.height - 1);
}

static WebRect elementBounds(WebFrame* frame, const WebString& id)
{
    return frame->document().getElementById(id).boundsInViewportSpace();
}

static std::string selectionAsString(WebFrame* frame)
{
    return std::string(frame->selectionAsText().utf8().data());
}

TEST_F(WebFrameTest, SelectRange)
{
    WebView* webView;
    WebFrame* frame;
    WebRect startWebRect;
    WebRect endWebRect;

    registerMockedHttpURLLoad("select_range_basic.html");
    registerMockedHttpURLLoad("select_range_scroll.html");
    registerMockedHttpURLLoad("select_range_iframe.html");
    registerMockedHttpURLLoad("select_range_editable.html");

    webView = createWebViewForTextSelection(m_baseURL + "select_range_basic.html");
    frame = webView->mainFrame();
    EXPECT_EQ("Some test text for testing.", selectionAsString(frame));
    webView->selectionBounds(startWebRect, endWebRect);
    frame->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_EQ("", selectionAsString(frame));
    frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("Some test text for testing.", selectionAsString(frame));
    webView->close();

    webView = createWebViewForTextSelection(m_baseURL + "select_range_scroll.html");
    frame = webView->mainFrame();
    EXPECT_EQ("Some offscreen test text for testing.", selectionAsString(frame));
    webView->selectionBounds(startWebRect, endWebRect);
    frame->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_EQ("", selectionAsString(frame));
    frame->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("Some offscreen test text for testing.", selectionAsString(frame));
    webView->close();

    webView = createWebViewForTextSelection(m_baseURL + "select_range_iframe.html");
    frame = webView->mainFrame();
    WebFrame* subframe = frame->findChildByExpression(WebString::fromUTF8("/html/body/iframe"));
    EXPECT_EQ("Some test text for testing.", selectionAsString(subframe));
    webView->selectionBounds(startWebRect, endWebRect);
    subframe->executeCommand(WebString::fromUTF8("Unselect"));
    EXPECT_EQ("", selectionAsString(subframe));
    subframe->selectRange(topLeft(startWebRect), bottomRightMinusOne(endWebRect));
    EXPECT_EQ("Some test text for testing.", selectionAsString(subframe));
    webView->close();

    // Select the middle of an editable element, then try to extend the selection to the top of the document.
    // The selection range should be clipped to the bounds of the editable element.
    webView = createWebViewForTextSelection(m_baseURL + "select_range_editable.html");
    frame = webView->mainFrame();
    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    webView->selectionBounds(startWebRect, endWebRect);
    frame->selectRange(bottomRightMinusOne(endWebRect), WebPoint(0, 0));
    EXPECT_EQ("16-char header. This text is initially selected.", selectionAsString(frame));
    webView->close();

    // As above, but extending the selection to the bottom of the document.
    webView = createWebViewForTextSelection(m_baseURL + "select_range_editable.html");
    frame = webView->mainFrame();
    EXPECT_EQ("This text is initially selected.", selectionAsString(frame));
    webView->selectionBounds(startWebRect, endWebRect);
    frame->selectRange(topLeft(startWebRect), WebPoint(640, 480));
    EXPECT_EQ("This text is initially selected. 16-char footer.", selectionAsString(frame));
    webView->close();
}

TEST_F(WebFrameTest, SelectRangeCanMoveSelectionStart)
{
    registerMockedHttpURLLoad("text_selection.html");
    WebView* webView = createWebViewForTextSelection(m_baseURL + "text_selection.html");
    WebFrame* frame = webView->mainFrame();

    // Select second span. We can move the start to include the first span.
    frame->executeScript(WebScriptSource("selectElement('header_2');"));
    EXPECT_EQ("Header 2.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "header_2")), topLeft(elementBounds(frame, "header_1")));
    EXPECT_EQ("Header 1. Header 2.", selectionAsString(frame));

    // We can move the start and end together.
    frame->executeScript(WebScriptSource("selectElement('header_1');"));
    EXPECT_EQ("Header 1.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "header_1")), bottomRightMinusOne(elementBounds(frame, "header_1")));
    EXPECT_EQ("", selectionAsString(frame));
    // Selection is a caret, not empty.
    EXPECT_FALSE(frame->selectionRange().isNull());

    // We can move the start across the end.
    frame->executeScript(WebScriptSource("selectElement('header_1');"));
    EXPECT_EQ("Header 1.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "header_1")), bottomRightMinusOne(elementBounds(frame, "header_2")));
    EXPECT_EQ(" Header 2.", selectionAsString(frame));

    // Can't extend the selection part-way into an editable element.
    frame->executeScript(WebScriptSource("selectElement('footer_2');"));
    EXPECT_EQ("Footer 2.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "footer_2")), topLeft(elementBounds(frame, "editable_2")));
    EXPECT_EQ(" [ Footer 1. Footer 2.", selectionAsString(frame));

    // Can extend the selection completely across editable elements.
    frame->executeScript(WebScriptSource("selectElement('footer_2');"));
    EXPECT_EQ("Footer 2.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "footer_2")), topLeft(elementBounds(frame, "header_2")));
    EXPECT_EQ("Header 2. ] [ Editable 1. Editable 2. ] [ Footer 1. Footer 2.", selectionAsString(frame));

    // If the selection is editable text, we can't extend it into non-editable text.
    frame->executeScript(WebScriptSource("selectElement('editable_2');"));
    EXPECT_EQ("Editable 2.", selectionAsString(frame));
    frame->selectRange(bottomRightMinusOne(elementBounds(frame, "editable_2")), topLeft(elementBounds(frame, "header_2")));
    EXPECT_EQ("[ Editable 1. Editable 2.", selectionAsString(frame));

    webView->close();
}

TEST_F(WebFrameTest, SelectRangeCanMoveSelectionEnd)
{
    registerMockedHttpURLLoad("text_selection.html");
    WebView* webView = createWebViewForTextSelection(m_baseURL + "text_selection.html");
    WebFrame* frame = webView->mainFrame();

    // Select first span. We can move the end to include the second span.
    frame->executeScript(WebScriptSource("selectElement('header_1');"));
    EXPECT_EQ("Header 1.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "header_1")), bottomRightMinusOne(elementBounds(frame, "header_2")));
    EXPECT_EQ("Header 1. Header 2.", selectionAsString(frame));

    // We can move the start and end together.
    frame->executeScript(WebScriptSource("selectElement('header_2');"));
    EXPECT_EQ("Header 2.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "header_2")), topLeft(elementBounds(frame, "header_2")));
    EXPECT_EQ("", selectionAsString(frame));
    // Selection is a caret, not empty.
    EXPECT_FALSE(frame->selectionRange().isNull());

    // We can move the end across the start.
    frame->executeScript(WebScriptSource("selectElement('header_2');"));
    EXPECT_EQ("Header 2.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "header_2")), topLeft(elementBounds(frame, "header_1")));
    EXPECT_EQ("Header 1. ", selectionAsString(frame));

    // Can't extend the selection part-way into an editable element.
    frame->executeScript(WebScriptSource("selectElement('header_1');"));
    EXPECT_EQ("Header 1.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "header_1")), bottomRightMinusOne(elementBounds(frame, "editable_1")));
    EXPECT_EQ("Header 1. Header 2. ] ", selectionAsString(frame));

    // Can extend the selection completely across editable elements.
    frame->executeScript(WebScriptSource("selectElement('header_1');"));
    EXPECT_EQ("Header 1.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "header_1")), bottomRightMinusOne(elementBounds(frame, "footer_1")));
    EXPECT_EQ("Header 1. Header 2. ] [ Editable 1. Editable 2. ] [ Footer 1.", selectionAsString(frame));

    // If the selection is editable text, we can't extend it into non-editable text.
    frame->executeScript(WebScriptSource("selectElement('editable_1');"));
    EXPECT_EQ("Editable 1.", selectionAsString(frame));
    frame->selectRange(topLeft(elementBounds(frame, "editable_1")), bottomRightMinusOne(elementBounds(frame, "footer_1")));
    EXPECT_EQ("Editable 1. Editable 2. ]", selectionAsString(frame));

    webView->close();
}

class DisambiguationPopupTestWebViewClient : public WebViewClient {
public:
    virtual bool didTapMultipleTargets(const WebGestureEvent&, const WebVector<WebRect>& targetRects) OVERRIDE
    {
        EXPECT_GE(targetRects.size(), 2u);
        m_triggered = true;
        return true;
    }

    bool triggered() const { return m_triggered; }
    void resetTriggered() { m_triggered = false; }
    bool m_triggered;
};

static WebGestureEvent fatTap(int x, int y)
{
    WebGestureEvent event;
    event.type = WebInputEvent::GestureTap;
    event.x = x;
    event.y = y;
    event.data.tap.width = 50;
    event.data.tap.height = 50;
    return event;
}

TEST_F(WebFrameTest, DisambiguationPopup)
{
    registerMockedHttpURLLoad("disambiguation_popup.html");

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(m_baseURL + "disambiguation_popup.html", true, 0, &client));
    webViewImpl->resize(WebSize(1000, 1000));
    webViewImpl->layout();

    client.resetTriggered();
    webViewImpl->handleInputEvent(fatTap(0, 0));
    EXPECT_FALSE(client.triggered());

    client.resetTriggered();
    webViewImpl->handleInputEvent(fatTap(200, 115));
    EXPECT_FALSE(client.triggered());

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        webViewImpl->handleInputEvent(fatTap(120, 230 + i * 5));

        int j = i % 10;
        if (j >= 7 && j <= 9)
            EXPECT_TRUE(client.triggered());
        else
            EXPECT_FALSE(client.triggered());
    }

    for (int i = 0; i <= 46; i++) {
        client.resetTriggered();
        webViewImpl->handleInputEvent(fatTap(10 + i * 5, 590));

        int j = i % 10;
        if (j >= 7 && j <= 9)
            EXPECT_TRUE(client.triggered());
        else
            EXPECT_FALSE(client.triggered());
    }

}

TEST_F(WebFrameTest, DisambiguationPopupNoContainer)
{
    registerMockedHttpURLLoad("disambiguation_popup_no_container.html");

    DisambiguationPopupTestWebViewClient client;

    // Make sure we initialize to minimum scale, even if the window size
    // only becomes available after the load begins.
    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(m_baseURL + "disambiguation_popup_no_container.html", true, 0, &client));
    webViewImpl->resize(WebSize(1000, 1000));
    webViewImpl->layout();

    client.resetTriggered();
    webViewImpl->handleInputEvent(fatTap(50, 50));
    EXPECT_FALSE(client.triggered());
}

class TestSubstituteDataWebFrameClient : public WebFrameClient {
public:
    TestSubstituteDataWebFrameClient()
        : m_commitCalled(false)
    {
    }

    virtual void didFailProvisionalLoad(WebFrame* frame, const WebURLError& error)
    {
        frame->loadHTMLString("This should appear", toKURL("data:text/html,chromewebdata"), error.unreachableURL, true);
        webkit_support::RunAllPendingMessages();
    }

    virtual void didCommitProvisionalLoad(WebFrame* frame, bool)
    {
        if (frame->dataSource()->response().url() != WebURL(URLTestHelpers::toKURL("about:blank")))
            m_commitCalled = true;
    }

    bool commitCalled() const { return m_commitCalled; }

private:
    bool m_commitCalled;
};

TEST_F(WebFrameTest, ReplaceNavigationAfterHistoryNavigation)
{
    TestSubstituteDataWebFrameClient webFrameClient;

    WebView* webView = FrameTestHelpers::createWebViewAndLoad("about:blank", true, &webFrameClient);
    webkit_support::RunAllPendingMessages();
    WebFrame* frame = webView->mainFrame();

    // Load a url as a history navigation that will return an error. TestSubstituteDataWebFrameClient
    // will start a SubstituteData load in response to the load failure, which should get fully committed.
    // Due to https://bugs.webkit.org/show_bug.cgi?id=91685, FrameLoader::didReceiveData() wasn't getting
    // called in this case, which resulted in the SubstituteData document not getting displayed.
    WebURLError error;
    error.reason = 1337;
    error.domain = "WebFrameTest";
    std::string errorURL = "http://0.0.0.0";
    WebURLResponse response;
    response.initialize();
    response.setURL(URLTestHelpers::toKURL(errorURL));
    response.setMIMEType("text/html");
    response.setHTTPStatusCode(500);
    WebHistoryItem errorHistoryItem;
    errorHistoryItem.initialize();
    errorHistoryItem.setURLString(WebString::fromUTF8(errorURL.c_str(), errorURL.length()));
    errorHistoryItem.setOriginalURLString(WebString::fromUTF8(errorURL.c_str(), errorURL.length()));
    webkit_support::RegisterMockedErrorURL(URLTestHelpers::toKURL(errorURL), response, error);
    frame->loadHistoryItem(errorHistoryItem);
    webkit_support::ServeAsynchronousMockedRequests();

    WebString text = frame->contentAsText(std::numeric_limits<size_t>::max());
    EXPECT_EQ("This should appear", std::string(text.utf8().data()));
    EXPECT_TRUE(webFrameClient.commitCalled());
}

} // namespace
