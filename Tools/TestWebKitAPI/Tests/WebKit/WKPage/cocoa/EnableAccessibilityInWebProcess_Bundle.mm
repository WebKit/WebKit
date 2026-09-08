/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "config.h"

#if WK_HAVE_C_SPI

#import "Helpers/PlatformUtilities.h"
#import "InjectedBundleTest.h"
#import <WebKit/WKBundlePage.h>
#import <WebKit/WKBundlePagePrivate.h>

namespace TestWebKitAPI {

// Enables accessibility inside the WebProcess on request, the way an assistive technology attaching
// to web content does.
//
// The enable deliberately happens in response to a message rather than in didCreatePage, so the test
// controls the timing. Enabling at page-creation time would transition the process before the test's
// second page exists, and that page's constructor would then pick the mode up on its own, which masks
// the defect this exists to catch.
class EnableAccessibilityInWebProcessTest : public InjectedBundleTest {
public:
    EnableAccessibilityInWebProcessTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

    virtual void didReceiveMessage(WKBundleRef bundle, WKStringRef messageName, WKTypeRef)
    {
        if (!WKStringIsEqualToUTF8CString(messageName, "EnableAccessibility"))
            return;

        WKAccessibilityEnable();
        WKBundlePostMessage(bundle, Util::toWK("DidEnableAccessibility").get(), nullptr);
    }
};

static InjectedBundleTest::Register<EnableAccessibilityInWebProcessTest> registrar("EnableAccessibilityInWebProcessTest");

} // namespace TestWebKitAPI

#endif // WK_HAVE_C_SPI
