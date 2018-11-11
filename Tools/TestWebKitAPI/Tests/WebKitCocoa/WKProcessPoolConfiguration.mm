/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import <WebKit/WKFoundation.h>

#if WK_API_ENABLED

#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/RetainPtr.h>

TEST(WKProcessPoolConfiguration, Copy)
{
    auto configuration = adoptNS([[_WKProcessPoolConfiguration alloc] init]);

    [configuration setInjectedBundleURL:[NSURL fileURLWithPath:@"/path/to/injected.wkbundle"]];
    [configuration setMaximumProcessCount:42];
    [configuration setCustomWebContentServiceBundleIdentifier:@"org.webkit.WebContent.custom"];
    [configuration setIgnoreSynchronousMessagingTimeoutsForTesting:YES];
    [configuration setAttrStyleEnabled:YES];
    [configuration setAdditionalReadAccessAllowedURLs:@[ [NSURL fileURLWithPath:@"/path/to/allow/read/access/"] ]];
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    [configuration setWirelessContextIdentifier:25];
#endif
    [configuration setDiskCacheSizeOverride:42000];
    [configuration setCachePartitionedURLSchemes:@[ @"ssh", @"vnc" ]];
    [configuration setAlwaysRevalidatedURLSchemes:@[ @"afp", @"smb" ]];
    [configuration setDiskCacheSpeculativeValidationEnabled:YES];
    [configuration setShouldCaptureAudioInUIProcess:YES];
#if PLATFORM(IOS_FAMILY)
    [configuration setCTDataConnectionServiceType:@"best"];
    [configuration setAlwaysRunsAtBackgroundPriority:YES];
    [configuration setShouldTakeUIBackgroundAssertion:YES];
#endif
    [configuration setPresentingApplicationPID:1000];
    [configuration setProcessSwapsOnNavigation:YES];
    [configuration setAlwaysKeepAndReuseSwappedProcesses:YES];
    [configuration setProcessSwapsOnWindowOpenWithOpener:YES];
    [configuration setPrewarmsProcessesAutomatically:YES];
    [configuration setPageCacheEnabled:YES];
    [configuration setSuppressesConnectionTerminationOnSystemChange:YES];

    auto copy = adoptNS([configuration copy]);

    EXPECT_TRUE([[configuration injectedBundleURL] isEqual:[copy injectedBundleURL]]);
    EXPECT_EQ([configuration maximumProcessCount], [copy maximumProcessCount]);
    EXPECT_TRUE([[configuration customWebContentServiceBundleIdentifier] isEqual:[copy customWebContentServiceBundleIdentifier]]);
    EXPECT_EQ([configuration ignoreSynchronousMessagingTimeoutsForTesting], [copy ignoreSynchronousMessagingTimeoutsForTesting]);
    EXPECT_EQ([configuration attrStyleEnabled], [copy attrStyleEnabled]);
    EXPECT_TRUE([[configuration additionalReadAccessAllowedURLs] isEqual:[copy additionalReadAccessAllowedURLs]]);
#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
    EXPECT_EQ([configuration wirelessContextIdentifier], [copy wirelessContextIdentifier]);
#endif
    EXPECT_EQ([configuration diskCacheSizeOverride], [copy diskCacheSizeOverride]);
    EXPECT_TRUE([[configuration cachePartitionedURLSchemes] isEqual:[copy cachePartitionedURLSchemes]]);
    EXPECT_TRUE([[configuration alwaysRevalidatedURLSchemes] isEqual:[copy alwaysRevalidatedURLSchemes]]);
    EXPECT_EQ([configuration diskCacheSpeculativeValidationEnabled], [copy diskCacheSpeculativeValidationEnabled]);
    EXPECT_EQ([configuration shouldCaptureAudioInUIProcess], [copy shouldCaptureAudioInUIProcess]);
#if PLATFORM(IOS_FAMILY)
    EXPECT_TRUE([[configuration CTDataConnectionServiceType] isEqual:[copy CTDataConnectionServiceType]]);
    EXPECT_EQ([configuration alwaysRunsAtBackgroundPriority], [copy alwaysRunsAtBackgroundPriority]);
    EXPECT_EQ([configuration shouldTakeUIBackgroundAssertion], [copy shouldTakeUIBackgroundAssertion]);
#endif
    EXPECT_EQ([configuration presentingApplicationPID], [copy presentingApplicationPID]);
    EXPECT_EQ([configuration processSwapsOnNavigation], [copy processSwapsOnNavigation]);
    EXPECT_EQ([configuration alwaysKeepAndReuseSwappedProcesses], [copy alwaysKeepAndReuseSwappedProcesses]);
    EXPECT_EQ([configuration processSwapsOnWindowOpenWithOpener], [copy processSwapsOnWindowOpenWithOpener]);
    EXPECT_EQ([configuration prewarmsProcessesAutomatically], [copy prewarmsProcessesAutomatically]);
    EXPECT_EQ([configuration pageCacheEnabled], [copy pageCacheEnabled]);
    EXPECT_EQ([configuration suppressesConnectionTerminationOnSystemChange], [copy suppressesConnectionTerminationOnSystemChange]);
}

#endif
