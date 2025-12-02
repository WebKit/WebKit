/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(VIDEO_PRESENTATION_MODE)

#include "FrameInfoData.h"
#include "LayerHostingContext.h"
#include "MessageReceiver.h"
#include "PlaybackSessionContextIdentifier.h"
#include <WebCore/AudioSession.h>
#include <WebCore/CocoaView.h>
#include <WebCore/HostingContext.h>
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/PlatformVideoPresentationInterface.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/SpatialVideoMetadata.h>
#include <WebCore/VideoPresentationModel.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/MachSendRightAnnotated.h>
#include <wtf/Observer.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS WKLayerHostView;
OBJC_CLASS WKSPlayableViewControllerHost;
OBJC_CLASS WKVideoView;
OBJC_CLASS WebAVPlayerLayer;
OBJC_CLASS WebAVPlayerLayerView;

namespace WebKit {

constexpr size_t DefaultMockPictureInPictureWindowWidth = 100;
constexpr size_t DefaultMockPictureInPictureWindowHeight = 100;

class WebPageProxy;
class PlaybackSessionManagerProxy;
class PlaybackSessionModelContext;
class VideoPresentationManagerProxy;
struct SharedPreferencesForWebProcess;

class VideoPresentationModelContext final
    : public WebCore::VideoPresentationModel  {
public:
    static Ref<VideoPresentationModelContext> create(VideoPresentationManagerProxy& manager, PlaybackSessionModelContext& playbackSessionModel, PlaybackSessionContextIdentifier contextId)
    {
        return adoptRef(*new VideoPresentationModelContext(manager, playbackSessionModel, contextId));
    }
    virtual ~VideoPresentationModelContext();

    void requestCloseAllMediaPresentations(bool finishedWithMedia, CompletionHandler<void()>&&);

private:
    friend class VideoPresentationManagerProxy;
    VideoPresentationModelContext(VideoPresentationManagerProxy&, PlaybackSessionModelContext&, PlaybackSessionContextIdentifier);

    void setVideoDimensions(const WebCore::FloatSize&);
    void audioSessionCategoryChanged(WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy);
    void routingContextUIDChanged(const String&);

    // VideoPresentationModel
    void addClient(WebCore::VideoPresentationModelClient&) override;
    void removeClient(WebCore::VideoPresentationModelClient&) override;
    void requestFullscreenMode(WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool finishedWithMedia = false) override;
    void setVideoLayerFrame(WebCore::FloatRect) override;
    void setVideoLayerGravity(WebCore::MediaPlayerEnums::VideoGravity) override;
    void setVideoFullscreenFrame(WebCore::FloatRect) override;
    void fullscreenModeChanged(WebCore::HTMLMediaElementEnums::VideoFullscreenMode, ShouldNotifyMediaElement) override;
    bool hasVideo() const override { return m_hasVideo; }
    bool isChildOfElementFullscreen() const final { return m_isChildOfElementFullscreen; }

    WebCore::FloatSize videoDimensions() const override { return m_videoDimensions; }
#if PLATFORM(IOS_FAMILY)
    UIViewController *presentingViewController() final;
    RetainPtr<UIViewController> createVideoFullscreenViewController(AVPlayerViewController*) final;
#endif
    void willEnterPictureInPicture() final;
    void didEnterPictureInPicture() final;
    void failedToEnterPictureInPicture() final;
    void willExitPictureInPicture() final;
    void didExitPictureInPicture() final;
    void requestRouteSharingPolicyAndContextUID(CompletionHandler<void(WebCore::RouteSharingPolicy, String)>&&) final;

    void requestUpdateInlineRect() final;
    void requestVideoContentLayer() final;
    void returnVideoContentLayer() final;
    void returnVideoView() final;
    void didSetupFullscreen() final;
    void failedToEnterFullscreen() final;
    void didEnterFullscreen(const WebCore::FloatSize&) final;
    void willExitFullscreen() final;
    void didExitFullscreen() final;
    void didCleanupFullscreen() final;
    void fullscreenMayReturnToInline() final;
#if ENABLE(LINEAR_MEDIA_PLAYER)
    void didEnterExternalPlayback() final;
    void didExitExternalPlayback() final;
#endif
    void setRequiresTextTrackRepresentation(bool) final;
    void setTextTrackRepresentationBounds(const WebCore::IntRect&) final;

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    void requestShowCaptionDisplaySettingsPreview() final;
    void requestHideCaptionDisplaySettingsPreview() final;
#endif

#if !RELEASE_LOG_DISABLED
    uint64_t logIdentifier() const final;
    uint64_t nextChildIdentifier() const final;
    const Logger* loggerPtr() const final;

    ASCIILiteral logClassName() const { return "VideoPresentationModelContext"_s; };
    WTFLogChannel& logChannel() const;
#endif

    WeakPtr<VideoPresentationManagerProxy> m_manager;
    Ref<PlaybackSessionModelContext> m_playbackSessionModel;
    PlaybackSessionContextIdentifier m_contextId;

    WeakHashSet<WebCore::VideoPresentationModelClient> m_clients;
    WebCore::FloatSize m_videoDimensions;
    bool m_hasVideo { false };
    bool m_isChildOfElementFullscreen { false };

#if !RELEASE_LOG_DISABLED
    mutable uint64_t m_childIdentifierSeed { 0 };
#endif
};

class VideoPresentationManagerProxy
    : public RefCounted<VideoPresentationManagerProxy>
    , public CanMakeWeakPtr<VideoPresentationManagerProxy>
    , private IPC::MessageReceiver {
public:
    USING_CAN_MAKE_WEAKPTR(CanMakeWeakPtr<VideoPresentationManagerProxy>);

    static Ref<VideoPresentationManagerProxy> create(WebPageProxy&, PlaybackSessionManagerProxy&);
    virtual ~VideoPresentationManagerProxy();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void invalidate();

    void requestHideAndExitFullscreen();
    bool hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) const;
    bool mayAutomaticallyShowVideoPictureInPicture() const;
    void applicationDidBecomeActive();
    bool isVisible() const;

    void setMockVideoPresentationModeEnabled(bool enabled) { m_mockVideoPresentationModeEnabled = enabled; }

    void requestRouteSharingPolicyAndContextUID(PlaybackSessionContextIdentifier, CompletionHandler<void(WebCore::RouteSharingPolicy, String)>&&);

    bool isPlayingVideoInEnhancedFullscreen() const;

    RefPtr<WebCore::PlatformVideoPresentationInterface> controlsManagerInterface();
    using VideoInPictureInPictureDidChangeObserver = WTF::Observer<void(bool)>;
    void addVideoInPictureInPictureDidChangeObserver(const VideoInPictureInPictureDidChangeObserver&);

    void forEachSession(Function<void(VideoPresentationModelContext&, WebCore::PlatformVideoPresentationInterface&)>&&);

    void requestBitmapImageForCurrentTime(PlaybackSessionContextIdentifier, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&&);

#if PLATFORM(IOS_FAMILY)
    RefPtr<WebCore::PlatformVideoPresentationInterface> returningToStandbyInterface() const;
    AVPlayerViewController *playerViewController(PlaybackSessionContextIdentifier) const;
    RetainPtr<WKVideoView> createViewWithID(PlaybackSessionContextIdentifier, const WebCore::HostingContext&, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& nativeSize, float hostingScaleFactor);
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
    WKSPlayableViewControllerHost *playableViewController(PlaybackSessionContextIdentifier) const;
#endif

    PlatformLayerContainer createLayerWithID(PlaybackSessionContextIdentifier, const WebCore::HostingContext&, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& nativeSize, float hostingScaleFactor);

    void willRemoveLayerForID(PlaybackSessionContextIdentifier);
    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess(IPC::Connection&) const;

    void swapFullscreenModes(PlaybackSessionContextIdentifier, PlaybackSessionContextIdentifier);

    RefPtr<WebCore::PlatformVideoPresentationInterface> bestVideoForElementFullscreen();

private:
    friend class RemotePageVideoPresentationManagerProxy;
    friend class VideoPresentationModelContext;

    explicit VideoPresentationManagerProxy(WebPageProxy&, PlaybackSessionManagerProxy&);
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    template <typename Message> void sendToWebProcess(PlaybackSessionContextIdentifier, Message&&);

    typedef std::pair<Ref<VideoPresentationModelContext>, Ref<WebCore::PlatformVideoPresentationInterface>> ModelInterfacePair;
    ModelInterfacePair createModelAndInterface(PlaybackSessionContextIdentifier);
    const ModelInterfacePair& ensureModelAndInterface(PlaybackSessionContextIdentifier);
    const ModelInterfacePair* findModelAndInterface(PlaybackSessionContextIdentifier) const;
    Ref<VideoPresentationModelContext> ensureModel(PlaybackSessionContextIdentifier);
    Ref<WebCore::PlatformVideoPresentationInterface> ensureInterface(PlaybackSessionContextIdentifier);
    RefPtr<WebCore::PlatformVideoPresentationInterface> findInterface(PlaybackSessionContextIdentifier) const;
    void ensureClientForContext(PlaybackSessionContextIdentifier);
    void addClientForContext(PlaybackSessionContextIdentifier);
    void removeClientForContext(PlaybackSessionContextIdentifier);
    void invalidateInterface(WebCore::PlatformVideoPresentationInterface&);

    void hasVideoInPictureInPictureDidChange(bool);

    RetainPtr<WKLayerHostView> createLayerHostViewWithID(PlaybackSessionContextIdentifier, const WebCore::HostingContext&, const WebCore::FloatSize& initialSize, float hostingScaleFactor);

#if USE(EXTENSIONKIT)
    void setVisibilityPropagationViewForLayerHostView(UIView *, WKLayerHostView *);
#endif

    // Messages from VideoPresentationManager
    void setupFullscreenWithID(PlaybackSessionContextIdentifier, const WebCore::HostingContext&, const WebCore::FloatRect& screenRect, const WebCore::FloatSize& initialSize, const WebCore::FloatSize& videoDimensions, float hostingScaleFactor, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool allowsPictureInPicture, bool standby, bool blocksReturnToFullscreenFromPictureInPicture);
    void setInlineRect(PlaybackSessionContextIdentifier, const WebCore::FloatRect& inlineRect, bool visible);
    void setHasVideoContentLayer(PlaybackSessionContextIdentifier, bool value);
    void setHasVideo(PlaybackSessionContextIdentifier, bool);
    void setDocumentVisibility(PlaybackSessionContextIdentifier, bool);
    void setIsChildOfElementFullscreen(PlaybackSessionContextIdentifier, bool);
    void audioSessionCategoryChanged(PlaybackSessionContextIdentifier, WebCore::AudioSessionCategory, WebCore::AudioSessionMode, WebCore::RouteSharingPolicy);
    void routingContextUIDChanged(PlaybackSessionContextIdentifier, const String&);
    void hasBeenInteractedWith(PlaybackSessionContextIdentifier);
    void setVideoDimensions(PlaybackSessionContextIdentifier, const WebCore::FloatSize&);
    void enterFullscreen(PlaybackSessionContextIdentifier);
    void exitFullscreen(PlaybackSessionContextIdentifier, WebCore::FloatRect finalRect, CompletionHandler<void(bool)>&&);
    void cleanupFullscreen(PlaybackSessionContextIdentifier);
    void preparedToReturnToInline(PlaybackSessionContextIdentifier, bool visible, WebCore::FloatRect inlineRect);
    void preparedToExitFullscreen(PlaybackSessionContextIdentifier);
    void exitFullscreenWithoutAnimationToMode(PlaybackSessionContextIdentifier, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void setVideoFullscreenMode(PlaybackSessionContextIdentifier, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void clearVideoFullscreenMode(PlaybackSessionContextIdentifier, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void setPlayerIdentifier(PlaybackSessionContextIdentifier, std::optional<WebCore::MediaPlayerIdentifier>);
    void textTrackRepresentationUpdate(PlaybackSessionContextIdentifier, WebCore::ShareableBitmap::Handle&& textTrack);
    void textTrackRepresentationSetContentsScale(PlaybackSessionContextIdentifier, float scale);
    void textTrackRepresentationSetHidden(PlaybackSessionContextIdentifier, bool hidden);
    void setRequiresTextTrackRepresentation(PlaybackSessionContextIdentifier, bool);
    void setTextTrackRepresentationBounds(PlaybackSessionContextIdentifier, const WebCore::IntRect&);

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    void requestShowCaptionDisplaySettingsPreview(PlaybackSessionContextIdentifier);
    void requestHideCaptionDisplaySettingsPreview(PlaybackSessionContextIdentifier);
    void performCaptionDisplaySettingsAction(PlaybackSessionContextIdentifier, Function<void(WebPageProxy&, const FrameInfoData&, WebCore::HTMLMediaElementIdentifier)>&& action);
#endif

    // Messages to VideoPresentationManager
    void requestFullscreenMode(PlaybackSessionContextIdentifier, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool finishedWithMedia = false);
    void requestUpdateInlineRect(PlaybackSessionContextIdentifier);
    void requestVideoContentLayer(PlaybackSessionContextIdentifier);
    void returnVideoContentLayer(PlaybackSessionContextIdentifier);
    void returnVideoView(PlaybackSessionContextIdentifier);
    void didSetupFullscreen(PlaybackSessionContextIdentifier);
    void willExitFullscreen(PlaybackSessionContextIdentifier);
    void didExitFullscreen(PlaybackSessionContextIdentifier);
    void failedToEnterFullscreen(PlaybackSessionContextIdentifier);
    void didEnterFullscreen(PlaybackSessionContextIdentifier, const WebCore::FloatSize&);
    void didCleanupFullscreen(PlaybackSessionContextIdentifier);
    void setVideoLayerFrame(PlaybackSessionContextIdentifier, WebCore::FloatRect);
    void setVideoLayerGravity(PlaybackSessionContextIdentifier, WebCore::MediaPlayerEnums::VideoGravity);
    void setVideoFullscreenFrame(PlaybackSessionContextIdentifier, WebCore::FloatRect);
    void fullscreenModeChanged(PlaybackSessionContextIdentifier, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void fullscreenMayReturnToInline(PlaybackSessionContextIdentifier);
#if ENABLE(LINEAR_MEDIA_PLAYER)
    void didEnterExternalPlayback(PlaybackSessionContextIdentifier);
    void didExitExternalPlayback(PlaybackSessionContextIdentifier);
#endif

    void requestCloseAllMediaPresentations(PlaybackSessionContextIdentifier, bool finishedWithMedia, CompletionHandler<void()>&&);
    void callCloseCompletionHandlers();

    void videosInElementFullscreenChanged();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const;
    uint64_t logIdentifier() const;
    ASCIILiteral logClassName() const;
    WTFLogChannel& logChannel() const;
#endif

    bool m_mockVideoPresentationModeEnabled { false };
    WebCore::FloatSize m_mockPictureInPictureWindowSize { DefaultMockPictureInPictureWindowWidth, DefaultMockPictureInPictureWindowHeight };

    WeakPtr<WebPageProxy> m_page;
    const Ref<PlaybackSessionManagerProxy> m_playbackSessionManagerProxy;
    HashMap<PlaybackSessionContextIdentifier, ModelInterfacePair> m_contextMap;
    HashMap<PlaybackSessionContextIdentifier, int> m_clientCounts;
    Vector<CompletionHandler<void()>> m_closeCompletionHandlers;
    WeakHashSet<VideoInPictureInPictureDidChangeObserver> m_pipChangeObservers;
    Markable<PlaybackSessionContextIdentifier> m_lastInteractedWithVideo;
};

} // namespace WebKit

#endif // ENABLE(VIDEO_PRESENTATION_MODE)
