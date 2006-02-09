/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *           (C) 2006 Alexander Kellett <lypanov@kde.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#if SVG_SUPPORT
#import "KCanvasItemQuartz.h"

#import <kxmlcore/Assertions.h>

#import "kcanvas/RenderPath.h"
#import "kcanvas/KCanvas.h"
#import "KCanvasRenderingStyle.h"
#import "KRenderingFillPainter.h"
#import "KRenderingStrokePainter.h"
#import "KCanvasMatrix.h"

#import "KCanvasPathQuartz.h"
#import "KRenderingDeviceQuartz.h"
#import "KCanvasFilterQuartz.h"
#import "KCanvasResourcesQuartz.h"
#import "KCanvasMaskerQuartz.h"
#import "QuartzSupport.h"

#import "SVGRenderStyle.h"
#import "SVGStyledElementImpl.h"
#import "KCanvasRenderingStyle.h"


KCanvasItemQuartz::KCanvasItemQuartz(khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node) : RenderPath(style, node)
{
}

typedef enum {
    Start,
    Mid,
    End
} MarkerType;

struct MarkerData {
    CGPoint origin;
    double strokeWidth;
    CGPoint inslopePoints[2];
    CGPoint outslopePoints[2];
    MarkerType type;
    KCanvasMarker *marker;
};

struct DrawMarkersData {
    DrawMarkersData(KCanvasMarker *startMarker, KCanvasMarker *midMarker, double strokeWidth);
    
    int elementIndex;
    MarkerData previousMarkerData;
    KCanvasMarker *midMarker;
};

DrawMarkersData::DrawMarkersData(KCanvasMarker *start, KCanvasMarker *mid, double strokeWidth)
{
    elementIndex = 0;
    midMarker = mid;
    
    previousMarkerData.origin = CGPointZero;
    previousMarkerData.strokeWidth = strokeWidth;
    previousMarkerData.marker = start;
    previousMarkerData.type = Start;
}

void drawMarkerWithData(MarkerData &data)
{
    if (!data.marker)
        return;
    
    CGPoint inslopeChange = CGPointSubtractPoints(data.inslopePoints[1], data.inslopePoints[0]);
    CGPoint outslopeChange = CGPointSubtractPoints(data.outslopePoints[1], data.outslopePoints[0]);
    
    static const double deg2rad = M_PI/180.0;
    double inslope = atan2(inslopeChange.y, inslopeChange.x) / deg2rad;
    double outslope = atan2(outslopeChange.y, outslopeChange.x) / deg2rad;
    
    double angle;
    if (data.type == Start)
        angle = outslope;
    else if (data.type == Mid)
        angle = (inslope + outslope) / 2;
    else // (data.type == End)
        angle = inslope;
    
    data.marker->draw(FloatRect(), data.origin.x, data.origin.y, data.strokeWidth, angle);
}

static inline void updateMarkerDataForElement(MarkerData &previousMarkerData, const CGPathElement *element)
{
    CGPoint *points = element->points;
    
    switch (element->type) {
    case kCGPathElementAddQuadCurveToPoint:
        // TODO
        previousMarkerData.origin = points[1];
        break;
    case kCGPathElementAddCurveToPoint:
        previousMarkerData.inslopePoints[0] = points[1];
        previousMarkerData.inslopePoints[1] = points[2];
        previousMarkerData.origin = points[2];
        break;
    default:
        previousMarkerData.inslopePoints[0] = previousMarkerData.origin;
        previousMarkerData.inslopePoints[1] = points[0];
        previousMarkerData.origin = points[0];
        break;
    }
}

void DrawStartAndMidMarkers(void *info, const CGPathElement *element)
{
    DrawMarkersData &data = *(DrawMarkersData *)info;

    int elementIndex = data.elementIndex;
    MarkerData &previousMarkerData = data.previousMarkerData;

    CGPoint *points = element->points;

    // First update the outslope for the previous element
    previousMarkerData.outslopePoints[0] = previousMarkerData.origin;
    previousMarkerData.outslopePoints[1] = points[0];

    // Draw the marker for the previous element
    if (elementIndex != 0)
        drawMarkerWithData(previousMarkerData);

    // Update our marker data for this element
    updateMarkerDataForElement(previousMarkerData, element);

    if (elementIndex == 1) {
        // After drawing the start marker, switch to drawing mid markers
        previousMarkerData.marker = data.midMarker;
        previousMarkerData.type = Mid;
    }

    data.elementIndex++;
}

void KCanvasItemQuartz::drawMarkersIfNeeded(const FloatRect& rect, const KCanvasPath *path) const
{
    KDOM::DocumentImpl *doc = document();
    const KSVG::SVGRenderStyle *svgStyle = style()->svgStyle();

    KCanvasMarker *startMarker = getMarkerById(doc, svgStyle->startMarker().mid(1));
    KCanvasMarker *midMarker = getMarkerById(doc, svgStyle->midMarker().mid(1));
    KCanvasMarker *endMarker = getMarkerById(doc, svgStyle->endMarker().mid(1));
    
    if (!startMarker && !midMarker && !endMarker)
        return;

    double strokeWidth = KSVG::KSVGPainterFactory::cssPrimitiveToLength(this, style()->svgStyle()->strokeWidth(), 1.0);

    DrawMarkersData data(startMarker, midMarker, strokeWidth);

    CGPathRef cgPath = static_cast<const KCanvasPathQuartz*>(path)->cgPath();
    CGPathApply(cgPath, &data, DrawStartAndMidMarkers);

    data.previousMarkerData.marker = endMarker;
    data.previousMarkerData.type = End;
    drawMarkerWithData(data.previousMarkerData);
}

#endif // SVG_SUPPORT
