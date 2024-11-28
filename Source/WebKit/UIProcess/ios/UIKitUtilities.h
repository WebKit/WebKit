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

#import <UIKit/UIKit.h>
#import <wtf/RetainPtr.h>

@interface UIScrollView (WebKitInternal)
@property (readonly, nonatomic) BOOL _wk_isInterruptingDeceleration;
@property (readonly, nonatomic) BOOL _wk_isScrolledBeyondExtents;
@property (readonly, nonatomic) BOOL _wk_canScrollHorizontallyWithoutBouncing;
@property (readonly, nonatomic) BOOL _wk_canScrollVerticallyWithoutBouncing;
@property (readonly, nonatomic) CGFloat _wk_contentWidthIncludingInsets;
@property (readonly, nonatomic) CGFloat _wk_contentHeightIncludingInsets;
@property (readonly, nonatomic) BOOL _wk_isScrollAnimating;
@property (readonly, nonatomic) BOOL _wk_isZoomAnimating;
- (void)_wk_setContentOffsetAndShowScrollIndicators:(CGPoint)offset animated:(BOOL)animated;
- (void)_wk_setTransfersHorizontalScrollingToParent:(BOOL)value;
- (void)_wk_setTransfersVerticalScrollingToParent:(BOOL)value;
- (void)_wk_stopScrollingAndZooming;
- (CGPoint)_wk_clampToScrollExtents:(CGPoint)contentOffset;
@end

@interface UIGestureRecognizer (WebKitInternal)
@property (nonatomic, readonly) BOOL _wk_isTextInteractionLoupeGesture;
@property (nonatomic, readonly) BOOL _wk_isTextInteractionTapGesture;
@property (nonatomic, readonly) BOOL _wk_hasRecognizedOrEnded;
@end

@interface UIView (WebKitInternal)
@property (nonatomic, readonly) UIScrollView *_wk_parentScrollView;
@property (nonatomic, readonly) UIViewController *_wk_viewControllerForFullScreenPresentation;
@end

@interface UIViewController (WebKitInternal)
@property (nonatomic, readonly) BOOL _wk_isInFullscreenPresentation;
@end

namespace WebKit {

RetainPtr<UIAlertController> createUIAlertController(NSString *title, NSString *message);
UIScrollView *scrollViewForTouches(NSSet<UITouch *> *);

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
