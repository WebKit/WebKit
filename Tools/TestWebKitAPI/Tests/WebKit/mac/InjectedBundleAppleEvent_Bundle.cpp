/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if WK_HAVE_C_SPI

#include "InjectedBundleTest.h"
#include "PlatformUtilities.h"
#include <Carbon/Carbon.h> // Needed for AppleEvents
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

class InjectedBundleAppleEventTest : public InjectedBundleTest {
public:
    InjectedBundleAppleEventTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

    virtual void didCreatePage(WKBundleRef bundle, WKBundlePageRef page)
    {
        FourCharCode appAddress = 'lgnw';
        const char* eventFormat = "'----':TEXT(@)";
        AppleEvent event;

        OSStatus rc = AEBuildAppleEvent('syso', 'gurl', 'sign', &appAddress, sizeof(appAddress), kAutoGenerateReturnID, 0, &event, nullptr, eventFormat, "file:///does-not-exist.html");
        assert(!rc);

        rc = AESendMessage(&event, nullptr, -1, 0);

        WKRetainPtr<WKDoubleRef> returnCode = adoptWK(WKDoubleCreate(rc));
        WKBundlePostMessage(bundle, Util::toWK("DidAttemptToSendAppleEvent").get(), returnCode.get());
    }
};

static InjectedBundleTest::Register<InjectedBundleAppleEventTest> registrar("InjectedBundleAppleEventTest");

} // namespace TestWebKitAPI

#endif
