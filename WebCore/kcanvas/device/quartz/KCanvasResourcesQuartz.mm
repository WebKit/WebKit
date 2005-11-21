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
#import "KCanvasResourcesQuartz.h"

#import "kcanvas/KCanvas.h"
#import "SVGRenderStyle.h"
#import "KCanvasMatrix.h"

#import "KRenderingDeviceQuartz.h"
#import "KCanvasFilterQuartz.h"
#import "QuartzSupport.h"

#import <kxmlcore/Assertions.h>

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

void KCanvasContainerQuartz::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown());
    m_minWidth = m_maxWidth = 0;
    setMinMaxKnown();
}

void KCanvasContainerQuartz::layout()
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());

    QRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = getAbsoluteRepaintRect();

    calcWidth();
    calcHeight();

    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
    
    setNeedsLayout(false);
}

void KCanvasContainerQuartz::paint(PaintInfo &paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled())
        return;
    
    int absoluteX = parentX + m_x;
    int absoluteY = parentY + m_y;
        
    if (shouldPaintBackgroundOrBorder() && paintInfo.phase != PaintActionOutline) 
        paintBoxDecorations(paintInfo, absoluteX, absoluteY);
    
    if (paintInfo.phase == PaintActionOutline && style()->outlineWidth() && style()->visibility() == khtml::VISIBLE)
        paintOutline(paintInfo.p, absoluteX, absoluteY, width(), height(), style());
    
    if (paintInfo.phase != PaintActionForeground)
        return;

    if (!firstChild())
        return;
    
    KRenderingDevice *renderingDevice = canvas()->renderingDevice();
    KRenderingDeviceContextQuartz *quartzContext = static_cast<KRenderingDeviceContextQuartz *>(paintInfo.p->renderingDeviceContext());
    renderingDevice->pushContext(quartzContext);
    paintInfo.p->save();
    
    CGContextRef context = paintInfo.p->currentContext();
    
    if(!localTransform().isIdentity())
        CGContextConcatCTM(context, CGAffineTransform(localTransform()));
    
    if(!viewBox().isNull())
        CGContextConcatCTM(context, CGAffineTransform(getAspectRatio(viewBox(), viewport()).qmatrix()));

    QRect dirtyRect = paintInfo.r;
    
    QString clipname = style()->svgStyle()->clipPath().mid(1);
    KCanvasClipperQuartz *clipper = static_cast<KCanvasClipperQuartz *>(getClipperById(document(), clipname));
    if (clipper)
        clipper->applyClip(context, bbox());
    
    float opacity = style()->opacity();
    if (opacity < 1.0f)
        paintInfo.p->beginTransparencyLayer(opacity);

    KCanvasFilter *filter = getFilterById(document(), style()->svgStyle()->filter().mid(1));
    if (filter)
        filter->prepareFilter(renderingDevice->currentContext(), bbox());
    
    RenderContainer::paint(paintInfo, parentX, parentY);
    
    if (filter)
        filter->applyFilter(renderingDevice->currentContext(), localTransform(), bbox()); // FIXME, I'm not sure if this should be "localTransform"
    
    if (opacity < 1.0f)
        paintInfo.p->endTransparencyLayer();
    
    // restore drawing state
    paintInfo.p->restore();
    renderingDevice->popContext();
}

void KCanvasContainerQuartz::setViewport(const QRect &viewport)
{
    m_viewport = viewport;
}

QRect KCanvasContainerQuartz::viewport() const
{
   return m_viewport;
}

void KCanvasContainerQuartz::setViewBox(const QRect &viewBox)
{
    m_viewBox = viewBox;
}

QRect KCanvasContainerQuartz::viewBox() const
{
    return m_viewBox;
}

void KCanvasContainerQuartz::setAlign(KCAlign align)
{
    m_align = align;
}

KCAlign KCanvasContainerQuartz::align() const
{
    return m_align;
}

void KCanvasClipperQuartz::applyClip(CGContextRef context, const QRect &bbox) const
{
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

