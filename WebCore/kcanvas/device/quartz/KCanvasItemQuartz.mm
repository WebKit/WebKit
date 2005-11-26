/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#import "KCanvasItemQuartz.h"

#import <kxmlcore/Assertions.h>

#import "kcanvas/KCanvas.h"
#import "KCanvasRenderingStyle.h"
#import "KRenderingFillPainter.h"
#import "KRenderingStrokePainter.h"
#import "KCanvasMatrix.h"

#import "KRenderingDeviceQuartz.h"
#import "KCanvasFilterQuartz.h"
#import "KCanvasResourcesQuartz.h"
#import "QuartzSupport.h"

#import "SVGRenderStyle.h"


KCanvasItemQuartz::KCanvasItemQuartz(khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node) : RenderPath(style, node)
{
	
}

void KCanvasItemQuartz::drawMarkersIfNeeded(const QRect &rect) const
{
    KDOM::DocumentImpl *doc = document();
    KSVG::SVGRenderStyle *svgStyle = style()->svgStyle();

    // find the start verticies...
    if(KCanvasMarker *marker = getMarkerById(doc, svgStyle->startMarker().mid(1)))
        marker->draw(rect, localTransform(), 0, 0, 0);

    // find the middle verticies... 
    if(KCanvasMarker *marker = getMarkerById(doc, svgStyle->midMarker().mid(1)))
        marker->draw(rect, localTransform(), 0, 0, 0);
    
    // find the end verticies...
    if(KCanvasMarker *marker = getMarkerById(doc, svgStyle->endMarker().mid(1)))
        marker->draw(rect, localTransform(), 0, 0, 0);
}

void KCanvasItemQuartz::paint(PaintInfo &paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled() || (paintInfo.phase != PaintActionForeground) || style()->visibility() == khtml::HIDDEN)
        return;
    
    KRenderingDeviceQuartz *quartzDevice = static_cast<KRenderingDeviceQuartz *>(canvas()->renderingDevice());
    quartzDevice->pushContext(paintInfo.p->createRenderingDeviceContext());
    paintInfo.p->save();
    CGContextRef context = quartzDevice->currentCGContext();

    QRect dirtyRect = paintInfo.r;
    
    RenderPath::setupForDraw();

    CGMutablePathRef cgPath = static_cast<KCanvasQuartzPathData *>(path())->path;
    ASSERT(cgPath != 0);

    CGAffineTransform transform = CGAffineTransform(localTransform());
    CGContextConcatCTM(context, transform);

    // setup to apply filters
    KCanvasFilter *filter = getFilterById(document(), style()->svgStyle()->filter().mid(1));
    if (filter) {
        filter->prepareFilter(quartzDevice, relativeBBox(true));
        context = quartzDevice->currentCGContext();
    }

    QString clipname = style()->svgStyle()->clipPath().mid(1);
    KCanvasClipperQuartz *clipper = static_cast<KCanvasClipperQuartz *>(getClipperById(document(), clipname));
    if (clipper)
        clipper->applyClip(context, CGRect(relativeBBox(true)));

    CGContextBeginPath(context);

    KCanvasCommonArgs args = commonArgs();

    // Fill and stroke as needed.
    if(canvasStyle()->isFilled()) {
        CGContextAddPath(context, cgPath);
        canvasStyle()->fillPainter()->draw(quartzDevice->currentContext(), args);
    }
    if(canvasStyle()->isStroked()) {
        CGContextAddPath(context, cgPath); // path is cleared when filled.
        canvasStyle()->strokePainter()->draw(quartzDevice->currentContext(), args);
    }

    drawMarkersIfNeeded(dirtyRect);

    // actually apply the filter
    if (filter) {
        filter->applyFilter(quartzDevice, relativeBBox(true));
        context = quartzDevice->currentCGContext();
    }
    
    // restore drawing state
    paintInfo.p->restore();
    quartzDevice->popContext();
}

#pragma mark -
#pragma mark Hit Testing, BBoxes

bool KCanvasItemQuartz::nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty,
                            HitTestAction hitTestAction)
{
    // We only draw in the forground phase, so we only hit-test then.
    if (hitTestAction != HitTestForeground)
        return false;

    if (hitsPath(QPoint(_x, _y), true)) {
        setInnerNode(info);
        return true;
    }
    return false;
}

CGContextRef getSharedContext()
{
    static CGContextRef sharedContext = NULL;
    if (!sharedContext) {
        CFMutableDataRef empty = CFDataCreateMutable(NULL, 0);
        CGDataConsumerRef consumer = CGDataConsumerCreateWithCFData(empty);
        sharedContext = CGPDFContextCreate(consumer, NULL, NULL);
        CGDataConsumerRelease(consumer);
        CFRelease(empty);

        float black[4] = {0,0,0,1};
        CGContextSetFillColor(sharedContext,black);
        CGContextSetStrokeColor(sharedContext,black);
    }
    return sharedContext;
}


QRect KCanvasItemQuartz::bboxForPath(bool includeStroke) const
{
    KCanvasQuartzPathData *pathData = static_cast<KCanvasQuartzPathData *>(path());
    CGMutablePathRef cgPath = pathData->path;
    ASSERT(cgPath != 0);
    CGRect bbox;

    // the bbox might grow if the path is stroked.
    // and CGPathGetBoundingBox doesn't support that, so we'll have
    // to make an alternative call...
    if(includeStroke && canvasStyle()->isStroked()) {
        CGContextRef sharedContext = getSharedContext();
        
        CGContextSaveGState(sharedContext);
        CGContextBeginPath(sharedContext);
        CGContextAddPath(sharedContext, cgPath);
        applyStrokeStyleToContext(sharedContext, canvasStyle());
        CGContextReplacePathWithStrokedPath(sharedContext);
        
        bbox = CGContextGetPathBoundingBox(sharedContext);
        
        CGContextRestoreGState(sharedContext);
    } else
        // the easy (and efficient) case:
        bbox = CGPathGetBoundingBox(cgPath);
    
    return QRect(bbox);
}

bool KCanvasItemQuartz::hitsPath(const QPoint &hitPoint, bool fill) const
{
    CGMutablePathRef cgPath = static_cast<KCanvasQuartzPathData *>(path())->path;
    ASSERT(cgPath != 0);

    bool hitSuccess = false;
    CGContextRef sharedContext = getSharedContext();
    CGContextSaveGState(sharedContext);

    CGContextBeginPath(sharedContext);
    CGContextAddPath(sharedContext, cgPath);

    CGAffineTransform transform = CGAffineTransform(absoluteTransform());
    /* we transform the hit point locally, instead of translating the shape to the hit point. */
    CGPoint localHitPoint = CGPointApplyAffineTransform(CGPoint(hitPoint), CGAffineTransformInvert(transform));

    if (fill && canvasStyle()->fillPainter()->paintServer()) {
        CGPathDrawingMode drawMode = (canvasStyle()->fillPainter()->fillRule() == RULE_EVENODD) ? kCGPathEOFill : kCGPathFill;
        hitSuccess = CGContextPathContainsPoint(sharedContext, localHitPoint, drawMode);
    } else if (!fill && canvasStyle()->strokePainter()->paintServer())
        hitSuccess = CGContextPathContainsPoint(sharedContext, localHitPoint, kCGPathStroke);

    CGContextRestoreGState(sharedContext);

    return hitSuccess;
}
