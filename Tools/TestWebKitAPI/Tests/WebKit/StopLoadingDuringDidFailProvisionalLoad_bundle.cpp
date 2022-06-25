/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundleFramePrivate.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundlePagePrivate.h>

namespace TestWebKitAPI {

class StopLoadingDuringDidFailProvisionalLoadTest : public InjectedBundleTest {
public:
    StopLoadingDuringDidFailProvisionalLoadTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

    void didCreatePage(WKBundleRef, WKBundlePageRef) override;
    void didFailProvisionalLoad(WKBundlePageRef, WKBundleFrameRef);

    WKBundleRef m_bundle;
};

static InjectedBundleTest::Register<StopLoadingDuringDidFailProvisionalLoadTest> registrar("StopLoadingDuringDidFailProvisionalLoadTest");

static void didFailProvisionalLoadWithErrorForFrameCallback(WKBundlePageRef page, WKBundleFrameRef frame, WKErrorRef, WKTypeRef*, const void *clientInfo)
{
    ((StopLoadingDuringDidFailProvisionalLoadTest*)clientInfo)->didFailProvisionalLoad(page, frame);
}

void StopLoadingDuringDidFailProvisionalLoadTest::didCreatePage(WKBundleRef bundle, WKBundlePageRef page)
{
    m_bundle = bundle;

    WKBundlePageLoaderClientV2 pageLoaderClient;
    memset(&pageLoaderClient, 0, sizeof(pageLoaderClient));

    pageLoaderClient.base.version = 2;
    pageLoaderClient.base.clientInfo = this;
    pageLoaderClient.didFailProvisionalLoadWithErrorForFrame = didFailProvisionalLoadWithErrorForFrameCallback;

    WKBundlePageSetPageLoaderClient(page, &pageLoaderClient.base);
}

void StopLoadingDuringDidFailProvisionalLoadTest::didFailProvisionalLoad(WKBundlePageRef page, WKBundleFrameRef)
{
    WKBundlePageStopLoading(page);
    WKBundlePostMessage(m_bundle, Util::toWK("StopLoadingDuringDidFailProvisionalLoadTestDone").get(), nullptr);
}

} // namespace TestWebKitAPI

#endif
