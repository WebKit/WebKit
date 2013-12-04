/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformUtilities.h"
#include "Test.h"

#include <WebKit2/WKContext.h>
#include <WebKit2/WKPage.h>
#include <WebKit2/WKRetainPtr.h>
#include <WebKit2/WKView.h>

namespace TestWebKitAPI {

static bool didWebProcessCrash = false;
static bool didWebProcessRelaunch = false;
static bool didFinishLoad = false;

static void didFinishLoadForFrame(WKPageRef, WKFrameRef, WKTypeRef, const void*)
{
    didFinishLoad = true;
}

static void webProcessCrashed(WKViewRef view, WKURLRef, const void*)
{
    // WebProcess crashed, so at this point the view should not be active.
    ASSERT_FALSE(WKViewIsActive(view));
    didWebProcessCrash = true;
}

static void webProcessRelaunched(WKViewRef view, const void*)
{
    // WebProcess just relaunched, so at this point the view should not be active.
    ASSERT_FALSE(WKViewIsActive(view));

    didWebProcessRelaunch = true;
}

TEST(WebKit2, WKViewIsActiveSetIsActive)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreate());
    WKRetainPtr<WKViewRef> view = adoptWK(WKViewCreate(context.get(), 0));

    WKViewInitialize(view.get());

    // At this point we should have an active view.
    ASSERT_TRUE(WKViewIsActive(view.get()));

    // Now we are going to play with its active state a few times.
    WKViewSetIsActive(view.get(), true);
    ASSERT_TRUE(WKViewIsActive(view.get()));

    WKViewSetIsActive(view.get(), false);
    ASSERT_FALSE(WKViewIsActive(view.get()));

    WKViewSetIsActive(view.get(), false);
    ASSERT_FALSE(WKViewIsActive(view.get()));

    WKViewSetIsActive(view.get(), true);
    ASSERT_TRUE(WKViewIsActive(view.get()));
}

TEST(WebKit2, WKViewIsActive)
{
    WKRetainPtr<WKContextRef> context = adoptWK(Util::createContextForInjectedBundleTest("WKViewIsActiveSetIsActiveTest"));
    WKRetainPtr<WKViewRef> view = adoptWK(WKViewCreate(context.get(), 0));

    WKViewClientV0 viewClient;
    memset(&viewClient, 0, sizeof(WKViewClientV0));
    viewClient.base.version = 0;
    viewClient.webProcessCrashed = webProcessCrashed;
    viewClient.webProcessDidRelaunch = webProcessRelaunched;
    WKViewSetViewClient(view.get(), &viewClient.base);

    WKViewInitialize(view.get());

    // At this point we should have an active view.
    ASSERT_TRUE(WKViewIsActive(view.get()));

    WKPageLoaderClientV3 pageLoaderClient;
    memset(&pageLoaderClient, 0, sizeof(WKPageLoaderClient));
    pageLoaderClient.base.version = 3;
    pageLoaderClient.didFinishLoadForFrame = didFinishLoadForFrame;
    WKPageSetPageLoaderClient(WKViewGetPage(view.get()), &pageLoaderClient.base);

    const WKSize size = WKSizeMake(100, 100);
    WKViewSetSize(view.get(), size);

    didFinishLoad = false;
    didWebProcessCrash = false;
    didWebProcessRelaunch = false;

    WKRetainPtr<WKURLRef> simpleUrl = adoptWK(Util::createURLForResource("../WebKit2/simple", "html"));
    WKPageLoadURL(WKViewGetPage(view.get()), simpleUrl.get());
    Util::run(&didFinishLoad);
    didFinishLoad = false;

    WKContextPostMessageToInjectedBundle(context.get(), Util::toWK("Crash").get(), 0);
    Util::run(&didWebProcessCrash);
    ASSERT_TRUE(didWebProcessCrash);

    WKPageLoadURL(WKViewGetPage(view.get()), simpleUrl.get());
    Util::run(&didFinishLoad);

    ASSERT_TRUE(didWebProcessRelaunch);
    ASSERT_TRUE(didFinishLoad);
}

} // TestWebKitAPI
