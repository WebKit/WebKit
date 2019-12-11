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

#pragma once

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)

#import <UIKit/UIViewController.h>

@class WKWebView;

NS_ASSUME_NONNULL_BEGIN

@interface WKFullScreenViewController : UIViewController
@property (retain, nonatomic) id target;
@property (assign, nonatomic) SEL action;
@property (copy, nonatomic) NSString *location;
@property (assign, nonatomic) BOOL prefersStatusBarHidden;
@property (assign, nonatomic) BOOL prefersHomeIndicatorAutoHidden;
@property (assign, nonatomic, getter=isPlaying) BOOL playing;
@property (assign, nonatomic, getter=isPictureInPictureActive) BOOL pictureInPictureActive;
@property (assign, nonatomic, getter=isAnimating) BOOL animating;

- (id)initWithWebView:(WKWebView *)webView;
- (void)showUI;
- (void)hideUI;
- (void)videoControlsManagerDidChange;
@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
