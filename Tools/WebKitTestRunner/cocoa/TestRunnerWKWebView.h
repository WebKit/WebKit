/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import <WebKit/WebKit.h>

@interface WKWebView(SpellChecking)
- (IBAction)toggleContinuousSpellChecking:(id)sender;
@end

@interface TestRunnerWKWebView : WKWebView

#if PLATFORM(IOS_FAMILY)

@property (nonatomic, copy) void (^didStartFormControlInteractionCallback)(void);
@property (nonatomic, copy) void (^didEndFormControlInteractionCallback)(void);
@property (nonatomic, copy) void (^didShowContextMenuCallback)(void);
@property (nonatomic, copy) void (^didDismissContextMenuCallback)(void);
@property (nonatomic, copy) void (^willBeginZoomingCallback)(void);
@property (nonatomic, copy) void (^didEndZoomingCallback)(void);
@property (nonatomic, copy) void (^didShowKeyboardCallback)(void);
@property (nonatomic, copy) void (^didHideKeyboardCallback)(void);
@property (nonatomic, copy) void (^willPresentPopoverCallback)(void);
@property (nonatomic, copy) void (^didDismissPopoverCallback)(void);
@property (nonatomic, copy) void (^didEndScrollingCallback)(void);
@property (nonatomic, copy) void (^rotationDidEndCallback)(void);
@property (nonatomic, copy) void (^windowTapRecognizedCallback)(void);
@property (nonatomic, copy) NSString *accessibilitySpeakSelectionContent;

- (void)setAllowedMenuActions:(NSArray<NSString *> *)actions;

- (void)resetCustomMenuAction;
- (void)installCustomMenuAction:(NSString *)name dismissesAutomatically:(BOOL)dismissesAutomatically callback:(dispatch_block_t)callback;

- (void)zoomToScale:(double)scale animated:(BOOL)animated completionHandler:(void (^)(void))completionHandler;
- (void)accessibilityRetrieveSpeakSelectionContentWithCompletionHandler:(void (^)(void))completionHandler;
- (void)_didEndRotation;

@property (nonatomic, assign) UIEdgeInsets overrideSafeAreaInsets;

@property (nonatomic, readonly, getter=isShowingKeyboard) BOOL showingKeyboard;
@property (nonatomic, readonly, getter=isDismissingMenu) BOOL dismissingMenu;
@property (nonatomic, readonly, getter=isShowingPopover) BOOL showingPopover;
@property (nonatomic, readonly, getter=isShowingContextMenu) BOOL showingContextMenu;
@property (nonatomic, assign) BOOL usesSafariLikeRotation;
@property (nonatomic, readonly, getter=isInteractingWithFormControl) BOOL interactingWithFormControl;

#endif

@property (nonatomic, readonly, getter=isShowingMenu) BOOL showingMenu;
@property (nonatomic, copy) void (^didShowMenuCallback)(void);
@property (nonatomic, copy) void (^didHideMenuCallback)(void);
@property (nonatomic, retain, setter=_setStableStateOverride:) NSNumber *_stableStateOverride;
@property (nonatomic, setter=_setScrollingUpdatesDisabledForTesting:) BOOL _scrollingUpdatesDisabledForTesting;

- (void)dismissActiveMenu;
- (void)resetInteractionCallbacks;

@end
