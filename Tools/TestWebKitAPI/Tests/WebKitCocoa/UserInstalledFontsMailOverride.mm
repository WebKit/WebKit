/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>

// This test verifies that when _shouldAllowUserInstalledFonts is set to NO,
// user-installed fonts are still accessible in the iOS Mail compose context.
// rdar://169732367
//
// NOTE: This test can only fully validate the fix when run inside the
// com.apple.MailCompositionService or com.apple.mobilemail process.
// When run under the test harness (com.apple.WebKit.TestWebKitAPI),
// IOSApplication::isMailCompositionService() returns false, so the
// override does not activate. This test documents the expected behavior
// and serves as a regression test framework for when bundle ID simulation
// becomes available in the test infrastructure.

TEST(WebKit, UserInstalledFontsBlockedByDefault)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration preferences]._shouldAllowUserInstalledFonts = NO;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration.get()]);

    // Load a page that tries to use a font-family and checks computed style.
    // Under the test harness (not Mail), user-installed fonts should be blocked.
    [webView synchronouslyLoadHTMLString:@"<html><body><p id='target' style='font-family: \"SomeUserFont\", sans-serif;'>Test</p>"
        "<script>"
        "var el = document.getElementById('target');"
        "var computed = window.getComputedStyle(el).fontFamily;"
        // The computed font-family should not resolve to the user font
        // when _shouldAllowUserInstalledFonts = NO (outside of Mail context).
        "document.title = computed;"
        "</script></body></html>"];

