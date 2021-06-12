/*
 * Copyright (C) 2018 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <objc/runtime.h>
#if PLATFORM(MAC)
#import <pal/spi/mac/NSImmediateActionGestureRecognizerSPI.h>
#endif // PLATFORM(MAC)
#import <wtf/SoftLinking.h>

#if ENABLE(REVEAL)

#if USE(APPLE_INTERNAL_SDK)

#if PLATFORM(MAC)
#import <Reveal/RVPresenter.h>
#import <Reveal/Reveal.h>
#endif // PLATFORM(MAC)
#import <RevealCore/RVItem_Private.h>
#import <RevealCore/RVSelection.h>
#import <RevealCore/RevealCore.h>
#else // USE(APPLE_INTERNAL_SDK)

@class DDScannerResult;
@class NSMenuItem;
@protocol RVPresenterHighlightDelegate;

@interface RVItem : NSObject <NSSecureCoding>
- (instancetype)initWithText:(NSString *)text selectedRange:(NSRange)selectedRange NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithDDResult:(DDScannerResult *)result NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithURL:(NSURL *)url rangeInContext:(NSRange)rangeInContext;
@property (readonly, nonatomic) NSRange highlightRange;
@end

@interface RVSelection : NSObject
+ (NSRange)revealRangeAtIndex:(NSUInteger)clickIndex selectedRanges:(NSArray <NSValue *> *)selectedRanges shouldUpdateSelection:(BOOL *)shouldUpdateSelection;
@end

#if PLATFORM(MAC)
@interface RVPresentingContext : NSObject
- (instancetype)initWithPointerLocationInView:(NSPoint)pointerLocationInView inView:(NSView *)view highlightDelegate:(id<RVPresenterHighlightDelegate>)highlightDelegate;
@property (readonly) NSArray <NSValue *> * itemRectsInView;
@end

@protocol RVPresenterHighlightDelegate <NSObject>
@required
- (NSArray <NSValue *> *)revealContext:(RVPresentingContext *)context rectsForItem:(RVItem *)item;
@optional
- (void)revealContext:(RVPresentingContext *)context stopHighlightingItem:(RVItem *)item;
- (void)revealContext:(RVPresentingContext *)context drawRectsForItem:(RVItem *)item;
@end
#endif

@interface RVDocumentContext : NSObject <NSSecureCoding>
@end

@interface RVPresenter : NSObject
#if PLATFORM(MAC)
- (id<NSImmediateActionAnimationController>)animationControllerForItem:(RVItem *)item documentContext:(RVDocumentContext *)documentContext presentingContext:(RVPresentingContext *)presentingContext options:(NSDictionary *)options;
- (BOOL)revealItem:(RVItem *)item documentContext:(RVDocumentContext *)documentContext presentingContext:(RVPresentingContext *)presentingContext options:(NSDictionary *)options;
- (NSArray<NSMenuItem *> *)menuItemsForItem:(RVItem *)item documentContext:(RVDocumentContext *)documentContext presentingContext:(RVPresentingContext *)presentingContext options:(NSDictionary *)options;
#endif // PLATFORM(MAC)
@end

#endif // !USE(APPLE_INTERNAL_SDK)

#endif // ENABLE(REVEAL)
