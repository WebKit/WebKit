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

#import "DeprecatedGlobalValues.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKPreferencesPrivate.h>

@interface WKPreferencesMessageHandler : NSObject <WKScriptMessageHandler>
@end

@implementation WKPreferencesMessageHandler

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message
{
    receivedScriptMessage = true;
    scriptMessages.append(message);
}

@end

static const char* simpleHTML = R"TESTRESOURCE(
<script>
function log(text)
{
    window.webkit.messageHandlers.testHandler.postMessage(text);
}
log("Loaded");
</script>
)TESTRESOURCE";

TEST(WKPreferencesPrivate, DisableRichJavaScriptFeatures)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration.get().preferences _disableRichJavaScriptFeatures];
    RetainPtr handler = adoptNS([[WKPreferencesMessageHandler alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:handler.get() name:@"testHandler"];
    RetainPtr webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);

    [webView loadHTMLString:[NSString stringWithUTF8String:simpleHTML] baseURL:[NSURL URLWithString:@"https://webkit.org"]];
    RetainPtr result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Loaded", result.get());

    [webView evaluateJavaScript:@"canvas = document.createElement('canvas'); log(canvas.getContext('webgl') ? 'WebGL Enabled' : 'WebGL Disabled');" completionHandler:nil];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"WebGL Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.gpu ? 'WebGPU Enabled' : 'WebGPU Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"WebGPU Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.AudioContext ? 'Web Audio Enabled' : 'Web Audio Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Web Audio Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.RTCPeerConnection ? 'Web RTC Enabled' : 'Web RTC Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Web RTC Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.getGamepads ? 'Gamepad Enabled' : 'Gamepad Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Gamepad Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.webkitSpeechRecognition ? 'SpeechRecognition Enabled' : 'SpeechRecognition Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"SpeechRecognition Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.SpeechSynthesis ? 'SpeechSynthesis Enabled' : 'SpeechSynthesis Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"SpeechSynthesis Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.geolocation ? 'Geolocation Enabled' : 'Geolocation Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Geolocation Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.ApplePaySession ? 'ApplePay Enabled' : 'ApplePay Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"ApplePay Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.setAppBadge ? 'Badging Enabled' : 'Badging Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Badging Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.BarcodeDetector ? 'Shape Detection Enabled' : 'Shape Detection Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Shape Detection Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.screen && screen.orientation ? 'Screen Orientation Enabled' : 'Screen Orientation Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Screen Orientation Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.identity && window.DigitalCredential ? 'Digital Credentials Enabled' : 'Digital Credentials Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Digital Credentials Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.Notification ? 'Notifications Enabled' : 'Notifications Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Notifications Disabled", result.get());

    [webView evaluateJavaScript:@"log('serviceWorker' in navigator && 'BackgroundFetch' in window ? 'Background Fetch Enabled' : 'Background Fetch Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Background Fetch Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.BroadcastChannel ? 'BroadcastChannel Enabled' : 'BroadcastChannel Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"BroadcastChannel Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.caches ? 'Cache API Enabled' : 'Cache API Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Cache API Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.contacts ? 'Contact Picker Enabled' : 'Contact Picker Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Contact Picker Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.cookieConsent ? 'Cookie Consent Enabled' : 'Cookie Consent Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Cookie Consent Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.cookieStore ? 'Cookie Store Enabled' : 'Cookie Store Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Cookie Store Disabled", result.get());

    [webView evaluateJavaScript:@"log('serviceWorker' in navigator && window.cookieStore ? 'Cookie Store Manager Enabled' : 'Cookie Store Manager Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Cookie Store Manager Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.DeviceOrientationEvent && DeviceOrientationEvent.requestPermission ? 'Device Orientation Permission Enabled' : 'Device Orientation Permission Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Device Orientation Permission Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.indexedDB ? 'IndexedDB Enabled' : 'IndexedDB Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"IndexedDB Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.ManagedMediaSource ? 'Managed Media Source Enabled' : 'Managed Media Source Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Managed Media Source Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.MediaRecorder ? 'Media Recorder Enabled' : 'Media Recorder Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Media Recorder Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.mediaSession && navigator.mediaSession.coordinator ? 'Media Session Coordinator Enabled' : 'Media Session Coordinator Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Media Session Coordinator Disabled", result.get());

    [webView evaluateJavaScript:@"log('serviceWorker' in navigator && window.Notification ? 'Notification Event Enabled' : 'Notification Event Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Notification Event Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.permissions ? 'Permissions API Enabled' : 'Permissions API Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Permissions API Disabled", result.get());

    [webView evaluateJavaScript:@"log('serviceWorker' in navigator && window.PushManager ? 'Push API Enabled' : 'Push API Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Push API Disabled", result.get());

    [webView evaluateJavaScript:@"log('serviceWorker' in navigator ? 'Service Worker Navigation Preload Enabled' : 'Service Worker Navigation Preload Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Service Worker Navigation Preload Disabled", result.get());

    [webView evaluateJavaScript:@"log('serviceWorker' in navigator ? 'Service Workers Enabled' : 'Service Workers Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Service Workers Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.SharedWorker ? 'Shared Worker Enabled' : 'Shared Worker Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Shared Worker Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.credentials && navigator.credentials.create ? 'Web Authentication Enabled' : 'Web Authentication Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"Web Authentication Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.WebSocket ? 'WebSocket Enabled' : 'WebSocket Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"WebSocket Disabled", result.get());

    [webView evaluateJavaScript:@"log(window.WebTransport ? 'WebTransport Enabled' : 'WebTransport Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"WebTransport Disabled", result.get());

    [webView evaluateJavaScript:@"log(navigator.xr ? 'WebXR Enabled' : 'WebXR Disabled');" completionHandler:^(id, NSError *error) {
        EXPECT_NULL(error);
    }];
    result = (NSString *)[getNextMessage() body];
    EXPECT_WK_STREQ(@"WebXR Disabled", result.get());
}
