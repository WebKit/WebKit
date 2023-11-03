/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"

@class WKContentView;

@interface WKTextInteractionWrapper : NSObject

- (instancetype)initWithView:(WKContentView *)contentView;

- (void)activateSelection;
- (void)deactivateSelection;
- (void)didEndScrollingOverflow;
- (void)selectionChanged;
- (void)setGestureRecognizers;
- (void)willStartScrollingOverflow;
- (void)selectionChangedWithGestureAt:(CGPoint)point withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState withFlags:(UIWKSelectionFlags)flags;
- (void)showDictionaryFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)selectionChangedWithTouchAt:(CGPoint)point withSelectionTouch:(UIWKSelectionTouch)touch withFlags:(UIWKSelectionFlags)flags;
- (void)lookup:(NSString *)textWithContext withRange:(NSRange)range fromRect:(CGRect)presentationRect;
- (void)showShareSheetFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)showTextServiceFor:(NSString *)selectedTerm fromRect:(CGRect)presentationRect;
- (void)scheduleReplacementsForText:(NSString *)text;
- (void)scheduleChineseTransliterationForText:(NSString *)text;
- (void)willStartScrollingOrZooming;
- (void)didEndScrollingOrZooming;
- (void)selectWord;
- (void)selectAll:(id)sender;
- (void)translate:(NSString *)text fromRect:(CGRect)presentationRect;

#if USE(UICONTEXTMENU)
- (void)setExternalContextMenuInteractionDelegate:(id<UIContextMenuInteractionDelegate>)delegate;
@property (nonatomic, strong, readonly) UIContextMenuInteraction *contextMenuInteraction;
#endif

@property (nonatomic, readonly) UIWKTextInteractionAssistant *textInteractionAssistant;

@end

#endif // PLATFORM(IOS_FAMILY)
