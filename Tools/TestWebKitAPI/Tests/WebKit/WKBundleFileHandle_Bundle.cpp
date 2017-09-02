/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundleFileHandleRef.h>
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundleScriptWorld.h>

namespace TestWebKitAPI {

static WKBundlePageRef loadedPage;

class WKBundleFileHandleTest : public InjectedBundleTest {
public:
    WKBundleFileHandleTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

private:
    void didReceiveMessage(WKBundleRef bundle, WKStringRef messageName, WKTypeRef messageBody) override
    {
        if (!WKStringIsEqualToUTF8CString(messageName, "TestFile")) {
            WKBundlePostMessage(bundle, Util::toWK("FAIL").get(), Util::toWK("Recieved invalid message").get());
            return;
        }

        if (!loadedPage) {
            WKBundlePostMessage(bundle, Util::toWK("FAIL").get(), Util::toWK("No loaded page").get());
            return;
        }

        if (WKGetTypeID(messageBody) != WKStringGetTypeID()) {
            WKBundlePostMessage(bundle, Util::toWK("FAIL").get(), Util::toWK("Message body has invalid type").get());
            return;
        }

        WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(loadedPage);
        WKBundleScriptWorldRef world = WKBundleScriptWorldNormalWorld();
        
        JSGlobalContextRef globalContext = WKBundleFrameGetJavaScriptContextForWorld(mainFrame, world);

        auto fileHandle = adoptWK(WKBundleFileHandleCreateWithPath((WKStringRef)messageBody));
        JSValueRef jsFileHandle = WKBundleFrameGetJavaScriptWrapperForFileForWorld(mainFrame, fileHandle.get(), world);

        JSObjectRef globalObject = JSContextGetGlobalObject(globalContext);

        JSStringRef jsString = JSStringCreateWithUTF8CString("testFile");
        JSValueRef function = JSObjectGetProperty(globalContext, globalObject, jsString, nullptr);
        JSStringRelease(jsString);
        
        JSValueRef result = JSObjectCallAsFunction(globalContext, (JSObjectRef)function, globalObject, 1, &jsFileHandle, nullptr);

        if (JSValueToBoolean(globalContext, result))
            WKBundlePostMessage(bundle, Util::toWK("SUCCESS").get(), nullptr);
        else
            WKBundlePostMessage(bundle, Util::toWK("FAIL").get(), Util::toWK("Script failed").get());
    }

    void didCreatePage(WKBundleRef bundle, WKBundlePageRef page) override
    {
        loadedPage = page;
    }
};

static InjectedBundleTest::Register<WKBundleFileHandleTest> registrar("WKBundleFileHandleTest");

} // namespace TestWebKitAPI

#endif
