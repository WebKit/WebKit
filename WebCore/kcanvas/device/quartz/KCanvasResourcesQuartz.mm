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


#import "KCanvasResourcesQuartz.h"

#import "KRenderingDeviceQuartz.h"
#import "KCanvasFilterQuartz.h"

#import "KRenderingStyle.h" // for style() call
#import "KCanvas.h" // for registry()

#import "QuartzSupport.h"

#import "KWQAssertions.h"

CGPathRef CGPathFromKCPathDataList(KCPathDataList pathData)
{
	CGMutablePathRef path = CGPathCreateMutable();
	
	KCPathDataList::ConstIterator it = pathData.begin();
	KCPathDataList::ConstIterator end = pathData.end();
	for(; it != end; ++it)
	{
		KCPathData data = *it;

		if(data.cmd == CMD_MOVE) {
			CGPathMoveToPoint(path, NULL, data.x3, data.y3);
		} else if(data.cmd == CMD_LINE) {
			CGPathAddLineToPoint(path, NULL, data.x3, data.y3);
		} else if(data.cmd == CMD_CLOSE_SUBPATH) {
			CGPathCloseSubpath(path);
		} else {
			CGPathAddCurveToPoint(path, NULL, data.x1, data.y1, data.x2, data.y2, data.x3, data.y3);
		}
	}
	return path;
}


KCanvasContainerQuartz::KCanvasContainerQuartz(KCanvas *canvas, KRenderingStyle *style) : KCanvasContainer(canvas, style)
{
	//NSLog(@"KCanvasContainerQuartz::KCanvasContainerQuartz()");
}

void KCanvasContainerQuartz::draw(const QRect &dirtyRect) const
{
	KRenderingDeviceQuartz *quartzDevice = static_cast<KRenderingDeviceQuartz *>(canvas()->renderingDevice());
	CGContextRef context = quartzDevice->currentCGContext();
	ASSERT(context != NULL);
	
	CGContextSaveGState(context);
	
	// clip
//	NSLog(@"clipping to viewport rect: %@", NSStringFromRect(NSRect(dirtyRect)));
//	CGContextAddRect(context,CGRect(dirtyRect));
//	CGContextClip(context);
	applyClipPathsForStyle(context, canvas()->registry(), style(), bbox()); // apply any explicit clips
	
	// handle opacity.
	float opacity = style()->opacity();
	if (opacity < 1.0f) {
		CGContextSetAlpha(context, opacity);
		CGContextBeginTransparencyLayer(context,NULL);
	}

	// setup to apply filters
	KCanvasFilterQuartz *filter = static_cast<KCanvasFilterQuartz *>(style()->filter());
	if (filter) {
		filter->prepareFilter(quartzDevice->currentContext(), bbox());
	}
	
	// do the draw
	KCanvasContainer::draw(dirtyRect);
	
	// actually apply the filter
	if (filter) {
		filter->applyFilter(quartzDevice->currentContext(), commonArgs(), bbox());
	}
	
	if (opacity < 1.0f)
		CGContextEndTransparencyLayer(context);
	
	CGContextRestoreGState(context);
}

void KCanvasClipperQuartz::applyClip(CGContextRef context, const QRect &bbox) const
{
	//NSLog(@"applyClip to context %p with data: %p this: %i", context, &m_clipData, this);
	// FIXME: until the path representation is fixed in
	// KCanvas, we have to convert a KCPathDataList to a CGPath
	
	BOOL heterogenousClipRules = NO;
	KCWindRule clipRule = RULE_NONZERO;
	if (m_clipData.count()) clipRule = m_clipData[0].rule;
	
	CGContextBeginPath(context);
	
	CGAffineTransform bboxTransform = CGAffineTransformMakeMapBetweenRects(CGRectMake(0,0,1,1),CGRect(bbox)); // could be done lazily.
	
	for( unsigned int x = 0; x < m_clipData.count(); x++) {
		KCClipData data = m_clipData[x];
		if (data.rule != clipRule) heterogenousClipRules = YES;
	
		CGPathRef clipPath = CGPathFromKCPathDataList(data.path);
		
		if (CGPathIsEmpty(clipPath)) // FIXME: occasionally we get empty clip paths...
			NSLog(@"WARNING: Asked to clip an empty path, ignoring.");
		
		if (data.bbox) {
			CGPathRef transformedPath = CGPathApplyTransform(clipPath, bboxTransform);
			CGPathRelease(clipPath);
			clipPath = transformedPath;
		}
		
		if (data.viewportClipped) {
			NSLog(@"Viewport clip?");
		}
		
		//NSLog(@"Applying clip path: %@ bbox: %@ (%@)", CFStringFromCGPath(clipPath), data.bbox ? @"YES" : @"NO", NSStringFromRect(NSRect(bbox)));
		
		CGContextAddPath(context, clipPath);
		
		CGPathRelease(clipPath);
	}
	
	if (m_clipData.count()) {
		// FIXME!
		// We don't currently allow for heterogenous clip rules.
		// we would have to detect such, draw to a mask, and then clip
		// to that mask
		if (heterogenousClipRules)
			NSLog(@"WARNING: Quartz does not yet support heterogenous clip rules, clipping will be incorrect.");
			
		if (CGContextIsPathEmpty(context)) {
			NSLog(@"ERROR: Final clip path empty, ignoring.");
		} else {
			if (m_clipData[0].rule == RULE_EVENODD) {
				CGContextEOClip(context);
			} else {
				CGContextClip(context);
			}
		}
	}
}


KCanvasImageQuartz::~KCanvasImageQuartz()
{
	CGLayerRelease(m_cgLayer);
}

CGLayerRef KCanvasImageQuartz::cgLayer()
{
	return m_cgLayer;
}

void KCanvasImageQuartz::setCGLayer(CGLayerRef layer)
{
	if (m_cgLayer != layer) {
		CGLayerRelease(m_cgLayer);
		m_cgLayer = CGLayerRetain(layer);
	}
}

