/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "HostWindow.h"
#include "Test.h"
#include <WebCore/COMPtr.h>
#include <WebKitLegacy/WebKit.h>
#include <WebKitLegacy/WebKitCOMAPI.h>

namespace TestWebKitAPI {

template <typename T>
static HRESULT WebKitCreateInstance(REFCLSID clsid, T** object)
{
    return WebKitCreateInstance(clsid, 0, __uuidof(T), reinterpret_cast<void**>(object));
}

class ScaleWebView : public ::testing::Test {
protected:
    virtual void SetUp();
    virtual void TearDown();

    COMPtr<IWebView> m_webView;
    HostWindow m_window;
};

void ScaleWebView::SetUp()
{
    EXPECT_HRESULT_SUCCEEDED(WebKitCreateInstance(__uuidof(WebView), &m_webView));
    EXPECT_TRUE(m_window.initialize());
    EXPECT_HRESULT_SUCCEEDED(m_webView->setHostWindow(m_window.window()));
}

void ScaleWebView::TearDown()
{
    m_webView = 0;
    shutDownWebKit();
}

// Tests that scaling a WebView before calling IWebView::initWithFrame doesn't crash.
TEST_F(ScaleWebView, ScaleBeforeInitWithFrame)
{
    COMPtr<IWebViewPrivate2> viewPrivate(Query, m_webView);
    ASSERT_NOT_NULL(viewPrivate);
    EXPECT_HRESULT_SUCCEEDED(viewPrivate->setCustomBackingScaleFactor(2));
    EXPECT_HRESULT_SUCCEEDED(m_webView->initWithFrame(m_window.clientRect(), 0, 0));
}

} // namespace WebKitAPITest
