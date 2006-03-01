/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

//#define DEBUG_LAYOUT

#include "config.h"
#include "render_canvasimage.h"

#if __APPLE__

#include "DocumentImpl.h"
#include "GraphicsContext.h"
#include "html_canvasimpl.h"
#include "htmlnames.h"

namespace WebCore {

using namespace HTMLNames;

RenderCanvasImage::RenderCanvasImage(NodeImpl *_node)
    : RenderImage(_node), _drawingContext(0), _drawingContextData(0), _drawnImage(0), _needsImageUpdate(0)
{
}

RenderCanvasImage::~RenderCanvasImage()
{
    if (_drawingContext) {
        CFRelease (_drawingContext);
        _drawingContext = 0;
    }
    
    fastFree(_drawingContextData);
    _drawingContextData = 0;
    
    if (_drawnImage) {
        CFRelease (_drawnImage);
        _drawnImage = 0;
    }
}

#define BITS_PER_COMPONENT 8
#define BYTES_PER_ROW(width,bitsPerComponent,numComponents) ((width * bitsPerComponent * numComponents + 7)/8)

void RenderCanvasImage::createDrawingContext()
{
    if (_drawingContext) {
        CFRelease (_drawingContext);
        _drawingContext = 0;
    }
    fastFree(_drawingContextData);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    int cWidth = contentWidth();
    int cHeight = contentHeight();
    size_t numComponents = CGColorSpaceGetNumberOfComponents(colorSpace);
    size_t bytesPerRow = BYTES_PER_ROW(cWidth,BITS_PER_COMPONENT,(numComponents+1)); // + 1 for alpha
    _drawingContextData = fastCalloc(height(), bytesPerRow);
    _drawingContext = CGBitmapContextCreate(_drawingContextData, cWidth, cHeight, BITS_PER_COMPONENT, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast);
    
#ifdef DEBUG_CANVAS_BACKGROUND
    CGContextSetRGBFillColor(_drawingContext, 1.0, 0., 0., 1.);
    CGContextFillRect (_drawingContext, CGRectMake (0, 0, width(), height()));
    CGContextFlush (_drawingContext);
#endif
    
    updateDrawnImage();
    
    CFRelease (colorSpace);
}

CGContextRef RenderCanvasImage::drawingContext()
{
    if (!_drawingContext) {
        document()->updateLayout();
        createDrawingContext();
    }
    
    return _drawingContext;
}

void RenderCanvasImage::setNeedsImageUpdate()
{
    _needsImageUpdate = true;
    repaint();
}


void RenderCanvasImage::updateDrawnImage()
{
    if (_drawnImage)
        CFRelease (_drawnImage);
    CGContextFlush (_drawingContext);
    _drawnImage = CGBitmapContextCreateImage (_drawingContext);
}

CGImageRef RenderCanvasImage::drawnImage()
{
    return _drawnImage;
}

void RenderCanvasImage::paint(PaintInfo& i, int _tx, int _ty)
{
    if (!shouldPaint(i, _tx, _ty))
        return;

    int x = _tx + m_x;
    int y = _ty + m_y;

    if (shouldPaintBackgroundOrBorder() && i.phase != PaintActionOutline) 
        paintBoxDecorations(i, x, y);

    GraphicsContext* p = i.p;
    if (p->paintingDisabled())
        return;
    
    if (i.phase == PaintActionOutline && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(p, x, y, width(), height(), style());
    
    if (i.phase != PaintActionForeground && i.phase != PaintActionSelection)
        return;

    if (!shouldPaintWithinRoot(i))
        return;

    bool isPrinting = i.p->printing();
    bool drawSelectionTint = (selectionState() != SelectionNone) && !isPrinting;
    if (i.phase == PaintActionSelection) {
        if (selectionState() == SelectionNone) {
            return;
        }
        drawSelectionTint = false;
    }

    int cWidth = contentWidth();
    int cHeight = contentHeight();
    int leftBorder = borderLeft();
    int topBorder = borderTop();
    int leftPad = paddingLeft();
    int topPad = paddingTop();

    x += leftBorder + leftPad;
    y += topBorder + topPad;
    
    if (_needsImageUpdate) {
        updateDrawnImage();
        _needsImageUpdate = false;
    }
    
    if (drawnImage()) {
        HTMLCanvasElementImpl* i = (element() && element()->hasTagName(canvasTag)) ? static_cast<HTMLCanvasElementImpl*>(element()) : 0;
        int oldOperation = 0;
        if (i) {
            oldOperation = GraphicsContext::getCompositeOperation(GraphicsContext::currentCGContext());
            GraphicsContext::setCompositeOperation(GraphicsContext::currentCGContext(), i->compositeOperator());
        }
        CGContextDrawImage(GraphicsContext::currentCGContext(), CGRectMake(x, y, cWidth, cHeight), drawnImage());
        if (i)
            GraphicsContext::setCompositeOperation(GraphicsContext::currentCGContext(), oldOperation);
    }

    if (drawSelectionTint)
        p->fillRect(selectionRect(), selectionColor(p));
}

void RenderCanvasImage::layout()
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = getAbsoluteRepaintRect();

    int oldwidth = m_width;
    int oldheight = m_height;
    
    calcWidth();
    calcHeight();

    if ( m_width != oldwidth || m_height != oldheight ) {
        createDrawingContext();
    }

    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);
    
    setNeedsLayout(false);
}

}

#endif
