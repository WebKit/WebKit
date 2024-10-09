/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WKTextSelectionRect.h"

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif
#import <WebCore/SelectionGeometry.h>

#if HAVE(UI_TEXT_SELECTION_RECT_CUSTOM_HANDLE_INFO)

@interface WKTextSelectionRectCustomHandleInfo : UITextSelectionRectCustomHandleInfo
- (instancetype)initWithFloatQuad:(const WebCore::FloatQuad&)quad isHorizontal:(BOOL)isHorizontal;
@end

@implementation WKTextSelectionRectCustomHandleInfo {
    WebCore::FloatQuad _quad;
    BOOL _isHorizontal;
}

- (instancetype)initWithFloatQuad:(const WebCore::FloatQuad&)quad isHorizontal:(BOOL)isHorizontal
{
    if (!(self = [super init]))
        return nil;

    _quad = quad;
    _isHorizontal = isHorizontal;
    return self;
}

- (CGPoint)bottomLeft
{
    return _isHorizontal ? _quad.p4() : _quad.p2();
}

- (CGPoint)topLeft
{
    return _isHorizontal ? _quad.p1() : _quad.p3();
}

- (CGPoint)bottomRight
{
    return _isHorizontal ? _quad.p3() : _quad.p1();
}

- (CGPoint)topRight
{
    return _isHorizontal ? _quad.p2() : _quad.p4();
}

@end

#endif // HAVE(UI_TEXT_SELECTION_RECT_CUSTOM_HANDLE_INFO)

#if PLATFORM(IOS_FAMILY) || HAVE(NSTEXTPLACEHOLDER_RECTS)

@implementation WKTextSelectionRect {
    WebCore::SelectionGeometry _selectionGeometry;
    CGFloat _scaleFactor;
}

- (instancetype)initWithCGRect:(CGRect)rect
{
    WebCore::SelectionGeometry selectionGeometry;
    selectionGeometry.setRect(WebCore::enclosingIntRect(rect));
    return [self initWithSelectionGeometry:WTFMove(selectionGeometry) scaleFactor:1];
}

- (instancetype)initWithSelectionGeometry:(const WebCore::SelectionGeometry&)selectionGeometry scaleFactor:(CGFloat)scaleFactor
{
    if (!(self = [super init]))
        return nil;

    _selectionGeometry = selectionGeometry;
    _scaleFactor = scaleFactor;
    return self;
}

#if PLATFORM(IOS_FAMILY)
- (UIBezierPath *)_path
{
    if (_selectionGeometry.behavior() == WebCore::SelectionRenderingBehavior::CoalesceBoundingRects)
        return nil;

    auto selectionBounds = _selectionGeometry.rect();
    auto quad = _selectionGeometry.quad();
    quad.scale(_scaleFactor);
    quad.move(-selectionBounds.x() * _scaleFactor, -selectionBounds.y() * _scaleFactor);

    auto result = [UIBezierPath bezierPath];
    [result moveToPoint:quad.p1()];
    [result addLineToPoint:quad.p2()];
    [result addLineToPoint:quad.p3()];
    [result addLineToPoint:quad.p4()];
    [result addLineToPoint:quad.p1()];
    [result closePath];
    return result;
}

- (NSWritingDirection)writingDirection
{
    if (_selectionGeometry.direction() == WebCore::TextDirection::LTR || _selectionGeometry.mayAppearLogicallyDiscontiguous())
        return NSWritingDirectionLeftToRight;

    return NSWritingDirectionRightToLeft;
}

- (UITextRange *)range
{
    return nil;
}
#endif

#if HAVE(UI_TEXT_SELECTION_RECT_CUSTOM_HANDLE_INFO)

- (WKTextSelectionRectCustomHandleInfo *)_customHandleInfo
{
    if (_selectionGeometry.behavior() == WebCore::SelectionRenderingBehavior::CoalesceBoundingRects)
        return nil;

    auto scaledQuad = _selectionGeometry.quad();
#if !HAVE(REDESIGNED_TEXT_CURSOR)
    scaledQuad.scale(_scaleFactor);
#endif
    return adoptNS([[WKTextSelectionRectCustomHandleInfo alloc] initWithFloatQuad:scaledQuad isHorizontal:_selectionGeometry.isHorizontal()]).autorelease();
}
#endif // HAVE(UI_TEXT_SELECTION_RECT_CUSTOM_HANDLE_INFO)

- (CGRect)rect
{
    return _selectionGeometry.rect();
}

- (BOOL)containsStart
{
    return _selectionGeometry.containsStart();
}

- (BOOL)containsEnd
{
    return _selectionGeometry.containsEnd();
}

- (BOOL)isVertical
{
    if (_selectionGeometry.behavior() == WebCore::SelectionRenderingBehavior::UseIndividualQuads) {
        // FIXME: Use `!_selectionGeometry.isHorizontal()` for this once rdar://106847585 is fixed.
        return NO;
    }

    return !_selectionGeometry.isHorizontal();
}

@end

#endif
