/*
 * Copyright (C) 2016 Igalia S.L.
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
#include "Test.h"
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

class ProvisionalURLAfterWillSendRequestCallbackTest : public InjectedBundleTest {
public:
    ProvisionalURLAfterWillSendRequestCallbackTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

    static WKURLRequestRef willSendRequestForFrame(WKBundlePageRef, WKBundleFrameRef frame, uint64_t resourceIdentifier, WKURLRequestRef request, WKURLResponseRef redirectResponse, const void*)
    {
        if (!WKBundleFrameIsMainFrame(frame)) {
            WKRetainPtr<WKURLRequestRef> newRequest = request;
            return newRequest.leakRef();
        }

        // Change the main frame URL.
        WKRetainPtr<WKURLRef> url = adoptWK(Util::createURLForResource("simple2", "html"));
        return WKURLRequestCreateWithWKURL(url.get());
    }

    static void didCommitLoadForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKTypeRef* userData, const void*)
    {
        if (!WKBundleFrameIsMainFrame(frame))
            return;
        *userData = WKBundleFrameCopyURL(frame);
    }

    void didCreatePage(WKBundleRef bundle, WKBundlePageRef page) override
    {
        WKBundlePageResourceLoadClientV0 resourceLoadClient;
        memset(&resourceLoadClient, 0, sizeof(resourceLoadClient));
        resourceLoadClient.base.version = 0;
        resourceLoadClient.willSendRequestForFrame = willSendRequestForFrame;
        WKBundlePageSetResourceLoadClient(page, &resourceLoadClient.base);

        WKBundlePageLoaderClientV0 pageLoaderClient;
        memset(&pageLoaderClient, 0, sizeof(pageLoaderClient));
        pageLoaderClient.base.version = 0;
        pageLoaderClient.didCommitLoadForFrame = didCommitLoadForFrame;
        WKBundlePageSetPageLoaderClient(page, &pageLoaderClient.base);
    }
};

static InjectedBundleTest::Register<ProvisionalURLAfterWillSendRequestCallbackTest> registrar("ProvisionalURLAfterWillSendRequestCallbackTest");

} // namespace TestWebKitAPI

#endif
