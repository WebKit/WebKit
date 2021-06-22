/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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
#include "EventNames.h"
#include "Frame.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "Page.h"
#include "PictureInPictureSupport.h"
#include "RenderImage.h"
#include "RenderVideo.h"
#include "ScriptController.h"
#include "Settings.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

#if ENABLE(VIDEO_PRESENTATION_MODE)
#include "VideoFullscreenModel.h"
#endif

#if ENABLE(PICTURE_IN_PICTURE_API)
#include "HTMLVideoElementPictureInPicture.h"
#include "PictureInPictureObserver.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLVideoElement);

using namespace HTMLNames;

inline HTMLVideoElement::HTMLVideoElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLMediaElement(tagName, document, createdByParser)
{
    ASSERT(hasTagName(videoTag));
    setHasCustomStyleResolveCallbacks();
    m_defaultPosterURL = document.settings().defaultVideoPosterURL();
}

Ref<HTMLVideoElement> HTMLVideoElement::create(const QualifiedName& tagName, Document& document, bool createdByParser)
{
    auto videoElement = adoptRef(*new HTMLVideoElement(tagName, document, createdByParser));

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
        if (auto* renderer = this->renderer())
            renderer->imageResource().setCachedImage(m_imageLoader->image());
    }
}

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

void HTMLVideoElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == posterAttr) {
        if (shouldDisplayPosterImage()) {
            if (!m_imageLoader)
                m_imageLoader = makeUnique<HTMLImageLoader>(*this);
            m_imageLoader->updateFromElementIgnoringPreviousError();
        } else {
            if (auto* renderer = this->renderer()) {
                renderer->imageResource().setCachedImage(nullptr);
                renderer->updateFromElement();
            }
        }
    }
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    else if (name == webkitwirelessvideoplaybackdisabledAttr)
        mediaSession().setWirelessVideoPlaybackDisabled(true);
#endif
    else {
        HTMLMediaElement::parseAttribute(name, value);    

#if PLATFORM(IOS_FAMILY) && ENABLE(WIRELESS_PLAYBACK_TARGET)
        if (name == webkitairplayAttr) {
            bool disabled = false;
            if (equalLettersIgnoringASCIICase(attributeWithoutSynchronization(HTMLNames::webkitairplayAttr), "deny"))
                disabled = true;
            mediaSession().setWirelessVideoPlaybackDisabled(disabled);
        }
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

    Page* page = document().page();
    if (!page) 
        return false;

    if (!player()->supportsFullscreen())
        return false;

#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(videoFullscreenMode);
    // Fullscreen implemented by player.
    return true;
#else

#if ENABLE(FULLSCREEN_API)
    if (videoFullscreenMode == HTMLMediaElementEnums::VideoFullscreenModeStandard && !document().settings().fullScreenEnabled())
        return false;

    // If the full screen API is enabled and is supported for the current element
    // do not require that the player has a video track to enter full screen.
    if (videoFullscreenMode == HTMLMediaElementEnums::VideoFullscreenModeStandard && page->chrome().client().supportsFullScreenForElement(*this, false))
        return true;
#endif

    if (!player()->hasVideo())
        return false;

    return page->chrome().client().supportsVideoFullscreen(videoFullscreenMode);
#endif // PLATFORM(IOS_FAMILY)
}

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
void HTMLVideoElement::webkitRequestFullscreen()
{
    webkitSetPresentationMode(HTMLVideoElement::VideoPresentationMode::Fullscreen);
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

void HTMLVideoElement::scheduleResizeEvent()
{
    m_lastReportedVideoWidth = videoWidth();
    m_lastReportedVideoHeight = videoHeight();
    scheduleEvent(eventNames().resizeEvent);
}

void HTMLVideoElement::scheduleResizeEventIfSizeChanged()
{
    if (m_lastReportedVideoWidth == videoWidth() && m_lastReportedVideoHeight == videoHeight())
        return;
    scheduleResizeEvent();
}

bool HTMLVideoElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == posterAttr || HTMLMediaElement::isURLAttribute(attribute);
}

const AtomString& HTMLVideoElement::imageSourceURL() const
{
    const AtomString& url = attributeWithoutSynchronization(posterAttr);
    if (!stripLeadingAndTrailingHTMLSpaces(url).isEmpty())
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
    INFO_LOG(LOGIDENTIFIER, "m_showPoster = ", showPosterFlag());

    if (showPosterFlag())
        return;

    invalidateStyleAndLayerComposition();

    if (auto player = this->player())
        player->prepareForRendering();

    if (auto* renderer = this->renderer())
        renderer->updateFromElement();
}

RefPtr<ImageBuffer> HTMLVideoElement::createBufferForPainting(const FloatSize& size, RenderingMode renderingMode, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat) const
{
    auto* hostWindow = document().view() && document().view()->root() ? document().view()->root()->hostWindow() : nullptr;
    auto shouldUseDisplayList = document().settings().displayListDrawingEnabled() ? ShouldUseDisplayList::Yes : ShouldUseDisplayList::No;
    return ImageBuffer::create(size, renderingMode, shouldUseDisplayList, RenderingPurpose::MediaPainting, 1, colorSpace, pixelFormat, hostWindow);
}

void HTMLVideoElement::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& destRect)
{
    RefPtr<MediaPlayer> player = HTMLMediaElement::player();
    if (!player)
        return;
    
    player->setVisibleForCanvas(true); // Make player visible or it won't draw.
    context.paintFrameForMedia(*player, destRect);
}

bool HTMLVideoElement::hasAvailableVideoFrame() const
{
    if (!player())
        return false;
    
    return player()->hasVideo() && player()->hasAvailableVideoFrame();
}

RefPtr<NativeImage> HTMLVideoElement::nativeImageForCurrentTime()
{
    if (!player())
        return nullptr;

    return player()->nativeImageForCurrentTime();
}

ExceptionOr<void> HTMLVideoElement::webkitEnterFullscreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isFullscreen())
        return { };

    // Generate an exception if this isn't called in response to a user gesture, or if the 
    // element does not support fullscreen, or the element is changing fullscreen mode.
    if (!mediaSession().fullscreenPermitted() || !supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard) || isChangingVideoFullscreenMode())
        return Exception { InvalidStateError };

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
    if (document().quirks().needsAkamaiMediaPlayerQuirk(*this))
        return isFullscreen() || isChangingVideoFullscreenMode();

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
    String url = stripLeadingAndTrailingHTMLSpaces(imageSourceURL());
    if (url.isEmpty())
        return URL();
    return document().completeURL(url);
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

    ASSERT_NOT_REACHED();
    return HTMLVideoElement::VideoPresentationMode::Inline;
}

void HTMLVideoElement::webkitSetPresentationMode(VideoPresentationMode mode)
{
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

void HTMLVideoElement::didEnterFullscreenOrPictureInPicture(const FloatSize& size)
{
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
    if (document().page()->chrome().client().supportsVideoFullscreen(fromMode))
        document().page()->chrome().client().exitVideoFullscreenToModeWithoutAnimation(*this, toMode);
}
#endif

}

#endif
