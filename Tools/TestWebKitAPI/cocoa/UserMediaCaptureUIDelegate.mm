/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
        _audioDecision = _WKPermissionDecisionGrant;
        _videoDecision = _WKPermissionDecisionGrant;
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

-(void)setAudioDecision:(_WKPermissionDecision)decision {
    _audioDecision = decision;
}

-(void)setVideoDecision:(_WKPermissionDecision)decision {
    _videoDecision = decision;
}

- (void)_webView:(WKWebView *)webView requestMediaCapturePermission:(BOOL)audio video:(BOOL)video decisionHandler:(void (^)(_WKPermissionDecision))decisionHandler {
    ++_numberOfPrompts;
    _wasPrompted = true;
    if (audio && _audioDecision == _WKPermissionDecisionDeny) {
        decisionHandler(_WKPermissionDecisionDeny);
        return;
    }
    if (video && _videoDecision == _WKPermissionDecisionDeny) {
        decisionHandler(_WKPermissionDecisionDeny);
        return;
    }
    if (audio && _audioDecision == _WKPermissionDecisionPrompt) {
        decisionHandler(_WKPermissionDecisionPrompt);
        return;
    }
    if (video && _videoDecision == _WKPermissionDecisionPrompt) {
        decisionHandler(_WKPermissionDecisionPrompt);
        return;
    }
    decisionHandler(_WKPermissionDecisionGrant);
}

- (void)_webView:(WKWebView *)webView checkUserMediaPermissionForURL:(NSURL *)url mainFrameURL:(NSURL *)mainFrameURL frameIdentifier:(NSUInteger)frameIdentifier decisionHandler:(void (^)(NSString *salt, BOOL authorized))decisionHandler {
    decisionHandler(@"0x9876543210", YES);
}
@end

#endif // ENABLE(MEDIA_STREAM)
