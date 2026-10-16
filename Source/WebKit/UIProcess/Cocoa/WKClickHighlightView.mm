/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "WKClickHighlightView.h"

#if ENABLE(TWO_PHASE_CLICKS)

#import <WebCore/CocoaBezierPath.h>
#import <WebCore/Path.h>
#import <WebCore/PathUtilities.h>
#import <wtf/RetainPtr.h>

static void setNeedsDisplay(CocoaView *view)
{
#if PLATFORM(IOS_FAMILY)
    [view setNeedsDisplay];
#else
    view.needsDisplay = YES;
#endif
}

static void fillBezierPathWithRoundedRectForCornerRadius(CGRect rectBounds, float radius)
{
#if PLATFORM(IOS_FAMILY)
    RetainPtr path = [UIBezierPath bezierPathWithRoundedRect:rectBounds cornerRadius:radius];
#else
    WebCore::Path corePath;
    corePath.addContinuousRoundedRect(rectBounds, radius);
    RetainPtr path = [NSBezierPath bezierPathWithCGPath:protect(corePath.platformPath())];
#endif
    [path fill];
}

static CGContextRef currentGraphicsContext()
{
#if PLATFORM(IOS_FAMILY)
    return UIGraphicsGetCurrentContext();
#else
    return [NSGraphicsContext currentContext].CGContext;
#endif
}

static void appendPath(CocoaBezierPath *path, CocoaBezierPath *subpath)
{
#if PLATFORM(IOS_FAMILY)
    [path appendPath:subpath];
#else
    [path appendBezierPath:subpath];
#endif
}

static void addLineToPoint(CocoaBezierPath *path, const WebCore::FloatPoint& point)
{
#if PLATFORM(IOS_FAMILY)
    [path addLineToPoint:point];
#else
    [path lineToPoint:point];
#endif
}

@implementation WKClickHighlightView {
    RetainPtr<WebCore::CocoaColor> _color;
    float _minimumCornerRadius;
    WebCore::CornerRadii _cornerRadii;
    Vector<WebCore::FloatRect> _innerFrames;
    Vector<WebCore::FloatQuad> _innerQuads;
}

- (id)initWithFrame:(CGRect)rect
{
    if (self = [super initWithFrame:rect])
        self.layer.needsDisplayOnBoundsChange = YES;
    return self;
}

- (void)cleanUp
{
    _innerFrames.clear();
    _innerQuads.clear();
}

- (void)setColor:(WebCore::CocoaColor *)color
{
    _color = color;
}

- (void)setMinimumCornerRadius:(float)radius
{
    _minimumCornerRadius = radius;
}

- (void)setCornerRadii:(WebCore::CornerRadii&&)radii
{
    _cornerRadii = WTF::move(radii);
}

- (void)setFrames:(Vector<WebCore::FloatRect>&&)frames
{
    [self cleanUp];

    if (frames.isEmpty())
        [self setFrame:CGRectZero];

    bool initialized = false;
    WebCore::FloatRect viewFrame;

    for (auto frame : frames) {
        if (std::exchange(initialized, true))
            viewFrame = WebCore::unionRect(viewFrame, frame);
        else {
            viewFrame = frame;
            viewFrame.inflate(_minimumCornerRadius);
        }
    }

    [super setFrame:viewFrame];

    _innerFrames = WTF::move(frames);
    for (auto& frame : _innerFrames)
        frame.moveBy(-viewFrame.location());

    setNeedsDisplay(self);
}

- (void)setQuads:(Vector<WebCore::FloatQuad>&&)quads boundaryRect:(const WebCore::FloatRect&)boundaryRect
{
    [self cleanUp];

    if (quads.isEmpty())
        [self setFrame:CGRectZero];

    bool initialized = false;
    WebCore::FloatPoint minExtent;
    WebCore::FloatPoint maxExtent;

    for (auto& quad : quads) {
        for (auto controlPoint : std::array { quad.p1(), quad.p2(), quad.p3(), quad.p4() }) {
            if (std::exchange(initialized, true)) {
                minExtent = minExtent.shrunkTo(controlPoint);
                maxExtent = maxExtent.expandedTo(controlPoint);
            } else {
                minExtent = controlPoint;
                maxExtent = controlPoint;
            }
        }
    }

    WebCore::FloatRect viewFrame { minExtent, maxExtent };

    viewFrame.inflate(4 * _minimumCornerRadius);
    viewFrame.intersect(boundaryRect);

    [super setFrame:viewFrame];

    _innerQuads = WTF::move(quads);
    for (auto& quad : _innerQuads)
        quad.move(-viewFrame.x(), -viewFrame.y());

    setNeedsDisplay(self);
}

- (void)setFrame:(CGRect)frame
{
    [self cleanUp];

    [super setFrame:frame];
}

- (void)drawRect:(CGRect)aRect
{
    if (_innerFrames.isEmpty() && _innerQuads.isEmpty()) {
        [_color set];
        fillBezierPathWithRoundedRectForCornerRadius(self.bounds, _minimumCornerRadius);
        return;
    }

    RetainPtr path = [CocoaBezierPath bezierPath];

    if (_innerFrames.size()) {
        auto corePath = WebCore::PathUtilities::pathWithShrinkWrappedRects(_innerFrames, _cornerRadii);
        appendPath(path, [CocoaBezierPath bezierPathWithCGPath:protect(corePath.platformPath())]);
    } else {
        for (auto& quad : _innerQuads) {
            RetainPtr subpath = [CocoaBezierPath bezierPath];
            [subpath moveToPoint:quad.p1()];
            addLineToPoint(subpath, quad.p2());
            addLineToPoint(subpath, quad.p3());
            addLineToPoint(subpath, quad.p4());
            [subpath closePath];
            appendPath(path, subpath);
        }
    }

    RetainPtr context = currentGraphicsContext();
    CGContextSaveGState(context);

    if (!_innerQuads.isEmpty())
        CGContextSetLineWidth(context, 4 * _minimumCornerRadius);

    CGContextSetLineJoin(context, kCGLineJoinRound);

    auto alpha = CGColorGetAlpha(protect([_color CGColor]));

    [[_color colorWithAlphaComponent:1] set];

    CGContextSetAlpha(context, alpha);
    CGContextBeginTransparencyLayer(context, nil);
    CGContextAddPath(context, protect([path CGPath]));
    CGContextDrawPath(context, kCGPathFillStroke);
    CGContextEndTransparencyLayer(context);

    CGContextRestoreGState(context);
}

#if PLATFORM(IOS_FAMILY)

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return nil;
}

#else

- (NSView *)hitTest:(NSPoint)point
{
    return nil;
}

- (BOOL)isFlipped
{
    return YES;
}

#endif

@end

#endif
