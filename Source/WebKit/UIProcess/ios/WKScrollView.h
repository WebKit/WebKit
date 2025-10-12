/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WKBaseScrollView.h"

@class WKUIScrollEdgeEffect;
@class WKWebView;

@interface WKScrollView : WKBaseScrollView

@property (nonatomic, assign) WKWebView <WKBEScrollViewDelegate> *internalDelegate;

- (void)_setBackgroundColorInternal:(UIColor *)backgroundColor;
- (void)_setIndicatorStyleInternal:(UIScrollViewIndicatorStyle)indicatorStyle;
- (void)_setContentSizePreservingContentOffsetDuringRubberband:(CGSize)contentSize;
- (void)_setScrollEnabledInternal:(BOOL)enabled;
- (void)_setZoomEnabledInternal:(BOOL)enabled;
- (void)_setBouncesInternal:(BOOL)horizontal vertical:(BOOL)vertical;
- (BOOL)_setContentScrollInsetInternal:(UIEdgeInsets)insets;
- (void)_setDecelerationRateInternal:(UIScrollViewDecelerationRate)rate;

- (void)_resetContentInset;
@property (nonatomic, readonly) BOOL _contentInsetWasExternallyOverridden;

// FIXME: Likely we can remove this special case for watchOS.
#if !PLATFORM(WATCHOS)
@property (nonatomic, assign, readonly) BOOL _contentInsetAdjustmentBehaviorWasExternallyOverridden;
- (void)_setContentInsetAdjustmentBehaviorInternal:(UIScrollViewContentInsetAdjustmentBehavior)insetAdjustmentBehavior;
- (void)_resetContentInsetAdjustmentBehavior;
#endif

#if HAVE(LIQUID_GLASS)
@property (nonatomic, readonly) BOOL _usesHardTopScrollEdgeEffect;
- (void)_didChangeTopScrollEdgeEffectStyle;
- (void)_setInternalTopPocketColor:(UIColor *)color;
- (WKUIScrollEdgeEffect *)_wk_topEdgeEffect;
- (WKUIScrollEdgeEffect *)_wk_leftEdgeEffect;
- (WKUIScrollEdgeEffect *)_wk_rightEdgeEffect;
- (WKUIScrollEdgeEffect *)_wk_bottomEdgeEffect;
#endif // HAVE(LIQUID_GLASS)

@end

#endif // PLATFORM(IOS_FAMILY)
