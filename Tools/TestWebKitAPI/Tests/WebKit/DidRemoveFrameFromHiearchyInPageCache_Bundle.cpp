/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include <WebKit/WKBundleFrame.h>
#include <WebKit/WKBundlePage.h>

namespace TestWebKitAPI {

class DidRemoveFrameFromHiearchyInPageCacheTest : public InjectedBundleTest {
public:
    DidRemoveFrameFromHiearchyInPageCacheTest(const std::string& identifier);

    virtual void didCreatePage(WKBundleRef, WKBundlePageRef);
};

static InjectedBundleTest::Register<DidRemoveFrameFromHiearchyInPageCacheTest> registrar("DidRemoveFrameFromHiearchyInPageCache");

static unsigned didRemoveFrameFromHierarchyCount;

void didRemoveFrameFromHierarchyCallback(WKBundlePageRef page, WKBundleFrameRef frame, WKTypeRef*, const void*)
{
    didRemoveFrameFromHierarchyCount++;

    RELEASE_ASSERT(!WKBundleFrameGetParentFrame(frame));

    WKRetainPtr<WKStringRef> message(AdoptWK, WKStringCreateWithUTF8CString("DidRemoveFrameFromHierarchy"));
    WKBundlePagePostMessage(page, message.get(), message.get());
}

DidRemoveFrameFromHiearchyInPageCacheTest::DidRemoveFrameFromHiearchyInPageCacheTest(const std::string& identifier)
    : InjectedBundleTest(identifier)
{
}

void DidRemoveFrameFromHiearchyInPageCacheTest::didCreatePage(WKBundleRef bundle, WKBundlePageRef page)
{
    WKBundlePageLoaderClientV8 pageLoaderClient;
    memset(&pageLoaderClient, 0, sizeof(pageLoaderClient));

    pageLoaderClient.base.version = 8;
    pageLoaderClient.base.clientInfo = this;
    pageLoaderClient.didRemoveFrameFromHierarchy = didRemoveFrameFromHierarchyCallback;

    WKBundlePageSetPageLoaderClient(page, &pageLoaderClient.base);
}

} // namespace TestWebKitAPI

#endif
