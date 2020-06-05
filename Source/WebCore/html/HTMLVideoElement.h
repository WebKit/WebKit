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

#pragma once

#if ENABLE(VIDEO)

#include "HTMLMediaElement.h"
#include "Supplementable.h"
#include <memory>

namespace WebCore {

class HTMLImageLoader;
class RenderVideo;
class PictureInPictureObserver;

class HTMLVideoElement final : public HTMLMediaElement, public Supplementable<HTMLVideoElement> {
    WTF_MAKE_ISO_ALLOCATED(HTMLVideoElement);
public:
    WEBCORE_EXPORT static Ref<HTMLVideoElement> create(Document&);
    static Ref<HTMLVideoElement> create(const QualifiedName&, Document&, bool createdByParser);

    WEBCORE_EXPORT unsigned videoWidth() const;
    WEBCORE_EXPORT unsigned videoHeight() const;

    WEBCORE_EXPORT ExceptionOr<void> webkitEnterFullscreen();
    WEBCORE_EXPORT void webkitExitFullscreen();
    WEBCORE_EXPORT bool webkitSupportsFullscreen();
    WEBCORE_EXPORT bool webkitDisplayingFullscreen();

    void ancestorWillEnterFullscreen() final;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    bool webkitWirelessVideoPlaybackDisabled() const;
    void setWebkitWirelessVideoPlaybackDisabled(bool);
#endif

#if ENABLE(MEDIA_STATISTICS)
    unsigned webkitDecodedFrameCount() const;
    unsigned webkitDroppedFrameCount() const;
#endif

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS_FAMILY)
    void webkitRequestFullscreen() override;
#endif

    // Used by canvas to gain raw pixel access
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&);

    NativeImagePtr nativeImageForCurrentTime();

    // Used by WebGL to do GPU-GPU textures copy if possible.
    // See more details at MediaPlayer::copyVideoTextureToPlatformTexture() defined in Source/WebCore/platform/graphics/MediaPlayer.h.
    bool copyVideoTextureToPlatformTexture(GraphicsContextGLOpenGL*, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY);

    bool shouldDisplayPosterImage() const { return displayMode() == Poster || displayMode() == PosterWaitingForVideo; }

    URL posterImageURL() const;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    enum class VideoPresentationMode { Inline, Fullscreen, PictureInPicture};
    WEBCORE_EXPORT bool webkitSupportsPresentationMode(VideoPresentationMode) const;
    void webkitSetPresentationMode(VideoPresentationMode);
    VideoPresentationMode webkitPresentationMode() const;
    void setFullscreenMode(VideoFullscreenMode);
    void fullscreenModeChanged(VideoFullscreenMode) final;

    WEBCORE_EXPORT void didBecomeFullscreenElement() final;
    WEBCORE_EXPORT void didStopBeingFullscreenElement() final;
    void setVideoFullscreenFrame(FloatRect) final;

#if ENABLE(PICTURE_IN_PICTURE_API)
    void setPictureInPictureObserver(PictureInPictureObserver*);
#endif
#endif

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    void exitToFullscreenModeWithoutAnimationIfPossible(HTMLMediaElementEnums::VideoFullscreenMode fromMode, HTMLMediaElementEnums::VideoFullscreenMode toMode);
#endif

    RenderVideo* renderer() const;

private:
    HTMLVideoElement(const QualifiedName&, Document&, bool createdByParser);

    void scheduleResizeEvent() final;
    void scheduleResizeEventIfSizeChanged() final;
    bool rendererIsNeeded(const RenderStyle&) final;
    void didAttachRenderers() final;
    void parseAttribute(const QualifiedName&, const AtomString&) final;
    bool isPresentationAttribute(const QualifiedName&) const final;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) final;
    bool isVideo() const final { return true; }
    bool hasVideo() const final { return player() && player()->hasVideo(); }
    bool supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenMode) const final;
    bool isURLAttribute(const Attribute&) const final;
    const AtomString& imageSourceURL() const final;

    bool hasAvailableVideoFrame() const;
    void updateDisplayState() final;
    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) final;
    void setDisplayMode(DisplayMode) final;

    PlatformMediaSession::MediaType presentationType() const final { return PlatformMediaSession::MediaType::Video; }

    std::unique_ptr<HTMLImageLoader> m_imageLoader;

    AtomString m_defaultPosterURL;

    unsigned m_lastReportedVideoWidth { 0 };
    unsigned m_lastReportedVideoHeight { 0 };
    bool m_isChangingPresentationMode { false };

#if ENABLE(VIDEO_PRESENTATION_MODE)
    bool m_isEnteringOrExitingPictureInPicture { false };
    bool m_isWaitingForPictureInPictureWindowFrame { false };
#endif

#if ENABLE(PICTURE_IN_PICTURE_API)
    PictureInPictureObserver* m_pictureInPictureObserver { nullptr };
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLVideoElement)
    static bool isType(const WebCore::HTMLMediaElement& element) { return element.hasTagName(WebCore::HTMLNames::videoTag); }
    static bool isType(const WebCore::Element& element) { return is<WebCore::HTMLMediaElement>(element) && isType(downcast<WebCore::HTMLMediaElement>(element)); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::HTMLMediaElement>(node) && isType(downcast<WebCore::HTMLMediaElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO)
