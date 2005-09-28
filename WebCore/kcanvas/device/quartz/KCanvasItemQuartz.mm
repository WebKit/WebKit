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


#import "KCanvasItemQuartz.h"

#import <kxmlcore/Assertions.h>

#import "KRenderingStyle.h"
#import "KCanvas.h"
#import "KRenderingFillPainter.h"
#import "KRenderingStrokePainter.h"
#import "KCanvasResources.h"
#import "KCanvasMatrix.h"

#import "KRenderingDeviceQuartz.h"
#import "KCanvasFilterQuartz.h"
#import "QuartzSupport.h"


KCanvasItemQuartz::KCanvasItemQuartz(KCanvas *canvas, KRenderingStyle *style, KCanvasUserData path) : KCanvasItem(canvas, style, path)
{
	
}

void KCanvasItemQuartz::drawMarkers() const
{
	NSLog(@"*** Marker support not implemented.");
	
	// find the start verticies...
	if(style()->startMarker()) {
		style()->startMarker()->draw(0, 0, 0);
	}

	// find the middle verticies... 
	if(style()->midMarker()) {
		style()->midMarker()->draw(0, 0, 0);
	}

	// find the end verticies...
	if(style()->endMarker()) {
		style()->endMarker()->draw(0, 0, 0);
	}
}


void KCanvasItemQuartz::draw(const QRect &rect) const
{
	KCanvasItem::draw(rect); // setup state.
	
	KRenderingDeviceQuartz *quartzDevice = static_cast<KRenderingDeviceQuartz *>(canvas()->renderingDevice());
	KRenderingDeviceContextQuartz *quartzContext = quartzDevice->quartzContext();
	CGMutablePathRef cgPath = static_cast<KCanvasQuartzPathData *>(path())->path;
	ASSERT(cgPath != 0);
	
	CGContextRef context = quartzDevice->currentCGContext();
	
	ASSERT(context != NULL);
		
	CGContextSaveGState(context);
	
	applyTransformForStyle(context, style());
	
	// setup to apply filters
	KCanvasFilterQuartz *filter = static_cast<KCanvasFilterQuartz *>(style()->filter());
	QRect bboxForFilter;
	if (filter) {
		// FIXME:: This should be fixed now that it has moved into KCanvasItem::draw()
		bboxForFilter = bboxPath(true, false); // FIXME: HACK! 30% of my time spent here!
		filter->prepareFilter(quartzContext, bboxForFilter);
		context = quartzContext->cgContext();
	}
	
    if (!style()->clipPaths().isEmpty()) 
        applyClipPathsForStyle(context, canvas()->registry(), style(), bboxPath(true)); // FIXME: need bbox when clipping.
	
	CGContextBeginPath(context);
	
	//CFStringRef string = CFStringFromCGPath(cgPath);
	//NSLog(@"renderPath context: %p path: %p (%@) style: %p bbox: %@", context, cgPath, string, style, NSStringFromRect(*(NSRect *)&CGContextGetPathBoundingBox(context)));
	//CFRelease(string);
	
	KCanvasCommonArgs args = commonArgs();
	
	// Fill and stroke as needed.
	if(style()->isFilled()) {
		CGContextAddPath(context, cgPath);
		style()->fillPainter()->draw(quartzContext, args);
	}
	if(style()->isStroked()) {
		CGContextAddPath(context, cgPath); // path is cleared when filled.
		style()->strokePainter()->draw(quartzContext, args);
	}

	// Draw markers, if needed
	if(style()->hasMarkers()) {
		drawMarkers();
	}
	
	// actually apply the filter
	if (filter) {
		filter->applyFilter(quartzContext, args, bboxForFilter);
		context = quartzContext->cgContext();
	}
	
	CGContextRestoreGState(context);
}



#pragma mark -
#pragma mark Hit Testing, BBoxes

CGContextRef getSharedContext() {
	static CGContextRef sharedContext = NULL;
	if (!sharedContext) {
		CFMutableDataRef empty = CFDataCreateMutable(NULL, 0);
		CGDataConsumerRef consumer = CGDataConsumerCreateWithCFData(empty);
		sharedContext = CGPDFContextCreate(consumer, NULL, NULL);
        CGDataConsumerRelease(consumer);
        CFRelease(empty);

	//	CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
	//	CGContextRef context = CGGLContextCreate(NULL, CGSizeMake(canvas->canvasWidth(), canvas->canvasHeight()), colorspace);
	//	CGColorSpaceRelease(colorspace);
		float black[4] = {0,0,0,1};
		CGContextSetFillColor(sharedContext,black);
		CGContextSetStrokeColor(sharedContext,black);
	}
	return sharedContext;
}


QRect KCanvasItemQuartz::bboxPath(bool includeStroke, bool applyTransforms) const
{
	KCanvasQuartzPathData *pathData = static_cast<KCanvasQuartzPathData *>(path());
	CGMutablePathRef cgPath = pathData->path;
	ASSERT(cgPath != 0);
	CGRect bbox;
	
	// the bbox might grow if the path is stroked.
	// and CGPathGetBoundingBox doesn't support that, so we'll have
	// to make an alternative call...
	if(includeStroke && style()->isStroked()) {
		CGContextRef sharedContext = getSharedContext();
		
		CGContextSaveGState(sharedContext);
		CGContextBeginPath(sharedContext);
		CGContextAddPath(sharedContext, cgPath);
		applyStrokeStyleToContext(sharedContext, style());
		CGContextReplacePathWithStrokedPath(sharedContext);
		
		bbox = CGContextGetPathBoundingBox(sharedContext);
		
		CGContextRestoreGState(sharedContext);
	} else {
		// the easy (and efficient) case:
		bbox = CGPathGetBoundingBox(cgPath);
	}
			
//	NSLog(@"Calculated bbox, rect: %@ for path: %@ CTM: %@",
//		NSStringFromRect(*(NSRect *)(&bbox)), CFStringFromCGPath(cgPath),
//		CFStringFromCGAffineTransform(CGContextGetCTM(context)));

	// apply any local transforms
	if (applyTransforms) {
		CGAffineTransform transform = CGAffineTransform(style()->objectMatrix().qmatrix());
		CGRect tranformedBBox = CGRectApplyAffineTransform(bbox, transform);
		return QRect(tranformedBBox);
	}
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
	
	CGAffineTransform transform = CGAffineTransform(style()->objectMatrix().qmatrix());
	/* we transform the hit point locally, instead of translating the shape to the hit point. */
	CGPoint localHitPoint = CGPointApplyAffineTransform(CGPoint(hitPoint), CGAffineTransformInvert(transform));
	
	if (fill && style()->fillPainter()->paintServer()) {
		CGPathDrawingMode drawMode = (style()->fillPainter()->fillRule() == RULE_EVENODD) ? kCGPathEOFill : kCGPathFill;
		hitSuccess = CGContextPathContainsPoint(sharedContext, localHitPoint, drawMode);
//		if (!hitSuccess)
//			NSLog(@"Point: %@ fails to hit path: %@ with bbox: %@",
//				NSStringFromPoint(NSPoint(hitPoint)), CFStringFromCGPath(cgPath),
//				NSStringFromRect(*(NSRect *)&CGContextGetPathBoundingBox(sharedContext)));
	} else if (!fill && style()->strokePainter()->paintServer()) {
		hitSuccess = CGContextPathContainsPoint(sharedContext, localHitPoint, kCGPathStroke);
	}
	
	CGContextRestoreGState(sharedContext);
	
	return hitSuccess;
}
