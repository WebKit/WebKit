/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#import <WebKit/WKProcessPoolPrivate.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>
#import <wtf/RetainPtr.h>

template<typename T>
RetainPtr<T> encodeAndDecode(T* t)
{
    if ([t conformsToProtocol:@protocol(NSSecureCoding)]) {
        auto data = securelyArchivedDataWithRootObject(t);
        return unarchivedObjectOfClassesFromData([NSSet setWithObjects:[t class], nil], data);
    }

    auto data = insecurelyArchivedDataWithRootObject(t);
    return insecurelyUnarchiveObjectFromData(data);
}

TEST(Coding, WKPreferences)
{
    auto a = adoptNS([[WKPreferences alloc] init]);

    // Change all defaults to something else.
    [a setMinimumFontSize:10];
    [a setJavaScriptEnabled:NO];
#if PLATFORM(IOS_FAMILY)
    [a setJavaScriptCanOpenWindowsAutomatically:YES];
#else
    [a setJavaScriptCanOpenWindowsAutomatically:NO];
    [a setJavaEnabled:YES];
    [a setPlugInsEnabled:YES];
    [a setTabFocusesLinks:YES];
#endif

    auto b = encodeAndDecode(a.get());

    EXPECT_EQ([a minimumFontSize], [b minimumFontSize]);
    EXPECT_EQ([a javaScriptEnabled], [b javaScriptEnabled]);
    EXPECT_EQ([a javaScriptCanOpenWindowsAutomatically], [b javaScriptCanOpenWindowsAutomatically]);

#if PLATFORM(MAC)
    EXPECT_EQ([a javaEnabled], [b javaEnabled]);
    EXPECT_EQ([a plugInsEnabled], [b plugInsEnabled]);
    EXPECT_EQ([a tabFocusesLinks], [b tabFocusesLinks]);
#endif
}

TEST(Coding, WKProcessPool_Shared)
{
    auto a = encodeAndDecode([WKProcessPool _sharedProcessPool]);
    EXPECT_EQ([WKProcessPool _sharedProcessPool], a.get());
}

TEST(Coding, WKProcessPool)
{
    auto a = encodeAndDecode(adoptNS([[WKProcessPool alloc] init]).get());
    EXPECT_NE([WKProcessPool _sharedProcessPool], a.get());
}

TEST(Coding, WKWebsiteDataStore_Default)
{
    auto a = encodeAndDecode([WKWebsiteDataStore defaultDataStore]);
    EXPECT_EQ([WKWebsiteDataStore defaultDataStore], a.get());
}

TEST(Coding, WKWebsiteDataStore_NonPersistent)
{
    auto a = encodeAndDecode([WKWebsiteDataStore nonPersistentDataStore]);
    EXPECT_FALSE([a isPersistent]);
}

TEST(Coding, WKWebViewConfiguration)
{
    auto a = adoptNS([[WKWebViewConfiguration alloc] init]);

    // Change all defaults to something else.
    [a setSuppressesIncrementalRendering:YES];
    [a setApplicationNameForUserAgent:@"Application Name"];
    [a setAllowsAirPlayForMediaPlayback:NO];

#if PLATFORM(IOS_FAMILY)
    [a setDataDetectorTypes:WKDataDetectorTypeAll];
    [a setAllowsInlineMediaPlayback:YES];
    [a setRequiresUserActionForMediaPlayback:NO];
    [a setSelectionGranularity:WKSelectionGranularityCharacter];
    [a setAllowsPictureInPictureMediaPlayback:NO];
#endif

    auto b = encodeAndDecode(a.get());

    EXPECT_EQ([a suppressesIncrementalRendering], [b suppressesIncrementalRendering]);
    EXPECT_TRUE([[a applicationNameForUserAgent] isEqualToString:[b applicationNameForUserAgent]]);
    EXPECT_EQ([a allowsAirPlayForMediaPlayback], [b allowsAirPlayForMediaPlayback]);

#if PLATFORM(IOS_FAMILY)
    EXPECT_EQ([a dataDetectorTypes], [b dataDetectorTypes]);
    EXPECT_EQ([a allowsInlineMediaPlayback], [b allowsInlineMediaPlayback]);
    EXPECT_EQ([a requiresUserActionForMediaPlayback], [b requiresUserActionForMediaPlayback]);
    EXPECT_EQ([a selectionGranularity], [b selectionGranularity]);
    EXPECT_EQ([a allowsPictureInPictureMediaPlayback], [b allowsPictureInPictureMediaPlayback]);
#endif
}

TEST(Coding, WKWebView)
{
    auto a = adoptNS([[WKWebView alloc] init]);

    // Change all defaults to something else.
    [a setAllowsBackForwardNavigationGestures:YES];
    [a setCustomUserAgent:@"CustomUserAgent"];

#if PLATFORM(IOS_FAMILY)
    [a setAllowsLinkPreview:YES];
#else
    [a setAllowsLinkPreview:NO];
    [a setAllowsMagnification:YES];
    [a setMagnification:2];
#endif

    auto b = encodeAndDecode(a.get());

    EXPECT_EQ([a allowsBackForwardNavigationGestures], [b allowsBackForwardNavigationGestures]);
    EXPECT_TRUE([[a customUserAgent] isEqualToString:[b customUserAgent]]);
    EXPECT_EQ([a allowsLinkPreview], [b allowsLinkPreview]);

#if PLATFORM(MAC)
    EXPECT_EQ([a allowsMagnification], [b allowsMagnification]);
    EXPECT_EQ([a magnification], [b magnification]);
#endif
}

TEST(Coding, WKWebView_SameConfiguration)
{
    // First, encode two WKWebViews sharing the same configuration.
    RetainPtr<NSData> data;
    {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);

        auto a = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
        auto b = adoptNS([[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);

        data = securelyArchivedDataWithRootObject(@[a.get(), b.get()]);
    }

    // Then, decode and verify that the important configuration properties are the same.
    NSArray *array = unarchivedObjectOfClassesFromData([NSSet setWithObject:[NSArray class]], data.get());

    WKWebView *aView = array[0];
    WKWebView *bView = array[1];

    WKWebViewConfiguration *a = aView.configuration;
    WKWebViewConfiguration *b = bView.configuration;

    EXPECT_EQ(a.processPool, b.processPool);
    EXPECT_EQ(a.preferences, b.preferences);
    EXPECT_EQ(a.userContentController, b.userContentController);
    EXPECT_EQ(a.websiteDataStore, b.websiteDataStore);
}
