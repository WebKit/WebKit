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

#import "config.h"

#if WK_API_ENABLED

#import <wtf/RetainPtr.h>

TEST(Copying, WKPreferences)
{
    // Change all defaults to something else.
    RetainPtr<WKPreferences> a = adoptNS([[WKPreferences alloc] init]);
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

    // Check that values are equal in both instances.
    RetainPtr<WKPreferences> b = adoptNS([a copy]);
    EXPECT_EQ([a minimumFontSize], [b minimumFontSize]);
    EXPECT_EQ([a javaScriptEnabled], [b javaScriptEnabled]);
    EXPECT_EQ([a javaScriptCanOpenWindowsAutomatically], [b javaScriptCanOpenWindowsAutomatically]);
#if PLATFORM(MAC)
    EXPECT_EQ([a javaEnabled], [b javaEnabled]);
    EXPECT_EQ([a plugInsEnabled], [b plugInsEnabled]);
    EXPECT_EQ([a tabFocusesLinks], [b tabFocusesLinks]);
#endif

    // Change all defaults on the copied instance.
    [b setMinimumFontSize:13];
    [b setJavaScriptEnabled:YES];
#if PLATFORM(IOS_FAMILY)
    [b setJavaScriptCanOpenWindowsAutomatically:NO];
#else
    [b setJavaScriptCanOpenWindowsAutomatically:YES];
    [b setJavaEnabled:NO];
    [b setPlugInsEnabled:NO];
    [b setTabFocusesLinks:NO];
#endif

    // Check that the mutations of 'b' did not affect 'a'.
    EXPECT_NE([a minimumFontSize], [b minimumFontSize]);
    EXPECT_NE([a javaScriptEnabled], [b javaScriptEnabled]);
    EXPECT_NE([a javaScriptCanOpenWindowsAutomatically], [b javaScriptCanOpenWindowsAutomatically]);
#if PLATFORM(MAC)
    EXPECT_NE([a javaEnabled], [b javaEnabled]);
    EXPECT_NE([a plugInsEnabled], [b plugInsEnabled]);
    EXPECT_NE([a tabFocusesLinks], [b tabFocusesLinks]);
#endif

}

#endif
