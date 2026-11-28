/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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

#import "UIKitSPI.h"
#import <wtf/OptionSet.h>
#import <wtf/RetainPtr.h>

@class WKTextSelectionRect;

namespace WebKit {

struct WebAutocorrectionContext;

enum class TextPositionAnchor : uint8_t {
    Start = 1 << 0,
    End = 1 << 1
};

} // namespace WebKit

#if USE(BROWSERENGINEKIT)

@interface WKUITextSelectionRect : UITextSelectionRect
+ (instancetype)selectionRectWithCGRect:(CGRect)rect;
@end

#endif // USE(BROWSERENGINEKIT)

@interface WKRelativeTextPosition : UITextPosition
- (instancetype)initWithAnchors:(OptionSet<WebKit::TextPositionAnchor>)start offset:(NSInteger)offset;
@property (nonatomic, readonly) OptionSet<WebKit::TextPositionAnchor> anchors;
@property (nonatomic, readonly, getter=isRelativeToStart) BOOL relativeToStart;
@property (nonatomic, readonly) NSInteger offset;
@end

@interface WKRelativeTextRange : UITextRange
- (instancetype)initWithStart:(UITextPosition *)start end:(UITextPosition *)end;
@property (nonatomic, readonly) WKRelativeTextPosition *start;
@property (nonatomic, readonly) WKRelativeTextPosition *end;
@end

@interface WKTextRange : UITextRange {
    CGRect _startRect;
    CGRect _endRect;
    BOOL _isNone;
    BOOL _isRange;
    BOOL _isEditable;
    RetainPtr<NSArray<WKTextSelectionRect *>> _selectionRects;
    NSUInteger _selectedTextLength;
}
@property (nonatomic) CGRect startRect;
@property (nonatomic) CGRect endRect;
@property (nonatomic) BOOL isNone;
@property (nonatomic) BOOL isRange;
@property (nonatomic) BOOL isEditable;
@property (nonatomic) NSUInteger selectedTextLength;
@property (copy, nonatomic) NSArray<WKTextSelectionRect *> *selectionRects;

+ (WKTextRange *)textRangeWithState:(BOOL)isNone isRange:(BOOL)isRange isEditable:(BOOL)isEditable startRect:(CGRect)startRect endRect:(CGRect)endRect selectionRects:(NSArray *)selectionRects selectedTextLength:(NSUInteger)selectedTextLength;

@end

@interface WKTextPosition : UITextPosition

@property (nonatomic) CGRect positionRect;
@property (nonatomic) OptionSet<WebKit::TextPositionAnchor> anchors;

+ (WKTextPosition *)textPositionWithRect:(CGRect)positionRect;

@end

@interface WKAutocorrectionRects : UIWKAutocorrectionRects
+ (WKAutocorrectionRects *)autocorrectionRectsWithFirstCGRect:(CGRect)firstRect lastCGRect:(CGRect)lastRect;
@end

@interface WKAutocorrectionContext : UIWKAutocorrectionContext
+ (WKAutocorrectionContext *)emptyAutocorrectionContext;
+ (WKAutocorrectionContext *)autocorrectionContextWithWebContext:(const WebKit::WebAutocorrectionContext&)context;
@end

#endif // PLATFORM(IOS_FAMILY)
