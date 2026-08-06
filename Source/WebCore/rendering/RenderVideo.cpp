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
#include "RenderMediaInlines.h"
#include "RenderObjectInlines.h"
#include "RenderVideoInlines.h"
#include "RenderView.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderVideo);

RenderVideo::RenderVideo(HTMLVideoElement& element, Style::ComputedStyle&& style)
    : RenderMedia(Type::Video, element, WTF::move(style))
{
    setIntrinsicSize(calculateIntrinsicSize());
    ASSERT(isRenderVideo());
}

// Do not add any code in below destructor. Add it to willBeDestroyed() instead.
RenderVideo::~RenderVideo() = default;

void RenderVideo::willBeDestroyed()
{
    visibleInViewportStateChanged();

    if (RefPtr player = videoElement().player())
        player->renderVideoWillBeDestroyed();

    RenderMedia::willBeDestroyed();
}

void RenderVideo::visibleInViewportStateChanged()
{
    protect(videoElement())->isVisibleInViewportChanged();
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
    if (protect(videoElement())->shouldDisplayPosterImage())
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
    if (protect(videoElement())->readyState() >= HTMLMediaElementEnums::HAVE_METADATA)
        incrementVisuallyNonEmptyPixelCountIfNeeded(roundedIntSize(size));

    if (size == intrinsicSize())
        return false;

    setIntrinsicSize(size);
    invalidateContentLogicalWidths();
    setNeedsLayout();
    return true;
}

LayoutSize RenderVideo::calculateIntrinsicSizeInternal()
{
    // This implements the intrinsic width/height calculation from:
    // https://html.spec.whatwg.org/#the-video-element:dimension-attributes:~:text=The%20intrinsic%20width%20of%20a%20video%20element's%20playback%20area
    // If the video playback area is currently represented by the poster image,
    // the intrinsic width and height are that of the poster image.
    Ref videoElement = this->videoElement();
    RefPtr player = videoElement->player();

    // Assume the intrinsic width is that of the video.
    if (player && videoElement->readyState() >= HTMLVideoElement::HAVE_METADATA) {
        LayoutSize size(player->naturalSize());
        if (!size.isEmpty())
            return size;
    }

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

    // Return cached poster size directly if we're using it, since it's already scaled.
    // Determine what we should display: poster or video.
    // If the show-poster-flag is set (or there is no video frame to display) AND
    // there is a poster image, display the poster.
    Ref videoElement = this->videoElement();
    RefPtr player = videoElement->player();
    bool shouldUsePoster = (videoElement->shouldDisplayPosterImage() || !player || !player->hasAvailableVideoFrame()) && hasPosterFrameSize();

    if (shouldUsePoster) {
        auto cachedSize = m_cachedImageSize;
        if (shouldApplyInlineSizeContainment()) {
            if (isHorizontalWritingMode())
                cachedSize.setWidth(intrinsicSize().width());
            else
                cachedSize.setHeight(intrinsicSize().height());
        }
        return cachedSize;
    }

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
    if (protect(videoElement())->shouldDisplayPosterImage())
        m_cachedImageSize = intrinsicSize();

    // The intrinsic size is now that of the image, but in case we already had the
    // intrinsic size of the video we call this here to restore the video size.
    if (updateIntrinsicSize() || selfNeedsLayout())
        invalidateLineLayout();
}

LayoutSize RenderVideo::posterAwareIntrinsicSize() const
{
    if (protect(videoElement())->shouldDisplayPosterImage())
        return m_cachedImageSize;
    return intrinsicSize();
}

IntRect RenderVideo::videoBox() const
{
    Ref videoElement = this->videoElement();
    RefPtr mediaPlayer = videoElement->player();
    if (mediaPlayer && mediaPlayer->shouldIgnoreIntrinsicSize())
        return snappedIntRect(contentBoxRect());

    LayoutSize intrinsicSize = posterAwareIntrinsicSize();

    // Only bypass the view-box crop once the fullscreen/PiP transition has genuinely settled
    // (isChangingVideoFullscreenMode() false): fullscreenMode() flips synchronously on request,
    // well before the async transition animation finishes, so bypassing immediately would show
    // full/natural content while the animation still anchors on the cropped inline geometry.
    if (isBypassingObjectViewBoxForPictureInPicture())
        return snappedIntRect(LayoutRect(LayoutPoint(), intrinsicSize));
    if (!style().objectViewBox().isNone() && videoElement->isFullscreen() && !videoElement->isChangingVideoFullscreenMode())
        return snappedIntRect(contentBoxRect());

    return snappedIntRect(replacedContentRect(intrinsicSize));
}

bool RenderVideo::isBypassingObjectViewBoxForPictureInPicture() const
{
    if (style().objectViewBox().isNone())
        return false;
    Ref videoElement = this->videoElement();
    return videoElement->isFullscreen() && !videoElement->isChangingVideoFullscreenMode()
        && videoElement->fullscreenMode() == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture;
}

std::optional<LayoutRect> RenderVideo::objectFitContentsRectForFullscreenCompositing(const LayoutRect& dest) const
{
    Ref videoElement = this->videoElement();
    // Deliberately not gated on !isChangingVideoFullscreenMode(): dest (from videoBox()) is
    // screen-shaped throughout the transition too, since the :fullscreen UA stylesheet resizes
    // the box synchronously on request, well before the transition animation finishes. Applying
    // this same fit math throughout (rather than only once settled) keeps the crop's framing
    // consistent across the transition, avoiding a jump to a differently-shaped crop the moment
    // the transition ends.
    if (!videoElement->isFullscreen())
        return std::nullopt;
    if (videoElement->fullscreenMode() == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
        return std::nullopt;

    // Fill doesn't need a bespoke uniform scale here: the plain per-axis stretch mapping
    // (the fallback below) already matches its semantics exactly.
    auto objectFit = style().objectFit();
    if (objectFit != ObjectFit::Cover && objectFit != ObjectFit::Contain)
        return std::nullopt;

    auto naturalSize = FloatSize(posterAwareIntrinsicSize());
    auto subrect = resolvedObjectViewBox(naturalSize);
    if (!subrect)
        return std::nullopt;

    // Match native fullscreen video presentation (and other browsers' behavior): scale the crop
    // subregion uniformly, either growing it to cover the destination with no letterboxing
    // (object-fit: cover, cropping further into the subregion as needed) or shrinking it to fit
    // within the destination with no cropping (object-fit: contain, letterboxing instead) — the
    // same semantics object-fit itself uses, just anchored on the view-box subrect rather than
    // the whole natural frame. The result is the oversized (cover) or undersized (contain)
    // full-frame rect to use as the contents rect; the destination rect itself (unchanged)
    // remains the clip.
    auto destRect = FloatRect(dest);
    float scale = objectFit == ObjectFit::Cover
        ? std::max(destRect.width() / subrect->width(), destRect.height() / subrect->height())
        : std::min(destRect.width() / subrect->width(), destRect.height() / subrect->height());
    // The scaled subrect exactly matches destRect on the constraining axis (the one that
    // determined `scale`), and either overflows (cover) or underflows (contain) it on the other
    // axis. Position that difference according to object-position — matching
    // RenderReplaced::replacedContentRect()'s handling of the same overflow/underflow — rather
    // than always centering, which would ignore an author-specified object-position.
    auto scaledSubrectSize = subrect->size() * scale;
    auto& objectPosition = style().objectPosition();
    auto zoom = style().usedZoomForLength();
    auto xOffset = Style::evaluate<LayoutUnit>(objectPosition.x, LayoutUnit(destRect.width() - scaledSubrectSize.width()), zoom);
    auto yOffset = Style::evaluate<LayoutUnit>(objectPosition.y, LayoutUnit(destRect.height() - scaledSubrectSize.height()), zoom);
    FloatPoint scaledSubrectOrigin {
        destRect.x() + xOffset,
        destRect.y() + yOffset
    };
    FloatRect fullRect {
        scaledSubrectOrigin.x() - subrect->x() * scale,
        scaledSubrectOrigin.y() - subrect->y() * scale,
        naturalSize.width() * scale,
        naturalSize.height() * scale
    };
    return LayoutRect(fullRect);
}

LayoutRect RenderVideo::croppedVideoBoxForCompositing() const
{
    auto dest = videoBox();
    if (style().objectViewBox().isNone())
        return dest;
    if (isBypassingObjectViewBoxForPictureInPicture())
        return dest;
    if (auto objectFitRect = objectFitContentsRectForFullscreenCompositing(dest))
        return *objectFitRect;
    return computePaintRectForObjectViewBox(dest, posterAwareIntrinsicSize());
}

LayoutRect RenderVideo::inlineVideoBox() const
{
    // Like videoBox(), but never applies the fullscreen/PiP object-view-box bypass.
    // Used for the fullscreen/PiP transition-anchor rect (VideoPresentationManager's
    // inlineVideoFrame()), which is queried while fullscreenMode() already/still
    // reports the target mode, and must reflect where the video sits when displayed
    // inline (i.e. cropped), not the fullscreen/PiP steady-state display box.
    Ref videoElement = this->videoElement();
    RefPtr mediaPlayer = videoElement->player();
    if (mediaPlayer && mediaPlayer->shouldIgnoreIntrinsicSize())
        return contentBoxRect();

    LayoutSize intrinsicSize = posterAwareIntrinsicSize();

    // For object-fit: cover, replacedContentRect() intentionally returns a rect larger
    // than contentBoxRect() (grown/centered to crop-to-fill) — correct for painting/
    // compositing, where the overflow is clipped separately, but wrong as a transition
    // anchor, since VideoPresentationInterfaceMac uses this rect directly as the on-screen
    // frame with no clip layer of its own. Intersecting with contentBoxRect() clamps cover's
    // overshoot to the true visible box; it's a no-op for fill/contain/none, whose rects
    // are already contained within (or equal to) contentBoxRect().
    LayoutRect result = replacedContentRect(intrinsicSize);
    result.intersect(contentBoxRect());

    // Snap to the same integer CSS-pixel grid as videoBox() (which bounds the steady-state
    // compositing clip, see RenderLayerBacking::updateContentsRects()'s use of videoBox() to
    // intersect the clipping rect). Without this, the unsnapped result here can differ from the
    // snapped clip boundary by a fraction of a pixel, producing a brief seam along one edge right
    // as the PiP/fullscreen transition hands off to steady-state compositing.
    return LayoutRect(snappedIntRect(result));
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
    return !protect(videoElement())->shouldDisplayPosterImage();
}

bool RenderVideo::failedToLoadPosterImage() const
{
    return protect(imageResource())->errorOccurred();
}

void RenderVideo::paintReplaced(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(!isSkippedContentRoot(*this));

    Ref videoElement = this->videoElement();
    Ref page = this->page();
    RefPtr mediaPlayer = videoElement->player();
    bool displayingPoster = videoElement->shouldDisplayPosterImage();
    GraphicsContext& context = paintInfo.context();

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

    LayoutRect contentRect = contentBoxRect();
    contentRect.moveBy(paintOffset);

    LayoutRect paintRect = computePaintRectForObjectViewBox(rect, posterAwareIntrinsicSize());

    bool clip = !contentRect.contains(paintRect);
    GraphicsContextStateSaver stateSaver(context, clip);
    if (clip)
        context.clip(contentRect);

    if (paintInfo.phase == PaintPhase::Foreground) {
        page->addRelevantRepaintedObject(*this, rect);
        if (displayingPoster && !context.paintingDisabled())
            protect(document())->didPaintImage(videoElement.get(), protect(cachedImage()), videoBoxRect);
    }

    if (context.detectingContentfulPaint()) {
        context.setContentfulPaintDetected();
        return;
    }

    if (displayingPoster) {
        paintIntoRect(paintInfo, paintRect);
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

    videoElement->paint(context, paintRect);
}

void RenderVideo::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    updateIntrinsicSize();
    RenderMedia::layout();
    updatePlayer();
}

void RenderVideo::styleDidChange(Style::Difference difference, const Style::ComputedStyle* oldStyle)
{
    RenderMedia::styleDidChange(difference, oldStyle);
    if (!oldStyle)
        return;

    if (style().objectFit() != oldStyle->objectFit())
        setNeedsLayout();

    // RenderReplaced::styleDidChange() already schedules a layout for an object-view-box change
    // when a dimension is auto, since the view box can affect the natural size in that case. With
    // explicit sizing that layout is skipped, but the compositing layer's cached crop rect and
    // video gravity still need to be refreshed, which a plain repaint won't do on its own.
    if (style().objectViewBox() != oldStyle->objectViewBox()) {
        if (!needsLayout())
            contentChanged(ContentChangeType::Video);
#if ENABLE(VIDEO_PRESENTATION_MODE)
        // Propagate the change to VideoPresentationInterfaceMac's cached hasObjectViewBox(),
        // which drives PiP video gravity, so a style change made while already in an active
        // fullscreen/PiP session doesn't leave it stale; see HTMLMediaElement::hasObjectViewBoxChanged().
        Ref videoElement = this->videoElement();
        videoElement->hasObjectViewBoxChanged(!style().objectViewBox().isNone());
#endif
    }
}

HTMLVideoElement& NODELETE RenderVideo::videoElement() const
{
    return downcast<HTMLVideoElement>(RenderMedia::mediaElement());
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

    // Keep in sync with RenderLayerBacking::updateVideoGravity()'s object-view-box handling:
    // the cropped-frame geometry computed for object-view-box already bakes in object-fit, so
    // the player shouldn't also letterbox to preserve aspect ratio. Without this, the player's
    // m_shouldMaintainAspectRatio (used as the default video gravity once a fullscreen/PiP
    // transition ends and videoFullscreenLayer() becomes null, see
    // MediaPlayerPrivateAVFoundationObjC::updateVideoLayerGravity()) would race against, and
    // briefly override, the correct Resize gravity for any non-Fill object-fit value.
    bool shouldMaintainAspectRatio = style().objectViewBox().isNone() && style().objectFit() != ObjectFit::Fill;
    videoElement->updateMediaPlayer(videoBox().size(), shouldMaintainAspectRatio);
    return intrinsicSizeChanged;
}

LayoutUnit RenderVideo::computeReplacedLogicalWidth(IsComputingIntrinsicSize isComputingIntrinsicSize) const
{
    return computeReplacedLogicalWidthRespectingMinMaxWidth(RenderReplaced::computeReplacedLogicalWidth(isComputingIntrinsicSize), isComputingIntrinsicSize);
}

LayoutUnit RenderVideo::minimumReplacedHeight() const 
{
    return RenderReplaced::minimumReplacedHeight(); 
}

bool RenderVideo::supportsAcceleratedRendering() const
{
    return protect(videoElement())->supportsAcceleratedRendering();
}

void RenderVideo::acceleratedRenderingStateChanged()
{
    protect(videoElement())->acceleratedRenderingStateChanged();
}

bool RenderVideo::requiresImmediateCompositing() const
{
    RefPtr player = videoElement().player();
    return player && player->requiresImmediateCompositing();
}

bool RenderVideo::foregroundIsKnownToBeOpaqueInRect(const LayoutRect& localRect, unsigned maxDepthToTest) const
{
    Ref videoElement = this->videoElement();
    if (videoElement->shouldDisplayPosterImage())
        return RenderImage::foregroundIsKnownToBeOpaqueInRect(localRect, maxDepthToTest);

    if (!videoBox().contains(enclosingIntRect(localRect)))
        return false;

    // object-view-box's inset() can be negative, making the view box a superset of the natural
    // size, in which case the painted frame is smaller than videoBox() and leaves gaps.
    if (!objectViewBoxIsContainedWithinNaturalSize())
        return false;

    if (RefPtr player = videoElement->player())
        return player->hasAvailableVideoFrame();

    return false;
}

bool RenderVideo::hasVideoMetadata() const
{
    if (RefPtr player = videoElement().player())
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
    return protect(videoElement())->shouldDisplayPosterImage() && !isEmpty && !protect(imageResource())->errorOccurred();
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
