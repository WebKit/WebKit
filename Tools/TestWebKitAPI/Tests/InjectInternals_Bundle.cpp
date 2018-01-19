/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "InjectedBundleTest.h"

#import "WebCoreTestSupport.h"
#import <WebKit/WKBundle.h>
#import <WebKit/WKBundleFrame.h>
#import <WebKit/WKBundlePage.h>

namespace TestWebKitAPI {

class InternalsInjectedBundleTest : public TestWebKitAPI::InjectedBundleTest {
public:
    InternalsInjectedBundleTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

private:
    virtual void initialize(WKBundleRef bundle, WKTypeRef)
    {
        WKBundleSetServiceWorkerProxyCreationCallback(bundle, WebCoreTestSupport::setupNewlyCreatedServiceWorker);
    }

    virtual void didCreatePage(WKBundleRef, WKBundlePageRef page)
    {
        WKBundlePageLoaderClientV0 loaderClient;
        memset(&loaderClient, 0, sizeof(loaderClient));

        loaderClient.base.version = 0;
        loaderClient.base.clientInfo = this;
        loaderClient.didClearWindowObjectForFrame = didClearWindowForFrame;

        WKBundlePageSetPageLoaderClient(page, &loaderClient.base);
    }
    static void didClearWindowForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKBundleScriptWorldRef world, const void*)
    {
        JSGlobalContextRef context = WKBundleFrameGetJavaScriptContextForWorld(frame, world);
        WebCoreTestSupport::injectInternalsObject(context);
    }
};

static TestWebKitAPI::InjectedBundleTest::Register<InternalsInjectedBundleTest> registrar("InternalsInjectedBundleTest");

} // namespace TestWebKitAPI

#endif
