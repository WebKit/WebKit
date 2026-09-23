/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#import "Helpers/cocoa/UserMediaCaptureUIDelegate.h"

#if ENABLE(MEDIA_STREAM)
#import "Helpers/PlatformUtilities.h"
#import "Helpers/Utilities.h"
#import <WebKit/WKMockMediaDevice.h>
#import <WebKit/WKString.h>
#import <WebKit/WKWebpagePreferencesPrivate.h>

@implementation UserMediaCaptureUIDelegate {
    Vector<RetainPtr<WKWebView>> _createdWebViews;
}
@synthesize createWebViewWithConfiguration = _createWebViewWithConfiguration;
@synthesize numberOfPrompts = _numberOfPrompts;
@synthesize decision = _decision;

-(id)init {
    self = [super init];
    if (self != nil) {
        _wasPrompted = false;
        _numberOfPrompts = 0;
        _audioDecision = WKPermissionDecisionGrant;
        _videoDecision = WKPermissionDecisionGrant;
        _getDisplayMediaDecision = WKDisplayCapturePermissionDecisionDeny;
    }

    return self;
}

-(BOOL)wasPrompted {
    return _wasPrompted ? YES : NO;
}

-(void)resetWasPrompted {
    _wasPrompted = false;
}

-(void)waitUntilPrompted {
    TestWebKitAPI::Util::run(&_wasPrompted);
    _wasPrompted = false;
}

-(void)setAudioDecision:(WKPermissionDecision)decision {
    _audioDecision = decision;
}

-(void)setVideoDecision:(WKPermissionDecision)decision {
    _videoDecision = decision;
}

-(void)setGetDisplayMediaDecision:(WKDisplayCapturePermissionDecision)decision {
    _getDisplayMediaDecision = decision;
}

- (void)_webView:(WKWebView *)webView queryPermission:(NSString*) name forOrigin:(WKSecurityOrigin *)origin completionHandler:(void (^)(WKPermissionDecision state))completionHandler {
    if ([name isEqualToString:@"camera"]) {
        completionHandler(_videoDecision);
        return;
    }
    if ([name isEqualToString:@"microphone"]) {
        completionHandler(_audioDecision);
        return;
    }
    ASSERT_NOT_REACHED();
    completionHandler(WKPermissionDecisionDeny);
}

- (void)webView:(WKWebView *)webView requestMediaCapturePermissionForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame type:(WKMediaCaptureType)type decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler {
    ++_numberOfPrompts;
    _wasPrompted = true;
    switch (type) {
    case WKMediaCaptureTypeCamera:
        if (_videoDecision == WKPermissionDecisionDeny) {
            decisionHandler(WKPermissionDecisionDeny);
            return;
        }
        break;
    case WKMediaCaptureTypeMicrophone:
        if (_audioDecision == WKPermissionDecisionDeny) {
            decisionHandler(WKPermissionDecisionDeny);
            return;
        }
        break;
    case WKMediaCaptureTypeCameraAndMicrophone:
        if (_audioDecision == WKPermissionDecisionDeny || _videoDecision == WKPermissionDecisionDeny) {
            decisionHandler(WKPermissionDecisionDeny);
            return;
        }
        break;
    }
    decisionHandler(WKPermissionDecisionGrant);
}

- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler {
    decisionHandler(@"0x9876543210", YES);
}

- (void)_webView:(WKWebView *)webView requestDisplayCapturePermissionForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame withSystemAudio:(BOOL)withSystemAudio decisionHandler:(void (^)(WKDisplayCapturePermissionDecision decision))decisionHandler
{
    ++_numberOfPrompts;
    _wasPrompted = true;
    decisionHandler(_getDisplayMediaDecision);
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures
{
    if (_createWebViewWithConfiguration)
        return _createWebViewWithConfiguration(configuration, navigationAction, windowFeatures);
    _createdWebViews.append(adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration]));
    return _createdWebViews.last().get();
}

- (void)_webView:(WKWebView *)webView decidePolicyForScreenCaptureUnmutingForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame decisionHandler:(void (^)(BOOL authorized))decisionHandler
{
    ++_numberOfPrompts;
    _wasPrompted = true;
    decisionHandler(_getDisplayMediaDecision != WKDisplayCapturePermissionDecisionDeny);
}

#if PLATFORM(IOS_FAMILY)
- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction preferences:(WKWebpagePreferences *)preferences decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))decisionHandler
{
    auto websitePolicies = adoptNS([[WKWebpagePreferences alloc] init]);
    [websitePolicies _setPopUpPolicy:_WKWebsitePopUpPolicyAllow];
    decisionHandler(WKNavigationActionPolicyAllow, websitePolicies.get());
}
#endif

@end

@implementation UserMediaCaptureUIDelegateWithDeviceChange {
}
-(id)init {
    self = [super init];
    return self;
}

- (void)webView:(WKWebView *)webView requestMediaCapturePermissionForOrigin:(WKSecurityOrigin *)origin initiatedByFrame:(WKFrameInfo *)frame type:(WKMediaCaptureType)type decisionHandler:(void (^)(WKPermissionDecision decision))decisionHandler {
    WKResetMockMediaDevices((__bridge WKContextRef)webView.configuration.processPool);
    decisionHandler(WKPermissionDecisionGrant);
}

-(void)addDefaultCamera:(WKWebViewConfiguration*)configuration {
    auto persistentId = adoptWK(WKStringCreateWithUTF8CString("PERSISTENTCAMERAID1"));
    auto label = adoptWK(WKStringCreateWithUTF8CString("NEWDEFAULTCAMERA"));
    auto type = adoptWK(WKStringCreateWithUTF8CString("camera"));
    WKAddMockMediaDevice((__bridge WKContextRef)configuration.processPool, persistentId.get(), label.get(), type.get(), nullptr, true);
}

-(void)addDefaultMicrophone:(WKWebViewConfiguration*)configuration {
    auto persistentId = adoptWK(WKStringCreateWithUTF8CString("PERSISTENTMICROPHONEID1"));
    auto label = adoptWK(WKStringCreateWithUTF8CString("NEWDEFAULTMICROPHONE"));
    auto type = adoptWK(WKStringCreateWithUTF8CString("microphone"));
    WKAddMockMediaDevice((__bridge WKContextRef)configuration.processPool, persistentId.get(), label.get(), type.get(), nullptr, true);
}

@end
#endif // ENABLE(MEDIA_STREAM)
