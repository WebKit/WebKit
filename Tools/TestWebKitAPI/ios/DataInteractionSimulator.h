/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(DATA_INTERACTION)

#import "TestWKWebView.h"
#import <UIKit/NSItemProvider+UIKitAdditions.h>

#if USE(APPLE_INTERNAL_SDK)
#import <UIKit/NSString+UIItemProvider.h>
#import <UIKit/NSURL+UIItemProvider.h>
#import <UIKit/UIImage+UIItemProvider.h>
#import <UIKit/UIItemProvider.h>
#import <UIKit/UIItemProvider_Private.h>
#else

@interface NSURL ()
@property (nonatomic, copy, setter=_setTitle:) NSString *_title;
@end

#define UIItemProviderRepresentationOptionsVisibilityAll NSItemProviderRepresentationVisibilityAll

@protocol UIItemProviderReading <NSItemProviderReading>

@required
- (instancetype)initWithItemProviderData:(NSData *)data typeIdentifier:(NSString *)typeIdentifier error:(NSError **)outError;

@end

@protocol UIItemProviderWriting <NSItemProviderWriting>

@required
- (NSProgress *)loadDataWithTypeIdentifier:(NSString *)typeIdentifier forItemProviderCompletionHandler:(void (^)(NSData *, NSError *))completionHandler;

@end

@interface NSAttributedString () <UIItemProviderReading, UIItemProviderWriting>
@end
@interface NSString () <UIItemProviderReading, UIItemProviderWriting>
@end
@interface NSURL () <UIItemProviderReading, UIItemProviderWriting>
@end
@interface UIImage () <UIItemProviderReading, UIItemProviderWriting>
@end

@interface UIItemProvider : NSItemProvider
@end

#endif

#import <UIKit/UIKit.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/_WKInputDelegate.h>
#import <wtf/BlockPtr.h>

@class MockDropSession;
@class MockDragSession;

@interface MockDragDropSession : NSObject <UIDragDropSession> {
@private
    RetainPtr<NSArray> _mockItems;
    RetainPtr<UIWindow> _window;
}
@property (nonatomic) CGPoint mockLocationInWindow;
@property (nonatomic) BOOL allowMove;
@end

@interface MockDropSession : MockDragDropSession <UIDropSession>
@property (nonatomic, strong) id localContext;
@end

@interface MockDragSession : MockDragDropSession <UIDragSession>
@property (nonatomic, strong) id localContext;
@property (nonatomic, strong) id context;
@end

extern NSString * const DataInteractionEnterEventName;
extern NSString * const DataInteractionOverEventName;
extern NSString * const DataInteractionPerformOperationEventName;
extern NSString * const DataInteractionLeaveEventName;
extern NSString * const DataInteractionStartEventName;

typedef NSDictionary<NSNumber *, NSValue *> *ProgressToCGPointValueMap;

typedef NS_ENUM(NSInteger, DataInteractionPhase) {
    DataInteractionCancelled = 0,
    DataInteractionBeginning = 1,
    DataInteractionBegan = 2,
    DataInteractionEntered = 3,
    DataInteractionPerforming = 4
};

@interface WKWebView (DragAndDropTesting)
- (id <UIDropInteractionDelegate>)dropInteractionDelegate;
- (id <UIDragInteractionDelegate>)dragInteractionDelegate;
- (UIDropInteraction *)dropInteraction;
- (UIDragInteraction *)dragInteraction;
@end

@interface DataInteractionSimulator : NSObject<WKUIDelegatePrivate, _WKInputDelegate> {
@private
    RetainPtr<TestWKWebView> _webView;
    RetainPtr<MockDragSession> _dragSession;
    RetainPtr<MockDropSession> _dropSession;
    RetainPtr<NSMutableArray> _observedEventNames;
    RetainPtr<NSArray> _externalItemProviders;
    RetainPtr<NSArray *> _sourceItemProviders;
    RetainPtr<NSArray *> _finalSelectionRects;
    CGPoint _startLocation;
    CGPoint _endLocation;
    CGRect _lastKnownDragCaretRect;

    RetainPtr<NSMutableDictionary<NSNumber *, NSValue *>>_remainingAdditionalItemRequestLocationsByProgress;
    RetainPtr<NSMutableArray<NSValue *>>_queuedAdditionalItemRequestLocations;
    RetainPtr<NSMutableArray<UITargetedDragPreview *>> _liftPreviews;

    RetainPtr<NSMutableArray<_WKAttachment *>> _insertedAttachments;
    RetainPtr<NSMutableArray<_WKAttachment *>> _removedAttachments;

    bool _isDoneWaitingForInputSession;
    BOOL _shouldPerformOperation;
    double _currentProgress;
    bool _isDoneWithCurrentRun;
    DataInteractionPhase _phase;
}

- (instancetype)initWithWebView:(TestWKWebView *)webView;
// The start location, end location, and locations of additional item requests are all in window coordinates.
- (void)runFrom:(CGPoint)startLocation to:(CGPoint)endLocation;
- (void)runFrom:(CGPoint)startLocation to:(CGPoint)endLocation additionalItemRequestLocations:(ProgressToCGPointValueMap)additionalItemRequestLocations;
- (void)waitForInputSession;

@property (nonatomic) BOOL allowsFocusToStartInputSession;
@property (nonatomic) BOOL shouldEnsureUIApplication;
@property (nonatomic) BOOL shouldAllowMoveOperation;
@property (nonatomic) BlockPtr<BOOL(_WKActivatedElementInfo *)> showCustomActionSheetBlock;
@property (nonatomic) BlockPtr<NSArray *(UIItemProvider *, NSArray *, NSDictionary *)> convertItemProvidersBlock;
@property (nonatomic) BlockPtr<NSArray *(id <UIDropSession>)> overridePerformDropBlock;
@property (nonatomic, strong) NSArray *externalItemProviders;
@property (nonatomic) BlockPtr<NSUInteger(NSUInteger, id)> overrideDataInteractionOperationBlock;
@property (nonatomic) BlockPtr<void(BOOL, NSArray *)> dataInteractionOperationCompletionBlock;

@property (nonatomic, readonly) NSArray *sourceItemProviders;
@property (nonatomic, readonly) NSArray *observedEventNames;
@property (nonatomic, readonly) NSArray *finalSelectionRects;
@property (nonatomic, readonly) DataInteractionPhase phase;
@property (nonatomic, readonly) CGRect lastKnownDragCaretRect;
@property (nonatomic, readonly) NSArray<UITargetedDragPreview *> *liftPreviews;

@property (nonatomic, readonly) NSArray<_WKAttachment *> *insertedAttachments;
@property (nonatomic, readonly) NSArray<_WKAttachment *> *removedAttachments;

@end

#endif // ENABLE(DATA_INTERACTION)
