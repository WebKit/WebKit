/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

    videoElement->finishInitialization();
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

    updateDisplayState();
    if (shouldDisplayPosterImage()) {
        if (!m_imageLoader)
            m_imageLoader = makeUnique<HTMLImageLoader>(*this);
        m_imageLoader->updateFromElement();
        if (auto* renderer = this->renderer())
            renderer->imageResource().setCachedImage(m_imageLoader->image());
    }
}

void HTMLVideoElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else
        HTMLMediaElement::collectStyleForPresentationAttribute(name, value, style);
}

bool HTMLVideoElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr)
        return true;
    return HTMLMediaElement::isPresentationAttribute(name);
}

void HTMLVideoElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == posterAttr) {
        if (hasAvailableVideoFrame())
            return;

        // Force a poster recalc by setting m_displayMode to Unknown directly before calling updateDisplayState.
        HTMLMediaElement::setDisplayMode(Unknown);
        updateDisplayState();

        if (shouldDisplayPosterImage()) {
            if (!m_imageLoader)
                m_imageLoader = makeUnique<HTMLImageLoader>(*this);
            m_imageLoader->updateFromElementIgnoringPreviousError();
        } else {
            if (auto* renderer = this->renderer())
                renderer->imageResource().setCachedImage(nullptr);
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

void HTMLVideoElement::setDisplayMode(DisplayMode mode)
{
    DisplayMode oldMode = displayMode();
    URL poster = posterImageURL();

    if (!poster.isEmpty()) {
        // We have a poster path, but only show it until the user triggers display by playing or seeking and the
        // media engine has something to display.
        if (mode == Video) {
            if (oldMode != Video && player())
                player()->prepareForRendering();
            if (!hasAvailableVideoFrame())
                mode = PosterWaitingForVideo;
        }
    } else if (oldMode != Video && player())
        player()->prepareForRendering();

    HTMLMediaElement::setDisplayMode(mode);

    if (auto* renderer = this->renderer()) {
        if (displayMode() != oldMode)
            renderer->updateFromElement();
    }
}

void HTMLVideoElement::updateDisplayState()
{
    if (posterImageURL().isEmpty() || hasAvailableVideoFrame())
        setDisplayMode(Video);
    else if (displayMode() < Poster)
        setDisplayMode(Poster);
}

void HTMLVideoElement::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& destRect)
{
    RefPtr<MediaPlayer> player = HTMLMediaElement::player();
    if (!player)
        return;
    
    player->setVisible(true); // Make player visible or it won't draw.
    player->paintCurrentFrameInContext(context, destRect);
}

bool HTMLVideoElement::copyVideoTextureToPlatformTexture(GraphicsContextGLOpenGL* context, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY)
{
    if (!player())
        return false;
    return player()->copyVideoTextureToPlatformTexture(context, texture, target, level, internalFormat, format, type, premultiplyAlpha, flipY);
}

bool HTMLVideoElement::hasAvailableVideoFrame() const
{
    if (!player())
        return false;
    
    return player()->hasVideo() && player()->hasAvailableVideoFrame();
}

NativeImagePtr HTMLVideoElement::nativeImageForCurrentTime()
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
    // element does not support fullscreen.
    if (!mediaSession().fullscreenPermitted() || !supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard))
        return Exception { InvalidStateError };

    enterFullscreen();
    return { };
}

void HTMLVideoElement::webkitExitFullscreen()
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (isFullscreen())
        exitFullscreen();
}

bool HTMLVideoElement::webkitSupportsFullscreen()
{
    return supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard);
}

bool HTMLVideoElement::webkitDisplayingFullscreen()
{
    if (document().quirks().needsAkamaiMediaPlayerQuirk(*this))
        return isFullscreen() || m_isChangingPresentationMode;

    // This function starts to return true after the video element has entered
    // fullscreen/picture-in-picture until it has exited fullscreen/picture-in-picture
    return (isFullscreen() && !waitingToEnterFullscreen()) || (!isFullscreen() && m_isChangingPresentationMode);
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

static inline HTMLVideoElement::VideoPresentationMode toPresentationMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
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
    setFullscreenMode(toFullscreenMode(mode));
}

void HTMLVideoElement::setFullscreenMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    INFO_LOG(LOGIDENTIFIER, ", mode = ", mode);

    if (m_isChangingPresentationMode)
        return;

    if (mode == VideoFullscreenModeNone) {
        if (isFullscreen()) {
            if (toPresentationMode(fullscreenMode()) == VideoPresentationMode::PictureInPicture)
                m_isEnteringOrExitingPictureInPicture = true;

            m_isChangingPresentationMode = true;
            exitFullscreen();
        }

        return;
    }

    if (!mediaSession().fullscreenPermitted() || !supportsFullscreen(mode))
        return;

    if (mode == VideoFullscreenModePictureInPicture)
        m_isEnteringOrExitingPictureInPicture = true;

    if (mode != fullscreenMode()) {
        m_isChangingPresentationMode = true;
        enterFullscreen(mode);
    }
}

auto HTMLVideoElement::webkitPresentationMode() const -> VideoPresentationMode
{
    return toPresentationMode(fullscreenMode());
}

void HTMLVideoElement::fullscreenModeChanged(VideoFullscreenMode mode)
{
    if (mode != fullscreenMode()) {
        INFO_LOG(LOGIDENTIFIER, "changed from ", fullscreenMode(), ", to ", mode);
        scheduleEvent(eventNames().webkitpresentationmodechangedEvent);
        setPreparedToReturnVideoLayerToInline(mode != HTMLMediaElementEnums::VideoFullscreenModePictureInPicture);
    }

    if (player())
        player()->setVideoFullscreenMode(mode);

    HTMLMediaElement::fullscreenModeChanged(mode);
}

void HTMLVideoElement::didBecomeFullscreenElement()
{
    m_isChangingPresentationMode = false;
    if (m_isEnteringOrExitingPictureInPicture)
        m_isWaitingForPictureInPictureWindowFrame = true;

    HTMLMediaElement::didBecomeFullscreenElement();
}

void HTMLVideoElement::didStopBeingFullscreenElement()
{
    m_isChangingPresentationMode = false;
    if (m_isEnteringOrExitingPictureInPicture) {
        m_isEnteringOrExitingPictureInPicture = false;
#if ENABLE(PICTURE_IN_PICTURE_API)
        if (m_pictureInPictureObserver)
            m_pictureInPictureObserver->didExitPictureInPicture();
#endif
    }
}

void HTMLVideoElement::setVideoFullscreenFrame(FloatRect frame)
{
    HTMLMediaElement::setVideoFullscreenFrame(frame);

    if (m_isWaitingForPictureInPictureWindowFrame) {
        m_isWaitingForPictureInPictureWindowFrame = false;
        m_isEnteringOrExitingPictureInPicture = false;
#if ENABLE(PICTURE_IN_PICTURE_API)
        if (m_pictureInPictureObserver)
            m_pictureInPictureObserver->didEnterPictureInPicture(IntSize(frame.size()));
#endif
        return;
    }

    if (toPresentationMode(fullscreenMode()) != VideoPresentationMode::PictureInPicture)
        return;

#if ENABLE(PICTURE_IN_PICTURE_API)
    if (!m_isEnteringOrExitingPictureInPicture && m_pictureInPictureObserver)
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
