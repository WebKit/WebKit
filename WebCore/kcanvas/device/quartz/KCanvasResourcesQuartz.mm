/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *               2005 Alexander Kellett <lypanov@kde.org>
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

#import "KCanvasPathQuartz.h"
#import "KRenderingDeviceQuartz.h"
#import "KCanvasMaskerQuartz.h"
#import "KCanvasFilterQuartz.h"
#import "QuartzSupport.h"

#import <kxmlcore/Assertions.h>

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

    IntRect oldBounds;
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
    //ASSERT(m_x == 0);
    //ASSERT(m_y == 0);
        
    if (shouldPaintBackgroundOrBorder() && paintInfo.phase != WebCore::PaintActionOutline) 
        paintBoxDecorations(paintInfo, parentX, parentY);
    
    if (paintInfo.phase == WebCore::PaintActionOutline && style()->outlineWidth() && style()->visibility() == khtml::VISIBLE)
        paintOutline(paintInfo.p, parentX, parentY, width(), height(), style());
    
    if (paintInfo.phase != WebCore::PaintActionForeground || !drawsContents() || style()->visibility() == khtml::HIDDEN)
        return;
    
    KCanvasFilter *filter = getFilterById(document(), style()->svgStyle()->filter().mid(1));
    if (!firstChild() && !filter)
        return; // Spec: groups w/o children still may render filter content.
    
    KRenderingDeviceQuartz *quartzDevice = static_cast<KRenderingDeviceQuartz *>(QPainter::renderingDevice());
    KRenderingDeviceContext *deviceContext = quartzDevice->currentContext();
    bool shouldPopContext = false;
    if (!deviceContext) {
        // I only need to setup for KCanvas rendering if it hasn't already been done.
        deviceContext = paintInfo.p->createRenderingDeviceContext();
        quartzDevice->pushContext(deviceContext);
        shouldPopContext = true;
    } else
        paintInfo.p->save();
    
    if (parentX != 0 || parentY != 0) {
        // Translate from parent offsets (khtml) to a relative transform (ksvg2/kcanvas)
        deviceContext->concatCTM(QMatrix().translate(parentX, parentY));
        parentX = parentY = 0;
    }
    
    if (viewport().isValid())
        deviceContext->concatCTM(QMatrix().translate(viewport().x(), viewport().y()));
    
    if (!localTransform().isIdentity())
        deviceContext->concatCTM(localTransform());
    
    if (KCanvasClipper *clipper = getClipperById(document(), style()->svgStyle()->clipPath().mid(1)))
        clipper->applyClip(relativeBBox(true));

    if (KCanvasMasker *masker = getMaskerById(document(), style()->svgStyle()->maskElement().mid(1)))
        masker->applyMask(relativeBBox(true));

    float opacity = style()->opacity();
    if (opacity < 1.0f)
        paintInfo.p->beginTransparencyLayer(opacity);

    if (filter)
        filter->prepareFilter(relativeBBox(true));
    
    if (!viewBox().isNull()) {
        FloatRect viewportRect = viewport();
        if (!parent()->isKCanvasContainer())
            viewportRect = FloatRect(viewport().x(), viewport().y(), width(), height());
        deviceContext->concatCTM(getAspectRatio(viewBox(), viewportRect));
    }
    
    RenderContainer::paint(paintInfo, 0, 0);
    
    if (filter)
        filter->applyFilter(relativeBBox(true));
    
    if (opacity < 1.0f)
        paintInfo.p->endTransparencyLayer();
    
    // restore drawing state
    if (shouldPopContext) {
        quartzDevice->popContext();
        delete deviceContext;
    } else
        paintInfo.p->restore();
}

void KCanvasContainerQuartz::setViewport(const FloatRect& viewport)
{
    m_viewport = viewport;
}

FloatRect KCanvasContainerQuartz::viewport() const
{
   return m_viewport;
}

void KCanvasContainerQuartz::setViewBox(const FloatRect& viewBox)
{
    m_viewBox = viewBox;
}

FloatRect KCanvasContainerQuartz::viewBox() const
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
        FloatRect viewportRect = viewport();
        if (!parent()->isKCanvasContainer())
            viewportRect = FloatRect(viewport().x(), viewport().y(), width(), height());
        transform *= getAspectRatio(viewBox(), viewportRect).qmatrix();
    }
    return transform;
}

void KCanvasClipperQuartz::applyClip(const FloatRect& boundingBox) const
{
    KRenderingDeviceContext *context = QPainter::renderingDevice()->currentContext();
    CGContextRef cgContext = static_cast<KRenderingDeviceContextQuartz*>(context)->cgContext();
    if (m_clipData.count() < 1)
        return;

    BOOL heterogenousClipRules = NO;
    KCWindRule clipRule = m_clipData[0].windRule;

    context->clearPath();

    CGAffineTransform bboxTransform = CGAffineTransformMakeMapBetweenRects(CGRectMake(0,0,1,1), CGRect(boundingBox));

    for (unsigned int x = 0; x < m_clipData.count(); x++) {
        KCClipData data = m_clipData[x];
        if (data.windRule != clipRule)
            heterogenousClipRules = YES;
        
        KCanvasPathQuartz *path = static_cast<KCanvasPathQuartz*>(data.path.get());        
        CGPathRef clipPath = static_cast<KCanvasPathQuartz*>(path)->cgPath();

        if (data.bboxUnits) {
            CGMutablePathRef transformedPath = CGPathCreateMutable();
            CGPathAddPath(transformedPath, &bboxTransform, clipPath);
            CGContextAddPath(cgContext, transformedPath);
            CGPathRelease(transformedPath);
        } else
            CGContextAddPath(cgContext, clipPath);
    }

    if (m_clipData.count()) {
        // FIXME!
        // We don't currently allow for heterogenous clip rules.
        // we would have to detect such, draw to a mask, and then clip
        // to that mask                
        if (!CGContextIsPathEmpty(cgContext)) {
            if (clipRule == RULE_EVENODD)
                CGContextEOClip(cgContext);
            else
                CGContextClip(cgContext);
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
