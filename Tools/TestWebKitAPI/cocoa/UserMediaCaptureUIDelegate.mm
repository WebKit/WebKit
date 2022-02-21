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
#import "UserMediaCaptureUIDelegate.h"

#if ENABLE(MEDIA_STREAM)
#import "PlatformUtilities.h"
#import "Utilities.h"

@implementation UserMediaCaptureUIDelegate
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

@end

#endif // ENABLE(MEDIA_STREAM)
