/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import <UIKit/UIKit.h>

namespace WebKit {

enum class ShouldDeferGestures : bool { No, Yes };
enum class ShouldPreventGestures : bool { No, Yes };

} // namespace WebKit

@class WKDeferringGestureRecognizer;

@protocol WKDeferringGestureRecognizerDelegate
- (BOOL)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer shouldDeferOtherGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer;
- (WebKit::ShouldDeferGestures)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer willBeginTouchesWithEvent:(UIEvent *)event;
- (void)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer didEndTouchesWithEvent:(UIEvent *)event;
- (void)deferringGestureRecognizer:(WKDeferringGestureRecognizer *)deferringGestureRecognizer didTransitionToState:(UIGestureRecognizerState)state;
@end

@interface WKDeferringGestureRecognizer : UIGestureRecognizer

- (instancetype)initWithDeferringGestureDelegate:(id <WKDeferringGestureRecognizerDelegate>)deferringGestureDelegate;

- (BOOL)shouldDeferGestureRecognizer:(UIGestureRecognizer *)gestureRecognizer;
- (void)endDeferral:(WebKit::ShouldPreventGestures)shouldPreventGestures;

@property (nonatomic) BOOL immediatelyFailsAfterTouchEnd;

@end

#endif // PLATFORM(IOS_FAMILY)
