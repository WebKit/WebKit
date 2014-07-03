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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "MediaPlayer.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderView.h"
#include <wtf/StackStats.h>

#if ENABLE(FULLSCREEN_API)
#include "RenderFullScreen.h"
#endif

namespace WebCore {

using namespace HTMLNames;

RenderVideo::RenderVideo(HTMLVideoElement& element, PassRef<RenderStyle> style)
    : RenderMedia(element, WTF::move(style))
{
    setIntrinsicSize(calculateIntrinsicSize());
}

RenderVideo::~RenderVideo()
{
    if (MediaPlayer* player = videoElement().player()) {
        player->setVisible(false);
        player->setFrameView(0);
    }
}

IntSize RenderVideo::defaultSize()
{
    // These values are specified in the spec.
    static const int cDefaultWidth = 300;
    static const int cDefaultHeight = 150;

    return IntSize(cDefaultWidth, cDefaultHeight);
}

void RenderVideo::intrinsicSizeChanged()
{
    if (videoElement().shouldDisplayPosterImage())
        RenderMedia::intrinsicSizeChanged();
    updateIntrinsicSize(); 
}

void RenderVideo::updateIntrinsicSize()
{
    LayoutSize size = calculateIntrinsicSize();
    size.scale(style().effectiveZoom());

    // Never set the element size to zero when in a media document.
    if (size.isEmpty() && document().isMediaDocument())
        return;

    if (size == intrinsicSize())
        return;

    setIntrinsicSize(size);
    setPreferredLogicalWidthsDirty(true);
    setNeedsLayout();
}
    
LayoutSize RenderVideo::calculateIntrinsicSize()
{
    // Spec text from 4.8.6
    //
    // The intrinsic width of a video element's playback area is the intrinsic width 
    // of the video resource, if that is available; otherwise it is the intrinsic 
    // width of the poster frame, if that is available; otherwise it is 300 CSS pixels.
    //
    // The intrinsic height of a video element's playback area is the intrinsic height 
    // of the video resource, if that is available; otherwise it is the intrinsic 
    // height of the poster frame, if that is available; otherwise it is 150 CSS pixels.
    MediaPlayer* player = videoElement().player();
    if (player && videoElement().readyState() >= HTMLVideoElement::HAVE_METADATA) {
        LayoutSize size = player->naturalSize();
        if (!size.isEmpty())
            return size;
    }

    if (videoElement().shouldDisplayPosterImage() && !m_cachedImageSize.isEmpty() && !imageResource().errorOccurred())
        return m_cachedImageSize;

    // <video> in standalone media documents should not use the default 300x150
    // size since they also have audio-only files. By setting the intrinsic
    // size to 300x1 the video will resize itself in these cases, and audio will
    // have the correct height (it needs to be > 0 for controls to render properly).
    if (videoElement().document().isMediaDocument())
        return LayoutSize(defaultSize().width(), 1);

    return defaultSize();
}

void RenderVideo::imageChanged(WrappedImagePtr newImage, const IntRect* rect)
{
    RenderMedia::imageChanged(newImage, rect);

    // Cache the image intrinsic size so we can continue to use it to draw the image correctly
    // even if we know the video intrinsic size but aren't able to draw video frames yet
    // (we don't want to scale the poster to the video size without keeping aspect ratio).
    if (videoElement().shouldDisplayPosterImage())
        m_cachedImageSize = intrinsicSize();

    // The intrinsic size is now that of the image, but in case we already had the
    // intrinsic size of the video we call this here to restore the video size.
    updateIntrinsicSize();
}

IntRect RenderVideo::videoBox() const
{
    LayoutSize intrinsicSize = this->intrinsicSize();

    if (videoElement().shouldDisplayPosterImage())
        intrinsicSize = m_cachedImageSize;

    return pixelSnappedIntRect(replacedContentRect(intrinsicSize));
}

bool RenderVideo::shouldDisplayVideo() const
{
    return !videoElement().shouldDisplayPosterImage();
}

void RenderVideo::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    MediaPlayer* mediaPlayer = videoElement().player();
    bool displayingPoster = videoElement().shouldDisplayPosterImage();

    Page* page = frame().page();

    if (!displayingPoster && !mediaPlayer) {
        if (page && paintInfo.phase == PaintPhaseForeground)
            page->addRelevantUnpaintedObject(this, visualOverflowRect());
        return;
    }

    LayoutRect rect = videoBox();
    if (rect.isEmpty()) {
        if (page && paintInfo.phase == PaintPhaseForeground)
            page->addRelevantUnpaintedObject(this, visualOverflowRect());
        return;
    }
    rect.moveBy(paintOffset);

    if (page && paintInfo.phase == PaintPhaseForeground)
        page->addRelevantRepaintedObject(this, rect);

    LayoutRect contentRect = contentBoxRect();
    contentRect.moveBy(paintOffset);
    GraphicsContext* context = paintInfo.context;
    bool clip = !contentRect.contains(rect);
    GraphicsContextStateSaver stateSaver(*context, clip);
    if (clip)
        context->clip(contentRect);

    if (displayingPoster)
        paintIntoRect(context, rect);
    else if (view().frameView().paintBehavior() & PaintBehaviorFlattenCompositingLayers)
        mediaPlayer->paintCurrentFrameInContext(context, pixelSnappedIntRect(rect));
    else
        mediaPlayer->paint(context, pixelSnappedIntRect(rect));
}

void RenderVideo::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    RenderMedia::layout();
    updatePlayer();
}
    
HTMLVideoElement& RenderVideo::videoElement() const
{
    return toHTMLVideoElement(RenderMedia::mediaElement());
}

void RenderVideo::updateFromElement()
{
    RenderMedia::updateFromElement();
    updatePlayer();
}

void RenderVideo::updatePlayer()
{
    if (documentBeingDestroyed())
        return;

    updateIntrinsicSize();

    MediaPlayer* mediaPlayer = videoElement().player();
    if (!mediaPlayer)
        return;

    if (!videoElement().inActiveDocument()) {
        mediaPlayer->setVisible(false);
        return;
    }

    contentChanged(VideoChanged);
    
    IntRect videoBounds = videoBox(); 
    mediaPlayer->setFrameView(&view().frameView());
    mediaPlayer->setSize(IntSize(videoBounds.width(), videoBounds.height()));
    mediaPlayer->setVisible(true);
    mediaPlayer->setShouldMaintainAspectRatio(style().objectFit() != ObjectFitFill);
}

LayoutUnit RenderVideo::computeReplacedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    return RenderReplaced::computeReplacedLogicalWidth(shouldComputePreferred);
}

LayoutUnit RenderVideo::computeReplacedLogicalHeight() const
{
    return RenderReplaced::computeReplacedLogicalHeight();
}

LayoutUnit RenderVideo::minimumReplacedHeight() const 
{
    return RenderReplaced::minimumReplacedHeight(); 
}

bool RenderVideo::supportsAcceleratedRendering() const
{
    if (MediaPlayer* player = videoElement().player())
        return player->supportsAcceleratedRendering();
    return false;
}

void RenderVideo::acceleratedRenderingStateChanged()
{
    if (MediaPlayer* player = videoElement().player())
        player->acceleratedRenderingStateChanged();
}

bool RenderVideo::requiresImmediateCompositing() const
{
    MediaPlayer* player = videoElement().player();
    return player && player->requiresImmediateCompositing();
}

#if ENABLE(FULLSCREEN_API)
static const RenderBlock* rendererPlaceholder(const RenderObject* renderer)
{
    RenderObject* parent = renderer->parent();
    if (!parent)
        return 0;
    
    RenderFullScreen* fullScreen = parent->isRenderFullScreen() ? toRenderFullScreen(parent) : 0;
    if (!fullScreen)
        return 0;
    
    return fullScreen->placeholder();
}

LayoutUnit RenderVideo::offsetLeft() const
{
    if (const RenderBlock* block = rendererPlaceholder(this))
        return block->offsetLeft();
    return RenderMedia::offsetLeft();
}

LayoutUnit RenderVideo::offsetTop() const
{
    if (const RenderBlock* block = rendererPlaceholder(this))
        return block->offsetTop();
    return RenderMedia::offsetTop();
}

LayoutUnit RenderVideo::offsetWidth() const
{
    if (const RenderBlock* block = rendererPlaceholder(this))
        return block->offsetWidth();
    return RenderMedia::offsetWidth();
}

LayoutUnit RenderVideo::offsetHeight() const
{
    if (const RenderBlock* block = rendererPlaceholder(this))
        return block->offsetHeight();
    return RenderMedia::offsetHeight();
}
#endif

bool RenderVideo::foregroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect, unsigned maxDepthToTest) const
{
    if (videoElement().shouldDisplayPosterImage())
        return RenderImage::foregroundIsKnownToBeOpaqueInRect(localRect, maxDepthToTest);

    if (!videoBox().contains(enclosingIntRect(localRect)))
        return false;

    if (MediaPlayer* player = videoElement().player())
        return player->hasAvailableVideoFrame();

    return false;
}

} // namespace WebCore

#endif
