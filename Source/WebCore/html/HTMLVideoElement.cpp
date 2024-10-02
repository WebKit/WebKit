/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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
#include "HTMLVideoElement.h"

#if ENABLE(VIDEO)

#include "CSSPropertyNames.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "ElementInlines.h"
#include "EventNames.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "ImageBuffer.h"
#include "JSDOMPromiseDeferred.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "MediaPlayerPrivate.h"
#include "Page.h"
#include "Performance.h"
#include "PictureInPictureSupport.h"
#include "RenderImage.h"
#include "RenderLayerCompositor.h"
#include "RenderVideo.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "Settings.h"
#include "VideoFrameMetadata.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

#if ENABLE(VIDEO_PRESENTATION_MODE)
#include "VideoPresentationModel.h"
#endif

#if ENABLE(PICTURE_IN_PICTURE_API)
#include "HTMLVideoElementPictureInPicture.h"
#include "PictureInPictureObserver.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLVideoElement);

using namespace HTMLNames;

inline HTMLVideoElement::HTMLVideoElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLMediaElement(tagName, document, createdByParser)
{
    ASSERT(hasTagName(videoTag));
    m_defaultPosterURL = AtomString { document.settings().defaultVideoPosterURL() };
}

HTMLVideoElement::~HTMLVideoElement() = default;

Ref<HTMLVideoElement> HTMLVideoElement::create(const QualifiedName& tagName, Document& document, bool createdByParser)
{
    Ref videoElement = adoptRef(*new HTMLVideoElement(tagName, document, createdByParser));

#if ENABLE(PICTURE_IN_PICTURE_API)
    HTMLVideoElementPictureInPicture::providePictureInPictureTo(videoElement);
#endif

    videoElement->suspendIfNeeded();
    return videoElement;
}

Ref<HTMLVideoElement> HTMLVideoElement::create(Document& document)
{
    return create(videoTag, document, false);
}

bool HTMLVideoElement::rendererIsNeeded(const RenderStyle& style)
{
    return HTMLElement::rendererIsNeeded(style); 
}

RenderPtr<RenderElement> HTMLVideoElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderVideo>(*this, WTFMove(style));
}

void HTMLVideoElement::didAttachRenderers()
{
    HTMLMediaElement::didAttachRenderers();

    if (shouldDisplayPosterImage()) {
        if (!m_imageLoader)
            m_imageLoader = makeUnique<HTMLImageLoader>(*this);
        m_imageLoader->updateFromElement();
        if (CheckedPtr renderer = this->renderer())
            renderer->checkedImageResource()->setCachedImage(m_imageLoader->protectedImage());
    }
}

void HTMLVideoElement::acceleratedRenderingStateChanged()
{
    computeAcceleratedRenderingStateAndUpdateMediaPlayer();
}

bool HTMLVideoElement::supportsAcceleratedRendering() const
{
    return RefPtr { player() } && player()->supportsAcceleratedRendering();
}

void HTMLVideoElement::mediaPlayerRenderingModeChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    // Kick off a fake recalcStyle that will update the compositing tree.
    computeAcceleratedRenderingStateAndUpdateMediaPlayer();
    invalidateStyleAndLayerComposition();
}

void HTMLVideoElement::computeAcceleratedRenderingStateAndUpdateMediaPlayer()
{
    RefPtr player = this->player();
    if (!player)
        return;
#if ENABLE(VIDEO_PRESENTATION_MODE)
    // This function must return "true" when the video is playing in the
    // picture-in-picture window or if it is in fullscreen.
    // Otherwise, the MediaPlayerPrivate* may destroy the video layer if
    // it is no longer in the DOM.
    bool isInFullScreen = fullscreenMode() != VideoFullscreenModeNone;
#else
    bool isInFullScreen = false;
#endif
    auto* renderer = this->renderer();
    bool canBeAccelerated = player->supportsAcceleratedRendering() && (isInFullScreen || (renderer && renderer->view().compositor().hasAcceleratedCompositing()));
    if (canBeAccelerated == m_renderingCanBeAccelerated)
        return;
    m_renderingCanBeAccelerated = canBeAccelerated;
    player->acceleratedRenderingStateChanged(); // This call will trigger a call back to `mediaPlayerRenderingCanBeAccelerated()` from the MediaPlayer.
}

#if PLATFORM(IOS_FAMILY)
bool HTMLVideoElement::canShowWhileLocked() const
{
    RefPtr page = document().page();
    return page && page->canShowWhileLocked();
}
#endif

void HTMLVideoElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == widthAttr) {
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
        applyAspectRatioFromWidthAndHeightAttributesToStyle(value, attributeWithoutSynchronization(heightAttr), style);
    } else if (name == heightAttr) {
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
        applyAspectRatioFromWidthAndHeightAttributesToStyle(attributeWithoutSynchronization(widthAttr), value, style);
    } else
        HTMLMediaElement::collectPresentationalHintsForAttribute(name, value, style);
}

bool HTMLVideoElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr)
        return true;
    return HTMLMediaElement::hasPresentationalHintsForAttribute(name);
}

void HTMLVideoElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == posterAttr) {
        if (shouldDisplayPosterImage()) {
            if (!m_imageLoader)
                m_imageLoader = makeUnique<HTMLImageLoader>(*this);
            m_imageLoader->updateFromElementIgnoringPreviousError();
        } else {
            if (CheckedPtr renderer = this->renderer()) {
                renderer->checkedImageResource()->setCachedImage(nullptr);
                renderer->updateFromElement();
            }
        }
    }
    else {
        HTMLMediaElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);

#if PLATFORM(IOS_FAMILY) && ENABLE(WIRELESS_PLAYBACK_TARGET)
        if (name == webkitairplayAttr)
            mediaSession().setWirelessVideoPlaybackDisabled(isWirelessPlaybackTargetDisabled());
#endif
    }
}

bool HTMLVideoElement::supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenMode videoFullscreenMode) const
{
    if (!player())
        return false;
    
    if (videoFullscreenMode == HTMLMediaElementEnums::VideoFullscreenModePictureInPicture) {
        if (!mediaSession().allowsPictureInPicture())
            return false;
        if (!player()->supportsPictureInPicture())
            return false;
    }

    RefPtr page = document().page();
    if (!page) 
        return false;

    if (!player()->supportsFullscreen())
        return false;

#if ENABLE(FULLSCREEN_API)
#if PLATFORM(IOS_FAMILY)
    // Fullscreen implemented by player.
    if (!document().settings().videoFullscreenRequiresElementFullscreen())
        return true;
#endif

    if (videoFullscreenMode == HTMLMediaElementEnums::VideoFullscreenModeStandard && !page->isFullscreenManagerEnabled())
        return false;

    // If the full screen API is enabled and is supported for the current element
    // do not require that the player has a video track to enter full screen.
    if (videoFullscreenMode == HTMLMediaElementEnums::VideoFullscreenModeStandard && page->chrome().client().supportsFullScreenForElement(*this, false))
        return true;
#endif

    if (!player()->hasVideo())
        return false;

    return page->chrome().client().supportsVideoFullscreen(videoFullscreenMode);
}

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
void HTMLVideoElement::requestFullscreen(FullscreenOptions&&, RefPtr<DeferredPromise>&& promise)
{
    webkitSetPresentationMode(HTMLVideoElement::VideoPresentationMode::Fullscreen);
    if (promise)
        promise->resolve();
}
#endif

unsigned HTMLVideoElement::videoWidth() const
{
    if (!player())
        return 0;
    return clampToUnsigned(player()->naturalSize().width());
}

unsigned HTMLVideoElement::videoHeight() const
{
    if (!player())
        return 0;
    return clampToUnsigned(player()->naturalSize().height());
}

void HTMLVideoElement::scheduleResizeEvent(const FloatSize& naturalSize)
{
    m_lastReportedNaturalSize = naturalSize;
    ALWAYS_LOG(LOGIDENTIFIER, naturalSize);
    scheduleEvent(eventNames().resizeEvent);
}

void HTMLVideoElement::scheduleResizeEventIfSizeChanged(const FloatSize& naturalSize)
{
    if (m_lastReportedNaturalSize != naturalSize)
        scheduleResizeEvent(naturalSize);
}

bool HTMLVideoElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == posterAttr || HTMLMediaElement::isURLAttribute(attribute);
}

const AtomString& HTMLVideoElement::imageSourceURL() const
{
    const auto& url = attributeWithoutSynchronization(posterAttr);
    if (!StringView(url).containsOnly<isASCIIWhitespace<UChar>>())
        return url;
    return m_defaultPosterURL;
}

bool HTMLVideoElement::shouldDisplayPosterImage() const
{
    if (!showPosterFlag())
        return false;

    if (posterImageURL().isEmpty())
        return false;

    auto* renderer = this->renderer();
    if (renderer && renderer->failedToLoadPosterImage())
        return false;

    return true;
}

void HTMLVideoElement::mediaPlayerFirstVideoFrameAvailable()
{
    ALWAYS_LOG(LOGIDENTIFIER, "m_showPoster = ", showPosterFlag());

    if (showPosterFlag())
        return;

    invalidateStyleAndLayerComposition();

    if (RefPtr player = this->player())
        player->prepareForRendering();

    if (CheckedPtr renderer = this->renderer())
        renderer->updateFromElement();
}

std::optional<DestinationColorSpace> HTMLVideoElement::colorSpace() const
{
    RefPtr player = this->player();
    if (!player)
        return std::nullopt;

    return player->colorSpace();
}

RefPtr<ImageBuffer> HTMLVideoElement::createBufferForPainting(const FloatSize& size, RenderingMode renderingMode, const DestinationColorSpace& colorSpace, ImageBufferPixelFormat pixelFormat) const
{
    auto* hostWindow = document().view() && document().view()->root() ? document().view()->root()->hostWindow() : nullptr;
    return ImageBuffer::create(size, RenderingPurpose::MediaPainting, 1, colorSpace, pixelFormat, bufferOptionsForRendingMode(renderingMode), hostWindow);
}

void HTMLVideoElement::paint(GraphicsContext& context, const FloatRect& destRect)
{
    RefPtr player = this->player();
    if (!player)
        return;
    player->paint(context, destRect);
}

void HTMLVideoElement::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& destRect)
{
    RefPtr player = this->player();
    if (!player)
        return;

    if (!player->isVisibleForCanvas()) {
        player->setVisibleForCanvas(true); // Make player visible or it won't draw.
        visibilityStateChanged();
    }
    player->paintCurrentFrameInContext(context, destRect);
}

bool HTMLVideoElement::hasAvailableVideoFrame() const
{
    if (!player())
        return false;
    
    return player()->hasVideo() && player()->hasAvailableVideoFrame();
}

bool HTMLVideoElement::shouldGetNativeImageForCanvasDrawing() const
{
    if (!player())
        return false;

    return player()->shouldGetNativeImageForCanvasDrawing();
}

RefPtr<NativeImage> HTMLVideoElement::nativeImageForCurrentTime()
{
    RefPtr player = this->player();
    return player? player->nativeImageForCurrentTime() : nullptr;
}

ExceptionOr<void> HTMLVideoElement::webkitEnterFullscreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isFullscreen())
        return { };

    // Generate an exception if this isn't called in response to a user gesture, or if the 
    // element does not support fullscreen, or the element is changing fullscreen mode.
    if (!mediaSession().fullscreenPermitted() || !supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard) || isChangingVideoFullscreenMode())
        return Exception { ExceptionCode::InvalidStateError };

    enterFullscreen();
    return { };
}

void HTMLVideoElement::webkitExitFullscreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isFullscreen() && !isChangingVideoFullscreenMode())
        exitFullscreen();
}

bool HTMLVideoElement::webkitSupportsFullscreen()
{
    return supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard);
}

bool HTMLVideoElement::webkitDisplayingFullscreen()
{
    return isFullscreen();
}

void HTMLVideoElement::ancestorWillEnterFullscreen()
{
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    if (fullscreenMode() == VideoFullscreenModeNone)
        return;

    // If this video element's presentation mode is not inline, but its ancestor
    // is entering fullscreen, exit its current fullscreen mode.
    exitToFullscreenModeWithoutAnimationIfPossible(fullscreenMode(), VideoFullscreenModeNone);
#endif
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
bool HTMLVideoElement::webkitWirelessVideoPlaybackDisabled() const
{
    return mediaSession().wirelessVideoPlaybackDisabled();
}

void HTMLVideoElement::setWebkitWirelessVideoPlaybackDisabled(bool disabled)
{
    setBooleanAttribute(webkitwirelessvideoplaybackdisabledAttr, disabled);
}
#endif

void HTMLVideoElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    if (m_imageLoader)
        m_imageLoader->elementDidMoveToNewDocument(oldDocument);
    HTMLMediaElement::didMoveToNewDocument(oldDocument, newDocument);
}

#if ENABLE(MEDIA_STATISTICS)
unsigned HTMLVideoElement::webkitDecodedFrameCount() const
{
    if (!player())
        return 0;

    return player()->decodedFrameCount();
}

unsigned HTMLVideoElement::webkitDroppedFrameCount() const
{
    if (!player())
        return 0;

    return player()->droppedFrameCount();
}
#endif

URL HTMLVideoElement::posterImageURL() const
{
    auto url = imageSourceURL().string().trim(isASCIIWhitespace);
    if (url.isEmpty())
        return URL();
    return protectedDocument()->completeURL(url);
}

#if ENABLE(VIDEO_PRESENTATION_MODE)

bool HTMLVideoElement::webkitSupportsPresentationMode(VideoPresentationMode mode) const
{
    if (mode == VideoPresentationMode::Fullscreen)
        return supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard);

    if (mode == VideoPresentationMode::PictureInPicture) {
        if (!supportsPictureInPicture())
            return false;

        return supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
    }

    if (mode == VideoPresentationMode::Inline)
        return !mediaSession().requiresFullscreenForVideoPlayback();

    return false;
}

static inline HTMLMediaElementEnums::VideoFullscreenMode toFullscreenMode(HTMLVideoElement::VideoPresentationMode mode)
{
    switch (mode) {
    case HTMLVideoElement::VideoPresentationMode::Fullscreen:
        return HTMLMediaElementEnums::VideoFullscreenModeStandard;
    case HTMLVideoElement::VideoPresentationMode::PictureInPicture:
        return HTMLMediaElementEnums::VideoFullscreenModePictureInPicture;
    case HTMLVideoElement::VideoPresentationMode::Inline:
        return HTMLMediaElementEnums::VideoFullscreenModeNone;
    case HTMLVideoElement::VideoPresentationMode::InWindow:
        return HTMLMediaElementEnums::VideoFullscreenModeInWindow;
    }
    ASSERT_NOT_REACHED();
    return HTMLMediaElementEnums::VideoFullscreenModeNone;
}

HTMLVideoElement::VideoPresentationMode HTMLVideoElement::toPresentationMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    if (mode == HTMLMediaElementEnums::VideoFullscreenModeStandard)
        return HTMLVideoElement::VideoPresentationMode::Fullscreen;

    if (mode & HTMLMediaElementEnums::VideoFullscreenModePictureInPicture)
        return HTMLVideoElement::VideoPresentationMode::PictureInPicture;

    if (mode == HTMLMediaElementEnums::VideoFullscreenModeNone)
        return HTMLVideoElement::VideoPresentationMode::Inline;

    if (mode == HTMLMediaElementEnums::VideoFullscreenModeInWindow)
        return HTMLVideoElement::VideoPresentationMode::InWindow;

    ASSERT_NOT_REACHED();
    return HTMLVideoElement::VideoPresentationMode::Inline;
}

void HTMLVideoElement::webkitSetPresentationMode(VideoPresentationMode mode)
{
    if (mode == VideoPresentationMode::InWindow && !document().settings().inWindowFullscreenEnabled())
        return;

    INFO_LOG(LOGIDENTIFIER, ", mode = ",  mode);
    if (!isChangingVideoFullscreenMode())
        setPresentationMode(mode);
}

void HTMLVideoElement::setPresentationMode(VideoPresentationMode mode)
{
    if (toPresentationMode(fullscreenMode()) == mode)
        return;

    auto videoFullscreenMode = toFullscreenMode(mode);
    INFO_LOG(LOGIDENTIFIER, ", videoFullscreenMode = ", videoFullscreenMode);

    if (videoFullscreenMode == VideoFullscreenModeNone) {
        if (isFullscreen()) {
            if (toPresentationMode(fullscreenMode()) == VideoPresentationMode::PictureInPicture)
                m_exitingPictureInPicture = true;

            exitFullscreen();
        }

        return;
    }

    if (!mediaSession().fullscreenPermitted() || !supportsFullscreen(videoFullscreenMode))
        return;

    if (videoFullscreenMode == VideoFullscreenModePictureInPicture)
        m_enteringPictureInPicture = true;
    else if (fullscreenMode() == VideoFullscreenModePictureInPicture)
        m_exitingPictureInPicture = true;

    enterFullscreen(videoFullscreenMode);
}

auto HTMLVideoElement::webkitPresentationMode() const -> VideoPresentationMode
{
    return toPresentationMode(fullscreenMode());
}

auto HTMLVideoElement::webkitPresentationModeForBindings() const -> VideoPresentationMode
{
    auto mode = webkitPresentationMode();
    if (mode == HTMLVideoElement::VideoPresentationMode::InWindow)
        return HTMLVideoElement::VideoPresentationMode::Inline;
    return mode;
}

void HTMLVideoElement::didEnterFullscreenOrPictureInPicture(const FloatSize& size)
{
    if (RefPtr player = this->player())
        player->setInFullscreenOrPictureInPicture(true);

    if (m_enteringPictureInPicture) {
        m_enteringPictureInPicture = false;
        setChangingVideoFullscreenMode(false);

#if ENABLE(PICTURE_IN_PICTURE_API)
        if (m_pictureInPictureObserver)
            m_pictureInPictureObserver->didEnterPictureInPicture(flooredIntSize(size));
#else
        UNUSED_PARAM(size);
#endif
        return;
    }

    if (m_exitingPictureInPicture) {
        m_exitingPictureInPicture = false;
#if ENABLE(PICTURE_IN_PICTURE_API)
        if (m_pictureInPictureObserver)
            m_pictureInPictureObserver->didExitPictureInPicture();
#endif
    }

    HTMLMediaElement::didBecomeFullscreenElement();
}

void HTMLVideoElement::didExitFullscreenOrPictureInPicture()
{
    if (RefPtr player = this->player())
        player->setInFullscreenOrPictureInPicture(false);

    if (m_exitingPictureInPicture) {
        m_exitingPictureInPicture = false;
        setChangingVideoFullscreenMode(false);

#if ENABLE(PICTURE_IN_PICTURE_API)
        if (m_pictureInPictureObserver)
            m_pictureInPictureObserver->didExitPictureInPicture();
#endif
        return;
    }

    HTMLMediaElement::didStopBeingFullscreenElement();
}

bool HTMLVideoElement::isChangingPresentationMode() const
{
    return isChangingVideoFullscreenMode();
}

void HTMLVideoElement::setVideoFullscreenFrame(const FloatRect& frame)
{
    HTMLMediaElement::setVideoFullscreenFrame(frame);

    if (toPresentationMode(fullscreenMode()) != VideoPresentationMode::PictureInPicture)
        return;

#if ENABLE(PICTURE_IN_PICTURE_API)
    if (!m_enteringPictureInPicture && !m_exitingPictureInPicture && m_pictureInPictureObserver)
        m_pictureInPictureObserver->pictureInPictureWindowResized(IntSize(frame.size()));
#endif
}

#if ENABLE(PICTURE_IN_PICTURE_API)
void HTMLVideoElement::setPictureInPictureObserver(PictureInPictureObserver* observer)
{
    m_pictureInPictureObserver = observer;
}
#endif

#endif // ENABLE(VIDEO_PRESENTATION_MODE)

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
void HTMLVideoElement::exitToFullscreenModeWithoutAnimationIfPossible(HTMLMediaElementEnums::VideoFullscreenMode fromMode, HTMLMediaElementEnums::VideoFullscreenMode toMode)
{
    RefPtr page = document().page();
    if (page->chrome().client().supportsVideoFullscreen(fromMode))
        page->chrome().client().exitVideoFullscreenToModeWithoutAnimation(*this, toMode);
}
#endif

unsigned HTMLVideoElement::requestVideoFrameCallback(Ref<VideoFrameRequestCallback>&& callback)
{
    if (m_videoFrameRequests.isEmpty()) {
        if (RefPtr player = this->player())
            player->startVideoFrameMetadataGathering();
    }

    auto identifier = ++m_nextVideoFrameRequestIndex;
    m_videoFrameRequests.append(makeUniqueRef<VideoFrameRequest>(identifier, WTFMove(callback)));

    if (RefPtr page = document().page())
        page->scheduleRenderingUpdate(RenderingUpdateStep::VideoFrameCallbacks);

    return identifier;
}

void HTMLVideoElement::cancelVideoFrameCallback(unsigned identifier)
{
    // Search first the requests currently being serviced, and mark them as cancelled if found.
    auto index = m_servicedVideoFrameRequests.findIf([identifier](auto& request) { return request->identifier == identifier; });
    if (index != notFound) {
        m_servicedVideoFrameRequests[index]->cancelled = true;
        return;
    }

    index = m_videoFrameRequests.findIf([identifier](auto& request) { return request->identifier == identifier; });
    if (index == notFound)
        return;
    m_videoFrameRequests.remove(index);

    if (m_videoFrameRequests.isEmpty()) {
        if (RefPtr player = this->player())
            player->stopVideoFrameMetadataGathering();
    }
}

void HTMLVideoElement::stop()
{
    m_videoFrameRequests.clear();
    m_servicedVideoFrameRequests.clear();
    HTMLMediaElement::stop();
}

static void processVideoFrameMetadataTimestamps(VideoFrameMetadata& metadata, Performance& performance)
{
    metadata.presentationTime = performance.relativeTimeFromTimeOriginInReducedResolution(MonotonicTime::fromRawSeconds(metadata.presentationTime));
    metadata.expectedDisplayTime = performance.relativeTimeFromTimeOriginInReducedResolution(MonotonicTime::fromRawSeconds(metadata.expectedDisplayTime));
    if (metadata.captureTime)
        metadata.captureTime = performance.relativeTimeFromTimeOriginInReducedResolution(MonotonicTime::fromRawSeconds(*metadata.captureTime));
    if (metadata.receiveTime)
        metadata.receiveTime = performance.relativeTimeFromTimeOriginInReducedResolution(MonotonicTime::fromRawSeconds(*metadata.receiveTime));
}

void HTMLVideoElement::serviceRequestVideoFrameCallbacks(ReducedResolutionSeconds now)
{
    if (!player())
        return;

    // If the requestVideoFrameCallback is called before the readyState >= HaveCurrentData,
    // calls to createImageBitmap() with this element will result in a failed promise. Delay
    // notifying the callback until we reach the HaveCurrentData state.
    if (readyState() < HAVE_CURRENT_DATA)
        return;

    auto videoFrameMetadata = player()->videoFrameMetadata();
    if (!videoFrameMetadata || !document().domWindow())
        return;

    processVideoFrameMetadataTimestamps(*videoFrameMetadata, document().domWindow()->protectedPerformance());

    Ref protectedThis { *this };

    m_videoFrameRequests.swap(m_servicedVideoFrameRequests);
    for (auto& request : m_servicedVideoFrameRequests) {
        if (!request->cancelled) {
            Ref { request->callback }->handleEvent(std::round(now.milliseconds()), *videoFrameMetadata);
            request->cancelled = true;
        }
    }
    m_servicedVideoFrameRequests.clear();

    if (m_videoFrameRequests.isEmpty()) {
        if (RefPtr player = this->player())
            player->stopVideoFrameMetadataGathering();
    }
}

void HTMLVideoElement::mediaPlayerEngineUpdated()
{
    HTMLMediaElement::mediaPlayerEngineUpdated();
    if (!m_videoFrameRequests.isEmpty()) {
        if (RefPtr player = this->player())
            player->startVideoFrameMetadataGathering();
    }
    // If the RenderLayerCompositor had queried the element's MediaPlayer::supportsAcceleratedRendering prior the player having been created it would have been set to false.
    mediaPlayerRenderingModeChanged();
}

void HTMLVideoElement::setVideoFullscreenStandby(bool value)
{
    if (videoFullscreenStandby() == value)
        return;

    if (!document().page())
        return;

    if (!document().page()->chrome().client().supportsVideoFullscreenStandby())
        return;

    setVideoFullscreenStandbyInternal(value);

#if ENABLE(VIDEO_PRESENTATION_MODE)
    if (RefPtr player = this->player())
        player->videoFullscreenStandbyChanged();
#endif

    if (fullscreenMode() != VideoFullscreenModeNone)
        return;

    if (videoFullscreenStandby())
        document().protectedPage()->chrome().client().enterVideoFullscreenForVideoElement(*this, VideoFullscreenModeNone, true);
    else {
        document().protectedPage()->chrome().client().exitVideoFullscreenForVideoElement(*this, [this, protectedThis = Ref { *this }](auto success) mutable {
            setVideoFullscreenStandbyInternal(!success);
        });
    }
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
