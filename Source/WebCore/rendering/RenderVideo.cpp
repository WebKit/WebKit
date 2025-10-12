/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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
#include "DocumentFullscreen.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "LayoutIntegrationLineLayout.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "MediaPlayer.h"
#include "MediaPlayerEnums.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderBoxInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderVideo);

RenderVideo::RenderVideo(HTMLVideoElement& element, RenderStyle&& style)
    : RenderMedia(Type::Video, element, WTFMove(style))
{
    setIntrinsicSize(calculateIntrinsicSize());
    ASSERT(isRenderVideo());
}

// Do not add any code in below destructor. Add it to willBeDestroyed() instead.
RenderVideo::~RenderVideo() = default;

void RenderVideo::willBeDestroyed()
{
    visibleInViewportStateChanged();

    if (RefPtr player = protectedVideoElement()->player())
        player->renderVideoWillBeDestroyed();

    RenderMedia::willBeDestroyed();
}

void RenderVideo::visibleInViewportStateChanged()
{
    protectedVideoElement()->isVisibleInViewportChanged();
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
    if (protectedVideoElement()->shouldDisplayPosterImage())
        RenderMedia::intrinsicSizeChanged();
    if (updateIntrinsicSize())
        invalidateLineLayout();
}

bool RenderVideo::updateIntrinsicSize()
{
    LayoutSize size = calculateIntrinsicSize();
    // Never set the element size to zero when in a media document.
    if (size.isEmpty() && document().isMediaDocument())
        return false;

    // Treat the media player's natural size as visually non-empty.
    if (protectedVideoElement()->readyState() >= HTMLMediaElementEnums::HAVE_METADATA)
        incrementVisuallyNonEmptyPixelCountIfNeeded(roundedIntSize(size));

    if (size == intrinsicSize())
        return false;

    setIntrinsicSize(size);
    setNeedsPreferredWidthsUpdate();
    setNeedsLayout();
    return true;
}

LayoutSize RenderVideo::calculateIntrinsicSizeInternal()
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
    Ref videoElement = this->videoElement();
    RefPtr player = videoElement->player();
    if (player && videoElement->readyState() >= HTMLVideoElement::HAVE_METADATA) {
        LayoutSize size(player->naturalSize());
        if (!size.isEmpty())
            return size;
    }

    if (hasPosterFrameSize())
        return m_cachedImageSize;

    // <video> in standalone media documents should not use the default 300x150
    // size since they also have audio-only files. By setting the intrinsic
    // size to 300x1 the video will resize itself in these cases, and audio will
    // have the correct height (it needs to be > 0 for controls to render properly).
    if (videoElement->document().isMediaDocument())
        return LayoutSize(defaultSize().width(), 1);

    return defaultSize();
}

LayoutSize RenderVideo::calculateIntrinsicSize()
{
    if (shouldApplySizeContainment())
        return intrinsicSize();

    auto calculatedIntrinsicSize = calculateIntrinsicSizeInternal();
    calculatedIntrinsicSize.scale(style().usedZoom());

    if (shouldApplyInlineSizeContainment()) {
        if (isHorizontalWritingMode())
            calculatedIntrinsicSize.setWidth(intrinsicSize().width());
        else
            calculatedIntrinsicSize.setHeight(intrinsicSize().height());
    }
    return calculatedIntrinsicSize;
}

void RenderVideo::imageChanged(WrappedImagePtr newImage, const IntRect* rect)
{
    RenderMedia::imageChanged(newImage, rect);

    // Cache the image intrinsic size so we can continue to use it to draw the image correctly
    // even if we know the video intrinsic size but aren't able to draw video frames yet
    // (we don't want to scale the poster to the video size without keeping aspect ratio).
    if (protectedVideoElement()->shouldDisplayPosterImage())
        m_cachedImageSize = intrinsicSize();

    // The intrinsic size is now that of the image, but in case we already had the
    // intrinsic size of the video we call this here to restore the video size.
    if (updateIntrinsicSize() || selfNeedsLayout())
        invalidateLineLayout();
}

IntRect RenderVideo::videoBox() const
{
    Ref videoElement = this->videoElement();
    RefPtr mediaPlayer = videoElement->player();
    if (mediaPlayer && mediaPlayer->shouldIgnoreIntrinsicSize())
        return snappedIntRect(contentBoxRect());

    LayoutSize intrinsicSize = this->intrinsicSize();

    if (videoElement->shouldDisplayPosterImage())
        intrinsicSize = m_cachedImageSize;

    return snappedIntRect(replacedContentRect(intrinsicSize));
}

IntRect RenderVideo::videoBoxInRootView() const
{
    RefPtr view = document().view();
    if (!view)
        return { };

    auto videoBox = this->videoBox();
    videoBox.moveBy(absoluteBoundingBoxRect().location());
    return view->contentsToRootView(videoBox);
}

bool RenderVideo::shouldDisplayVideo() const
{
    return !protectedVideoElement()->shouldDisplayPosterImage();
}

bool RenderVideo::failedToLoadPosterImage() const
{
    return checkedImageResource()->errorOccurred();
}

void RenderVideo::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(!isSkippedContentRoot(*this));

    Ref videoElement = this->videoElement();
    Ref page = this->page();
    RefPtr mediaPlayer = videoElement->player();
    bool displayingPoster = videoElement->shouldDisplayPosterImage();

    if (!displayingPoster && !mediaPlayer) {
        if (paintInfo.phase == PaintPhase::Foreground)
            page->addRelevantUnpaintedObject(*this, visualOverflowRect());
        return;
    }

    LayoutRect videoBoxRect = videoBox();
    if (videoBoxRect.isEmpty()) {
        if (paintInfo.phase == PaintPhase::Foreground)
            page->addRelevantUnpaintedObject(*this, visualOverflowRect());
        return;
    }

    auto rect = videoBoxRect;
    rect.moveBy(paintOffset);
    GraphicsContext& context = paintInfo.context();

    if (paintInfo.phase == PaintPhase::Foreground) {
        page->addRelevantRepaintedObject(*this, rect);
        if (displayingPoster && !context.paintingDisabled())
            protectedDocument()->didPaintImage(videoElement.get(), cachedImage(), videoBoxRect);
    }

    LayoutRect contentRect = contentBoxRect();
    contentRect.moveBy(paintOffset);

    if (context.detectingContentfulPaint()) {
        context.setContentfulPaintDetected();
        return;
    }

    bool clip = !contentRect.contains(rect);
    GraphicsContextStateSaver stateSaver(context, clip);
    if (clip)
        context.clip(contentRect);

    if (displayingPoster) {
        paintIntoRect(paintInfo, rect);
        return;
    }

    if (!mediaPlayer)
        return;

    // Painting contents during fullscreen playback causes stutters on iOS when the device is rotated.
    // https://bugs.webkit.org/show_bug.cgi?id=142097
    if (videoElement->supportsAcceleratedRendering() && videoElement->isFullscreen())
        return;

    // Avoid unnecessary paints by skipping software painting if
    // the renderer is accelerated, and the paint operation does
    // not flatten compositing layers and is not snapshotting.
    if (hasAcceleratedCompositing()
        && videoElement->supportsAcceleratedRendering()
        && !paintInfo.paintBehavior.contains(PaintBehavior::FlattenCompositingLayers)
        && !paintInfo.paintBehavior.contains(PaintBehavior::Snapshotting))
        return;

    videoElement->paint(context, rect);
}

void RenderVideo::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    updateIntrinsicSize();
    RenderMedia::layout();
    updatePlayer();
}

void RenderVideo::styleDidChange(StyleDifference difference, const RenderStyle* oldStyle)
{
    RenderMedia::styleDidChange(difference, oldStyle);
    if (!oldStyle || style().objectFit() != oldStyle->objectFit())
        setNeedsLayout();
}

HTMLVideoElement& RenderVideo::videoElement() const
{
    return downcast<HTMLVideoElement>(RenderMedia::mediaElement());
}

Ref<HTMLVideoElement> RenderVideo::protectedVideoElement() const
{
    return videoElement();
}

void RenderVideo::updateFromElement()
{
    RenderMedia::updateFromElement();
    if (updatePlayer())
        invalidateLineLayout();
}

bool RenderVideo::updatePlayer()
{
    if (renderTreeBeingDestroyed())
        return false;

    auto intrinsicSizeChanged = updateIntrinsicSize();
    ASSERT(!intrinsicSizeChanged || !view().frameView().layoutContext().isInRenderTreeLayout());

    Ref videoElement = this->videoElement();
    RefPtr mediaPlayer = videoElement->player();
    if (!mediaPlayer)
        return intrinsicSizeChanged;

    if (videoElement->inActiveDocument())
        contentChanged(ContentChangeType::Video);

    videoElement->updateMediaPlayer(videoBox().size(), style().objectFit() != ObjectFit::Fill);
    return intrinsicSizeChanged;
}

LayoutUnit RenderVideo::computeReplacedLogicalWidth(ShouldComputePreferred shouldComputePreferred) const
{
    return computeReplacedLogicalWidthRespectingMinMaxWidth(RenderReplaced::computeReplacedLogicalWidth(shouldComputePreferred), shouldComputePreferred);
}

LayoutUnit RenderVideo::minimumReplacedHeight() const 
{
    return RenderReplaced::minimumReplacedHeight(); 
}

bool RenderVideo::supportsAcceleratedRendering() const
{
    return protectedVideoElement()->supportsAcceleratedRendering();
}

void RenderVideo::acceleratedRenderingStateChanged()
{
    protectedVideoElement()->acceleratedRenderingStateChanged();
}

bool RenderVideo::requiresImmediateCompositing() const
{
    RefPtr player = protectedVideoElement()->player();
    return player && player->requiresImmediateCompositing();
}

bool RenderVideo::foregroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect, unsigned maxDepthToTest) const
{
    Ref videoElement = this->videoElement();
    if (videoElement->shouldDisplayPosterImage())
        return RenderImage::foregroundIsKnownToBeOpaqueInRect(localRect, maxDepthToTest);

    if (!videoBox().contains(enclosingIntRect(localRect)))
        return false;

    if (RefPtr player = videoElement->player())
        return player->hasAvailableVideoFrame();

    return false;
}

bool RenderVideo::hasVideoMetadata() const
{
    if (RefPtr player = protectedVideoElement()->player())
        return player->readyState() >= MediaPlayerEnums::ReadyState::HaveMetadata;
    return false;
}

bool RenderVideo::hasPosterFrameSize() const
{
    bool isEmpty = m_cachedImageSize.isEmpty();
    // For contain: inline-size, if the block-size is not empty, it shouldn't be treated as empty here,
    // so that contain: inline-size could affect the intrinsic size, which should be 0 x block-size.
    if (shouldApplyInlineSizeContainment())
        isEmpty = isHorizontalWritingMode() ? !m_cachedImageSize.height() : !m_cachedImageSize.width();
    return protectedVideoElement()->shouldDisplayPosterImage() && !isEmpty && !checkedImageResource()->errorOccurred();
}

bool RenderVideo::hasDefaultObjectSize() const
{
    return !hasVideoMetadata() && !hasPosterFrameSize() && !shouldApplySizeContainment();
}

void RenderVideo::invalidateLineLayout()
{
    if (CheckedPtr inlineLayout = LayoutIntegration::LineLayout::containing(*this))
        inlineLayout->boxContentWillChange(*this);
}

} // namespace WebCore

#endif
