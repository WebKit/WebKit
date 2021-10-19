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

#if PLATFORM(IOS_FAMILY)

#import <WebCore/FloatLine.h>
#import <WebCore/FloatQuad.h>
#import <WebCore/FontCascade.h>
#import <WebCore/FontCascadeDescription.h>
#import <WebCore/GeometryUtilities.h>
#import <WebCore/TextRun.h>

@implementation WKInspectorHighlightView

- (instancetype)initWithFrame:(CGRect)frame
{
    if (!(self = [super initWithFrame:frame]))
        return nil;
    _layers = adoptNS([[NSMutableArray alloc] init]);
    _gridOverlayLayers = adoptNS([[NSMutableArray alloc] init]);
    return self;
}

- (void)dealloc
{
    [self _removeAllLayers];
    [super dealloc];
}

- (void)_removeAllLayers
{
    for (CAShapeLayer *layer in _layers.get())
        [layer removeFromSuperlayer];
    [_layers removeAllObjects];
    for (CALayer *layer in _gridOverlayLayers.get())
        [layer removeFromSuperlayer];
    [_gridOverlayLayers removeAllObjects];
}

- (void)_createLayers:(NSUInteger)numLayers
{
    if ([_layers count] == numLayers)
        return;

    for (NSUInteger i = 0; i < numLayers; ++i) {
        auto layer = adoptNS([[CAShapeLayer alloc] init]);
        [_layers addObject:layer.get()];
        [self.layer addSublayer:layer.get()];
    }
}

static bool findIntersectionOnLineBetweenPoints(const WebCore::FloatPoint& p1, const WebCore::FloatPoint& p2, const WebCore::FloatPoint& d1, const WebCore::FloatPoint& d2, WebCore::FloatPoint& intersection) 
{
    // Do the lines intersect?
    WebCore::FloatPoint temporaryIntersectionPoint;
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
static WebCore::FloatQuad quadIntersection(WebCore::FloatQuad bounds, WebCore::FloatQuad toClamp)
{
    // Resulting points.
    WebCore::FloatPoint p1, p2, p3, p4;
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

    return WebCore::FloatQuad(p1, p2, p3, p4);
}

static void layerPathWithHole(CAShapeLayer *layer, const WebCore::FloatQuad& outerQuad, const WebCore::FloatQuad& holeQuad)
{
    // Nothing to show.
    if (outerQuad == holeQuad || holeQuad.containsQuad(outerQuad)) {
        layer.path = NULL;
        return;
    }

    // If there is a negative margin / padding then the outer box might not
    // fully contain the hole box. In such cases we recalculate the hole to
    // be the intersection of the two quads.
    WebCore::FloatQuad innerHole;
    if (outerQuad.containsQuad(holeQuad))
        innerHole = holeQuad;
    else
        innerHole = quadIntersection(outerQuad, holeQuad);

    // Clockwise inside rect (hole), Counter-Clockwise outside rect (fill).
    auto path = adoptCF(CGPathCreateMutable());
    CGPathMoveToPoint(path.get(), 0, innerHole.p1().x(), innerHole.p1().y());
    CGPathAddLineToPoint(path.get(), 0, innerHole.p2().x(), innerHole.p2().y());
    CGPathAddLineToPoint(path.get(), 0, innerHole.p3().x(), innerHole.p3().y());
    CGPathAddLineToPoint(path.get(), 0, innerHole.p4().x(), innerHole.p4().y());
    CGPathMoveToPoint(path.get(), 0, outerQuad.p1().x(), outerQuad.p1().y());
    CGPathAddLineToPoint(path.get(), 0, outerQuad.p4().x(), outerQuad.p4().y());
    CGPathAddLineToPoint(path.get(), 0, outerQuad.p3().x(), outerQuad.p3().y());
    CGPathAddLineToPoint(path.get(), 0, outerQuad.p2().x(), outerQuad.p2().y());
    layer.path = path.get();
}

static void layerPath(CAShapeLayer *layer, const WebCore::FloatQuad& outerQuad)
{
    auto path = adoptCF(CGPathCreateMutable());
    CGPathMoveToPoint(path.get(), 0, outerQuad.p1().x(), outerQuad.p1().y());
    CGPathAddLineToPoint(path.get(), 0, outerQuad.p4().x(), outerQuad.p4().y());
    CGPathAddLineToPoint(path.get(), 0, outerQuad.p3().x(), outerQuad.p3().y());
    CGPathAddLineToPoint(path.get(), 0, outerQuad.p2().x(), outerQuad.p2().y());
    CGPathCloseSubpath(path.get());
    layer.path = path.get();
}

- (void)_layoutForNodeHighlight:(const WebCore::InspectorOverlay::Highlight&)highlight offset:(unsigned)offset
{
    ASSERT([_layers count] >= offset + 4);
    ASSERT(highlight.quads.size() >= offset + 4);
    if ([_layers count] < offset + 4 || highlight.quads.size() < offset + 4)
        return;

    CAShapeLayer *marginLayer = [_layers objectAtIndex:offset];
    CAShapeLayer *borderLayer = [_layers objectAtIndex:offset + 1];
    CAShapeLayer *paddingLayer = [_layers objectAtIndex:offset + 2];
    CAShapeLayer *contentLayer = [_layers objectAtIndex:offset + 3];

    WebCore::FloatQuad marginQuad = highlight.quads[offset];
    WebCore::FloatQuad borderQuad = highlight.quads[offset + 1];
    WebCore::FloatQuad paddingQuad = highlight.quads[offset + 2];
    WebCore::FloatQuad contentQuad = highlight.quads[offset + 3];

    marginLayer.fillColor = cachedCGColor(highlight.marginColor).get();
    borderLayer.fillColor = cachedCGColor(highlight.borderColor).get();
    paddingLayer.fillColor = cachedCGColor(highlight.paddingColor).get();
    contentLayer.fillColor = cachedCGColor(highlight.contentColor).get();

    layerPathWithHole(marginLayer, marginQuad, borderQuad);
    layerPathWithHole(borderLayer, borderQuad, paddingQuad);
    layerPathWithHole(paddingLayer, paddingQuad, contentQuad);
    layerPath(contentLayer, contentQuad);
}

- (void)_layoutForNodeListHighlight:(const WebCore::InspectorOverlay::Highlight&)highlight
{
    if (!highlight.quads.size())
        return;

    unsigned nodeCount = highlight.quads.size() / 4;
    [self _createLayers:nodeCount * 4];

    for (unsigned i = 0; i < nodeCount; ++i)
        [self _layoutForNodeHighlight:highlight offset:i * 4];
}

- (void)_layoutForRectsHighlight:(const WebCore::InspectorOverlay::Highlight&)highlight
{
    NSUInteger numLayers = highlight.quads.size();
    if (!numLayers)
        return;

    [self _createLayers:numLayers];

    auto contentColor = cachedCGColor(highlight.contentColor);
    for (NSUInteger i = 0; i < numLayers; ++i) {
        CAShapeLayer *layer = [_layers objectAtIndex:i];
        layer.fillColor = contentColor.get();
        layerPath(layer, highlight.quads[i]);
    }
}

- (void)_createGridOverlayLayers:(const WebCore::InspectorOverlay::Highlight&)highlight scale:(double)scale
{
    for (auto gridOverlay : highlight.gridHighlightOverlays) {
        auto layer = [self _createGridOverlayLayer:gridOverlay scale:scale];
        [_gridOverlayLayers addObject:layer];
        [self.layer addSublayer:layer];
    }
}

static CALayer * createLayoutHatchingLayer(WebCore::FloatQuad quad, WebCore::Color strokeColor)
{
    CAShapeLayer *layer = [CAShapeLayer layer];

    constexpr auto hatchSpacing = 12;
    auto hatchPath = adoptCF(CGPathCreateMutable());

    WebCore::FloatLine topSide = { quad.p1(), quad.p2() };
    WebCore::FloatLine leftSide = { quad.p1(), quad.p4() };

    // The opposite axis' length is used to determine how far to draw a hatch line in both dimensions, which keeps the lines at a 45deg angle.
    if (topSide.length() > leftSide.length()) {
        WebCore::FloatLine bottomSide = { quad.p4(), quad.p3() };
        // Move across the relative top of the quad, starting left of `0, 0` to ensure that the tail of the previous hatch line is drawn while scrolling.
        for (float x = -leftSide.length(); x < topSide.length(); x += hatchSpacing) {
            auto startPoint = topSide.pointAtAbsoluteDistance(x);
            auto endPoint = bottomSide.pointAtAbsoluteDistance(x + leftSide.length());
            CGPathMoveToPoint(hatchPath.get(), 0, startPoint.x(), startPoint.y());
            CGPathAddLineToPoint(hatchPath.get(), 0, endPoint.x(), endPoint.y());
        }
    } else {
        WebCore::FloatLine rightSide = { quad.p2(), quad.p3() };
        // Move down the relative left side of the quad, starting above `0, 0` to ensure that the tail of the previous hatch line is drawn while scrolling.
        for (float y = -topSide.length(); y < leftSide.length(); y += hatchSpacing) {
            auto startPoint = leftSide.pointAtAbsoluteDistance(y);
            auto endPoint = rightSide.pointAtAbsoluteDistance(y + topSide.length());
            CGPathMoveToPoint(hatchPath.get(), 0, startPoint.x(), startPoint.y());
            CGPathAddLineToPoint(hatchPath.get(), 0, endPoint.x(), endPoint.y());
        }
    }
    layer.path = hatchPath.get();
    layer.strokeColor = cachedCGColor(strokeColor).get();
    layer.lineWidth = 0.5;
    layer.lineDashPattern = @[[NSNumber numberWithInt:2], [NSNumber numberWithInt:2]];

    CAShapeLayer *maskLayer = [CAShapeLayer layer];
    layerPath(maskLayer, quad);
    layer.mask = maskLayer;
    return layer;
}

static CALayer * createLayoutLabelLayer(String label, WebCore::FloatPoint point, WebCore::InspectorOverlay::LabelArrowDirection arrowDirection, WebCore::InspectorOverlay::LabelArrowEdgePosition arrowEdgePosition, WebCore::Color backgroundColor, WebCore::Color strokeColor, double scale, float maximumWidth = 0)
{
    auto font = WebCore::InspectorOverlay::fontForLayoutLabel();

    constexpr auto padding = 4;
    constexpr auto arrowSize = 6;
    float textHeight = font.fontMetrics().floatHeight();

    float textWidth = font.width(WebCore::TextRun(label));
    if (maximumWidth && textWidth + (padding * 2) > maximumWidth) {
        label.append("..."_s);
        while (textWidth + (padding * 2) > maximumWidth && label.length() >= 4) {
            // Remove the fourth from last character (the character before the ellipsis) and remeasure.
            label.remove(label.length() - 4);
            textWidth = font.width(WebCore::TextRun(label));
        }
    }

    // Note: Implementation Difference - The textPosition is the center of text, unlike WebCore::InspectorOverlay, where the textPosition is leftmost point on the baseline of the text.
    WebCore::FloatPoint textPosition;
    switch (arrowDirection) {
    case WebCore::InspectorOverlay::LabelArrowDirection::Down:
        switch (arrowEdgePosition) {
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Leading:
            textPosition = WebCore::FloatPoint((textWidth / 2) + padding, -(textHeight / 2) - arrowSize - padding);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Middle:
            textPosition = WebCore::FloatPoint(0, -(textHeight / 2) - arrowSize - padding);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Trailing:
            textPosition = WebCore::FloatPoint(-(textWidth / 2) - padding, -(textHeight / 2) - arrowSize - padding);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }
        break;
    case WebCore::InspectorOverlay::LabelArrowDirection::Up:
        switch (arrowEdgePosition) {
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Leading:
            textPosition = WebCore::FloatPoint((textWidth / 2) + padding, (textHeight / 2) + arrowSize + padding);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Middle:
            textPosition = WebCore::FloatPoint(0, (textHeight / 2) + arrowSize + padding);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Trailing:
            textPosition = WebCore::FloatPoint(-(textWidth / 2) - padding, (textHeight / 2) + arrowSize + padding);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }
        break;
    case WebCore::InspectorOverlay::LabelArrowDirection::Right:
        switch (arrowEdgePosition) {
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Leading:
            textPosition = WebCore::FloatPoint(-(textWidth / 2) - arrowSize - padding, (textHeight / 2) + padding);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Middle:
            textPosition = WebCore::FloatPoint(-(textWidth / 2) - arrowSize - padding, 0);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Trailing:
            textPosition = WebCore::FloatPoint(-(textWidth / 2) - arrowSize - padding, -(textHeight / 2) - padding);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }
        break;
    case WebCore::InspectorOverlay::LabelArrowDirection::Left:
        switch (arrowEdgePosition) {
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Leading:
            textPosition = WebCore::FloatPoint((textWidth / 2) + arrowSize + padding, (textHeight / 2) + padding);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Middle:
            textPosition = WebCore::FloatPoint((textWidth / 2) + arrowSize + padding, 0);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::Trailing:
            textPosition = WebCore::FloatPoint((textWidth / 2) + arrowSize + padding, -(textHeight / 2) - padding);
            break;
        case WebCore::InspectorOverlay::LabelArrowEdgePosition::None:
            break;
        }
        break;
    case WebCore::InspectorOverlay::LabelArrowDirection::None:
        // Text position will remain (0, 0).
        break;
    }

    CALayer *layer = [CALayer layer];

#if USE(CG)
    // WebCore::Path::PlatformPathPtr is only a CGPath* when `USE(CG)` is true.
    auto labelPath = WebCore::InspectorOverlay::backgroundPathForLayoutLabel(textWidth + (padding * 2), textHeight + (padding * 2), arrowDirection, arrowEdgePosition, arrowSize);
    CGPath* platformLabelPath = labelPath.ensurePlatformPath();

    CAShapeLayer *labelPathLayer = [CAShapeLayer layer];
    labelPathLayer.path = platformLabelPath;
    labelPathLayer.fillColor = cachedCGColor(backgroundColor).get();
    labelPathLayer.strokeColor = cachedCGColor(strokeColor).get();
    labelPathLayer.position = CGPointMake(point.x(), point.y());
    [layer addSublayer:labelPathLayer];
#endif

    CATextLayer *textLayer = [CATextLayer layer];
    textLayer.frame = CGRectMake(0, 0, textWidth, textHeight);
    textLayer.string = label;
    textLayer.font = font.primaryFont().getCTFont();
    textLayer.fontSize = 12;
    textLayer.alignmentMode = kCAAlignmentLeft;
    textLayer.foregroundColor = CGColorGetConstantColor(kCGColorBlack);
    textLayer.position = CGPointMake(textPosition.x() + point.x(), textPosition.y() + point.y());
    textLayer.contentsScale = [[UIScreen mainScreen] scale] * scale;
    [layer addSublayer:textLayer];

    return layer;
}

- (CALayer *)_createGridOverlayLayer:(const WebCore::InspectorOverlay::Highlight::GridHighlightOverlay&)overlay scale:(double)scale
{
    // Keep implementation roughly equivalent to `WebCore::InspectorOverlay::drawGridOverlay`.
    CALayer *layer = [CALayer layer];

    auto gridLinesPath = adoptCF(CGPathCreateMutable());
    for (auto gridLine : overlay.gridLines) {
        CGPathMoveToPoint(gridLinesPath.get(), 0, gridLine.start().x(), gridLine.start().y());
        CGPathAddLineToPoint(gridLinesPath.get(), 0, gridLine.end().x(), gridLine.end().y());
    }
    CAShapeLayer *gridLinesLayer = [CAShapeLayer layer];
    gridLinesLayer.path = gridLinesPath.get();
    gridLinesLayer.lineWidth = 1;
    gridLinesLayer.strokeColor = cachedCGColor(overlay.color).get();
    [layer addSublayer:gridLinesLayer];

    for (auto gapQuad : overlay.gaps)
        [layer addSublayer:createLayoutHatchingLayer(gapQuad, overlay.color)];

    for (auto area : overlay.areas) {
        CAShapeLayer *areaLayer = [CAShapeLayer layer];
        layerPath(areaLayer, area.quad);
        areaLayer.lineWidth = 3;
        areaLayer.fillColor = CGColorGetConstantColor(kCGColorClear);
        areaLayer.strokeColor = cachedCGColor(overlay.color).get();
        [layer addSublayer:areaLayer];
    }

    constexpr auto translucentLabelBackgroundColor = WebCore::Color::white.colorWithAlphaByte(230);

    for (auto area : overlay.areas)
        [layer addSublayer:createLayoutLabelLayer(area.name, area.quad.center(), WebCore::InspectorOverlay::LabelArrowDirection::None, WebCore::InspectorOverlay::LabelArrowEdgePosition::None, translucentLabelBackgroundColor, overlay.color, scale, area.quad.boundingBox().width())];

    for (auto label : overlay.labels)
        [layer addSublayer:createLayoutLabelLayer(label.text, label.location, label.arrowDirection, label.arrowEdgePosition, label.backgroundColor, overlay.color, scale)];

    return layer;
}

- (void)update:(const WebCore::InspectorOverlay::Highlight&)highlight scale:(double)scale
{
    [self _removeAllLayers];

    if (highlight.type == WebCore::InspectorOverlay::Highlight::Type::Node || highlight.type == WebCore::InspectorOverlay::Highlight::Type::NodeList)
        [self _layoutForNodeListHighlight:highlight];
    else if (highlight.type == WebCore::InspectorOverlay::Highlight::Type::Rects)
        [self _layoutForRectsHighlight:highlight];

    [self _createGridOverlayLayers:highlight scale:scale];
}

@end

#endif
