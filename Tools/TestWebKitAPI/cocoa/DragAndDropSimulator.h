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

#if ENABLE(DRAG_SUPPORT)

#import "TestWKWebView.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/_WKInputDelegate.h>
#import <wtf/BlockPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#import <UIKit/NSItemProvider+UIKitAdditions.h>
#endif

#if PLATFORM(IOS_FAMILY)

typedef NS_ENUM(NSInteger, DragAndDropPhase) {
    DragAndDropPhaseCancelled = 0,
    DragAndDropPhaseBeginning = 1,
    DragAndDropPhaseBegan = 2,
    DragAndDropPhaseEntered = 3,
    DragAndDropPhasePerformingDrop = 4
};

typedef NSDictionary<NSNumber *, NSValue *> *ProgressToCGPointValueMap;

@interface MockDragDropSession : NSObject <UIDragDropSession> {
@private
    RetainPtr<NSArray> _mockItems;
    RetainPtr<UIWindow> _window;
}
@property (nonatomic) CGPoint mockLocationInWindow;
@property (nonatomic) BOOL allowMove;
@end

@interface MockDropSession : MockDragDropSession <UIDropSession>
@end

@interface MockDragSession : MockDragDropSession <UIDragSession>
@end

@interface WKWebView (DragAndDropTesting)
- (id <UIDropInteractionDelegate>)dropInteractionDelegate;
- (id <UIDragInteractionDelegate>)dragInteractionDelegate;
- (UIDropInteraction *)dropInteraction;
- (UIDragInteraction *)dragInteraction;
@end

#endif // PLATFORM(IOS_FAMILY)

@interface DragAndDropSimulator : NSObject<WKUIDelegatePrivate, _WKInputDelegate>

- (instancetype)initWithWebViewFrame:(CGRect)frame;
- (instancetype)initWithWebViewFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration;
// The start location, end location, and locations of additional item requests are all in window coordinates.
- (void)runFrom:(CGPoint)startLocation to:(CGPoint)endLocation;
- (void)endDataTransfer;
- (void)clearExternalDragInformation;
@property (nonatomic, readonly) NSArray<_WKAttachment *> *insertedAttachments;
@property (nonatomic, readonly) NSArray<_WKAttachment *> *removedAttachments;
@property (nonatomic, readonly) TestWKWebView *webView;
@property (nonatomic) WKDragDestinationAction dragDestinationAction;

#if PLATFORM(IOS_FAMILY)

- (instancetype)initWithWebView:(TestWKWebView *)webView;
- (void)runFrom:(CGPoint)startLocation to:(CGPoint)endLocation additionalItemRequestLocations:(ProgressToCGPointValueMap)additionalItemRequestLocations;
- (void)ensureInputSession;

@property (nonatomic, readonly) DragAndDropPhase phase;
@property (nonatomic) BOOL allowsFocusToStartInputSession;
@property (nonatomic) BOOL shouldEnsureUIApplication;
@property (nonatomic) BOOL shouldBecomeFirstResponder;
@property (nonatomic) BOOL shouldAllowMoveOperation;
@property (nonatomic, strong) NSArray *externalItemProviders;
@property (nonatomic, readonly) UIDropProposal *lastKnownDropProposal;

@property (nonatomic, copy) BOOL(^showCustomActionSheetBlock)(_WKActivatedElementInfo *);
@property (nonatomic, copy) NSArray *(^convertItemProvidersBlock)(NSItemProvider *, NSArray *, NSDictionary *);
@property (nonatomic, copy) NSArray *(^overridePerformDropBlock)(id <UIDropSession>);
@property (nonatomic, copy) void(^dropCompletionBlock)(BOOL, NSArray *);
@property (nonatomic, copy) dispatch_block_t sessionWillBeginBlock;
@property (nonatomic, copy) UIDropOperation(^overrideDragUpdateBlock)(UIDropOperation, id <UIDropSession>);

@property (nonatomic, readonly) NSArray *sourceItemProviders;
@property (nonatomic, readonly) NSArray *observedEventNames;
@property (nonatomic, readonly) CGRect finalSelectionStartRect;
@property (nonatomic, readonly) CGRect lastKnownDragCaretRect;
@property (nonatomic, readonly) NSArray<UITargetedDragPreview *> *liftPreviews;
@property (nonatomic, readonly) NSArray<UITargetedDragPreview *> *cancellationPreviews;
@property (nonatomic, readonly) NSArray *dropPreviews;
@property (nonatomic, readonly) NSArray *delayedDropPreviews;

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)

@property (nonatomic, readonly) id <NSDraggingInfo> draggingInfo;
@property (nonatomic, readonly) NSPoint initialDragImageLocationInView;
@property (nonatomic, readonly) NSDragOperation currentDragOperation;
@property (nonatomic, strong) NSPasteboard *externalDragPasteboard;
@property (nonatomic, strong) NSImage *externalDragImage;
@property (nonatomic, readonly) NSArray<NSURL *> *externalPromisedFiles;
@property (nonatomic, copy) dispatch_block_t willEndDraggingHandler;

- (void)writePromisedFiles:(NSArray<NSURL *> *)fileURLs;
- (void)writeFiles:(NSArray<NSURL *> *)fileURLs;
- (NSArray<NSURL *> *)receivePromisedFiles;

#endif // PLATFORM(MAC)

@end

#endif // ENABLE(DRAG_SUPPORT)
