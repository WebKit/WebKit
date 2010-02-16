/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc.  All rights reserved.
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
#include "RenderView.h"

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

static const int cDefaultWidth = 300;
static const int cDefaultHeight = 150;

RenderVideo::RenderVideo(HTMLVideoElement* video)
    : RenderMedia(video)
{
    if (video->player())
        setIntrinsicSize(video->player()->naturalSize());
    else {
        // When the natural size of the video is unavailable, we use the provided
        // width and height attributes of the video element as the intrinsic size until
        // better values become available. If these attributes are not set, we fall back
        // to a default video size (300x150).
        if (video->hasAttribute(widthAttr) && video->hasAttribute(heightAttr))
            setIntrinsicSize(IntSize(video->width(), video->height()));
        else if (video->ownerDocument() && video->ownerDocument()->isMediaDocument()) {
            // Video in standalone media documents should not use the default 300x150
            // size since they also have audio thrown at them. By setting the intrinsic
            // size to 300x1 the video will resize itself in these cases, and audio will
            // have the correct height (it needs to be > 0 for controls to render properly).
            setIntrinsicSize(IntSize(cDefaultWidth, 1));
        }
        else
            setIntrinsicSize(IntSize(cDefaultWidth, cDefaultHeight));
    }
}

RenderVideo::~RenderVideo()
{
    if (MediaPlayer* p = player()) {
        p->setVisible(false);
        p->setFrameView(0);
    }
}

void RenderVideo::intrinsicSizeChanged()
{
    if (videoElement()->shouldDisplayPosterImage())
        RenderMedia::intrinsicSizeChanged();
    videoSizeChanged(); 
}


void RenderVideo::videoSizeChanged()
{
    if (!player())
        return;
    IntSize size = player()->naturalSize();
    if (!size.isEmpty() && size != intrinsicSize()) {
        setIntrinsicSize(size);
        setPrefWidthsDirty(true);
        setNeedsLayout(true);
    }
}

void RenderVideo::imageChanged(WrappedImagePtr newImage, const IntRect* rect)
{
    RenderMedia::imageChanged(newImage, rect);

    // Cache the image intrinsic size so we can continue to use it to draw the image correctly
    // even after we know the video intrisic size but aren't able to draw video frames yet
    // (we don't want to scale the poster to the video size).
    if (videoElement()->shouldDisplayPosterImage())
        m_cachedImageSize = intrinsicSize();
}

IntRect RenderVideo::videoBox() const
{
    if (m_cachedImageSize.isEmpty() && videoElement()->shouldDisplayPosterImage())
        return IntRect();

    IntSize elementSize;
    if (videoElement()->shouldDisplayPosterImage())
        elementSize = m_cachedImageSize;
    else
        elementSize = intrinsicSize();

    IntRect contentRect = contentBoxRect();
    if (elementSize.isEmpty() || contentRect.isEmpty())
        return IntRect();

    IntRect renderBox = contentRect;
    int ratio = renderBox.width() * elementSize.height() - renderBox.height() * elementSize.width();
    if (ratio > 0) {
        int newWidth = renderBox.height() * elementSize.width() / elementSize.height();
        // Just fill the whole area if the difference is one pixel or less (in both sides)
        if (renderBox.width() - newWidth > 2)
            renderBox.setWidth(newWidth);
        renderBox.move((contentRect.width() - renderBox.width()) / 2, 0);
    } else if (ratio < 0) {
        int newHeight = renderBox.width() * elementSize.height() / elementSize.width();
        if (renderBox.height() - newHeight > 2)
            renderBox.setHeight(newHeight);
        renderBox.move(0, (contentRect.height() - renderBox.height()) / 2);
    }

    return renderBox;
}
    
void RenderVideo::paintReplaced(PaintInfo& paintInfo, int tx, int ty)
{
    MediaPlayer* mediaPlayer = player();
    bool displayingPoster = videoElement()->shouldDisplayPosterImage();

    if (displayingPoster && document()->printing() && !view()->printImages())
        return;

    if (!displayingPoster) {
        if (!mediaPlayer)
            return;
        updatePlayer();
    }

    IntRect rect = videoBox();
    if (rect.isEmpty())
        return;
    rect.move(tx, ty);
    if (displayingPoster)
        paintIntoRect(paintInfo.context, rect);
    else
        mediaPlayer->paint(paintInfo.context, rect);
}

void RenderVideo::layout()
{
    RenderMedia::layout();
    updatePlayer();
}
    
HTMLVideoElement* RenderVideo::videoElement() const
{
    ASSERT(node()->hasTagName(videoTag));
    return static_cast<HTMLVideoElement*>(node()); 
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
    if (!videoElement()->inActiveDocument()) {
        mediaPlayer->setVisible(false);
        return;
    }

#if USE(ACCELERATED_COMPOSITING)
    layer()->rendererContentChanged();
#endif
    
    IntRect videoBounds = videoBox(); 
    mediaPlayer->setFrameView(document()->view());
    mediaPlayer->setSize(IntSize(videoBounds.width(), videoBounds.height()));
    mediaPlayer->setVisible(true);
}

int RenderVideo::calcReplacedWidth(bool includeMaxWidth) const
{
    int width;
    if (isWidthSpecified())
        width = calcReplacedWidthUsing(style()->width());
    else
        width = calcAspectRatioWidth() * style()->effectiveZoom();

    int minW = calcReplacedWidthUsing(style()->minWidth());
    int maxW = !includeMaxWidth || style()->maxWidth().isUndefined() ? width : calcReplacedWidthUsing(style()->maxWidth());

    return max(minW, min(width, maxW));
}

int RenderVideo::calcReplacedHeight() const
{
    int height;
    if (isHeightSpecified())
        height = calcReplacedHeightUsing(style()->height());
    else
        height = calcAspectRatioHeight() * style()->effectiveZoom();

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

int RenderVideo::minimumReplacedHeight() const 
{
    return 0; 
}

#if USE(ACCELERATED_COMPOSITING)
bool RenderVideo::supportsAcceleratedRendering() const
{
    MediaPlayer* p = player();
    if (p)
        return p->supportsAcceleratedRendering();

    return false;
}

void RenderVideo::acceleratedRenderingStateChanged()
{
    MediaPlayer* p = player();
    if (p)
        p->acceleratedRenderingStateChanged();
}
#endif  // USE(ACCELERATED_COMPOSITING)

} // namespace WebCore

#endif
