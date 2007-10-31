/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#if ENABLE(VIDEO)
#include "RenderVideo.h"

#include "Document.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "Movie.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderVideo::RenderVideo(HTMLMediaElement* video)
    : RenderReplaced(video)
{
    setIntrinsicSize(IntSize(0, 0));
}

RenderVideo::~RenderVideo()
{
    if (Movie* m = movie()) {
        m->setVisible(false);
        m->setParentWidget(0);
    }
}

Movie* RenderVideo::movie() const
{
    return static_cast<HTMLMediaElement*>(element())->movie();
}

void RenderVideo::videoSizeChanged()
{
    if (!movie())
        return;
    IntSize size = movie()->naturalSize();
    if (size != intrinsicSize()) {
        setIntrinsicSize(size);
        
        int oldWidth = width();
        int oldHeight = height();
        
        calcWidth();
        calcHeight();
        
        if (oldWidth != m_width || oldHeight != m_height) {
            setPrefWidthsDirty(true);
            setNeedsLayout(true);
        }
    }
}

void RenderVideo::paint(PaintInfo& paintInfo, int tx, int ty)
{
    if (!shouldPaint(paintInfo, tx, ty))
        return;

    tx += m_x;
    ty += m_y;
        
    if (hasBoxDecorations() && paintInfo.phase != PaintPhaseOutline && paintInfo.phase != PaintPhaseSelfOutline) 
        paintBoxDecorations(paintInfo, tx, ty);

    GraphicsContext* context = paintInfo.context;

    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(context, tx, ty, width(), height(), style());

    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection)
        return;

    if (!shouldPaintWithinRoot(paintInfo))
        return;
        
    bool isPrinting = document()->printing();
    if (isPrinting)
        return;
    
    if (!movie())
        return;
           
    updateMovie();
        
    int cWidth = contentWidth();
    int cHeight = contentHeight();
    int leftBorder = borderLeft();
    int topBorder = borderTop();
    int leftPad = paddingLeft();
    int topPad = paddingTop();

    IntRect rect(IntPoint(tx + leftBorder + leftPad, ty + topBorder + topPad), IntSize(cWidth, cHeight));

    movie()->paint(context, rect);
}

void RenderVideo::layout()
{
    ASSERT(needsLayout());

    IntRect oldBounds;
    IntRect oldOutlineBox;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint) {
        oldBounds = absoluteClippedOverflowRect();
        oldOutlineBox = absoluteOutlineBox();
    }

    calcWidth();
    calcHeight();
    
    updateMovie();
        
    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldOutlineBox);
    
    setNeedsLayout(false);
}

void RenderVideo::updateFromElement()
{
    updateMovie();
}

void RenderVideo::updateMovie()
{
    int x;
    int y;
    absolutePosition(x, y);
    x += borderLeft() + paddingLeft();
    y += borderTop() + paddingTop();
    
    int width = m_width - borderLeft() - borderRight() - paddingLeft() - paddingRight();
    int height = m_height - borderTop() - borderBottom() - paddingTop() - paddingBottom();
    
    IntRect newBounds(x, y, width, height);
    
    if (Movie* m = movie()) {
        m->setParentWidget(document()->view());
        m->setRect(newBounds);    
    }    
}

bool RenderVideo::isWidthSpecified() const
{
    switch (style()->width().type()) {
        case Fixed:
        case Percent:
            return true;
        default:
            return false;
    }
    ASSERT(false);
    return false;
}

bool RenderVideo::isHeightSpecified() const
{
    switch (style()->height().type()) {
        case Fixed:
        case Percent:
            return true;
        default:
            return false;
    }
    ASSERT(false);
    return false;
}

int RenderVideo::calcReplacedWidth() const
{
    int width;
    if (isWidthSpecified())
        width = calcReplacedWidthUsing(style()->width());
    else
        width = calcAspectRatioWidth();

    int minW = calcReplacedWidthUsing(style()->minWidth());
    int maxW = style()->maxWidth().isUndefined() ? width : calcReplacedWidthUsing(style()->maxWidth());

    return max(minW, min(width, maxW));
}

int RenderVideo::calcReplacedHeight() const
{
    int height;
    if (isHeightSpecified())
        height = calcReplacedHeightUsing(style()->height());
    else
        height = calcAspectRatioHeight();

    int minH = calcReplacedHeightUsing(style()->minHeight());
    int maxH = style()->maxHeight().isUndefined() ? height : calcReplacedHeightUsing(style()->maxHeight());

    return max(minH, min(height, maxH));
}

int RenderVideo::calcAspectRatioWidth() const
{
    int intrinsicWidth = intrinsicSize().width();
    int intrinsicHeight = intrinsicSize().height();
    if (!intrinsicHeight)
        return 0;
    //if (!m_cachedImage || m_cachedImage->errorOccurred())
   //     return intrinsicWidth(); // Don't bother scaling.
    return RenderReplaced::calcReplacedHeight() * intrinsicWidth / intrinsicHeight;
}

int RenderVideo::calcAspectRatioHeight() const
{
    int intrinsicWidth = intrinsicSize().width();
    int intrinsicHeight = intrinsicSize().height();
    if (!intrinsicWidth)
        return 0;
    //if (!m_cachedImage || m_cachedImage->errorOccurred())
    //    return intrinsicHeight(); // Don't bother scaling.
    return RenderReplaced::calcReplacedWidth() * intrinsicHeight / intrinsicWidth;
}

void RenderVideo::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    m_maxPrefWidth = calcReplacedWidth() + paddingLeft() + paddingRight() + borderLeft() + borderRight();

    if (style()->width().isPercent() || style()->height().isPercent() || 
        style()->maxWidth().isPercent() || style()->maxHeight().isPercent() ||
        style()->minWidth().isPercent() || style()->minHeight().isPercent())
        m_minPrefWidth = 0;
    else
        m_minPrefWidth = m_maxPrefWidth;

    setPrefWidthsDirty(false);
}

} // namespace WebCore

#endif
