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
#include "MediaPlayer.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderVideo::RenderVideo(HTMLMediaElement* video)
    : RenderMedia(video, video->player() ? video->player()->naturalSize() : IntSize(300, 150))
{
}

RenderVideo::~RenderVideo()
{
    if (MediaPlayer* p = player()) {
        p->setVisible(false);
        p->setParentWidget(0);
    }
}
    
void RenderVideo::videoSizeChanged()
{
    if (!player())
        return;
    IntSize size = player()->naturalSize();
    if (size != intrinsicSize()) {
        setIntrinsicSize(size);
        setPrefWidthsDirty(true);
        setNeedsLayout(true);
    }
}

IntRect RenderVideo::videoBox() const 
{
    IntRect contentRect = contentBox();
    
    if (intrinsicSize().isEmpty() || contentRect.isEmpty())
        return IntRect();

    IntRect resultRect = contentRect;
    int ratio = contentRect.width() * intrinsicSize().height() - contentRect.height() * intrinsicSize().width();
    if (ratio > 0) {
        int newWidth = contentRect.height() * intrinsicSize().width() / intrinsicSize().height();
        // Just fill the whole area if the difference is one pixel or less (in both sides)
        if (resultRect.width() - newWidth > 2)
            resultRect.setWidth(newWidth);
        resultRect.move((contentRect.width() - resultRect.width()) / 2, 0);
    } else if (ratio < 0) {
        int newHeight = contentRect.width() * intrinsicSize().height() / intrinsicSize().width();
        if (resultRect.height() - newHeight > 2)
            resultRect.setHeight(newHeight);
        resultRect.move(0, (contentRect.height() - resultRect.height()) / 2);
    }
    return resultRect;
}
    
void RenderVideo::paintReplaced(PaintInfo& paintInfo, int tx, int ty)
{
    MediaPlayer* mediaPlayer = player();
    if (!mediaPlayer)
        return;
    updatePlayer();
    IntRect rect = videoBox();
    if (rect.isEmpty())
        return;
    rect.move(tx, ty);
    mediaPlayer->paint(paintInfo.context, rect);
}

void RenderVideo::layout()
{
    RenderMedia::layout();
    updatePlayer();
}
    
void RenderVideo::updateFromElement()
{
    RenderMedia::updateFromElement();
    updatePlayer();
}

void RenderVideo::updatePlayer()
{
    MediaPlayer* mediaPlayer = player();
    if (!mediaPlayer)
        return;
    Document* doc = document();
    if (doc->inPageCache())
        return;
    int x;
    int y;
    absolutePosition(x, y);
    IntRect videoBounds = videoBox(); 
    videoBounds.move(x, y);
    mediaPlayer->setParentWidget(doc->view());
    mediaPlayer->setRect(videoBounds);
    mediaPlayer->setVisible(true);
}

bool RenderVideo::isWidthSpecified() const
{
    switch (style()->width().type()) {
        case Fixed:
        case Percent:
            return true;
        case Auto:
        case Relative: // FIXME: Shouldn't this case return true? It doesn't for images.
        case Static:
        case Intrinsic:
        case MinIntrinsic:
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
        case Auto:
        case Relative: // FIXME: Shouldn't this case return true? It doesn't for images.
        case Static:
        case Intrinsic:
        case MinIntrinsic:
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
