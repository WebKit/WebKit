/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include "Test.h"
#include <WebKit/WKContextPrivate.h>
#include <WebKit/WKRetainPtr.h>

namespace TestWebKitAPI {

static bool didHitRelevantRepaintedObjectsAreaThresholdMoreThanOnce;
static unsigned didHitRelevantRepaintedObjectsAreaThresholdCounter;
static bool test1Done;
static bool test2Done;
    
static void didForceRepaint(WKErrorRef error, void*)
{
    EXPECT_NULL(error);
    test2Done = true;
}

static void didFinishNavigation(WKPageRef page, WKNavigationRef, WKTypeRef userData, const void* clientInfo)
{
    test1Done = true;
    WKPageForceRepaint(page, 0, didForceRepaint);
}

static void renderingProgressDidChange(WKPageRef, WKPageRenderingProgressEvents type, WKTypeRef, const void *)
{
    if (type != WKPageRenderingProgressEventFirstPaintWithSignificantArea)
        return;

    ++didHitRelevantRepaintedObjectsAreaThresholdCounter;
    if (didHitRelevantRepaintedObjectsAreaThresholdCounter > 1)
        didHitRelevantRepaintedObjectsAreaThresholdMoreThanOnce = true;
}

static void setPageLoaderClient(WKPageRef page)
{
    WKPageNavigationClientV3 loaderClient;
    memset(&loaderClient, 0, sizeof(loaderClient));

    loaderClient.base.version = 3;
    loaderClient.didFinishNavigation = didFinishNavigation;
    loaderClient.renderingProgressDidChange = renderingProgressDidChange;

    WKPageSetPageNavigationClient(page, &loaderClient.base);
}

TEST(WebKit, NewFirstVisuallyNonEmptyLayoutFrames)
{
    didHitRelevantRepaintedObjectsAreaThresholdCounter = 0;
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("NewFirstVisuallyNonEmptyLayoutFramesTest"));

    PlatformWebView webView(context.get());
    setPageLoaderClient(webView.page());

    WKPageLoadURL(webView.page(), adoptWK(Util::createURLForResource("lots-of-iframes", "html")).get());

    Util::run(&test1Done);
    Util::run(&test2Done);

    // By the time the forced repaint has finished, the counter would have been hit 
    // if it was sized reasonably for the page.
    EXPECT_FALSE(didHitRelevantRepaintedObjectsAreaThresholdMoreThanOnce);
}

} // namespace TestWebKitAPI

#endif
