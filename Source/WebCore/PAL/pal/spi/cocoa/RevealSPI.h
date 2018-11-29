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

SOFT_LINK_PRIVATE_FRAMEWORK(Reveal)
SOFT_LINK_PRIVATE_FRAMEWORK(RevealCore)
SOFT_LINK_CLASS_OPTIONAL(Reveal, RVPresenter)
SOFT_LINK_CLASS_OPTIONAL(Reveal, RVPresentingContext)
SOFT_LINK_CLASS_OPTIONAL(RevealCore, RVItem)
SOFT_LINK_CLASS_OPTIONAL(RevealCore, RVSelection)

#if USE(APPLE_INTERNAL_SDK)

#if PLATFORM(MAC)
#import <Reveal/RVPresenter.h>
#import <Reveal/Reveal.h>
#endif // PLATFORM(MAC)
#import <RevealCore/RVSelection.h>
#import <RevealCore/RevealCore.h>

#else // USE(APPLE_INTERNAL_SDK)


@protocol RVPresenterHighlightDelegate;

@interface RVItem : NSObject <NSSecureCoding>
- (instancetype)initWithText:(NSString *)text selectedRange:(NSRange)selectedRange NS_DESIGNATED_INITIALIZER;
@end

@interface RVSelection : NSObject
+ (NSRange)revealRangeAtIndex:(NSUInteger)clickIndex selectedRanges:(NSArray <NSValue *> *)selectedRanges shouldUpdateSelection:(BOOL *)shouldUpdateSelection;
@end

@interface RVPresentingContext : NSObject
- (instancetype)initWithPointerLocationInView:(NSPoint)pointerLocationInView inView:(NSView *)view highlightDelegate:(id<RVPresenterHighlightDelegate>)highlightDelegate;
@end

@protocol RVPresenterHighlightDelegate <NSObject>
@required
- (NSArray <NSValue *> *)revealContext:(RVPresentingContext *)context rectsForItem:(RVItem *)item;
@optional
- (void)revealContext:(RVPresentingContext *)context stopHighlightingItem:(RVItem *)item;
- (void)revealContext:(RVPresentingContext *)context drawRectsForItem:(RVItem *)item;
@end

@interface RVDocumentContext : NSObject < NSSecureCoding >
@end

@interface RVPresenter : NSObject
#if PLATFORM(MAC)
- (id<NSImmediateActionAnimationController>)animationControllerForItem:(RVItem *)item documentContext:(RVDocumentContext *)documentContext presentingContext:(RVPresentingContext *)presentingContext options:(NSDictionary *)options;
#endif // PLATFORM(MAC)
- (BOOL)revealItem:(RVItem *)item documentContext:(RVDocumentContext *)documentContext presentingContext:(RVPresentingContext *)presentingContext options:(NSDictionary *)options;
@end

#endif // !USE(APPLE_INTERNAL_SDK)
