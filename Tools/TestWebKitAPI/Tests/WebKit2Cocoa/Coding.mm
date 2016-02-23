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

#import "config.h"

#if WK_API_ENABLED

#import <WebKit/WKPreferences.h>
#import <wtf/RetainPtr.h>

template<typename T>
RetainPtr<T> encodeAndDecode(T* t)
{
    auto data = [NSKeyedArchiver archivedDataWithRootObject:t];

    return [NSKeyedUnarchiver unarchiveObjectWithData:data];
}

TEST(Coding, WKPreferences)
{
    auto a = adoptNS([[WKPreferences alloc] init]);

    // Change all defaults to something else.
    [a setMinimumFontSize:10];
    [a setJavaScriptEnabled:NO];
#if PLATFORM(IOS)
    [a setJavaScriptCanOpenWindowsAutomatically:YES];
#else
    [a setJavaScriptCanOpenWindowsAutomatically:NO];
    [a setJavaEnabled:YES];
    [a setPlugInsEnabled:YES];
#endif

    auto b = encodeAndDecode(a.get());

    EXPECT_EQ([a minimumFontSize], [b minimumFontSize]);
    EXPECT_EQ([a javaScriptEnabled], [b javaScriptEnabled]);
    EXPECT_EQ([a javaScriptCanOpenWindowsAutomatically], [b javaScriptCanOpenWindowsAutomatically]);

#if PLATFORM(MAC)
    EXPECT_EQ([a javaEnabled], [b javaEnabled]);
    EXPECT_EQ([a plugInsEnabled], [b plugInsEnabled]);
#endif
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
    [a setDataDetectorTypes:WKDataDetectorTypeAll];

#if PLATFORM(IOS)
    [a setAllowsInlineMediaPlayback:YES];
    [a setRequiresUserActionForMediaPlayback:NO];
    [a setSelectionGranularity:WKSelectionGranularityCharacter];
    [a setAllowsPictureInPictureMediaPlayback:NO];
#endif

    auto b = encodeAndDecode(a.get());

    EXPECT_EQ([a suppressesIncrementalRendering], [b suppressesIncrementalRendering]);
    EXPECT_TRUE([[a applicationNameForUserAgent] isEqualToString:[b applicationNameForUserAgent]]);
    EXPECT_EQ([a allowsAirPlayForMediaPlayback], [b allowsAirPlayForMediaPlayback]);
    EXPECT_EQ([a dataDetectorTypes], [b dataDetectorTypes]);

#if PLATFORM(IOS)
    EXPECT_EQ([a allowsInlineMediaPlayback], [b allowsInlineMediaPlayback]);
    EXPECT_EQ([a requiresUserActionForMediaPlayback], [b requiresUserActionForMediaPlayback]);
    EXPECT_EQ([a selectionGranularity], [b selectionGranularity]);
    EXPECT_EQ([a allowsPictureInPictureMediaPlayback], [b allowsPictureInPictureMediaPlayback]);
#endif
}
#endif
