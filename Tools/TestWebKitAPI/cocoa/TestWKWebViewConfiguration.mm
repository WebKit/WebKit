/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "Test.h"

#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>

TEST(WKWebViewConfiguration, FlagsThroughWKWebView)
{
    {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto view = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        BOOL defaultRespectsImageOrientation = [configuration _respectsImageOrientation];
        EXPECT_EQ([[view configuration] _respectsImageOrientation], defaultRespectsImageOrientation);
    }

    {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        BOOL defaultRespectsImageOrientation = [configuration _respectsImageOrientation];
        auto view = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        EXPECT_EQ([[view configuration] _respectsImageOrientation], defaultRespectsImageOrientation);
    }

    {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        BOOL defaultRespectsImageOrientation = [configuration _respectsImageOrientation];
        [configuration _setRespectsImageOrientation:!defaultRespectsImageOrientation];
        auto view = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        EXPECT_EQ([[view configuration] _respectsImageOrientation], !defaultRespectsImageOrientation);
    }

    {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        BOOL defaultRespectsImageOrientation = [configuration _respectsImageOrientation];
        auto view = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
        EXPECT_EQ([[view configuration] _respectsImageOrientation], defaultRespectsImageOrientation);
        // Spooky action at a distance, due to API::PageConfiguration::copy() doing a shallow copy and keeping a reference to the same WebPreferences.
        [configuration _setRespectsImageOrientation:!defaultRespectsImageOrientation];
        EXPECT_EQ([[view configuration] _respectsImageOrientation], !defaultRespectsImageOrientation);
    }
}
