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
        
        switch (data.cmd) {
        case CMD_MOVE: 
            CGPathMoveToPoint(path, NULL, data.x3, data.y3);
            break;
        case CMD_LINE:
            CGPathAddLineToPoint(path, NULL, data.x3, data.y3);
            break;
        case CMD_CURVE:
            CGPathAddCurveToPoint(path, NULL, data.x1, data.y1, data.x2, data.y2, data.x3, data.y3);
            break;
        case CMD_CLOSE_SUBPATH:
            CGPathCloseSubpath(path);
            break;
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
        
    RenderContainer::layout();
}

void KCanvasContainerQuartz::paint(PaintInfo &paintInfo, int parentX, int parentY)
{
    if (paintInfo.p->paintingDisabled())
        return;
    
    // No one should be transforming us via these.
    ASSERT(m_x == 0);
    ASSERT(m_y == 0);
        
    if (shouldPaintBackgroundOrBorder() && paintInfo.phase != PaintActionOutline) 
        paintBoxDecorations(paintInfo, parentX, parentY);
    
    if (paintInfo.phase == PaintActionOutline && style()->outlineWidth() && style()->visibility() == khtml::VISIBLE)
        paintOutline(paintInfo.p, parentX, parentY, width(), height(), style());
    
    if (paintInfo.phase != PaintActionForeground || !drawsContents() || style()->visibility() == khtml::HIDDEN)
        return;
    
    KCanvasFilter *filter = getFilterById(document(), style()->svgStyle()->filter().mid(1));
    if (!firstChild() && !filter)
        return; // Spec: groups w/o children still may render filter content.
    
    KRenderingDeviceQuartz *quartzDevice = static_cast<KRenderingDeviceQuartz *>(QPainter::renderingDevice());
    KRenderingDeviceContext *deviceContext = 0;
    if (!parent()->isKCanvasContainer()) {
        // I only need to setup for KCanvas rendering if it hasn't already been done.
        deviceContext = paintInfo.p->createRenderingDeviceContext();
        quartzDevice->pushContext(deviceContext);
    }
    paintInfo.p->save();
    
    CGContextRef context = paintInfo.p->currentContext();
    
    if (parentX != 0 || parentY != 0) {
        // Translate from parent offsets (khtml) to a relative transform (ksvg2/kcanvas)
        CGContextConcatCTM(context, CGAffineTransformMakeTranslation(parentX, parentY));
        parentX = parentY = 0;
    }
    
    if (viewport().isValid())
        CGContextConcatCTM(context, CGAffineTransformMakeTranslation(viewport().x(), viewport().y()));
    
    if (!localTransform().isIdentity())
        CGContextConcatCTM(context, CGAffineTransform(localTransform()));
    
    QRect dirtyRect = paintInfo.r;
    
    QString clipname = style()->svgStyle()->clipPath().mid(1);
    KCanvasClipperQuartz *clipper = static_cast<KCanvasClipperQuartz *>(getClipperById(document(), clipname));
    if (clipper)
        clipper->applyClip(context, CGRect(relativeBBox(true)));
    
    float opacity = style()->opacity();
    if (opacity < 1.0f)
        paintInfo.p->beginTransparencyLayer(opacity);

    if (filter)
        filter->prepareFilter(quartzDevice, relativeBBox(true));
    
    if (!viewBox().isNull()) {
        QRect viewportRect = viewport();
        if (!parent()->isKCanvasContainer())
            viewportRect = QRect(viewport().x(), viewport().y(), width(), height());
        CGContextConcatCTM(paintInfo.p->currentContext(), CGAffineTransform(getAspectRatio(viewBox(), viewportRect).qmatrix()));
    }
    
    RenderContainer::paint(paintInfo, 0, 0);
    
    if (filter)
        filter->applyFilter(quartzDevice, relativeBBox(true));
    
    if (opacity < 1.0f)
        paintInfo.p->endTransparencyLayer();
    
    // restore drawing state
    paintInfo.p->restore();
    if (!parent()->isKCanvasContainer()) {
        quartzDevice->popContext();
        delete deviceContext;
    }
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

QMatrix KCanvasContainerQuartz::absoluteTransform() const
{
    QMatrix transform = KCanvasContainer::absoluteTransform();
    if (!viewBox().isNull()) {
        QRect viewportRect = viewport();
        if (!parent()->isKCanvasContainer())
            viewportRect = QRect(viewport().x(), viewport().y(), width(), height());
        transform *= getAspectRatio(viewBox(), viewportRect).qmatrix();
    }
    return transform;
}

void KCanvasClipperQuartz::applyClip(CGContextRef context, CGRect relativeBBox) const
{
    // FIXME: until the path representation is fixed in
    // KCanvas, we have to convert a KCPathDataList to a CGPath

    if (m_clipData.count() < 1) {
        NSLog(@"WARNING: Applying empty clipper, ignoring.");
        return;
    }

    BOOL heterogenousClipRules = NO;
    KCWindRule clipRule = m_clipData[0].rule;

    CGContextBeginPath(context);

    CGAffineTransform bboxTransform = CGAffineTransformMakeMapBetweenRects(CGRectMake(0,0,1,1), relativeBBox); // could be done lazily.

    for (unsigned int x = 0; x < m_clipData.count(); x++) {
        KCClipData data = m_clipData[x];
        if (data.rule != clipRule)
            heterogenousClipRules = YES;

        CGPathRef clipPath = CGPathFromKCPathDataList(data.path);
        
        if (CGPathIsEmpty(clipPath)) // FIXME: occasionally we get empty clip paths...
            NSLog(@"WARNING: Asked to clip an empty path, ignoring.");

        if (data.bbox) {
            CGPathRef transformedPath = CGPathApplyTransform(clipPath, bboxTransform);
            CGPathRelease(clipPath);
            clipPath = transformedPath;
        }
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
                
        if (!CGContextIsPathEmpty(context)) {
            if (m_clipData[0].rule == RULE_EVENODD)
                CGContextEOClip(context);
            else
                CGContextClip(context);
        } else
            NSLog(@"ERROR: Final clip path empty, ignoring.");
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

