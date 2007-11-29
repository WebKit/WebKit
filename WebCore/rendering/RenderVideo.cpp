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
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "Movie.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderVideo::RenderVideo(HTMLMediaElement* video)
    : RenderMedia(video, video->movie() ? video->movie()->naturalSize() : IntSize(300, 150))
{
}

RenderVideo::~RenderVideo()
{
    if (Movie* m = movie()) {
        m->setVisible(false);
        m->setParentWidget(0);
    }
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

void RenderVideo::paintObject(PaintInfo& paintInfo, int tx, int ty)
{
    if (style()->visibility() != VISIBLE)
        return;

    if (paintInfo.phase == PaintPhaseForeground && movie() && !document()->printing()) {
        updateMovie();
            
        IntRect rect = contentBox();
        rect.move(tx, ty);
        movie()->paint(paintInfo.context, rect);
    }
    
    // Paint the children.
    RenderBlock::paintObject(paintInfo, tx, ty);
}

void RenderVideo::layout()
{
    RenderBlock::layout();
    updateMovie();
}
    
void RenderVideo::updateFromElement()
{
    RenderMedia::updateFromElement();
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
        m->setVisible(true);
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
    return RenderBox::calcReplacedHeight() * intrinsicWidth / intrinsicHeight;
}

int RenderVideo::calcAspectRatioHeight() const
{
    int intrinsicWidth = intrinsicSize().width();
    int intrinsicHeight = intrinsicSize().height();
    if (!intrinsicWidth)
        return 0;
    return RenderBox::calcReplacedWidth() * intrinsicHeight / intrinsicWidth;
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
