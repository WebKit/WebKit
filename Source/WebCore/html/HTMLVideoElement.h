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

#pragma once

#if ENABLE(VIDEO)

#include "HTMLMediaElement.h"
#include "Supplementable.h"
#include "VideoFrameRequestCallback.h"
#include <memory>

namespace WebCore {

class DestinationColorSpace;
class HTMLImageLoader;
class ImageBuffer;
class RenderVideo;
class PictureInPictureObserver;
class VideoFrameRequestCallback;

enum class PixelFormat : uint8_t;
enum class RenderingMode : bool;

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
    void requestFullscreen(FullscreenOptions&&, RefPtr<DeferredPromise>&&) override;
#endif

    RefPtr<ImageBuffer> createBufferForPainting(const FloatSize&, RenderingMode, const DestinationColorSpace&, PixelFormat) const;

    // Used by canvas to gain raw pixel access
    void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&);

    bool shouldGetNativeImageForCanvasDrawing() const;
    WEBCORE_EXPORT RefPtr<NativeImage> nativeImageForCurrentTime();
    std::optional<DestinationColorSpace> colorSpace() const;

    WEBCORE_EXPORT bool shouldDisplayPosterImage() const;

    URL posterImageURL() const;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    enum class VideoPresentationMode { Inline, Fullscreen, PictureInPicture };
    static VideoPresentationMode toPresentationMode(HTMLMediaElementEnums::VideoFullscreenMode);
    WEBCORE_EXPORT bool webkitSupportsPresentationMode(VideoPresentationMode) const;
    VideoPresentationMode webkitPresentationMode() const;
    void webkitSetPresentationMode(VideoPresentationMode);

    WEBCORE_EXPORT void setPresentationMode(VideoPresentationMode);
    WEBCORE_EXPORT void didEnterFullscreenOrPictureInPicture(const FloatSize&);
    WEBCORE_EXPORT void didExitFullscreenOrPictureInPicture();
    WEBCORE_EXPORT bool isChangingPresentationMode() const;

    void setVideoFullscreenFrame(const FloatRect&) final;

#if ENABLE(PICTURE_IN_PICTURE_API)
    void setPictureInPictureObserver(PictureInPictureObserver*);
#endif
#endif

#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    void exitToFullscreenModeWithoutAnimationIfPossible(HTMLMediaElementEnums::VideoFullscreenMode fromMode, HTMLMediaElementEnums::VideoFullscreenMode toMode);
#endif

    RenderVideo* renderer() const;

    bool shouldServiceRequestVideoFrameCallbacks() const { return !m_videoFrameRequests.isEmpty(); }
    void serviceRequestVideoFrameCallbacks(ReducedResolutionSeconds);

    unsigned requestVideoFrameCallback(Ref<VideoFrameRequestCallback>&&);
    void cancelVideoFrameCallback(unsigned);

private:
    HTMLVideoElement(const QualifiedName&, Document&, bool createdByParser);

    void scheduleResizeEvent() final;
    void scheduleResizeEventIfSizeChanged() final;
    bool rendererIsNeeded(const RenderStyle&) final;
    void didAttachRenderers() final;
    void parseAttribute(const QualifiedName&, const AtomString&) final;
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const final;
    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) final;
    bool isVideo() const final { return true; }
    bool hasVideo() const final { return player() && player()->hasVideo(); }
    bool supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenMode) const final;
    bool isURLAttribute(const Attribute&) const final;
    const AtomString& imageSourceURL() const final;

    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) final;

    bool hasAvailableVideoFrame() const;
    void mediaPlayerFirstVideoFrameAvailable() final;

    PlatformMediaSession::MediaType presentationType() const final { return PlatformMediaSession::MediaType::Video; }

    void mediaPlayerEngineUpdated() final;

    std::unique_ptr<HTMLImageLoader> m_imageLoader;

    AtomString m_defaultPosterURL;

    unsigned m_lastReportedVideoWidth { 0 };
    unsigned m_lastReportedVideoHeight { 0 };

#if ENABLE(VIDEO_PRESENTATION_MODE)
    bool m_enteringPictureInPicture { false };
    bool m_exitingPictureInPicture { false };
#endif

#if ENABLE(PICTURE_IN_PICTURE_API)
    PictureInPictureObserver* m_pictureInPictureObserver { nullptr };
#endif

    struct VideoFrameRequest {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        VideoFrameRequest(unsigned identifier, Ref<VideoFrameRequestCallback>&& callback)
            : identifier(identifier)
            , callback(WTFMove(callback))
        {
        }

        unsigned identifier { 0 };
        Ref<VideoFrameRequestCallback> callback;
        bool cancelled { false };
    };
    Vector<UniqueRef<VideoFrameRequest>> m_videoFrameRequests;
    Vector<UniqueRef<VideoFrameRequest>> m_servicedVideoFrameRequests;
    unsigned m_nextVideoFrameRequestIndex { 0 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLVideoElement)
    static bool isType(const WebCore::HTMLMediaElement& element) { return element.hasTagName(WebCore::HTMLNames::videoTag); }
    static bool isType(const WebCore::Element& element) { return is<WebCore::HTMLMediaElement>(element) && isType(downcast<WebCore::HTMLMediaElement>(element)); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::HTMLMediaElement>(node) && isType(downcast<WebCore::HTMLMediaElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO)
