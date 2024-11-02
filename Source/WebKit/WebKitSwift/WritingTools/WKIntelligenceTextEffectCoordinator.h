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

#import <Foundation/Foundation.h>

#if !TARGET_OS_WATCH && !TARGET_OS_TV && __has_include(<WritingTools/WritingTools.h>)

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#import <WebKit/_WKTextPreview.h>
#endif

#import <WebKit/WKWebView.h>

NS_ASSUME_NONNULL_BEGIN

@class WKIntelligenceTextEffectCoordinator;

@class WTTextSuggestion;

NS_SWIFT_UI_ACTOR
@protocol WKIntelligenceTextEffectCoordinatorDelegate <NSObject>

- (WKWebView *)viewForIntelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator;

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator textPreviewsForRange:(NSRange)range completion:(void (^)(UITargetedPreview *))completion;
#else
- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator textPreviewsForRange:(NSRange)range completion:(void (^)(NSArray<_WKTextPreview *> *))completion;
#endif

- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator rectsForProofreadingSuggestionsInRange:(NSRange)range completion:(void (^)(NSArray<NSValue *> *))completion;

- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator updateTextVisibilityForRange:(NSRange)range visible:(BOOL)visible identifier:(NSUUID *)identifier completion:(void (^)(void))completion;

- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator decorateReplacementsForRange:(NSRange)range completion:(void (^)(void))completion;

- (void)intelligenceTextEffectCoordinator:(WKIntelligenceTextEffectCoordinator *)coordinator setSelectionForRange:(NSRange)range completion:(void (^)(void))completion;

@end

NS_SWIFT_UI_ACTOR
@interface WKIntelligenceTextEffectCoordinator : NSObject

+ (NSInteger)characterDeltaForReceivedSuggestions:(NSArray<WTTextSuggestion *> *)suggestions;

- (instancetype)initWithDelegate:(id<WKIntelligenceTextEffectCoordinatorDelegate>)delegate;

- (void)startAnimationForRange:(NSRange)range completion:(void (^)(void))completion;

- (void)requestReplacementWithProcessedRange:(NSRange)range finished:(BOOL)finished characterDelta:(NSInteger)characterDelta operation:(void (^)(void (^)(void)))operation completion:(void (^)(void))completion;

- (void)flushReplacementsWithCompletion:(void (^)(void))completion;

- (void)restoreSelectionAcceptedReplacements:(BOOL)acceptedReplacements completion:(void (^)(void))completion;

@end

NS_ASSUME_NONNULL_END

#endif
