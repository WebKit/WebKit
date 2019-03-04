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

#import "config.h"

#import "Test.h"
#import "Utilities.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegate.h>
#import <WebKit/_WKExperimentalFeature.h>
#import <wtf/RetainPtr.h>

TEST(WebKit, DefaultWKPreferences)
{
    RetainPtr<WKPreferences> preferences = adoptNS([[WKPreferences alloc] init]);

    EXPECT_FALSE([preferences _isStandalone]);
    [preferences _setStandalone:YES];
    EXPECT_TRUE([preferences _isStandalone]);
}

TEST(WebKit, LoadsImagesAutomatically)
{
    RetainPtr<WKPreferences> preferences = adoptNS([[WKPreferences alloc] init]);

    EXPECT_TRUE([preferences _loadsImagesAutomatically]);
    [preferences _setLoadsImagesAutomatically:NO];
    EXPECT_FALSE([preferences _loadsImagesAutomatically]);
    [preferences _setLoadsImagesAutomatically:YES];
    EXPECT_TRUE([preferences _loadsImagesAutomatically]);
}

TEST(WebKit, ExperimentalFeatures)
{
    NSArray *features = [WKPreferences _experimentalFeatures];
    EXPECT_NOT_NULL(features);

    RetainPtr<WKPreferences> preferences = adoptNS([[WKPreferences alloc] init]);

    for (_WKExperimentalFeature *feature in features) {
        BOOL value = [preferences _isEnabledForFeature:feature];
        [preferences _setEnabled:value forFeature:feature];
    }
}

#if PLATFORM(MAC)

static bool receivedAlert;

@interface AlertSaver : NSObject <WKUIDelegate>
@end

@implementation AlertSaver {
    RetainPtr<NSString> alert;
}

- (NSString *)alert
{
    return alert.get();
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler
{
    alert = message;
    completionHandler();
    receivedAlert = true;
}

@end

TEST(WebKit, WebGLEnabled)
{
    NSString *html = @"<script>if(document.createElement('canvas').getContext('webgl')){alert('enabled')}else{alert('disabled')}</script>";
    auto delegate = adoptNS([[AlertSaver alloc] init]);

    auto webView = adoptNS([[WKWebView alloc] init]);
    [webView setUIDelegate:delegate.get()];
    [webView loadHTMLString:html baseURL:nil];
    TestWebKitAPI::Util::run(&receivedAlert);
    EXPECT_STREQ([delegate alert].UTF8String, "enabled");

    receivedAlert = false;
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setWebGLEnabled:NO];
    webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView setUIDelegate:delegate.get()];
    [webView loadHTMLString:html baseURL:nil];
    TestWebKitAPI::Util::run(&receivedAlert);
    EXPECT_STREQ([delegate alert].UTF8String, "disabled");
}

#endif // PLATFORM(MAC)
