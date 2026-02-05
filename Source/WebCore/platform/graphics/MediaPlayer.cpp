/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
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
#include "MediaPlayer.h"

#if ENABLE(VIDEO)

#include "CommonAtomStrings.h"
#include "ContentType.h"
#include "DeprecatedGlobalSettings.h"
#include "FourCC.h"
#include "GraphicsContext.h"
#include "ImmersiveVideoMetadata.h"
#include "InbandTextTrackPrivate.h"
#include "IntRect.h"
#include "LegacyCDMSession.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayerPrivate.h"
#include "MediaStrategy.h"
#include "MessageClientForTesting.h"
#include "OriginAccessPatterns.h"
#include "PlatformMediaResourceLoader.h"
#include "PlatformMediaSessionManager.h"
#include "PlatformScreen.h"
#include "PlatformStrategies.h"
#include "PlatformTextTrack.h"
#include "PlatformTimeRanges.h"
#include "ResourceError.h"
#include "SecurityOrigin.h"
#include "ShareableBitmap.h"
#include "VideoFrame.h"
#include "VideoFrameMetadata.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/Identified.h>
#include <wtf/Lock.h>
#include <wtf/NativePromise.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

#if ENABLE(MEDIA_SOURCE)
#include "MediaSourcePrivateClient.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "MediaStreamPrivate.h"
#endif

#if USE(GSTREAMER)
#include "MediaPlayerPrivateGStreamer.h"
#if ENABLE(MEDIA_SOURCE)
#include "MediaPlayerPrivateGStreamerMSE.h"
#endif
#endif // USE(GSTREAMER)

#if USE(MEDIA_FOUNDATION)
#include "MediaPlayerPrivateMediaFoundation.h"
#endif

#if PLATFORM(COCOA)

#if USE(AVFOUNDATION)
#include "MediaPlayerPrivateAVFoundationObjC.h"
#include "MediaSessionManagerCocoa.h"
#endif

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
#include "MediaPlayerPrivateMediaSourceAVFObjC.h"
#endif

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)
#include "MediaPlayerPrivateMediaStreamAVFObjC.h"
#endif

#if ENABLE(COCOA_WEBM_PLAYER)
#include "MediaPlayerPrivateWebM.h"
#endif

#endif // PLATFORM(COCOA)

#if USE(EXTERNAL_HOLEPUNCH)
#include "MediaPlayerPrivateHolePunch.h"
#endif

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
#include "MediaDeviceRouteController.h"
#include "MediaPlayerPrivateWirelessPlayback.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaPlayer);
WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaPlayerFactory);

// a null player to make MediaPlayer logic simpler

class NullMediaPlayerPrivate final : public MediaPlayerPrivateInterface, public ThreadSafeRefCounted<NullMediaPlayerPrivate, WTF::DestructionThread::Main> {
public:
    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    static Ref<NullMediaPlayerPrivate> create(MediaPlayer& player) { return adoptRef(*new NullMediaPlayerPrivate(player)); }

    constexpr MediaPlayerType mediaPlayerType() const final { return MediaPlayerType::Null; }

    void load(const String&) final { }
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const LoadOptions&, MediaSourcePrivateClient&) final { }
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) final { }
#endif
    void cancelLoad() final { }

    void prepareToPlay() final { }
    void play() final { }
    void pause() final { }

    String engineDescription() const final { return "NullMediaPlayer"_s; }

    PlatformLayer* platformLayer() const final { return nullptr; }
    FloatSize naturalSize() const final { return FloatSize(); }

    bool hasVideo() const final { return false; }
    bool hasAudio() const final { return false; }

    void setPageIsVisible(bool) final { }

    void seekToTarget(const SeekTarget&) final { }
    bool seeking() const final { return false; }

    void setRateDouble(double) final { }
    void setPreservesPitch(bool) final { }
    bool paused() const final { return true; }

    void setVolumeDouble(double) final { }

    void setMuted(bool) final { }

    bool hasClosedCaptions() const final { return false; }
    void setClosedCaptionsVisible(bool) final { };

    MediaPlayer::NetworkState networkState() const final { return MediaPlayer::NetworkState::Empty; }
    MediaPlayer::ReadyState readyState() const final { return MediaPlayer::ReadyState::HaveNothing; }

    const PlatformTimeRanges& buffered() const final { return PlatformTimeRanges::emptyRanges(); }

    double seekableTimeRangesLastModifiedTime() const final { return 0; }
    double liveUpdateInterval() const final { return 0; }

    unsigned long long totalBytes() const final { return 0; }
    bool didLoadingProgress() const final { return false; }

    void setPresentationSize(const IntSize&) final { }

    void paint(GraphicsContext&, const FloatRect&) final { }
    DestinationColorSpace colorSpace() final { return DestinationColorSpace::SRGB(); }
private:
    explicit NullMediaPlayerPrivate(MediaPlayer&) { }
};

#if !RELEASE_LOG_DISABLED
static RefPtr<Logger>& nullLogger()
{
    static NeverDestroyed<RefPtr<Logger>> logger;
    return logger;
}
#endif

static const Vector<WebCore::ContentType>& nullContentTypeVector()
{
    static NeverDestroyed<Vector<WebCore::ContentType>> vector;
    return vector;
}

static const std::optional<Vector<String>>& nullOptionalStringVector()
{
    static NeverDestroyed<std::optional<Vector<String>>> vector;
    return vector;
}

static const std::optional<Vector<FourCC>>& nullOptionalFourCCVector()
{
    static NeverDestroyed<std::optional<Vector<FourCC>>> vector;
    return vector;
}

class NullMediaPlayerClient final
    : public MediaPlayerClient
    , public RefCounted<NullMediaPlayerClient>
    , public Identified<MediaPlayerClientIdentifier> {
public:
    static Ref<NullMediaPlayerClient> create()
    {
        return adoptRef(*new NullMediaPlayerClient);
    }

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    NullMediaPlayerClient() = default;

#if !RELEASE_LOG_DISABLED
    const Logger& mediaPlayerLogger() final
    {
        if (!nullLogger().get()) {
            nullLogger() = Logger::create(this);
            nullLogger()->setEnabled(this, false);
        }

        return *nullLogger().get();
    }
#endif

    const Vector<WebCore::ContentType>& mediaContentTypesRequiringHardwareSupport() const final { return nullContentTypeVector(); }

    Ref<PlatformMediaResourceLoader> mediaPlayerCreateResourceLoader() final
    {
        ASSERT_NOT_REACHED();
        return adoptRef(*new NullMediaResourceLoader());
    }

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<ArrayBuffer> mediaPlayerCachedKeyForKeyId(const String&) const final { return nullptr; }
#endif

    const std::optional<Vector<String>>& allowedMediaContainerTypes() const final { return nullOptionalStringVector(); }
    const std::optional<Vector<String>>& allowedMediaCodecTypes() const final { return nullOptionalStringVector(); }
    const std::optional<Vector<FourCC>>& allowedMediaVideoCodecIDs() const final { return nullOptionalFourCCVector(); }
    const std::optional<Vector<FourCC>>& allowedMediaAudioCodecIDs() const final { return nullOptionalFourCCVector(); }
    const std::optional<Vector<FourCC>>& allowedMediaCaptionFormatTypes() const final { return nullOptionalFourCCVector(); }

    MediaPlayerClientIdentifier mediaPlayerClientIdentifier() const final { return identifier(); }

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    MediaPlaybackTargetType playbackTargetType() const final { return MediaPlaybackTargetType::None; }
#endif

    class NullMediaResourceLoader final
        : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<NullMediaResourceLoader>
        , public PlatformMediaResourceLoader {
        WTF_MAKE_TZONE_ALLOCATED_INLINE(NullMediaResourceLoader);
    public:
        void ref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::ref(); }
        void deref() const final { ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::deref(); }
        ThreadSafeWeakPtrControlBlock& controlBlock() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::controlBlock(); }
        uint32_t weakRefCount() const final { return ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr::weakRefCount(); }
    private:
        void sendH2Ping(const URL&, CompletionHandler<void(Expected<Seconds, ResourceError>&&)>&& completionHandler) final
        {
            completionHandler(makeUnexpected(ResourceError { }));
        }
        RefPtr<PlatformMediaResource> requestResource(ResourceRequest&&, LoadOptions) final { return nullptr; }
    };
};

const Vector<ContentType>& MediaPlayerClient::mediaContentTypesRequiringHardwareSupport() const
{
    static NeverDestroyed<Vector<ContentType>> contentTypes;
    return contentTypes;
}

static MediaPlayerClient& nullMediaPlayerClient()
{
    static NeverDestroyed<Ref<NullMediaPlayerClient>> client = NullMediaPlayerClient::create();
    return client.get();
}

// engine support

static void addMediaEngine(std::unique_ptr<MediaPlayerFactory>&&);

static Lock mediaEngineVectorLock;

static bool& haveMediaEnginesVector() WTF_REQUIRES_LOCK(mediaEngineVectorLock)
{
    static bool haveVector;
    return haveVector;
}

static Vector<std::unique_ptr<MediaPlayerFactory>>& mutableInstalledMediaEnginesVector()
{
    static NeverDestroyed<Vector<std::unique_ptr<MediaPlayerFactory>>> installedEngines;
    return installedEngines;
}

static RemoteMediaPlayerSupport::RegisterRemotePlayerCallback& registerRemotePlayerCallback()
{
    static NeverDestroyed<RemoteMediaPlayerSupport::RegisterRemotePlayerCallback> callback;
    return callback;
}

void RemoteMediaPlayerSupport::setRegisterRemotePlayerCallback(RegisterRemotePlayerCallback&& callback)
{
    registerRemotePlayerCallback() = WTF::move(callback);
}

static void buildMediaEnginesVector() WTF_REQUIRES_LOCK(mediaEngineVectorLock)
{
    ASSERT(mediaEngineVectorLock.isLocked());

#if USE(AVFOUNDATION)
    auto& registerRemoteEngine = registerRemotePlayerCallback();
#if ENABLE(MEDIA_SOURCE)
    bool useMSERemoteRenderer = hasPlatformStrategies() && platformStrategies()->mediaStrategy()->hasRemoteRendererFor(MediaPlayerMediaEngineIdentifier::AVFoundationMSE);
    if (!useMSERemoteRenderer && registerRemoteEngine && platformStrategies()->mediaStrategy()->mockMediaSourceEnabled())
        registerRemoteEngine(addMediaEngine, MediaPlayerEnums::MediaEngineIdentifier::MockMSE);
#endif

    if (DeprecatedGlobalSettings::isAVFoundationEnabled()) {
        if (registerRemoteEngine)
            registerRemoteEngine(addMediaEngine, MediaPlayerEnums::MediaEngineIdentifier::AVFoundation);
        else
            MediaPlayerPrivateAVFoundationObjC::registerMediaEngine(addMediaEngine);

#if ENABLE(MEDIA_SOURCE)
        if (registerRemoteEngine && !useMSERemoteRenderer)
            registerRemoteEngine(addMediaEngine, MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE);
        else
            MediaPlayerPrivateMediaSourceAVFObjC::registerMediaEngine(addMediaEngine);
#endif

#if ENABLE(COCOA_WEBM_PLAYER)
        bool useRemoteRenderer = hasPlatformStrategies() && platformStrategies()->mediaStrategy()->hasRemoteRendererFor(MediaPlayerMediaEngineIdentifier::CocoaWebM);
        if (!hasPlatformStrategies() || platformStrategies()->mediaStrategy()->enableWebMMediaPlayer()) {
            if (registerRemoteEngine && !useRemoteRenderer)
                registerRemoteEngine(addMediaEngine, MediaPlayerEnums::MediaEngineIdentifier::CocoaWebM);
            else
                MediaPlayerPrivateWebM::registerMediaEngine(addMediaEngine);
        }
#endif

#if ENABLE(MEDIA_STREAM)
        MediaPlayerPrivateMediaStreamAVFObjC::registerMediaEngine(addMediaEngine);
#endif
    }
#endif // USE(AVFOUNDATION)

#if USE(GSTREAMER)
    if (DeprecatedGlobalSettings::isGStreamerEnabled()) {
        MediaPlayerPrivateGStreamer::registerMediaEngine(addMediaEngine);
#if ENABLE(MEDIA_SOURCE)
        MediaPlayerPrivateGStreamerMSE::registerMediaEngine(addMediaEngine);
#endif
    }
#endif

#if USE(MEDIA_FOUNDATION)
    MediaPlayerPrivateMediaFoundation::registerMediaEngine(addMediaEngine);
#endif

#if USE(EXTERNAL_HOLEPUNCH)
    MediaPlayerPrivateHolePunch::registerMediaEngine(addMediaEngine);
#endif

#if ENABLE(WIRELESS_PLAYBACK_MEDIA_PLAYER)
    if (!hasPlatformStrategies() || platformStrategies()->mediaStrategy()->wirelessPlaybackMediaPlayerEnabled()) {
        if (registerRemoteEngine && !mockMediaDeviceRouteControllerEnabled())
            registerRemoteEngine(addMediaEngine, MediaPlayerEnums::MediaEngineIdentifier::WirelessPlayback);
        else
            MediaPlayerPrivateWirelessPlayback::registerMediaEngine(addMediaEngine);
    }
#endif

    haveMediaEnginesVector() = true;
}

static const Vector<std::unique_ptr<MediaPlayerFactory>>& installedMediaEngines()
{
    {
        Locker locker { mediaEngineVectorLock };
        if (!haveMediaEnginesVector())
            buildMediaEnginesVector();
    }

    return mutableInstalledMediaEnginesVector();
}

static void addMediaEngine(std::unique_ptr<MediaPlayerFactory>&& factory)
{
    mutableInstalledMediaEnginesVector().append(WTF::move(factory));
}

static String applicationOctetStream()
{
    if (isMainThread())
        return applicationOctetStreamAtom();
    return String { "application/octet-stream"_s };
}

const MediaPlayerPrivateInterface* MediaPlayer::playerPrivate() const
{
    return m_private.get();
}

MediaPlayerPrivateInterface* MediaPlayer::playerPrivate()
{
    return m_private.get();
}

RefPtr<MediaPlayerPrivateInterface> MediaPlayer::protectedPlayerPrivate()
{
    return m_private.get();
}

const MediaPlayerFactory* MediaPlayer::mediaEngine(MediaPlayerEnums::MediaEngineIdentifier identifier)
{
    auto& engines = installedMediaEngines();
    auto currentIndex = engines.findIf([identifier] (auto& engine) {
        return engine->identifier() == identifier;
    });

    if (currentIndex == notFound) {
#if PLATFORM(IOS_FAMILY_SIMULATOR)
        ASSERT(identifier == MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE);
#endif
        return nullptr;
    }

    return engines[currentIndex].get();
}

static const MediaPlayerFactory* bestMediaEngineForSupportParameters(const MediaEngineSupportParameters& parameters, const WeakHashSet<const MediaPlayerFactory>& attemptedEngines = { }, const MediaPlayerFactory* current = nullptr)
{
    if (parameters.type.isEmpty() && !parameters.isMediaSource && !parameters.isMediaStream)
        return nullptr;

    // 4.8.10.3 MIME types - In the absence of a specification to the contrary, the MIME type "application/octet-stream"
    // when used with parameters, e.g. "application/octet-stream;codecs=theora", is a type that the user agent knows 
    // it cannot render.
    if (parameters.type.containerType() == applicationOctetStream()) {
        if (!parameters.type.codecs().isEmpty())
            return nullptr;
    }

    const MediaPlayerFactory* foundEngine = nullptr;
    MediaPlayer::SupportsType supported = MediaPlayer::SupportsType::IsNotSupported;
    for (auto& engine : installedMediaEngines()) {
        if (current) {
            if (current == engine.get())
                current = nullptr;
            continue;
        }
        if (attemptedEngines.contains(*engine))
            continue;
        MediaPlayer::SupportsType engineSupport = engine->supportsTypeAndCodecs(parameters);
        if (engineSupport > supported) {
            supported = engineSupport;
            foundEngine = engine.get();
        }
    }

    return foundEngine;
}

CheckedPtr<const MediaPlayerFactory> MediaPlayer::nextMediaEngine(const MediaPlayerFactory* current)
{
    if (m_activeEngineIdentifier) {
        CheckedPtr engine = mediaEngine(m_activeEngineIdentifier.value());
        return current != engine ? engine : nullptr;
    }

    auto& engines = installedMediaEngines();
    if (engines.isEmpty())
        return nullptr;

    if (!current) 
        return engines.first().get();

    auto currentIndex = engines.findIf([current] (auto& engine) {
        return engine.get() == current;
    });
    if (currentIndex == notFound) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    if (currentIndex + 1 >= engines.size())
        return nullptr;

    CheckedPtr nextEngine = engines[currentIndex + 1].get();
    
    if (m_attemptedEngines.contains(*nextEngine))
        return nextMediaEngine(nextEngine.get());
    
    return nextEngine;
}

// media player

Ref<MediaPlayer> MediaPlayer::create(MediaPlayerClient& client)
{
    return adoptRef(*new MediaPlayer(client));
}

Ref<MediaPlayer> MediaPlayer::create(MediaPlayerClient& client, MediaPlayerEnums::MediaEngineIdentifier mediaEngineIdentifier)
{
    return adoptRef(*new MediaPlayer(client, mediaEngineIdentifier));
}

MediaPlayer::MediaPlayer(MediaPlayerClient& client)
    : m_client(client)
    , m_reloadTimer(*this, &MediaPlayer::reloadTimerFired)
    , m_private(NullMediaPlayerPrivate::create(*this))
    , m_preferredDynamicRangeMode(DynamicRangeMode::Standard)
{
}

MediaPlayer::MediaPlayer(MediaPlayerClient& client, MediaPlayerEnums::MediaEngineIdentifier mediaEngineIdentifier)
    : m_client(client)
    , m_reloadTimer(*this, &MediaPlayer::reloadTimerFired)
    , m_private(NullMediaPlayerPrivate::create(*this))
    , m_activeEngineIdentifier(mediaEngineIdentifier)
    , m_preferredDynamicRangeMode(DynamicRangeMode::Standard)
{
}

MediaPlayer::~MediaPlayer()
{
    ASSERT(!m_initializingMediaEngine);
    if (RefPtr privatePtr = m_private)
        privatePtr->mediaPlayerWillBeDestroyed();
}

void MediaPlayer::invalidate()
{
    m_client = nullMediaPlayerClient();
}

bool MediaPlayer::load(const URL& url, const LoadOptions& options)
{
    ASSERT(!m_reloadTimer.isActive());

    // Protect against MediaPlayer being destroyed during a MediaPlayerClient callback.
    Ref<MediaPlayer> protectedThis(*this);

    m_url = url;
    m_loadOptions = options;

#if ENABLE(MEDIA_SOURCE)
    m_mediaSource = nullptr;
#endif
#if ENABLE(MEDIA_STREAM)
    m_mediaStream = nullptr;
#endif

    loadWithNextMediaEngine(nullptr);
    return static_cast<bool>(m_currentMediaEngine);
}

#if ENABLE(MEDIA_SOURCE)
bool MediaPlayer::load(const URL& url, const LoadOptions& options, MediaSourcePrivateClient& mediaSource)
{
    ASSERT(!m_reloadTimer.isActive());

    m_mediaSource = mediaSource;
    m_url = url;
    m_loadOptions = options;
#if USE(AVFOUNDATION)
    if (DeprecatedGlobalSettings::isAVFoundationEnabled() && hasPlatformStrategies() && platformStrategies()->mediaStrategy()->hasRemoteRendererFor(MediaPlayerMediaEngineIdentifier::AVFoundationMSE))
        m_activeEngineIdentifier = MediaPlayerMediaEngineIdentifier::AVFoundationMSE;
#endif
    loadWithNextMediaEngine(nullptr);
    return static_cast<bool>(m_currentMediaEngine);
}
#endif // ENABLE(MEDIA_SOURCE)

#if ENABLE(MEDIA_STREAM)
bool MediaPlayer::load(MediaStreamPrivate& mediaStream)
{
    ASSERT(!m_reloadTimer.isActive());

    m_mediaStream = mediaStream;
    m_loadOptions = { };
    loadWithNextMediaEngine(nullptr);
    return static_cast<bool>(m_currentMediaEngine);
}
#endif

CheckedPtr<const MediaPlayerFactory> MediaPlayer::nextBestMediaEngine(const MediaPlayerFactory* current)
{
    MediaEngineSupportParameters parameters;
    parameters.type = m_loadOptions.contentType;
    parameters.url = m_url;
#if ENABLE(MEDIA_SOURCE)
    parameters.isMediaSource = !!m_mediaSource.get();
#endif
#if ENABLE(MEDIA_STREAM)
    parameters.isMediaStream = !!m_mediaStream;
#endif
    parameters.supportsLimitedMatroska = m_loadOptions.supportsLimitedMatroska;
    parameters.allowedMediaContainerTypes = allowedMediaContainerTypes();
    parameters.allowedMediaCodecTypes = allowedMediaCodecTypes();
    parameters.allowedMediaVideoCodecIDs = allowedMediaVideoCodecIDs();
    parameters.allowedMediaAudioCodecIDs = allowedMediaAudioCodecIDs();
    parameters.allowedMediaCaptionFormatTypes = allowedMediaCaptionFormatTypes();
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    parameters.playbackTargetType = playbackTargetType();
#endif

    if (m_activeEngineIdentifier) {
        if (current)
            return nullptr;

        CheckedPtr engine = mediaEngine(m_activeEngineIdentifier.value());
        if (engine && engine->supportsTypeAndCodecs(parameters) != SupportsType::IsNotSupported)
            return engine;

        return nullptr;
    }

    return bestMediaEngineForSupportParameters(parameters, m_attemptedEngines, current);
}

void MediaPlayer::reloadAndResumePlaybackIfNeeded()
{
    protectedClient()->mediaPlayerReloadAndResumePlaybackIfNeeded();
}

void MediaPlayer::loadWithNextMediaEngine(const MediaPlayerFactory* current)
{

#if ENABLE(MEDIA_SOURCE)
#define MEDIASOURCE m_mediaSource.get()
#else
#define MEDIASOURCE 0
#endif

#if ENABLE(MEDIA_STREAM)
#define MEDIASTREAM m_mediaStream
#else
#define MEDIASTREAM 0
#endif

    ASSERT(!m_initializingMediaEngine);
    m_initializingMediaEngine = true;
    protectedClient()->mediaPlayerWillInitializeMediaEngine();

    CheckedPtr<const MediaPlayerFactory> engine;

    if (!contentType().isEmpty() || MEDIASTREAM || MEDIASOURCE)
        engine = nextBestMediaEngine(current);

    // Exhaust all remaining engines
    if (!engine)
        engine = nextMediaEngine(current);

    // Don't delete and recreate the player unless it comes from a different engine.
    if (!engine) {
        LOG(Media, "MediaPlayer::loadWithNextMediaEngine - no media engine found for type \"%s\"", contentType().raw().utf8().data());
        m_currentMediaEngine = engine.get();
        m_private = nullptr;
    } else if (m_currentMediaEngine.get() != engine.get()) {
        m_currentMediaEngine = engine.get();
        m_attemptedEngines.add(*engine);
        RefPtr playerPrivate = engine->createMediaEnginePlayer(*this);
        m_private = playerPrivate;
        if (playerPrivate) {
            protectedClient()->mediaPlayerEngineUpdated();
            playerPrivate->setMessageClientForTesting(m_internalMessageClient);

            if (m_pageIsVisible)
                playerPrivate->setPageIsVisible(m_pageIsVisible);
            if (m_visibleInViewport)
                playerPrivate->setVisibleInViewport(m_visibleInViewport);
            if (m_isGatheringVideoFrameMetadata)
                playerPrivate->startVideoFrameMetadataGathering();
            if (m_processIdentity)
                playerPrivate->setResourceOwner(m_processIdentity);
#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
            if (m_shouldContinueAfterKeyNeeded)
                playerPrivate->setShouldContinueAfterKeyNeeded(m_shouldContinueAfterKeyNeeded);
#endif
            playerPrivate->prepareForPlayback(m_inPrivateBrowsingMode, m_preload, m_preservesPitch, m_shouldPrepareToPlay, m_shouldPrepareToRender);
#if HAVE(SPATIAL_TRACKING_LABEL)
            playerPrivate->setDefaultSpatialTrackingLabel(m_defaultSpatialTrackingLabel);
            playerPrivate->setSpatialTrackingLabel(m_spatialTrackingLabel);
#endif
        }
    }

    Ref client = this->client();
    if (RefPtr playerPrivate = m_private) {
        playerPrivate->setShouldCheckHardwareSupport(client->mediaPlayerShouldCheckHardwareSupport());

#if ENABLE(MEDIA_SOURCE)
        if (RefPtr mediaSource = m_mediaSource.get())
            playerPrivate->load(m_url, m_loadOptions, *mediaSource);
        else
#endif
#if ENABLE(MEDIA_STREAM)
        if (RefPtr mediaStream = m_mediaStream)
            playerPrivate->load(*mediaStream);
        else
#endif
        playerPrivate->load(m_url, m_loadOptions);
    } else {
        m_private = NullMediaPlayerPrivate::create(*this);
        CheckedPtr currentMediaEngine = m_currentMediaEngine.get();
        if (!m_activeEngineIdentifier
            && installedMediaEngines().size() > 1
            && (nextBestMediaEngine(currentMediaEngine.get()) || nextMediaEngine(currentMediaEngine.get())))
            m_reloadTimer.startOneShot(0_s);
        else {
            client->mediaPlayerEngineUpdated();
            client->mediaPlayerResourceNotSupported();
        }
    }

    m_initializingMediaEngine = false;
    client->mediaPlayerDidInitializeMediaEngine();
}

void MediaPlayer::queueTaskOnEventLoop(Function<void()>&& task)
{
    ASSERT(isMainThread());
    protectedClient()->mediaPlayerQueueTaskOnEventLoop(WTF::move(task));
}

bool MediaPlayer::hasAvailableVideoFrame() const
{
    return protectedPrivate()->hasAvailableVideoFrame();
}

void MediaPlayer::prepareForRendering()
{
    m_shouldPrepareToRender = true;
    protectedPrivate()->prepareForRendering();
}

void MediaPlayer::cancelLoad()
{
    protectedPrivate()->cancelLoad();
}    

void MediaPlayer::prepareToPlay()
{
    Ref<MediaPlayer> protectedThis(*this);

    m_shouldPrepareToPlay = true;
    protectedPrivate()->prepareToPlay();
}

void MediaPlayer::play()
{
    protectedPrivate()->play();
}

void MediaPlayer::pause()
{
    protectedPrivate()->pause();
}

void MediaPlayer::setBufferingPolicy(BufferingPolicy policy)
{
    protectedPrivate()->setBufferingPolicy(policy);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

RefPtr<LegacyCDMSession> MediaPlayer::createSession(const String& keySystem, LegacyCDMSessionClient& client)
{
    return protectedPrivate()->createSession(keySystem, client);
}

void MediaPlayer::setCDM(LegacyCDM* cdm)
{
    protectedPrivate()->setCDM(cdm);
}

void MediaPlayer::setCDMSession(LegacyCDMSession* session)
{
    protectedPrivate()->setCDMSession(session);
}

void MediaPlayer::keyAdded()
{
    protectedPrivate()->keyAdded();
}

#endif
    
#if ENABLE(ENCRYPTED_MEDIA)

void MediaPlayer::cdmInstanceAttached(CDMInstance& instance)
{
    protectedPrivate()->cdmInstanceAttached(instance);
}

void MediaPlayer::cdmInstanceDetached(CDMInstance& instance)
{
    protectedPrivate()->cdmInstanceDetached(instance);
}

void MediaPlayer::attemptToDecryptWithInstance(CDMInstance& instance)
{
    protectedPrivate()->attemptToDecryptWithInstance(instance);
}

#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
void MediaPlayer::setShouldContinueAfterKeyNeeded(bool should)
{
    m_shouldContinueAfterKeyNeeded = should;
    protectedPrivate()->setShouldContinueAfterKeyNeeded(m_shouldContinueAfterKeyNeeded);
}
#endif

MediaTime MediaPlayer::duration() const
{
    return protectedPrivate()->duration();
}

MediaTime MediaPlayer::startTime() const
{
    return protectedPrivate()->startTime();
}

MediaTime MediaPlayer::initialTime() const
{
    return protectedPrivate()->initialTime();
}

MediaTime MediaPlayer::currentTime() const
{
    return protectedPrivate()->currentTime();
}

bool MediaPlayer::timeIsProgressing() const
{
    return protectedPrivate()->timeIsProgressing();
}

bool MediaPlayer::setCurrentTimeDidChangeCallback(CurrentTimeDidChangeCallback&& callback)
{
    return protectedPrivate()->setCurrentTimeDidChangeCallback(WTF::move(callback));
}

MediaTime MediaPlayer::getStartDate() const
{
    return protectedPrivate()->getStartDate();
}

void MediaPlayer::willSeekToTarget(const MediaTime& time)
{
    protectedPrivate()->willSeekToTarget(time);
}

void MediaPlayer::seekToTarget(const SeekTarget& target)
{
    RefPtr playerPrivate = m_private;
    playerPrivate->willSeekToTarget(MediaTime::invalidTime());
    playerPrivate->seekToTarget(target);
}

void MediaPlayer::seekToTime(const MediaTime& time)
{
    seekToTarget(SeekTarget { time });
}

void MediaPlayer::seekWhenPossible(const MediaTime& time)
{
    if (protectedPrivate()->readyState() < MediaPlayer::ReadyState::HaveMetadata)
        m_pendingSeekRequest = time;
    else
        seekToTime(time);
}

void MediaPlayer::seeked(const MediaTime& time)
{
    protectedClient()->mediaPlayerSeeked(time);
}

bool MediaPlayer::paused() const
{
    return protectedPrivate()->paused();
}

bool MediaPlayer::seeking() const
{
    return protectedPrivate()->seeking();
}

bool MediaPlayer::supportsFullscreen() const
{
    return protectedPrivate()->supportsFullscreen();
}

bool MediaPlayer::canSaveMediaData() const
{
    return protectedPrivate()->canSaveMediaData();
}

bool MediaPlayer::supportsScanning() const
{
    return protectedPrivate()->supportsScanning();
}

bool MediaPlayer::supportsProgressMonitoring() const
{
    return protectedPrivate()->supportsProgressMonitoring();
}

bool MediaPlayer::requiresImmediateCompositing() const
{
    return protectedPrivate()->requiresImmediateCompositing();
}

FloatSize MediaPlayer::naturalSize()
{
    return protectedPrivate()->naturalSize();
}

bool MediaPlayer::hasVideo() const
{
    return protectedPrivate()->hasVideo();
}

bool MediaPlayer::hasAudio() const
{
    return protectedPrivate()->hasAudio();
}

PlatformLayer* MediaPlayer::platformLayer() const
{
    return protectedPrivate()->platformLayer();
}
    
#if ENABLE(VIDEO_PRESENTATION_MODE)

RetainPtr<PlatformLayer> MediaPlayer::createVideoFullscreenLayer()
{
    return protectedPrivate()->createVideoFullscreenLayer();
}

void MediaPlayer::setVideoFullscreenLayer(PlatformLayer* layer, Function<void()>&& completionHandler)
{
    protectedPrivate()->setVideoFullscreenLayer(layer, WTF::move(completionHandler));
}

void MediaPlayer::updateVideoFullscreenInlineImage()
{
    protectedPrivate()->updateVideoFullscreenInlineImage();
}

void MediaPlayer::setVideoFullscreenFrame(const FloatRect& frame)
{
    protectedPrivate()->setVideoFullscreenFrame(frame);
}

void MediaPlayer::setVideoFullscreenGravity(MediaPlayer::VideoGravity gravity)
{
    protectedPrivate()->setVideoFullscreenGravity(gravity);
}

void MediaPlayer::setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode mode)
{
    protectedPrivate()->setVideoFullscreenMode(mode);
}

MediaPlayer::VideoFullscreenMode MediaPlayer::fullscreenMode() const
{
    return protectedClient()->mediaPlayerFullscreenMode();
}

void MediaPlayer::videoFullscreenStandbyChanged()
{
    protectedPrivate()->videoFullscreenStandbyChanged();
}

bool MediaPlayer::isVideoFullscreenStandby() const
{
    return protectedClient()->mediaPlayerIsVideoFullscreenStandby();
}

#endif

FloatSize MediaPlayer::videoLayerSize() const
{
    return protectedClient()->mediaPlayerVideoLayerSize();
}

#if PLATFORM(IOS_FAMILY)
bool MediaPlayer::canShowWhileLocked() const
{
    return protectedClient()->canShowWhileLocked();
}

void MediaPlayer::setSceneIdentifier(const String& identifier)
{
    if (m_sceneIdentifier == identifier)
        return;
    m_sceneIdentifier = identifier;
    protectedPrivate()->sceneIdentifierDidChange();
}
#endif

void MediaPlayer::videoLayerSizeDidChange(const FloatSize& size)
{
    protectedClient()->mediaPlayerVideoLayerSizeDidChange(size);
}

void MediaPlayer::setVideoLayerSizeFenced(const FloatSize& size, WTF::MachSendRightAnnotated&& fence)
{
    protectedPrivate()->setVideoLayerSizeFenced(size, WTF::move(fence));
}

#if PLATFORM(IOS_FAMILY)

NSArray* MediaPlayer::timedMetadata() const
{
    return protectedPrivate()->timedMetadata();
}

String MediaPlayer::accessLog() const
{
    return protectedPrivate()->accessLog();
}

String MediaPlayer::errorLog() const
{
    return protectedPrivate()->errorLog();
}

#endif

MediaPlayer::NetworkState MediaPlayer::networkState()
{
    return protectedPrivate()->networkState();
}

MediaPlayer::ReadyState MediaPlayer::readyState() const
{
    return protectedPrivate()->readyState();
}

void MediaPlayer::setVolumeLocked(bool volumeLocked)
{
    protectedPrivate()->setVolumeLocked(volumeLocked);
}

double MediaPlayer::volume() const
{
    return m_volume;
}

void MediaPlayer::setVolume(double volume)
{
    m_volume = volume;
    protectedPrivate()->setVolumeDouble(volume);
}

bool MediaPlayer::muted() const
{
    return m_muted;
}

void MediaPlayer::setMuted(bool muted)
{
    m_muted = muted;

    protectedPrivate()->setMuted(muted);
}

bool MediaPlayer::hasClosedCaptions() const
{
    return protectedPrivate()->hasClosedCaptions();
}

void MediaPlayer::setClosedCaptionsVisible(bool closedCaptionsVisible)
{
    protectedPrivate()->setClosedCaptionsVisible(closedCaptionsVisible);
}

double MediaPlayer::rate() const
{
    return protectedPrivate()->rate();
}

void MediaPlayer::setRate(double rate)
{
    protectedPrivate()->setRateDouble(rate);
}

double MediaPlayer::effectiveRate() const
{
    return protectedPrivate()->effectiveRate();
}

double MediaPlayer::requestedRate() const
{
    return protectedClient()->mediaPlayerRequestedPlaybackRate();
}

bool MediaPlayer::preservesPitch() const
{
    return m_preservesPitch;
}

void MediaPlayer::setPreservesPitch(bool preservesPitch)
{
    m_preservesPitch = preservesPitch;
    protectedPrivate()->setPreservesPitch(preservesPitch);
}

void MediaPlayer::setPitchCorrectionAlgorithm(PitchCorrectionAlgorithm pitchCorrectionAlgorithm)
{
    if (m_pitchCorrectionAlgorithm == pitchCorrectionAlgorithm)
        return;

    m_pitchCorrectionAlgorithm = pitchCorrectionAlgorithm;
    protectedPrivate()->setPitchCorrectionAlgorithm(pitchCorrectionAlgorithm);
}

RefPtr<MediaPlayerPrivateInterface> MediaPlayer::protectedPrivate() const
{
    return m_private;
}

const PlatformTimeRanges& MediaPlayer::buffered() const
{
    return protectedPrivate()->buffered();
}

const PlatformTimeRanges& MediaPlayer::seekable() const
{
    return protectedPrivate()->seekable();
}

MediaTime MediaPlayer::maxTimeSeekable() const
{
    return protectedPrivate()->maxTimeSeekable();
}

MediaTime MediaPlayer::minTimeSeekable() const
{
    return protectedPrivate()->minTimeSeekable();
}

double MediaPlayer::seekableTimeRangesLastModifiedTime()
{
    return protectedPrivate()->seekableTimeRangesLastModifiedTime();
}

void MediaPlayer::bufferedTimeRangesChanged()
{
    protectedClient()->mediaPlayerBufferedTimeRangesChanged();
}

void MediaPlayer::seekableTimeRangesChanged()
{
    protectedClient()->mediaPlayerSeekableTimeRangesChanged();
}

double MediaPlayer::liveUpdateInterval()
{
    return protectedPrivate()->liveUpdateInterval();
}

void MediaPlayer::didLoadingProgress(DidLoadingProgressCompletionHandler&& callback) const
{
    protectedPrivate()->didLoadingProgressAsync(WTF::move(callback));
}

void MediaPlayer::setPresentationSize(const IntSize& size)
{
    if (m_presentationSize == size)
        return;

    m_presentationSize = size;
    protectedPrivate()->setPresentationSize(size);
}

void MediaPlayer::setPageIsVisible(bool visible)
{
    m_pageIsVisible = visible;
    protectedPrivate()->setPageIsVisible(visible);
}

void MediaPlayer::setVisibleForCanvas(bool visible)
{
    if (visible == m_visibleForCanvas)
        return;

    m_visibleForCanvas = visible;
    protectedPrivate()->setVisibleForCanvas(visible);
}

void MediaPlayer::setVisibleInViewport(bool visible)
{
    if (visible == m_visibleInViewport)
        return;

    m_visibleInViewport = visible;
    protectedPrivate()->setVisibleInViewport(visible);
}

void MediaPlayer::setResourceOwner(const ProcessIdentity& processIdentity)
{
    m_processIdentity = processIdentity;
    protectedPrivate()->setResourceOwner(processIdentity);
}

MediaPlayer::Preload MediaPlayer::preload() const
{
    return m_preload;
}

void MediaPlayer::setPreload(MediaPlayer::Preload preload)
{
    m_preload = preload;
    protectedPrivate()->setPreload(preload);
}

void MediaPlayer::paint(GraphicsContext& context, const FloatRect& destination)
{
    protectedPrivate()->paint(context, destination);
}

void MediaPlayer::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& destination)
{
    protectedPrivate()->paintCurrentFrameInContext(context, destination);
}

RefPtr<VideoFrame> MediaPlayer::videoFrameForCurrentTime()
{
    return protectedPrivate()->videoFrameForCurrentTime();
}

RefPtr<NativeImage> MediaPlayer::nativeImageForCurrentTime()
{
    return protectedPrivate()->nativeImageForCurrentTime();
}

RefPtr<ShareableBitmap> MediaPlayer::bitmapImageForCurrentTimeSync()
{
    return protectedPrivate()->bitmapImageForCurrentTimeSync();
}

Ref<MediaPlayer::BitmapImagePromise> MediaPlayer::bitmapImageForCurrentTime()
{
    return protectedPrivate()->bitmapImageForCurrentTime();
}

DestinationColorSpace MediaPlayer::colorSpace()
{
    return protectedPrivate()->colorSpace();
}

bool MediaPlayer::shouldGetNativeImageForCanvasDrawing() const
{
    return protectedPrivate()->shouldGetNativeImageForCanvasDrawing();
}

MediaPlayer::SupportsType MediaPlayer::supportsType(const MediaEngineSupportParameters& parameters)
{
    // 4.8.10.3 MIME types - The canPlayType(type) method must return the empty string if type is a type that the 
    // user agent knows it cannot render or is the type "application/octet-stream"
    AtomString containerType { parameters.type.containerType() };
    if (containerType == applicationOctetStream())
        return SupportsType::IsNotSupported;

    if (!startsWithLettersIgnoringASCIICase(containerType, "video/"_s) && !startsWithLettersIgnoringASCIICase(containerType, "audio/"_s) && !startsWithLettersIgnoringASCIICase(containerType, "application/"_s))
        return SupportsType::IsNotSupported;

    CheckedPtr engine = bestMediaEngineForSupportParameters(parameters);
    if (!engine)
        return SupportsType::IsNotSupported;

    return engine->supportsTypeAndCodecs(parameters);
}

void MediaPlayer::getSupportedTypes(HashSet<String>& types)
{
    for (auto& engine : installedMediaEngines()) {
        HashSet<String> engineTypes;
        engine->getSupportedTypes(engineTypes);
        types.addAll(WTF::move(engineTypes));
    }
} 

bool MediaPlayer::isAvailable()
{
#if PLATFORM(IOS_FAMILY)
    if (DeprecatedGlobalSettings::isAVFoundationEnabled())
        return true;
#endif
    return !installedMediaEngines().isEmpty();
}

bool MediaPlayer::supportsPictureInPicture() const
{
    return protectedPrivate()->supportsPictureInPicture();
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

bool MediaPlayer::isCurrentPlaybackTargetWireless() const
{
    return protectedPrivate()->isCurrentPlaybackTargetWireless();
}

String MediaPlayer::wirelessPlaybackTargetName() const
{
    return protectedPrivate()->wirelessPlaybackTargetName();
}

MediaPlayer::WirelessPlaybackTargetType MediaPlayer::wirelessPlaybackTargetType() const
{
    return protectedPrivate()->wirelessPlaybackTargetType();
}

bool MediaPlayer::wirelessVideoPlaybackDisabled() const
{
    return protectedPrivate()->wirelessVideoPlaybackDisabled();
}

void MediaPlayer::setWirelessVideoPlaybackDisabled(bool disabled)
{
    protectedPrivate()->setWirelessVideoPlaybackDisabled(disabled);
}

void MediaPlayer::currentPlaybackTargetIsWirelessChanged(bool isCurrentPlaybackTargetWireless)
{
    protectedClient()->mediaPlayerCurrentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless);
}

OptionSet<MediaPlaybackTargetType> MediaPlayer::supportedPlaybackTargetTypes() const
{
    return protectedPrivate()->supportedPlaybackTargetTypes();
}

void MediaPlayer::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& device)
{
    protectedPrivate()->setWirelessPlaybackTarget(WTF::move(device));
}

void MediaPlayer::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    protectedPrivate()->setShouldPlayToPlaybackTarget(shouldPlay);
}

#endif

double MediaPlayer::maxFastForwardRate() const
{
    return protectedPrivate()->maxFastForwardRate();
}

double MediaPlayer::minFastReverseRate() const
{
    return protectedPrivate()->minFastReverseRate();
}

void MediaPlayer::acceleratedRenderingStateChanged()
{
    protectedPrivate()->acceleratedRenderingStateChanged();
}

bool MediaPlayer::supportsAcceleratedRendering() const
{
    return protectedPrivate()->supportsAcceleratedRendering();
}

void MediaPlayer::setShouldMaintainAspectRatio(bool maintainAspectRatio)
{
    protectedPrivate()->setShouldMaintainAspectRatio(maintainAspectRatio);
}

void MediaPlayer::requestHostingContext(LayerHostingContextCallback&& callback)
{
    return protectedPrivate()->requestHostingContext(WTF::move(callback));
}

HostingContext MediaPlayer::hostingContext() const
{
    return protectedPrivate()->hostingContext();
}

bool MediaPlayer::didPassCORSAccessCheck() const
{
    return protectedPrivate()->didPassCORSAccessCheck();
}

bool MediaPlayer::isCrossOrigin(const SecurityOrigin& origin) const
{
    if (auto crossOrigin = protectedPrivate()->isCrossOrigin(origin))
        return *crossOrigin;

    if (m_url.protocolIsData())
        return false;

    return !origin.canRequest(m_url, originAccessPatternsForWebProcessOrEmpty());
}

MediaPlayer::MovieLoadType MediaPlayer::movieLoadType() const
{
    return protectedPrivate()->movieLoadType();
}

MediaTime MediaPlayer::mediaTimeForTimeValue(const MediaTime& timeValue) const
{
    return protectedPrivate()->mediaTimeForTimeValue(timeValue);
}

unsigned MediaPlayer::decodedFrameCount() const
{
    return protectedPrivate()->decodedFrameCount();
}

unsigned MediaPlayer::droppedFrameCount() const
{
    return protectedPrivate()->droppedFrameCount();
}

unsigned MediaPlayer::audioDecodedByteCount() const
{
    return protectedPrivate()->audioDecodedByteCount();
}

unsigned MediaPlayer::videoDecodedByteCount() const
{
    return protectedPrivate()->videoDecodedByteCount();
}

void MediaPlayer::reloadTimerFired()
{
    protectedPrivate()->cancelLoad();
    loadWithNextMediaEngine(CheckedPtr { m_currentMediaEngine.get() });
}

template<typename T>
static void addToHash(HashSet<T>& toHash, HashSet<T>&& fromHash)
{
    if (toHash.isEmpty())
        toHash = WTF::move(fromHash);
    else
        toHash.addAll(WTF::move(fromHash));
}
    
HashSet<SecurityOriginData> MediaPlayer::originsInMediaCache(const String& path)
{
    HashSet<SecurityOriginData> origins;
    for (auto& engine : installedMediaEngines())
        addToHash(origins, engine->originsInMediaCache(path));

    return origins;
}

void MediaPlayer::clearMediaCache(const String& path, WallTime modifiedSince)
{
    for (auto& engine : installedMediaEngines())
        engine->clearMediaCache(path, modifiedSince);
}

void MediaPlayer::clearMediaCacheForOrigins(const String& path, const HashSet<SecurityOriginData>& origins)
{
    for (auto& engine : installedMediaEngines())
        engine->clearMediaCacheForOrigins(path, origins);
}

bool MediaPlayer::supportsKeySystem(const String& keySystem, const String& mimeType)
{
    for (auto& engine : installedMediaEngines()) {
        if (engine->supportsKeySystem(keySystem, mimeType))
            return true;
    }
    return false;
}

void MediaPlayer::setPrivateBrowsingMode(bool privateBrowsingMode)
{
    m_inPrivateBrowsingMode = privateBrowsingMode;
    if (RefPtr privateInterface = m_private)
        privateInterface->setPrivateBrowsingMode(m_inPrivateBrowsingMode);
}

// Client callbacks.
void MediaPlayer::networkStateChanged()
{
    RefPtr playerPrivate = m_private;
    if (playerPrivate->networkState() >= MediaPlayer::NetworkState::FormatError)
        m_lastErrorMessage = playerPrivate->errorMessage();
    Ref client = this->client();
    // If more than one media engine is installed and this one failed before finding metadata,
    // let the next engine try.
    if (playerPrivate->networkState() >= MediaPlayer::NetworkState::FormatError && playerPrivate->readyState() < MediaPlayer::ReadyState::HaveMetadata) {
        client->mediaPlayerEngineFailedToLoad();
        CheckedPtr currentMediaEngine = m_currentMediaEngine.get();
        if (!m_activeEngineIdentifier
            && installedMediaEngines().size() > 1
            && (nextBestMediaEngine(currentMediaEngine.get()) || nextMediaEngine(currentMediaEngine.get()))) {
            m_reloadTimer.startOneShot(0_s);
            return;
        }
    }
    client->mediaPlayerNetworkStateChanged();
}

void MediaPlayer::readyStateChanged()
{
    protectedClient()->mediaPlayerReadyStateChanged();
    if (m_pendingSeekRequest && protectedPrivate()->readyState() == MediaPlayer::ReadyState::HaveMetadata)
        seekToTime(*std::exchange(m_pendingSeekRequest, std::nullopt));
}

void MediaPlayer::volumeChanged(double newVolume)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(newVolume);
    m_volume = protectedPrivate()->volume();
#else
    m_volume = newVolume;
#endif
    protectedClient()->mediaPlayerVolumeChanged();
}

void MediaPlayer::muteChanged(bool newMuted)
{
    if (newMuted == m_muted)
        return;

    m_muted = newMuted;
    protectedClient()->mediaPlayerMuteChanged();
}

void MediaPlayer::timeChanged()
{
    protectedClient()->mediaPlayerTimeChanged();
}

void MediaPlayer::sizeChanged()
{
    protectedClient()->mediaPlayerSizeChanged();
}

void MediaPlayer::repaint()
{
    protectedClient()->mediaPlayerRepaint();
}

void MediaPlayer::durationChanged()
{
    protectedClient()->mediaPlayerDurationChanged();
}

void MediaPlayer::rateChanged()
{
    protectedClient()->mediaPlayerRateChanged();
}

void MediaPlayer::playbackStateChanged()
{
    protectedClient()->mediaPlayerPlaybackStateChanged();
}

void MediaPlayer::firstVideoFrameAvailable()
{
    protectedClient()->mediaPlayerFirstVideoFrameAvailable();
}

void MediaPlayer::characteristicChanged()
{
    protectedClient()->mediaPlayerCharacteristicChanged();
}

#if ENABLE(WEB_AUDIO)

AudioSourceProvider* MediaPlayer::audioSourceProvider()
{
    return protectedPrivate()->audioSourceProvider();
}

#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

RefPtr<ArrayBuffer> MediaPlayer::cachedKeyForKeyId(const String& keyId) const
{
    return protectedClient()->mediaPlayerCachedKeyForKeyId(keyId);
}

void MediaPlayer::keyNeeded(const SharedBuffer& initData)
{
    protectedClient()->mediaPlayerKeyNeeded(initData);
}

String MediaPlayer::mediaKeysStorageDirectory() const
{
    return protectedClient()->mediaPlayerMediaKeysStorageDirectory();
}

#endif

#if ENABLE(ENCRYPTED_MEDIA)

void MediaPlayer::initializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
    protectedClient()->mediaPlayerInitializationDataEncountered(initDataType, WTF::move(initData));
}

void MediaPlayer::waitingForKeyChanged()
{
    protectedClient()->mediaPlayerWaitingForKeyChanged();
}

bool MediaPlayer::waitingForKey() const
{
    if (!m_private)
        return false;
    return protectedPrivate()->waitingForKey();
}
#endif

String MediaPlayer::referrer() const
{
    return protectedClient()->mediaPlayerReferrer();
}

String MediaPlayer::userAgent() const
{
    return protectedClient()->mediaPlayerUserAgent();
}

String MediaPlayer::engineDescription() const
{
    RefPtr playerPrivate = m_private;
    return playerPrivate ? playerPrivate->engineDescription() : String();
}

long MediaPlayer::platformErrorCode() const
{
    if (!m_private)
        return 0;

    return protectedPrivate()->platformErrorCode();
}

CachedResourceLoader* MediaPlayer::cachedResourceLoader() const
{
    return protectedClient()->mediaPlayerCachedResourceLoader();
}

Ref<PlatformMediaResourceLoader> MediaPlayer::mediaResourceLoader()
{
    if (!m_mediaResourceLoader)
        m_mediaResourceLoader = protectedClient()->mediaPlayerCreateResourceLoader();

    return *m_mediaResourceLoader;
}

void MediaPlayer::addAudioTrack(AudioTrackPrivate& track)
{
    protectedClient()->mediaPlayerDidAddAudioTrack(track);
}

void MediaPlayer::removeAudioTrack(AudioTrackPrivate& track)
{
    protectedClient()->mediaPlayerDidRemoveAudioTrack(track);
}

void MediaPlayer::addTextTrack(InbandTextTrackPrivate& track)
{
    protectedClient()->mediaPlayerDidAddTextTrack(track);
}

void MediaPlayer::removeTextTrack(InbandTextTrackPrivate& track)
{
    protectedClient()->mediaPlayerDidRemoveTextTrack(track);
}

void MediaPlayer::addVideoTrack(VideoTrackPrivate& track)
{
    protectedClient()->mediaPlayerDidAddVideoTrack(track);
}

void MediaPlayer::removeVideoTrack(VideoTrackPrivate& track)
{
    protectedClient()->mediaPlayerDidRemoveVideoTrack(track);
}

void MediaPlayer::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    protectedPrivate()->setTextTrackRepresentation(representation);
}

void MediaPlayer::syncTextTrackBounds()
{
    protectedPrivate()->syncTextTrackBounds();
}

void MediaPlayer::tracksChanged()
{
    protectedPrivate()->tracksChanged();
}

void MediaPlayer::notifyTrackModeChanged()
{
    if (RefPtr privateInterface = m_private)
        privateInterface->notifyTrackModeChanged();
}

Vector<Ref<PlatformTextTrack>> MediaPlayer::outOfBandTrackSources()
{
    return protectedClient()->outOfBandTrackSources();
}

void MediaPlayer::resetMediaEngines()
{
    Locker locker { mediaEngineVectorLock };

    mutableInstalledMediaEnginesVector().clear();
    haveMediaEnginesVector() = false;
}

void MediaPlayer::reset()
{
    m_attemptedEngines.clear();
}

#if USE(GSTREAMER)
void MediaPlayer::simulateAudioInterruption()
{
    if (!m_private)
        return;

    protectedPrivate()->simulateAudioInterruption();
}

bool MediaPlayer::isGStreamerHolePunchingEnabled()
{
    return protectedClient()->isGStreamerHolePunchingEnabled();
}
#endif

String MediaPlayer::languageOfPrimaryAudioTrack() const
{
    RefPtr playerPrivate = m_private;
    return playerPrivate ? playerPrivate->languageOfPrimaryAudioTrack() : emptyString();
}

size_t MediaPlayer::extraMemoryCost() const
{
    RefPtr playerPrivate = m_private;
    return playerPrivate ? playerPrivate->extraMemoryCost() : 0;
}

void MediaPlayer::reportGPUMemoryFootprint(uint64_t footPrint) const
{
    protectedClient()->mediaPlayerDidReportGPUMemoryFootprint(footPrint);
}

unsigned long long MediaPlayer::fileSize() const
{
    if (!m_private)
        return 0;
    
    return protectedPrivate()->fileSize();
}

bool MediaPlayer::ended() const
{
    return protectedPrivate()->ended();
}

std::optional<VideoPlaybackQualityMetrics> MediaPlayer::videoPlaybackQualityMetrics()
{
    RefPtr playerPrivate = m_private;
    if (!playerPrivate)
        return std::nullopt;

    return playerPrivate->videoPlaybackQualityMetrics();
}

Ref<MediaPlayer::VideoPlaybackQualityMetricsPromise> MediaPlayer::asyncVideoPlaybackQualityMetrics()
{
    RefPtr playerPrivate = m_private;
    if (!playerPrivate)
        return VideoPlaybackQualityMetricsPromise::createAndReject(PlatformMediaError::Cancelled);

    return playerPrivate->asyncVideoPlaybackQualityMetrics();
}

String MediaPlayer::sourceApplicationIdentifier() const
{
    return protectedClient()->mediaPlayerSourceApplicationIdentifier();
}

Vector<String> MediaPlayer::preferredAudioCharacteristics() const
{
    return protectedClient()->mediaPlayerPreferredAudioCharacteristics();
}

void MediaPlayerFactorySupport::callRegisterMediaEngine(MediaEngineRegister registerMediaEngine)
{
    registerMediaEngine(addMediaEngine);
}

bool MediaPlayer::doesHaveAttribute(const AtomString& attribute, AtomString* value) const
{
    return protectedClient()->doesHaveAttribute(attribute, value);
}

#if PLATFORM(IOS_FAMILY)
String MediaPlayer::mediaPlayerNetworkInterfaceName() const
{
    return protectedClient()->mediaPlayerNetworkInterfaceName();
}

void MediaPlayer::getRawCookies(const URL& url, MediaPlayerClient::GetRawCookiesCallback&& completionHandler) const
{
    protectedClient()->mediaPlayerGetRawCookies(url, WTF::move(completionHandler));
}
#endif

void MediaPlayer::setShouldDisableSleep(bool flag)
{
    if (RefPtr privateInterface = m_private)
        privateInterface->setShouldDisableSleep(flag);
}

bool MediaPlayer::shouldDisableSleep() const
{
    return protectedClient()->mediaPlayerShouldDisableSleep();
}

String MediaPlayer::contentMIMEType() const
{
    return contentType().containerType();
}

String MediaPlayer::contentTypeCodecs() const
{
    return contentType().parameter(ContentType::codecsParameter());
}

bool MediaPlayer::contentMIMETypeWasInferredFromExtension() const
{
    return contentType().typeWasInferredFromExtension();
}

const Vector<ContentType>& MediaPlayer::mediaContentTypesRequiringHardwareSupport() const
{
    return protectedClient()->mediaContentTypesRequiringHardwareSupport();
}

const std::optional<Vector<String>>& MediaPlayer::allowedMediaContainerTypes() const
{
    return protectedClient()->allowedMediaContainerTypes();
}

const std::optional<Vector<String>>& MediaPlayer::allowedMediaCodecTypes() const
{
    return protectedClient()->allowedMediaCodecTypes();
}

const std::optional<Vector<FourCC>>& MediaPlayer::allowedMediaVideoCodecIDs() const
{
    return protectedClient()->allowedMediaVideoCodecIDs();
}

const std::optional<Vector<FourCC>>& MediaPlayer::allowedMediaAudioCodecIDs() const
{
    return protectedClient()->allowedMediaAudioCodecIDs();
}

const std::optional<Vector<FourCC>>& MediaPlayer::allowedMediaCaptionFormatTypes() const
{
    return protectedClient()->allowedMediaCaptionFormatTypes();
}

void MediaPlayer::applicationWillResignActive()
{
    protectedPrivate()->applicationWillResignActive();
}

void MediaPlayer::applicationDidBecomeActive()
{
    protectedPrivate()->applicationDidBecomeActive();
}

#if USE(AVFOUNDATION)

AVPlayer* MediaPlayer::objCAVFoundationAVPlayer() const
{
    return protectedPrivate()->objCAVFoundationAVPlayer();
}

#endif

bool MediaPlayer::performTaskAtTime(Function<void(const MediaTime&)>&& task, const MediaTime& time)
{
    return protectedPrivate()->performTaskAtTime(WTF::move(task), time);
}

bool MediaPlayer::shouldIgnoreIntrinsicSize()
{
    return protectedPrivate()->shouldIgnoreIntrinsicSize();
}

void MediaPlayer::isLoopingChanged()
{
    protectedPrivate()->isLoopingChanged();
}

void MediaPlayer::remoteEngineFailedToLoad()
{
    protectedClient()->mediaPlayerEngineFailedToLoad();
}

SecurityOriginData MediaPlayer::documentSecurityOrigin() const
{
    return protectedClient()->documentSecurityOrigin();
}

void MediaPlayer::setPreferredDynamicRangeMode(DynamicRangeMode mode)
{
    m_preferredDynamicRangeMode = mode;
    protectedPrivate()->setPreferredDynamicRangeMode(mode);
}

void MediaPlayer::setPlatformDynamicRangeLimit(PlatformDynamicRangeLimit platformDynamicRangeLimit)
{
    if (m_platformDynamicRangeLimit == platformDynamicRangeLimit)
        return;
    m_platformDynamicRangeLimit = platformDynamicRangeLimit;
    protectedPrivate()->setPlatformDynamicRangeLimit(platformDynamicRangeLimit);
}

void MediaPlayer::audioOutputDeviceChanged()
{
    protectedPrivate()->audioOutputDeviceChanged();
}

std::optional<MediaPlayerIdentifier> MediaPlayer::identifier() const
{
    return protectedPrivate()->identifier();
}

std::optional<VideoFrameMetadata> MediaPlayer::videoFrameMetadata()
{
    return protectedPrivate()->videoFrameMetadata();
}

void MediaPlayer::startVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = true;
    protectedPrivate()->startVideoFrameMetadataGathering();
}

void MediaPlayer::stopVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = false;
    protectedPrivate()->stopVideoFrameMetadataGathering();
}

void MediaPlayer::renderVideoWillBeDestroyed()
{
    protectedPrivate()->renderVideoWillBeDestroyed();
}

void MediaPlayer::setShouldDisableHDR(bool shouldDisable)
{
    protectedPrivate()->setShouldDisableHDR(shouldDisable);
}

void MediaPlayer::playerContentBoxRectChanged(const LayoutRect& rect)
{
    protectedPrivate()->playerContentBoxRectChanged(rect);
}

#if PLATFORM(COCOA)
void MediaPlayer::onNewVideoFrameMetadata(VideoFrameMetadata&& metadata, RetainPtr<CVPixelBufferRef>&& buffer)
{
    protectedClient()->mediaPlayerOnNewVideoFrameMetadata(WTF::move(metadata), WTF::move(buffer));
}
#endif

String MediaPlayer::elementId() const
{
    return protectedClient()->mediaPlayerElementId();
}

bool MediaPlayer::supportsPlayAtHostTime() const
{
    return protectedPrivate()->supportsPlayAtHostTime();
}

bool MediaPlayer::supportsPauseAtHostTime() const
{
    return protectedPrivate()->supportsPauseAtHostTime();
}

bool MediaPlayer::playAtHostTime(const MonotonicTime& hostTime)
{
    // It is invalid to call playAtHostTime() if the underlying
    // media player does not support it.
    ASSERT(supportsPlayAtHostTime());
    return protectedPrivate()->playAtHostTime(hostTime);
}

bool MediaPlayer::pauseAtHostTime(const MonotonicTime& hostTime)
{
    // It is invalid to call pauseAtHostTime() if the underlying
    // media player does not support it.
    ASSERT(supportsPauseAtHostTime());
    return protectedPrivate()->pauseAtHostTime(hostTime);
}

void MediaPlayer::setShouldCheckHardwareSupport(bool value)
{
    protectedPrivate()->setShouldCheckHardwareSupport(value);
}

#if HAVE(SPATIAL_TRACKING_LABEL)
String MediaPlayer::defaultSpatialTrackingLabel() const
{
    return m_defaultSpatialTrackingLabel;
}

void MediaPlayer::setDefaultSpatialTrackingLabel(const String& defaultSpatialTrackingLabel)
{
    if (m_defaultSpatialTrackingLabel == defaultSpatialTrackingLabel)
        return;
    m_defaultSpatialTrackingLabel = defaultSpatialTrackingLabel;
    protectedPrivate()->setDefaultSpatialTrackingLabel(defaultSpatialTrackingLabel);
}

String MediaPlayer::spatialTrackingLabel() const
{
    return m_spatialTrackingLabel;
}

void MediaPlayer::setSpatialTrackingLabel(const String& spatialTrackingLabel)
{
    if (m_spatialTrackingLabel == spatialTrackingLabel)
        return;
    m_spatialTrackingLabel = spatialTrackingLabel;
    protectedPrivate()->setSpatialTrackingLabel(spatialTrackingLabel);
}
#endif

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
void MediaPlayer::setPrefersSpatialAudioExperience(bool value)
{
    if (m_prefersSpatialAudioExperience == value)
        return;
    m_prefersSpatialAudioExperience = value;
    protectedPrivate()->prefersSpatialAudioExperienceChanged();
}
#endif

auto MediaPlayer::soundStageSize() const -> SoundStageSize
{
    return protectedClient()->mediaPlayerSoundStageSize();
}

void MediaPlayer::soundStageSizeDidChange()
{
    protectedPrivate()->soundStageSizeDidChange();
}

void MediaPlayer::setInFullscreenOrPictureInPicture(bool isInFullscreenOrPictureInPicture)
{
    if (m_isInFullscreenOrPictureInPicture == isInFullscreenOrPictureInPicture)
        return;

    m_isInFullscreenOrPictureInPicture = isInFullscreenOrPictureInPicture;
    protectedPrivate()->isInFullscreenOrPictureInPictureChanged(isInFullscreenOrPictureInPicture);
}

bool MediaPlayer::isInFullscreenOrPictureInPicture() const
{
    return m_isInFullscreenOrPictureInPicture;
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
bool MediaPlayer::supportsLinearMediaPlayer() const
{
    return protectedPrivate()->supportsLinearMediaPlayer();
}
#endif

#if !RELEASE_LOG_DISABLED
const Logger& MediaPlayer::mediaPlayerLogger()
{
    return protectedClient()->mediaPlayerLogger();
}
#endif

void MediaPlayer::setMessageClientForTesting(WeakPtr<MessageClientForTesting> internalMessageClient)
{
    m_internalMessageClient = WTF::move(internalMessageClient);
    protectedPrivate()->setMessageClientForTesting(m_internalMessageClient);
}

MessageClientForTesting* MediaPlayer::messageClientForTesting() const
{
    return m_internalMessageClient.get();
}

void MediaPlayer::elementIdChanged(const String& id) const
{
    if (RefPtr playerPrivate = m_private)
        playerPrivate->elementIdChanged(id);
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
MediaPlaybackTargetType MediaPlayer::playbackTargetType() const
{
    return protectedClient()->playbackTargetType();
}
#endif

String convertEnumerationToString(MediaPlayer::ReadyState enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 5> values {
        MAKE_STATIC_STRING_IMPL("HaveNothing"),
        MAKE_STATIC_STRING_IMPL("HaveMetadata"),
        MAKE_STATIC_STRING_IMPL("HaveCurrentData"),
        MAKE_STATIC_STRING_IMPL("HaveFutureData"),
        MAKE_STATIC_STRING_IMPL("HaveEnoughData"),
    };
    static_assert(static_cast<size_t>(MediaPlayer::ReadyState::HaveNothing) == 0, "MediaPlayer::ReadyState::HaveNothing is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::ReadyState::HaveMetadata) == 1, "MediaPlayer::ReadyState::HaveMetadata is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::ReadyState::HaveCurrentData) == 2, "MediaPlayer::ReadyState::HaveCurrentData is not 2 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::ReadyState::HaveFutureData) == 3, "MediaPlayer::ReadyState::HaveFutureData is not 3 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::ReadyState::HaveEnoughData) == 4, "MediaPlayer::ReadyState::HaveEnoughData is not 4 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(MediaPlayer::NetworkState enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 7> values {
        MAKE_STATIC_STRING_IMPL("Empty"),
        MAKE_STATIC_STRING_IMPL("Idle"),
        MAKE_STATIC_STRING_IMPL("Loading"),
        MAKE_STATIC_STRING_IMPL("Loaded"),
        MAKE_STATIC_STRING_IMPL("FormatError"),
        MAKE_STATIC_STRING_IMPL("NetworkError"),
        MAKE_STATIC_STRING_IMPL("DecodeError"),
    };
    static_assert(static_cast<size_t>(MediaPlayer::NetworkState::Empty) == 0, "MediaPlayer::NetworkState::Empty is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::NetworkState::Idle) == 1, "MediaPlayer::NetworkState::Idle is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::NetworkState::Loading) == 2, "MediaPlayer::NetworkState::Loading is not 2 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::NetworkState::Loaded) == 3, "MediaPlayer::NetworkState::Loaded is not 3 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::NetworkState::FormatError) == 4, "MediaPlayer::NetworkState::FormatError is not 4 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::NetworkState::NetworkError) == 5, "MediaPlayer::NetworkError is not 5 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::NetworkState::DecodeError) == 6, "MediaPlayer::NetworkState::DecodeError is not 6 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(MediaPlayer::Preload enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 3> values {
        MAKE_STATIC_STRING_IMPL("None"),
        MAKE_STATIC_STRING_IMPL("MetaData"),
        MAKE_STATIC_STRING_IMPL("Auto"),
    };
    static_assert(!static_cast<size_t>(MediaPlayer::Preload::None), "MediaPlayer::Preload::None is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::Preload::MetaData) == 1, "MediaPlayer::Preload::MetaData is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::Preload::Auto) == 2, "MediaPlayer::Preload::Auto is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(MediaPlayer::SupportsType enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 3> values {
        MAKE_STATIC_STRING_IMPL("IsNotSupported"),
        MAKE_STATIC_STRING_IMPL("IsSupported"),
        MAKE_STATIC_STRING_IMPL("MayBeSupported"),
    };
    static_assert(!static_cast<size_t>(MediaPlayer::SupportsType::IsNotSupported), "MediaPlayer::SupportsType::IsNotSupported is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::SupportsType::IsSupported) == 1, "MediaPlayer::SupportsType::IsSupported is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::SupportsType::MayBeSupported) == 2, "MediaPlayer::SupportsType::MayBeSupported is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

WTF::TextStream& operator<<(TextStream& ts, MediaPlayerEnums::VideoGravity gravity)
{
    switch (gravity) {
    case MediaPlayerEnums::VideoGravity::Resize:
        ts << "resize"_s;
        break;
    case MediaPlayerEnums::VideoGravity::ResizeAspect:
        ts << "resize-aspect"_s;
        break;
    case MediaPlayerEnums::VideoGravity::ResizeAspectFill:
        ts << "resize-aspect-fill"_s;
        break;
    }
    return ts;
}

String convertEnumerationToString(MediaPlayer::BufferingPolicy enumerationValue)
{
    static const std::array<NeverDestroyed<String>, 4> values {
        MAKE_STATIC_STRING_IMPL("Default"),
        MAKE_STATIC_STRING_IMPL("LimitReadAhead"),
        MAKE_STATIC_STRING_IMPL("MakeResourcesPurgeable"),
        MAKE_STATIC_STRING_IMPL("PurgeResources"),
    };
    static_assert(!static_cast<size_t>(MediaPlayer::BufferingPolicy::Default), "MediaPlayer::Default is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::BufferingPolicy::LimitReadAhead) == 1, "MediaPlayer::LimitReadAhead is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::BufferingPolicy::MakeResourcesPurgeable) == 2, "MediaPlayer::MakeResourcesPurgeable is not 2 as expected");
    static_assert(static_cast<size_t>(MediaPlayer::BufferingPolicy::PurgeResources) == 3, "MediaPlayer::PurgeResources is not 3 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String MediaPlayer::lastErrorMessage() const
{
    return m_lastErrorMessage;
}

String SeekTarget::toString() const
{
    return makeString('[', WTF::LogArgument<MediaTime>::toString(time),
        WTF::LogArgument<MediaTime>::toString(negativeThreshold),
        WTF::LogArgument<MediaTime>::toString(positiveThreshold), ']');
}

String convertEnumerationToString(VideoProjectionMetadataKind kind)
{
    static const std::array<NeverDestroyed<String>, 8> values {
        MAKE_STATIC_STRING_IMPL("Unknown"),
        MAKE_STATIC_STRING_IMPL("Rectilinear"),
        MAKE_STATIC_STRING_IMPL("Equirectangular"),
        MAKE_STATIC_STRING_IMPL("HalfEquirectangular"),
        MAKE_STATIC_STRING_IMPL("EquiAngularCubemap"),
        MAKE_STATIC_STRING_IMPL("Parametric"),
        MAKE_STATIC_STRING_IMPL("Pyramid"),
        MAKE_STATIC_STRING_IMPL("AppleImmersiveVideo"),
    };
    static_assert(static_cast<size_t>(VideoProjectionMetadataKind::Rectilinear) == 1, "VideoProjectionMetadataKind::Rectilinear is not 1 as expected");
    static_assert(static_cast<size_t>(VideoProjectionMetadataKind::Equirectangular) == 2, "VideoProjectionMetadataKind::Equirectangular is not 2 as expected");
    static_assert(static_cast<size_t>(VideoProjectionMetadataKind::HalfEquirectangular) == 3, "VideoProjectionMetadataKind::HalfEquirectangular is not 3 as expected");
    static_assert(static_cast<size_t>(VideoProjectionMetadataKind::EquiAngularCubemap) == 4, "VideoProjectionMetadataKind::EquiAngularCubemap is not 4 as expected");
    static_assert(static_cast<size_t>(VideoProjectionMetadataKind::Parametric) == 5, "VideoProjectionMetadataKind::Parametric is not 5 as expected");
    static_assert(static_cast<size_t>(VideoProjectionMetadataKind::Pyramid) == 6, "VideoProjectionMetadataKind::Pyramid is not 6 as expected");
    static_assert(static_cast<size_t>(VideoProjectionMetadataKind::AppleImmersiveVideo) == 7, "VideoProjectionMetadataKind::AppleImmersiveVideo is not 7 as expected");
    ASSERT(static_cast<size_t>(kind) < std::size(values));
    return values[static_cast<size_t>(kind)];
}

String convertImmersiveVideoMetadataToString(const ImmersiveVideoMetadata& metadata)
{
    return makeString("ImmersiveVideoMetadata {"_s, convertEnumerationToString(metadata.kind), WTF::LogArgument<WebCore::IntSize>::toString(metadata.size),
        WTF::LogArgument<std::optional<int32_t>>::toString(metadata.horizontalFieldOfView),
        WTF::LogArgument<std::optional<uint32_t>>::toString(metadata.stereoCameraBaseline),
        WTF::LogArgument<std::optional<int32_t>>::toString(metadata.horizontalDisparityAdjustment), " }"_s);
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
