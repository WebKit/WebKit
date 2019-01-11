/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(DARK_MODE_CSS) && WK_HAVE_C_SPI

#include "InjectedBundleTest.h"

#include "PlatformUtilities.h"
#include <JavaScriptCore/JSRetainPtr.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundlePagePrivate.h>

namespace TestWebKitAPI {

class ForceLightAppearanceInBundleTest : public InjectedBundleTest {
public:
    ForceLightAppearanceInBundleTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

    virtual void didCreatePage(WKBundleRef bundle, WKBundlePageRef page)
    {
        m_page = page;
    }

    virtual void didReceiveMessage(WKBundleRef bundle, WKStringRef messageName, WKTypeRef messageBody)
    {
        if (!WKStringIsEqualToUTF8CString(messageName, "RunTest"))
            return;

        if (!WKBundlePageIsUsingDarkAppearance(m_page))
            WKBundlePostMessage(bundle, Util::toWK("TestFailed").get(), Util::toWK("Page isn't in dark mode at start of test.").get());

        WKBundlePageSetUseDarkAppearance(m_page, false);

        if (WKBundlePageIsUsingDarkAppearance(m_page))
            WKBundlePostMessage(bundle, Util::toWK("TestFailed").get(), Util::toWK("Switching to light mode failed.").get());

        auto mainFrame = WKBundlePageGetMainFrame(m_page);
        auto scriptContext = WKBundleFrameGetJavaScriptContext(mainFrame);
        auto script = adopt(JSStringCreateWithUTF8CString("window.getComputedStyle(document.body).getPropertyValue('color')"));

        auto result = JSEvaluateScript(scriptContext, script.get(), nullptr, nullptr, 0, nullptr);
        auto resultString = adopt(JSValueToStringCopy(scriptContext, result, nullptr));

        auto bufferSize = JSStringGetMaximumUTF8CStringSize(resultString.get());
        auto buffer = std::make_unique<char[]>(bufferSize);
        JSStringGetUTF8CString(resultString.get(), buffer.get(), bufferSize);

        WKBundlePageSetUseDarkAppearance(m_page, true);

        if (!WKBundlePageIsUsingDarkAppearance(m_page))
            WKBundlePostMessage(bundle, Util::toWK("TestFailed").get(), Util::toWK("Switching back to dark mode failed.").get());

        WKBundlePostMessage(bundle, Util::toWK("TestDone").get(), Util::toWK(buffer.get()).get());
    }

private:
    WKBundlePageRef m_page { nullptr };
};

static InjectedBundleTest::Register<ForceLightAppearanceInBundleTest> registrar("ForceLightAppearanceInBundleTest");

} // namespace TestWebKitAPI

#endif
