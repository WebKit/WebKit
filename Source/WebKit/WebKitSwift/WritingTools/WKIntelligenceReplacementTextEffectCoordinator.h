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
#import <wtf/Platform.h>

#if ENABLE(WRITING_TOOLS)

#import "WKIntelligenceTextEffectCoordinator.h"

NS_ASSUME_NONNULL_BEGIN

@class WTTextSuggestion;

NS_SWIFT_UI_ACTOR
@interface WKIntelligenceReplacementTextEffectCoordinator : NSObject <WKIntelligenceTextEffectCoordinating>

+ (NSInteger)characterDeltaForReceivedSuggestions:(NSArray<WTTextSuggestion *> *)suggestions;

// FIXME: (rdar://142275772) Remove these duplicate protocol method declarations when Swift is able to properly synthesize their Swift names.

- (void)startAnimationForRange:(NSRange)range completion:(NS_SWIFT_UI_ACTOR void (^)(void))completion;

- (void)requestReplacementWithProcessedRange:(NSRange)range finished:(BOOL)finished characterDelta:(NSInteger)characterDelta operation:(NS_SWIFT_UI_ACTOR void (^)(NS_SWIFT_UI_ACTOR void (^)(void)))operation completion:(NS_SWIFT_UI_ACTOR void (^)(void))completion;

- (void)flushReplacementsWithCompletionHandler:(NS_SWIFT_UI_ACTOR void (^)(void))completionHandler;

- (void)restoreSelectionAcceptedReplacements:(BOOL)acceptedReplacements completionHandler:(NS_SWIFT_UI_ACTOR void (^)(void))completionHandler;

- (void)hideEffectsWithCompletionHandler:(NS_SWIFT_UI_ACTOR void (^)(void))completionHandler;

- (void)showEffectsWithCompletionHandler:(NS_SWIFT_UI_ACTOR void (^)(void))completionHandler;

@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(WRITING_TOOLS)
