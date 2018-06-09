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

#if PLATFORM(WATCHOS)

#import "UIKitSPI.h"

@class WKFocusedFormControlView;

@protocol WKFocusedFormControlViewDelegate <NSObject>

- (void)focusedFormControlViewDidSubmit:(WKFocusedFormControlView *)view;
- (void)focusedFormControlViewDidCancel:(WKFocusedFormControlView *)view;
- (void)focusedFormControlViewDidBeginEditing:(WKFocusedFormControlView *)view;
- (CGRect)rectForFocusedFormControlView:(WKFocusedFormControlView *)view;
- (NSString *)actionNameForFocusedFormControlView:(WKFocusedFormControlView *)view;

// Support for focusing upstream and downstream nodes.
- (void)focusedFormControlViewDidRequestNextNode:(WKFocusedFormControlView *)view;
- (void)focusedFormControlViewDidRequestPreviousNode:(WKFocusedFormControlView *)view;
- (BOOL)hasNextNodeForFocusedFormControlView:(WKFocusedFormControlView *)view;
- (BOOL)hasPreviousNodeForFocusedFormControlView:(WKFocusedFormControlView *)view;
- (CGRect)nextRectForFocusedFormControlView:(WKFocusedFormControlView *)view;
- (CGRect)previousRectForFocusedFormControlView:(WKFocusedFormControlView *)view;
- (UIScrollView *)scrollViewForFocusedFormControlView:(WKFocusedFormControlView *)view;

- (void)focusedFormControllerDidUpdateSuggestions:(WKFocusedFormControlView *)view;
@end

@interface WKFocusedFormControlView : UIView <UITextInputSuggestionDelegate>

- (instancetype)initWithFrame:(CGRect)frame delegate:(id <WKFocusedFormControlViewDelegate>)delegate;
- (instancetype)initWithCoder:(NSCoder *)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (void)reloadData:(BOOL)animated;
- (void)show:(BOOL)animated;
- (void)hide:(BOOL)animated;

- (void)engageFocusedFormControlNavigation;
- (void)disengageFocusedFormControlNavigation;

- (BOOL)handleWheelEvent:(UIEvent *)event;

@property (nonatomic, weak) id <WKFocusedFormControlViewDelegate> delegate;
@property (nonatomic, readonly, getter=isVisible) BOOL visible;
@property (nonatomic, copy) NSArray<UITextSuggestion *> *suggestions;

@end

#endif
