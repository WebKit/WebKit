/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && ENABLE(VIDEO)

#include "Connection.h"
#if PLATFORM(COCOA)
#include "LayerHostingContextManager.h"
#endif
#include "MessageReceiver.h"
#include "RemoteAudioVideoRendererIdentifier.h"
#include "RemoteAudioVideoRendererState.h"
#include "RemoteCDMInstanceIdentifier.h"
#include "RemoteLegacyCDMSessionIdentifier.h"
#include "RemoteVideoFrameProxy.h"
#include <WebCore/AudioVideoRenderer.h>
#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/MediaPlayerEnums.h>
#include <WebCore/MediaPromiseTypes.h>
#include <wtf/Forward.h>
#include <wtf/Logger.h>
#include <wtf/MediaTime.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class AudioVideoRenderer;
struct MediaPlayerIdentifierType;
using MediaPlayerIdentifier = ObjectIdentifier<MediaPlayerIdentifierType>;
class TextTrackRepresentation;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteVideoFrameObjectHeap;
struct SharedPreferencesForWebProcess;

class RemoteAudioVideoRendererProxyManager final : public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteAudioVideoRendererProxyManager);
public:
    RemoteAudioVideoRendererProxyManager(GPUConnectionToWebProcess&);
    ~RemoteAudioVideoRendererProxyManager();

    void ref() const final;
    void deref() const final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    bool allowsExitUnderMemoryPressure() { return m_renderers.isEmpty(); }

    std::optional<SharedPreferencesForWebProcess> sharedPreferencesForWebProcess() const;

    RefPtr<WebCore::AudioVideoRenderer> rendererFor(std::optional<WebCore::MediaPlayerIdentifier>) const;

private:
    // Messages
    void create(RemoteAudioVideoRendererIdentifier, WebCore::HTMLMediaElementIdentifier, WebCore::MediaPlayerIdentifier);
    void shutdown(RemoteAudioVideoRendererIdentifier);

    void setPreferences(RemoteAudioVideoRendererIdentifier, WebCore::VideoRendererPreferences);
    void setHasProtectedVideoContent(RemoteAudioVideoRendererIdentifier, bool);

    // TracksRendererInterface
    using TrackIdentifier = WebCore::AudioVideoRenderer::TrackIdentifier;

    void addTrack(RemoteAudioVideoRendererIdentifier, WebCore::TrackInfo::TrackType, CompletionHandler<void(Expected<TrackIdentifier, WebCore::PlatformMediaError>)>&&);
    void removeTrack(RemoteAudioVideoRendererIdentifier, TrackIdentifier);

    void enqueueSample(RemoteAudioVideoRendererIdentifier, TrackIdentifier, WebCore::MediaSamplesBlock&&, std::optional<MediaTime>, CompletionHandler<void(bool)>&&);
    void requestMediaDataWhenReady(RemoteAudioVideoRendererIdentifier, TrackIdentifier);
    void stopRequestingMediaData(RemoteAudioVideoRendererIdentifier, TrackIdentifier);

    void notifyTimeReachedAndStall(RemoteAudioVideoRendererIdentifier, const MediaTime&);
    void cancelTimeReachedAction(RemoteAudioVideoRendererIdentifier);
    void performTaskAtTime(RemoteAudioVideoRendererIdentifier, const MediaTime&);

    void flush(RemoteAudioVideoRendererIdentifier);
    void flushTrack(RemoteAudioVideoRendererIdentifier, TrackIdentifier);

    void applicationWillResignActive(RemoteAudioVideoRendererIdentifier);
    void setSpatialTrackingInfo(RemoteAudioVideoRendererIdentifier, bool, WebCore::MediaPlayerSoundStageSize, const String&, const String&, const String&);

    void notifyWhenErrorOccurs(RemoteAudioVideoRendererIdentifier, CompletionHandler<void(WebCore::PlatformMediaError)>&&);

    // SynchronizerInterface
    void play(RemoteAudioVideoRendererIdentifier, std::optional<MonotonicTime>);
    void pause(RemoteAudioVideoRendererIdentifier, std::optional<MonotonicTime>);
    void setRate(RemoteAudioVideoRendererIdentifier, double);
    void stall(RemoteAudioVideoRendererIdentifier);
    void prepareToSeek(RemoteAudioVideoRendererIdentifier);
    void seekTo(RemoteAudioVideoRendererIdentifier, const MediaTime&, CompletionHandler<void(WebCore::MediaTimePromise::Result&&)>&&);

    // AudioInterface
    void setVolume(RemoteAudioVideoRendererIdentifier, float);
    void setMuted(RemoteAudioVideoRendererIdentifier, bool);
    void setPreservesPitchAndCorrectionAlgorithm(RemoteAudioVideoRendererIdentifier, bool, std::optional<WebCore::MediaPlayerPitchCorrectionAlgorithm>);
#if HAVE(AUDIO_OUTPUT_DEVICE_UNIQUE_ID)
    void setOutputDeviceId(RemoteAudioVideoRendererIdentifier, const String&);
#endif

    // VideoInterface
    void setIsVisible(RemoteAudioVideoRendererIdentifier, bool);
    void setPresentationSize(RemoteAudioVideoRendererIdentifier, const WebCore::IntSize&);
    void setShouldMaintainAspectRatio(RemoteAudioVideoRendererIdentifier, bool);
    void renderingCanBeAcceleratedChanged(RemoteAudioVideoRendererIdentifier, bool);
    void contentBoxRectChanged(RemoteAudioVideoRendererIdentifier, const WebCore::LayoutRect&);
    void notifyWhenHasAvailableVideoFrame(RemoteAudioVideoRendererIdentifier, bool);
    void expectMinimumUpcomingPresentationTime(RemoteAudioVideoRendererIdentifier, const MediaTime&);
    void setShouldDisableHDR(RemoteAudioVideoRendererIdentifier, bool);
    void setPlatformDynamicRangeLimit(RemoteAudioVideoRendererIdentifier, const WebCore::PlatformDynamicRangeLimit&);
    void setResourceOwner(RemoteAudioVideoRendererIdentifier, const WebCore::ProcessIdentity& resourceOwner);
    void flushAndRemoveImage(RemoteAudioVideoRendererIdentifier);
    void currentVideoFrame(RemoteAudioVideoRendererIdentifier, CompletionHandler<void(std::optional<RemoteVideoFrameProxy::Properties>)>&&) const;

    // VideoFullscreenInterface
#if ENABLE(VIDEO_PRESENTATION_MODE)
    void setVideoFullscreenFrame(RemoteAudioVideoRendererIdentifier, const WebCore::FloatRect&);
    void isInFullscreenOrPictureInPictureChanged(RemoteAudioVideoRendererIdentifier, bool);
#endif
    void setTextTrackRepresentation(RemoteAudioVideoRendererIdentifier, WebCore::TextTrackRepresentation*);
    void syncTextTrackBounds(RemoteAudioVideoRendererIdentifier);

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void setLegacyCDMSession(RemoteAudioVideoRendererIdentifier, std::optional<RemoteLegacyCDMSessionIdentifier>);
#endif
#if ENABLE(ENCRYPTED_MEDIA)
    void setCDMInstance(RemoteAudioVideoRendererIdentifier, std::optional<RemoteCDMInstanceIdentifier>);
    void setInitData(RemoteAudioVideoRendererIdentifier, Ref<WebCore::SharedBuffer>, CompletionHandler<void(Expected<void, WebCore::PlatformMediaError>)>&&);
    void attemptToDecrypt(RemoteAudioVideoRendererIdentifier);
#endif

    struct RendererContext {
        RefPtr<WebCore::AudioVideoRenderer> renderer;
        Markable<WebCore::HTMLMediaElementIdentifier> mediaElementIdentifier;
        Markable<WebCore::MediaPlayerIdentifier> playerIdentifier;
#if PLATFORM(COCOA)
        LayerHostingContextManager layerHostingContextManager;
#endif
        WebCore::VideoRendererPreferences preferences { };
    };
    RefPtr<WebCore::AudioVideoRenderer> createRenderer();
    RefPtr<WebCore::AudioVideoRenderer> rendererFor(RemoteAudioVideoRendererIdentifier) const;
    RemoteAudioVideoRendererState stateFor(RemoteAudioVideoRendererIdentifier) const;
    RendererContext& contextFor(RemoteAudioVideoRendererIdentifier);
    void rendereringModeChanged(RemoteAudioVideoRendererIdentifier);
    using LayerHostingContextCallback = CompletionHandler<void(WebCore::HostingContext)>;
    void requestHostingContext(RemoteAudioVideoRendererIdentifier, LayerHostingContextCallback&&);

#if PLATFORM(COCOA)
    void setVideoLayerSizeFenced(RemoteAudioVideoRendererIdentifier, const WebCore::FloatSize&, WTF::MachSendRightAnnotated&&);
#endif

#if !RELEASE_LOG_DISABLED
    Logger& logger() { return m_logger; }
    WTFLogChannel& logChannel() const;
    ASCIILiteral logClassName() const { return "RemoteAudioVideoRendererProxyManager"; }
    uint64_t logIdentifier() const { return m_logIdentifier; }
#endif

    HashMap<RemoteAudioVideoRendererIdentifier, RendererContext> m_renderers;
    const Ref<RemoteVideoFrameObjectHeap> m_videoFrameObjectHeap;

    ThreadSafeWeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
#if !RELEASE_LOG_DISABLED
    uint64_t m_logIdentifier { 0 };
    const Ref<Logger> m_logger;
#endif
};

}

#endif
