/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WKInspectorHighlightView.h"

#if PLATFORM(IOS)

#import <WebCore/FloatQuad.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/InspectorOverlay.h>

using namespace WebCore;

@implementation WKInspectorHighlightView

- (instancetype)initWithFrame:(CGRect)frame
{
    if (!(self = [super initWithFrame:frame]))
        return nil;
    _layers = [[NSMutableArray alloc] init];
    return self;
}

- (void)dealloc
{
    [self _removeAllLayers];
    [_layers release];
    [super dealloc];
}

- (void)_removeAllLayers
{
    for (CAShapeLayer *layer in _layers)
        [layer removeFromSuperlayer];
    [_layers removeAllObjects];
}

- (void)_createLayers:(NSUInteger)numLayers
{
    if ([_layers count] == numLayers)
        return;

    [self _removeAllLayers];

    for (NSUInteger i = 0; i < numLayers; ++i) {
        CAShapeLayer *layer = [[CAShapeLayer alloc] init];
        [_layers addObject:layer];
        [self.layer addSublayer:layer];
        [layer release];
    }
}

static bool findIntersectionOnLineBetweenPoints(const FloatPoint& p1, const FloatPoint& p2, const FloatPoint& d1, const FloatPoint& d2, FloatPoint& intersection) 
{
    // Do the lines intersect?
    FloatPoint temporaryIntersectionPoint;
    if (!findIntersection(p1, p2, d1, d2, temporaryIntersectionPoint))
        return false;

    // Is the intersection between the two points on the line?
    if (p1.x() >= p2.x()) {
        if (temporaryIntersectionPoint.x() > p1.x() || temporaryIntersectionPoint.x() < p2.x())
            return false;
    } else {
        if (temporaryIntersectionPoint.x() > p2.x() || temporaryIntersectionPoint.x() < p1.x())
            return false;
    }
    if (p1.y() >= p2.y()) {
        if (temporaryIntersectionPoint.y() > p1.y() || temporaryIntersectionPoint.y() < p2.y())
            return false;
    } else {
        if (temporaryIntersectionPoint.y() > p2.y() || temporaryIntersectionPoint.y() < p1.y())
            return false;
    }

    intersection = temporaryIntersectionPoint;
    return true;
}

// This quad intersection works because the two quads are known to be at the same
// rotation and clockwise-ness.
static FloatQuad quadIntersection(FloatQuad bounds, FloatQuad toClamp)
{
    // Resulting points.
    FloatPoint p1, p2, p3, p4;
    bool containsPoint1 = false;
    bool containsPoint2 = false;
    bool containsPoint3 = false;
    bool containsPoint4 = false;
    bool intersectForPoint1 = false;
    bool intersectForPoint2 = false;
    bool intersectForPoint3 = false;
    bool intersectForPoint4 = false;

    // Top / bottom vertical clamping.
    if (bounds.containsPoint(toClamp.p1())) {
        containsPoint1 = true;
        p1 = toClamp.p1();
    } else if (!(intersectForPoint1 = findIntersectionOnLineBetweenPoints(bounds.p1(), bounds.p2(), toClamp.p1(), toClamp.p4(), p1)))
        p1 = toClamp.p1();

    if (bounds.containsPoint(toClamp.p2())) {
        containsPoint2 = true;
        p2 = toClamp.p2();
    } else if (!(intersectForPoint2 = findIntersectionOnLineBetweenPoints(bounds.p1(), bounds.p2(), toClamp.p2(), toClamp.p3(), p2)))
        p2 = toClamp.p2();

    if (bounds.containsPoint(toClamp.p3())) {
        containsPoint3 = true;
        p3 = toClamp.p3();
    } else if (!(intersectForPoint3 = findIntersectionOnLineBetweenPoints(bounds.p4(), bounds.p3(), toClamp.p2(), toClamp.p3(), p3)))
        p3 = toClamp.p3();

    if (bounds.containsPoint(toClamp.p4())) {
        containsPoint4 = true;
        p4 = toClamp.p4();
    } else if (!(intersectForPoint4 = findIntersectionOnLineBetweenPoints(bounds.p4(), bounds.p3(), toClamp.p1(), toClamp.p4(), p4)))
        p4 = toClamp.p4();

    // If only one of the points intersected on either the top or bottom line then we
    // can clamp the other point on that line to the corner of the bounds.
    if (!containsPoint1 && intersectForPoint2 && !intersectForPoint1) {
        containsPoint1 = true;
        p1 = bounds.p1();
    } else if (!containsPoint2 && intersectForPoint1 && !intersectForPoint2) {
        containsPoint2 = true;
        p2 = bounds.p2();
    }
    if (!containsPoint4 && intersectForPoint3 && !intersectForPoint4) {
        containsPoint4 = true;
        p4 = bounds.p4();
    } else if (!containsPoint3 && intersectForPoint4 && !intersectForPoint3) {
        containsPoint3 = true;
        p3 = bounds.p3();
    }

    // Now we only need to perform horizontal clamping for unadjusted points.
    if (!containsPoint2 && !intersectForPoint2)
        findIntersectionOnLineBetweenPoints(bounds.p2(), bounds.p3(), p1, p2, p2);
    if (!containsPoint3 && !intersectForPoint3)
        findIntersectionOnLineBetweenPoints(bounds.p2(), bounds.p3(), p4, p3, p3);
    if (!containsPoint1 && !intersectForPoint1)
        findIntersectionOnLineBetweenPoints(bounds.p1(), bounds.p4(), p1, p2, p1);
    if (!containsPoint4 && !intersectForPoint4)
        findIntersectionOnLineBetweenPoints(bounds.p1(), bounds.p4(), p4, p3, p4);

    return FloatQuad(p1, p2, p3, p4);
}

static void layerPathWithHole(CAShapeLayer *layer, const FloatQuad& outerQuad, const FloatQuad& holeQuad)
{
    // Nothing to show.
    if (outerQuad == holeQuad || holeQuad.containsQuad(outerQuad)) {
        layer.path = NULL;
        return;
    }

    // If there is a negative margin / padding then the outer box might not
    // fully contain the hole box. In such cases we recalculate the hole to
    // be the intersection of the two quads.
    FloatQuad innerHole;
    if (outerQuad.containsQuad(holeQuad))
        innerHole = holeQuad;
    else
        innerHole = quadIntersection(outerQuad, holeQuad);

    // Clockwise inside rect (hole), Counter-Clockwise outside rect (fill).
    CGMutablePathRef path = CGPathCreateMutable();
    CGPathMoveToPoint(path, 0, innerHole.p1().x(), innerHole.p1().y());
    CGPathAddLineToPoint(path, 0, innerHole.p2().x(), innerHole.p2().y());
    CGPathAddLineToPoint(path, 0, innerHole.p3().x(), innerHole.p3().y());
    CGPathAddLineToPoint(path, 0, innerHole.p4().x(), innerHole.p4().y());
    CGPathMoveToPoint(path, 0, outerQuad.p1().x(), outerQuad.p1().y());
    CGPathAddLineToPoint(path, 0, outerQuad.p4().x(), outerQuad.p4().y());
    CGPathAddLineToPoint(path, 0, outerQuad.p3().x(), outerQuad.p3().y());
    CGPathAddLineToPoint(path, 0, outerQuad.p2().x(), outerQuad.p2().y());
    layer.path = path;
    CGPathRelease(path);
}

static void layerPath(CAShapeLayer *layer, const FloatQuad& outerQuad)
{
    CGMutablePathRef path = CGPathCreateMutable();
    CGPathMoveToPoint(path, 0, outerQuad.p1().x(), outerQuad.p1().y());
    CGPathAddLineToPoint(path, 0, outerQuad.p4().x(), outerQuad.p4().y());
    CGPathAddLineToPoint(path, 0, outerQuad.p3().x(), outerQuad.p3().y());
    CGPathAddLineToPoint(path, 0, outerQuad.p2().x(), outerQuad.p2().y());
    layer.path = path;
    CGPathRelease(path);
}

- (void)_layoutForNodeHighlight:(const Highlight&)highlight
{
    [self _createLayers:4];

    CAShapeLayer *marginLayer = [_layers objectAtIndex:0];
    CAShapeLayer *borderLayer = [_layers objectAtIndex:1];
    CAShapeLayer *paddingLayer = [_layers objectAtIndex:2];
    CAShapeLayer *contentLayer = [_layers objectAtIndex:3];

    FloatQuad marginQuad = highlight.quads[0];
    FloatQuad borderQuad = highlight.quads[1];
    FloatQuad paddingQuad = highlight.quads[2];
    FloatQuad contentQuad = highlight.quads[3];

    marginLayer.fillColor = cachedCGColor(highlight.marginColor, ColorSpaceDeviceRGB);
    borderLayer.fillColor = cachedCGColor(highlight.borderColor, ColorSpaceDeviceRGB);
    paddingLayer.fillColor = cachedCGColor(highlight.paddingColor, ColorSpaceDeviceRGB);
    contentLayer.fillColor = cachedCGColor(highlight.contentColor, ColorSpaceDeviceRGB);

    layerPathWithHole(marginLayer, marginQuad, borderQuad);
    layerPathWithHole(borderLayer, borderQuad, paddingQuad);
    layerPathWithHole(paddingLayer, paddingQuad, contentQuad);
    layerPath(contentLayer, contentQuad);
}

- (void)_layoutForRectsHighlight:(const Highlight&)highlight
{
    NSUInteger numLayers = (NSUInteger)highlight.quads.size();
    if (!numLayers) {
        [self _removeAllLayers];
        return;
    }

    [self _createLayers:numLayers];

    CGColorRef contentColor = cachedCGColor(highlight.contentColor, ColorSpaceDeviceRGB);
    for (NSUInteger i = 0; i < numLayers; ++i) {
        CAShapeLayer *layer = [_layers objectAtIndex:i];
        layer.fillColor = contentColor;
        layerPath(layer, highlight.quads[i]);
    }
}

- (void)update:(const Highlight&)highlight
{
    if (highlight.type == HighlightTypeNode)
        [self _layoutForNodeHighlight:highlight];
    else if (highlight.type == HighlightTypeRects)
        [self _layoutForRectsHighlight:highlight];
}

@end

#endif
