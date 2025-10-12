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

#pragma once

#import <wtf/Compiler.h>
#import <wtf/Platform.h>

DECLARE_SYSTEM_HEADER

#if ENABLE(WRITING_TOOLS)

// FIXME: (rdar://149216417) Import WritingTools when using the internal SDK instead of using forward declarations.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol BSXPCCoding;
@protocol BSXPCSecureCoding;

extern NSAttributedStringKey const WTWritingToolsPreservedAttributeName;

typedef NS_ENUM(NSInteger, WTRequestedTool) {
    WTRequestedToolIndex = 0,

    WTRequestedToolProofread = 1,
    WTRequestedToolRewrite = 2,
    WTRequestedToolRewriteProofread = 3,

    WTRequestedToolRewriteFriendly = 11,
    WTRequestedToolRewriteProfessional = 12,
    WTRequestedToolRewriteConcise = 13,
    WTRequestedToolRewriteOpenEnded = 19,

    WTRequestedToolTransformSummary = 21,
    WTRequestedToolTransformKeyPoints = 22,
    WTRequestedToolTransformList = 23,
    WTRequestedToolTransformTable = 24,

    WTRequestedToolSmartReply = 101,

    WTRequestedToolCompose = 201,
};

// MARK: WTContext

@protocol WTTextViewDelegate_Proposed_v1;
@protocol WTTextViewDelegate;

@interface WTContext : NSObject<NSSecureCoding, NSCopying>

@property (nonatomic, readonly) NSUUID *uuid;

@property (nonatomic, readonly) NSAttributedString *attributedText;

@property (nonatomic) NSRange range;

- (instancetype)initWithAttributedText:(NSAttributedString *)attributedText range:(NSRange)range;

@end

// MARK: WTSession

typedef NS_ENUM(NSInteger, WTCompositionSessionType) {
    WTCompositionSessionTypeNone,
    WTCompositionSessionTypeMagic,
    WTCompositionSessionTypeFriendly,
    WTCompositionSessionTypeProfessional,
    WTCompositionSessionTypeConcise,
    WTCompositionSessionTypeOpenEnded,
    WTCompositionSessionTypeSummary,
    WTCompositionSessionTypeKeyPoints,
    WTCompositionSessionTypeList,
    WTCompositionSessionTypeTable,
    WTCompositionSessionTypeCompose,
    WTCompositionSessionTypeSmartReply,
    WTCompositionSessionTypeProofread,
};

typedef NS_ENUM(NSInteger, WTSessionType) {
    WTSessionTypeProofreading = 1,
    WTSessionTypeComposition,
};

@interface WTSession : NSObject<BSXPCSecureCoding, NSSecureCoding>

@property (nonatomic, readonly) NSUUID *uuid;
@property (nonatomic, readonly) WTSessionType type;

@property (nonatomic, weak) id<WTTextViewDelegate_Proposed_v1> textViewDelegate;

- (instancetype)initWithType:(WTSessionType)type textViewDelegate:(id<WTTextViewDelegate> _Nullable)textViewDelegate;

@end

@interface WTSession (Private)

@property (nonatomic) WTCompositionSessionType compositionSessionType;
@property (nonatomic, readonly) WTRequestedTool requestedTool;

@end

// MARK: WTTextSuggestion

typedef NS_ENUM(NSInteger, WTTextSuggestionState) {
    WTTextSuggestionStatePending = 0,
    WTTextSuggestionStateReviewing = 1,
    WTTextSuggestionStateRejected = 3,
    WTTextSuggestionStateInvalid = 4,

    WTTextSuggestionStateAccepted = 2,
};

@interface WTTextSuggestion : NSObject<BSXPCCoding, NSSecureCoding>

@property (nonatomic, readonly) NSUUID *uuid;

@property (nonatomic, readonly) NSRange originalRange;

@property (nonatomic, readonly) NSString *replacement;

@property (nonatomic, nullable, readonly) NSString *suggestionCategory;
@property (nonatomic, nullable, readonly) NSString *suggestionShortDescription;
@property (nonatomic, nullable, readonly) NSString *suggestionDescription;

@property (nonatomic) WTTextSuggestionState state;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithOriginalRange:(NSRange)originalRange replacement:(NSString *)replacement suggestionCategory:(NSString * _Nullable)suggestionCategory suggestionDescription:(NSString * _Nullable)suggestionDescription NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithOriginalRange:(NSRange)originalRange replacement:(NSString *)replacement suggestionCategory:(NSString * _Nullable)suggestionCategory suggestionShortDescription:(NSString * _Nullable)suggestionShortDescription suggestionDescription:(NSString * _Nullable)suggestionDescription;

- (instancetype)initWithOriginalRange:(NSRange)originalRange replacement:(NSString *)replacement;

- (instancetype)initWithOriginalRange:(NSRange)originalRange replacement:(NSString *)replacement suggestionDescription:(NSString *)suggestionDescription;

@end

// MARK: WTTextViewDelegate

#if PLATFORM(IOS_FAMILY)
@class UIView;
#else
@class NSView;
#endif

@protocol WTTextViewDelegate_Proposed_v1

- (void)proofreadingSessionWithUUID:(NSUUID *)sessionUUID updateState:(WTTextSuggestionState)state forSuggestionWithUUID:(NSUUID *)suggestionUUID;

#if PLATFORM(IOS_FAMILY)
- (void)proofreadingSessionWithUUID:(NSUUID *)sessionUUID showDetailsForSuggestionWithUUID:(NSUUID *)suggestionUUID relativeToRect:(CGRect)rect inView:(UIView *)sourceView;
#else
- (void)proofreadingSessionWithUUID:(NSUUID *)sessionUUID showDetailsForSuggestionWithUUID:(NSUUID *)suggestionUUID relativeToRect:(NSRect)rect inView:(NSView *)sourceView;
#endif

- (void)textSystemWillBeginEditingDuringSessionWithUUID:(NSUUID *)sessionUUID;

@end

@protocol WTTextViewDelegate <WTTextViewDelegate_Proposed_v1>

@end

// MARK: WTWritingToolsDelegate

@protocol WTWritingToolsDelegate_Proposed_v3

- (void)willBeginWritingToolsSession:(nullable WTSession *)session requestContexts:(void (^)(NSArray<WTContext *> *contexts))completion;

- (void)didBeginWritingToolsSession:(WTSession *)session contexts:(NSArray<WTContext *> *)contexts;

typedef NS_ENUM(NSInteger, WTAction) {
    WTActionShowOriginal = 1,
    WTActionShowRewritten,
    WTActionCompositionRestart,
    WTActionCompositionRefine,
};

typedef NS_ENUM(NSInteger, WTFormSheetUIType) {
    WTFormSheetUITypeUnspecified,
    WTFormSheetUITypeEnrollment,
    WTFormSheetUITypeShareSheet,
};

- (void)writingToolsSession:(WTSession *)session didReceiveAction:(WTAction)action;

- (void)didEndWritingToolsSession:(WTSession *)session accepted:(BOOL)accepted;

- (void)proofreadingSession:(WTSession *)session didReceiveSuggestions:(NSArray<WTTextSuggestion *> *)suggestions processedRange:(NSRange)range inContext:(WTContext *)context finished:(BOOL)finished;

- (void)proofreadingSession:(WTSession *)session didUpdateState:(WTTextSuggestionState)state forSuggestionWithUUID:(NSUUID *)uuid inContext:(WTContext *)context;

- (void)compositionSession:(WTSession *)session didReceiveText:(NSAttributedString *)attributedText replacementRange:(NSRange)range inContext:(WTContext *)context finished:(BOOL)finished;

@optional

- (BOOL)supportsWritingToolsAction:(WTAction)action;

@property (readonly, nonatomic) BOOL includesTextListMarkers;

@end

@protocol WTWritingToolsDelegate <WTWritingToolsDelegate_Proposed_v3>

@end

NS_ASSUME_NONNULL_END

#endif // ENABLE(WRITING_TOOLS)
