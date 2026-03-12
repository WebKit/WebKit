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

const Vector<ContentType>& MediaPlayerClient::mediaContentTypesRequiringHardwareSupport() const
{
    return nullContentTypeVector();
}

const std::optional<Vector<String>>& MediaPlayerClient::allowedMediaContainerTypes() const
{
    return nullOptionalStringVector();
}

const std::optional<Vector<String>>& MediaPlayerClient::allowedMediaCodecTypes() const
{
    return nullOptionalStringVector();
}

const std::optional<Vector<FourCC>>& MediaPlayerClient::allowedMediaVideoCodecIDs() const
{
    return nullOptionalFourCCVector();
}

const std::optional<Vector<FourCC>>& MediaPlayerClient::allowedMediaAudioCodecIDs() const
{
    return nullOptionalFourCCVector();
}

const std::optional<Vector<FourCC>>& MediaPlayerClient::allowedMediaCaptionFormatTypes() const
{
    return nullOptionalFourCCVector();
}

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

Ref<PlatformMediaResourceLoader> MediaPlayerClient::mediaPlayerCreateResourceLoader()
{
    return *new NullMediaResourceLoader;
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
    MediaPlayerClientIdentifier mediaPlayerClientIdentifier() const final { return identifier(); }
};

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
    if (parameters.type.isEmpty() && parameters.platformType == PlatformMediaDecodingType::FileOrHLS)
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
    MediaEngineSupportParameters parameters {
        .platformType = [&] {
#if ENABLE(MEDIA_SOURCE)
            if (!!m_mediaSource.get())
                return PlatformMediaDecodingType::MediaSource;
#endif
#if ENABLE(MEDIA_STREAM)
            if (!!m_mediaStream)
                return PlatformMediaDecodingType::MediaStream;
#endif
            return PlatformMediaDecodingType::FileOrHLS;
        }(),
        .type = m_loadOptions.contentType,
        .url = m_url,
        .supportsLimitedMatroska = m_loadOptions.supportsLimitedMatroska,
        .allowedMediaContainerTypes = allowedMediaContainerTypes(),
        .allowedMediaCodecTypes = allowedMediaCodecTypes(),
        .allowedMediaVideoCodecIDs = allowedMediaVideoCodecIDs(),
        .allowedMediaAudioCodecIDs = allowedMediaAudioCodecIDs(),
        .allowedMediaCaptionFormatTypes = allowedMediaCaptionFormatTypes(),
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
        .playbackTargetType = playbackTargetType()
#endif
    };

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
    protect(client())->mediaPlayerReloadAndResumePlaybackIfNeeded();
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
    protect(client())->mediaPlayerWillInitializeMediaEngine();

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
            protect(client())->mediaPlayerEngineUpdated();
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
    protect(client())->mediaPlayerQueueTaskOnEventLoop(WTF::move(task));
}

bool MediaPlayer::hasAvailableVideoFrame() const
{
    return protect(m_private)->hasAvailableVideoFrame();
}

void MediaPlayer::prepareForRendering()
{
    m_shouldPrepareToRender = true;
    protect(m_private)->prepareForRendering();
}

void MediaPlayer::cancelLoad()
{
    protect(m_private)->cancelLoad();
}    

void MediaPlayer::prepareToPlay()
{
    Ref<MediaPlayer> protectedThis(*this);

    m_shouldPrepareToPlay = true;
    protect(m_private)->prepareToPlay();
}

void MediaPlayer::play()
{
    protect(m_private)->play();
}

void MediaPlayer::pause()
{
    protect(m_private)->pause();
}

void MediaPlayer::setBufferingPolicy(BufferingPolicy policy)
{
    protect(m_private)->setBufferingPolicy(policy);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

RefPtr<LegacyCDMSession> MediaPlayer::createSession(const String& keySystem, LegacyCDMSessionClient& client)
{
    return protect(m_private)->createSession(keySystem, client);
}

void MediaPlayer::setCDM(LegacyCDM* cdm)
{
    protect(m_private)->setCDM(cdm);
}

void MediaPlayer::setCDMSession(LegacyCDMSession* session)
{
    protect(m_private)->setCDMSession(session);
}

void MediaPlayer::keyAdded()
{
    protect(m_private)->keyAdded();
}

#endif
    
#if ENABLE(ENCRYPTED_MEDIA)

void MediaPlayer::cdmInstanceAttached(CDMInstance& instance)
{
    protect(m_private)->cdmInstanceAttached(instance);
}

void MediaPlayer::cdmInstanceDetached(CDMInstance& instance)
{
    protect(m_private)->cdmInstanceDetached(instance);
}

void MediaPlayer::attemptToDecryptWithInstance(CDMInstance& instance)
{
    protect(m_private)->attemptToDecryptWithInstance(instance);
}

#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
void MediaPlayer::setShouldContinueAfterKeyNeeded(bool should)
{
    m_shouldContinueAfterKeyNeeded = should;
    protect(m_private)->setShouldContinueAfterKeyNeeded(m_shouldContinueAfterKeyNeeded);
}
#endif

MediaTime MediaPlayer::duration() const
{
    return protect(m_private)->duration();
}

MediaTime MediaPlayer::startTime() const
{
    return protect(m_private)->startTime();
}

MediaTime MediaPlayer::initialTime() const
{
    return protect(m_private)->initialTime();
}

MediaTime MediaPlayer::currentTime() const
{
    return protect(m_private)->currentTime();
}

bool MediaPlayer::timeIsProgressing() const
{
    return protect(m_private)->timeIsProgressing();
}

bool MediaPlayer::setCurrentTimeDidChangeCallback(CurrentTimeDidChangeCallback&& callback)
{
    return protect(m_private)->setCurrentTimeDidChangeCallback(WTF::move(callback));
}

MediaTime MediaPlayer::getStartDate() const
{
    return protect(m_private)->getStartDate();
}

void MediaPlayer::willSeekToTarget(const MediaTime& time)
{
    protect(m_private)->willSeekToTarget(time);
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
    if (protect(m_private)->readyState() < MediaPlayer::ReadyState::HaveMetadata)
        m_pendingSeekRequest = time;
    else
        seekToTime(time);
}

void MediaPlayer::seeked(const MediaTime& time)
{
    protect(client())->mediaPlayerSeeked(time);
}

bool MediaPlayer::paused() const
{
    return protect(m_private)->paused();
}

bool MediaPlayer::seeking() const
{
    return protect(m_private)->seeking();
}

bool MediaPlayer::supportsFullscreen() const
{
    return protect(m_private)->supportsFullscreen();
}

bool MediaPlayer::canSaveMediaData() const
{
    return protect(m_private)->canSaveMediaData();
}

bool MediaPlayer::supportsScanning() const
{
    return protect(m_private)->supportsScanning();
}

bool MediaPlayer::supportsProgressMonitoring() const
{
    return protect(m_private)->supportsProgressMonitoring();
}

bool MediaPlayer::requiresImmediateCompositing() const
{
    return protect(m_private)->requiresImmediateCompositing();
}

FloatSize MediaPlayer::naturalSize()
{
    return protect(m_private)->naturalSize();
}

bool MediaPlayer::hasVideo() const
{
    return protect(m_private)->hasVideo();
}

bool MediaPlayer::hasAudio() const
{
    return protect(m_private)->hasAudio();
}

PlatformLayer* MediaPlayer::platformLayer() const
{
    return protect(m_private)->platformLayer();
}
    
#if ENABLE(VIDEO_PRESENTATION_MODE)

RetainPtr<PlatformLayer> MediaPlayer::createVideoFullscreenLayer()
{
    return protect(m_private)->createVideoFullscreenLayer();
}

void MediaPlayer::setVideoFullscreenLayer(PlatformLayer* layer, Function<void()>&& completionHandler)
{
    protect(m_private)->setVideoFullscreenLayer(layer, WTF::move(completionHandler));
}

void MediaPlayer::updateVideoFullscreenInlineImage()
{
    protect(m_private)->updateVideoFullscreenInlineImage();
}

void MediaPlayer::setVideoFullscreenFrame(const FloatRect& frame)
{
    protect(m_private)->setVideoFullscreenFrame(frame);
}

void MediaPlayer::setVideoFullscreenGravity(MediaPlayer::VideoGravity gravity)
{
    protect(m_private)->setVideoFullscreenGravity(gravity);
}

void MediaPlayer::setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode mode)
{
    protect(m_private)->setVideoFullscreenMode(mode);
}

MediaPlayer::VideoFullscreenMode MediaPlayer::fullscreenMode() const
{
    return protect(client())->mediaPlayerFullscreenMode();
}

void MediaPlayer::videoFullscreenStandbyChanged()
{
    protect(m_private)->videoFullscreenStandbyChanged();
}

bool MediaPlayer::isVideoFullscreenStandby() const
{
    return protect(client())->mediaPlayerIsVideoFullscreenStandby();
}

#endif

FloatSize MediaPlayer::videoLayerSize() const
{
    return protect(client())->mediaPlayerVideoLayerSize();
}

#if PLATFORM(IOS_FAMILY)
bool MediaPlayer::canShowWhileLocked() const
{
    return protect(client())->canShowWhileLocked();
}

void MediaPlayer::setSceneIdentifier(const String& identifier)
{
    if (m_sceneIdentifier == identifier)
        return;
    m_sceneIdentifier = identifier;
    protect(m_private)->sceneIdentifierDidChange();
}
#endif

void MediaPlayer::videoLayerSizeDidChange(const FloatSize& size)
{
    protect(client())->mediaPlayerVideoLayerSizeDidChange(size);
}

void MediaPlayer::setVideoLayerSizeFenced(const FloatSize& size, WTF::MachSendRightAnnotated&& fence)
{
    protect(m_private)->setVideoLayerSizeFenced(size, WTF::move(fence));
}

#if PLATFORM(IOS_FAMILY)

NSArray* MediaPlayer::timedMetadata() const
{
    return protect(m_private)->timedMetadata();
}

String MediaPlayer::accessLog() const
{
    return protect(m_private)->accessLog();
}

String MediaPlayer::errorLog() const
{
    return protect(m_private)->errorLog();
}

#endif

MediaPlayer::NetworkState MediaPlayer::networkState()
{
    return protect(m_private)->networkState();
}

MediaPlayer::ReadyState MediaPlayer::readyState() const
{
    return protect(m_private)->readyState();
}

void MediaPlayer::setVolumeLocked(bool volumeLocked)
{
    protect(m_private)->setVolumeLocked(volumeLocked);
}

double MediaPlayer::volume() const
{
    return m_volume;
}

void MediaPlayer::setVolume(double volume)
{
    m_volume = volume;
    protect(m_private)->setVolumeDouble(volume);
}

bool MediaPlayer::muted() const
{
    return m_muted;
}

void MediaPlayer::setMuted(bool muted)
{
    m_muted = muted;

    protect(m_private)->setMuted(muted);
}

bool MediaPlayer::hasClosedCaptions() const
{
    return protect(m_private)->hasClosedCaptions();
}

void MediaPlayer::setClosedCaptionsVisible(bool closedCaptionsVisible)
{
    protect(m_private)->setClosedCaptionsVisible(closedCaptionsVisible);
}

double MediaPlayer::rate() const
{
    return protect(m_private)->rate();
}

void MediaPlayer::setRate(double rate)
{
    protect(m_private)->setRateDouble(rate);
}

double MediaPlayer::effectiveRate() const
{
    return protect(m_private)->effectiveRate();
}

double MediaPlayer::requestedRate() const
{
    return protect(client())->mediaPlayerRequestedPlaybackRate();
}

bool MediaPlayer::preservesPitch() const
{
    return m_preservesPitch;
}

void MediaPlayer::setPreservesPitch(bool preservesPitch)
{
    m_preservesPitch = preservesPitch;
    protect(m_private)->setPreservesPitch(preservesPitch);
}

void MediaPlayer::setPitchCorrectionAlgorithm(PitchCorrectionAlgorithm pitchCorrectionAlgorithm)
{
    if (m_pitchCorrectionAlgorithm == pitchCorrectionAlgorithm)
        return;

    m_pitchCorrectionAlgorithm = pitchCorrectionAlgorithm;
    protect(m_private)->setPitchCorrectionAlgorithm(pitchCorrectionAlgorithm);
}


const PlatformTimeRanges& MediaPlayer::buffered() const
{
    return protect(m_private)->buffered();
}

const PlatformTimeRanges& MediaPlayer::seekable() const
{
    return protect(m_private)->seekable();
}

MediaTime MediaPlayer::maxTimeSeekable() const
{
    return protect(m_private)->maxTimeSeekable();
}

MediaTime MediaPlayer::minTimeSeekable() const
{
    return protect(m_private)->minTimeSeekable();
}

double MediaPlayer::seekableTimeRangesLastModifiedTime()
{
    return protect(m_private)->seekableTimeRangesLastModifiedTime();
}

void MediaPlayer::bufferedTimeRangesChanged()
{
    protect(client())->mediaPlayerBufferedTimeRangesChanged();
}

void MediaPlayer::seekableTimeRangesChanged()
{
    protect(client())->mediaPlayerSeekableTimeRangesChanged();
}

double MediaPlayer::liveUpdateInterval()
{
    return protect(m_private)->liveUpdateInterval();
}

void MediaPlayer::didLoadingProgress(DidLoadingProgressCompletionHandler&& callback) const
{
    protect(m_private)->didLoadingProgressAsync(WTF::move(callback));
}

void MediaPlayer::setPresentationSize(const IntSize& size)
{
    if (m_presentationSize == size)
        return;

    m_presentationSize = size;
    protect(m_private)->setPresentationSize(size);
}

void MediaPlayer::setPageIsVisible(bool visible)
{
    m_pageIsVisible = visible;
    protect(m_private)->setPageIsVisible(visible);
}

void MediaPlayer::setVisibleForCanvas(bool visible)
{
    if (visible == m_visibleForCanvas)
        return;

    m_visibleForCanvas = visible;
    protect(m_private)->setVisibleForCanvas(visible);
}

void MediaPlayer::setVisibleInViewport(bool visible)
{
    if (visible == m_visibleInViewport)
        return;

    m_visibleInViewport = visible;
    protect(m_private)->setVisibleInViewport(visible);
}

void MediaPlayer::setResourceOwner(const ProcessIdentity& processIdentity)
{
    m_processIdentity = processIdentity;
    protect(m_private)->setResourceOwner(processIdentity);
}

MediaPlayer::Preload MediaPlayer::preload() const
{
    return m_preload;
}

void MediaPlayer::setPreload(MediaPlayer::Preload preload)
{
    m_preload = preload;
    protect(m_private)->setPreload(preload);
}

void MediaPlayer::paint(GraphicsContext& context, const FloatRect& destination)
{
    protect(m_private)->paint(context, destination);
}

void MediaPlayer::paintCurrentFrameInContext(GraphicsContext& context, const FloatRect& destination)
{
    protect(m_private)->paintCurrentFrameInContext(context, destination);
}

RefPtr<VideoFrame> MediaPlayer::videoFrameForCurrentTime()
{
    return protect(m_private)->videoFrameForCurrentTime();
}

RefPtr<NativeImage> MediaPlayer::nativeImageForCurrentTime()
{
    return protect(m_private)->nativeImageForCurrentTime();
}

RefPtr<ShareableBitmap> MediaPlayer::bitmapImageForCurrentTimeSync()
{
    return protect(m_private)->bitmapImageForCurrentTimeSync();
}

Ref<MediaPlayer::BitmapImagePromise> MediaPlayer::bitmapImageForCurrentTime()
{
    return protect(m_private)->bitmapImageForCurrentTime();
}

DestinationColorSpace MediaPlayer::colorSpace()
{
    return protect(m_private)->colorSpace();
}

bool MediaPlayer::shouldGetNativeImageForCanvasDrawing() const
{
    return protect(m_private)->shouldGetNativeImageForCanvasDrawing();
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
    return protect(m_private)->supportsPictureInPicture();
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

bool MediaPlayer::isCurrentPlaybackTargetWireless() const
{
    return protect(m_private)->isCurrentPlaybackTargetWireless();
}

String MediaPlayer::wirelessPlaybackTargetName() const
{
    return protect(m_private)->wirelessPlaybackTargetName();
}

MediaPlayer::WirelessPlaybackTargetType MediaPlayer::wirelessPlaybackTargetType() const
{
    return protect(m_private)->wirelessPlaybackTargetType();
}

bool MediaPlayer::wirelessVideoPlaybackDisabled() const
{
    return protect(m_private)->wirelessVideoPlaybackDisabled();
}

void MediaPlayer::setWirelessVideoPlaybackDisabled(bool disabled)
{
    protect(m_private)->setWirelessVideoPlaybackDisabled(disabled);
}

void MediaPlayer::currentPlaybackTargetIsWirelessChanged(bool isCurrentPlaybackTargetWireless)
{
    protect(client())->mediaPlayerCurrentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless);
}

OptionSet<MediaPlaybackTargetType> MediaPlayer::supportedPlaybackTargetTypes() const
{
    return protect(m_private)->supportedPlaybackTargetTypes();
}

void MediaPlayer::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& device)
{
    protect(m_private)->setWirelessPlaybackTarget(WTF::move(device));
}

void MediaPlayer::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    protect(m_private)->setShouldPlayToPlaybackTarget(shouldPlay);
}

#endif

double MediaPlayer::maxFastForwardRate() const
{
    return protect(m_private)->maxFastForwardRate();
}

double MediaPlayer::minFastReverseRate() const
{
    return protect(m_private)->minFastReverseRate();
}

void MediaPlayer::acceleratedRenderingStateChanged()
{
    protect(m_private)->acceleratedRenderingStateChanged();
}

bool MediaPlayer::supportsAcceleratedRendering() const
{
    return protect(m_private)->supportsAcceleratedRendering();
}

void MediaPlayer::setShouldMaintainAspectRatio(bool maintainAspectRatio)
{
    protect(m_private)->setShouldMaintainAspectRatio(maintainAspectRatio);
}

void MediaPlayer::requestHostingContext(LayerHostingContextCallback&& callback)
{
    return protect(m_private)->requestHostingContext(WTF::move(callback));
}

HostingContext MediaPlayer::hostingContext() const
{
    return protect(m_private)->hostingContext();
}

bool MediaPlayer::didPassCORSAccessCheck() const
{
    return protect(m_private)->didPassCORSAccessCheck();
}

bool MediaPlayer::isCrossOrigin(const SecurityOrigin& origin) const
{
    if (auto crossOrigin = protect(m_private)->isCrossOrigin(origin))
        return *crossOrigin;

    if (m_url.protocolIsData())
        return false;

    return !origin.canRequest(m_url, originAccessPatternsForWebProcessOrEmpty());
}

MediaPlayer::MovieLoadType MediaPlayer::movieLoadType() const
{
    return protect(m_private)->movieLoadType();
}

MediaTime MediaPlayer::mediaTimeForTimeValue(const MediaTime& timeValue) const
{
    return protect(m_private)->mediaTimeForTimeValue(timeValue);
}

unsigned MediaPlayer::decodedFrameCount() const
{
    return protect(m_private)->decodedFrameCount();
}

unsigned MediaPlayer::droppedFrameCount() const
{
    return protect(m_private)->droppedFrameCount();
}

unsigned MediaPlayer::audioDecodedByteCount() const
{
    return protect(m_private)->audioDecodedByteCount();
}

unsigned MediaPlayer::videoDecodedByteCount() const
{
    return protect(m_private)->videoDecodedByteCount();
}

void MediaPlayer::reloadTimerFired()
{
    protect(m_private)->cancelLoad();
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
    protect(client())->mediaPlayerReadyStateChanged();
    if (m_pendingSeekRequest && protect(m_private)->readyState() == MediaPlayer::ReadyState::HaveMetadata)
        seekToTime(*std::exchange(m_pendingSeekRequest, std::nullopt));
}

void MediaPlayer::volumeChanged(double newVolume)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(newVolume);
    m_volume = protect(m_private)->volume();
#else
    m_volume = newVolume;
#endif
    protect(client())->mediaPlayerVolumeChanged();
}

void MediaPlayer::muteChanged(bool newMuted)
{
    if (newMuted == m_muted)
        return;

    m_muted = newMuted;
    protect(client())->mediaPlayerMuteChanged();
}

void MediaPlayer::timeChanged()
{
    protect(client())->mediaPlayerTimeChanged();
}

void MediaPlayer::sizeChanged()
{
    protect(client())->mediaPlayerSizeChanged();
}

void MediaPlayer::repaint()
{
    protect(client())->mediaPlayerRepaint();
}

void MediaPlayer::durationChanged()
{
    protect(client())->mediaPlayerDurationChanged();
}

void MediaPlayer::rateChanged()
{
    protect(client())->mediaPlayerRateChanged();
}

void MediaPlayer::playbackStateChanged()
{
    protect(client())->mediaPlayerPlaybackStateChanged();
}

void MediaPlayer::firstVideoFrameAvailable()
{
    protect(client())->mediaPlayerFirstVideoFrameAvailable();
}

void MediaPlayer::characteristicChanged()
{
    protect(client())->mediaPlayerCharacteristicChanged();
}

#if ENABLE(WEB_AUDIO)

AudioSourceProvider* MediaPlayer::audioSourceProvider()
{
    return protect(m_private)->audioSourceProvider();
}

#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

RefPtr<ArrayBuffer> MediaPlayer::cachedKeyForKeyId(const String& keyId) const
{
    return protect(client())->mediaPlayerCachedKeyForKeyId(keyId);
}

void MediaPlayer::keyNeeded(const SharedBuffer& initData)
{
    protect(client())->mediaPlayerKeyNeeded(initData);
}

String MediaPlayer::mediaKeysStorageDirectory() const
{
    return protect(client())->mediaPlayerMediaKeysStorageDirectory();
}

#endif

#if ENABLE(ENCRYPTED_MEDIA)

void MediaPlayer::initializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
    protect(client())->mediaPlayerInitializationDataEncountered(initDataType, WTF::move(initData));
}

void MediaPlayer::waitingForKeyChanged()
{
    protect(client())->mediaPlayerWaitingForKeyChanged();
}

bool MediaPlayer::waitingForKey() const
{
    if (!m_private)
        return false;
    return protect(m_private)->waitingForKey();
}
#endif

String MediaPlayer::referrer() const
{
    return protect(client())->mediaPlayerReferrer();
}

String MediaPlayer::userAgent() const
{
    return protect(client())->mediaPlayerUserAgent();
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

    return protect(m_private)->platformErrorCode();
}

CachedResourceLoader* MediaPlayer::cachedResourceLoader() const
{
    return protect(client())->mediaPlayerCachedResourceLoader();
}

Ref<PlatformMediaResourceLoader> MediaPlayer::mediaResourceLoader()
{
    if (!m_mediaResourceLoader)
        m_mediaResourceLoader = protect(client())->mediaPlayerCreateResourceLoader();

    return *m_mediaResourceLoader;
}

void MediaPlayer::addAudioTrack(AudioTrackPrivate& track)
{
    protect(client())->mediaPlayerDidAddAudioTrack(track);
}

void MediaPlayer::removeAudioTrack(AudioTrackPrivate& track)
{
    protect(client())->mediaPlayerDidRemoveAudioTrack(track);
}

void MediaPlayer::addTextTrack(InbandTextTrackPrivate& track)
{
    protect(client())->mediaPlayerDidAddTextTrack(track);
}

void MediaPlayer::removeTextTrack(InbandTextTrackPrivate& track)
{
    protect(client())->mediaPlayerDidRemoveTextTrack(track);
}

void MediaPlayer::addVideoTrack(VideoTrackPrivate& track)
{
    protect(client())->mediaPlayerDidAddVideoTrack(track);
}

void MediaPlayer::removeVideoTrack(VideoTrackPrivate& track)
{
    protect(client())->mediaPlayerDidRemoveVideoTrack(track);
}

void MediaPlayer::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    protect(m_private)->setTextTrackRepresentation(representation);
}

void MediaPlayer::syncTextTrackBounds()
{
    protect(m_private)->syncTextTrackBounds();
}

void MediaPlayer::tracksChanged()
{
    protect(m_private)->tracksChanged();
}

void MediaPlayer::notifyTrackModeChanged()
{
    if (RefPtr privateInterface = m_private)
        privateInterface->notifyTrackModeChanged();
}

Vector<Ref<PlatformTextTrack>> MediaPlayer::outOfBandTrackSources()
{
    return protect(client())->outOfBandTrackSources();
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

    protect(m_private)->simulateAudioInterruption();
}

bool MediaPlayer::isGStreamerHolePunchingEnabled()
{
    return protect(client())->isGStreamerHolePunchingEnabled();
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
    protect(client())->mediaPlayerDidReportGPUMemoryFootprint(footPrint);
}

unsigned long long MediaPlayer::fileSize() const
{
    if (!m_private)
        return 0;
    
    return protect(m_private)->fileSize();
}

bool MediaPlayer::ended() const
{
    return protect(m_private)->ended();
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
    return protect(client())->mediaPlayerSourceApplicationIdentifier();
}

Vector<String> MediaPlayer::preferredAudioCharacteristics() const
{
    return protect(client())->mediaPlayerPreferredAudioCharacteristics();
}

void MediaPlayerFactorySupport::callRegisterMediaEngine(MediaEngineRegister registerMediaEngine)
{
    registerMediaEngine(addMediaEngine);
}

bool MediaPlayer::doesHaveAttribute(const AtomString& attribute, AtomString* value) const
{
    return protect(client())->doesHaveAttribute(attribute, value);
}

#if PLATFORM(IOS_FAMILY)
String MediaPlayer::mediaPlayerNetworkInterfaceName() const
{
    return protect(client())->mediaPlayerNetworkInterfaceName();
}

void MediaPlayer::getRawCookies(const URL& url, MediaPlayerClient::GetRawCookiesCallback&& completionHandler) const
{
    protect(client())->mediaPlayerGetRawCookies(url, WTF::move(completionHandler));
}
#endif

void MediaPlayer::setShouldDisableSleep(bool flag)
{
    if (RefPtr privateInterface = m_private)
        privateInterface->setShouldDisableSleep(flag);
}

bool MediaPlayer::shouldDisableSleep() const
{
    return protect(client())->mediaPlayerShouldDisableSleep();
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
    return protect(client())->mediaContentTypesRequiringHardwareSupport();
}

const std::optional<Vector<String>>& MediaPlayer::allowedMediaContainerTypes() const
{
    return protect(client())->allowedMediaContainerTypes();
}

const std::optional<Vector<String>>& MediaPlayer::allowedMediaCodecTypes() const
{
    return protect(client())->allowedMediaCodecTypes();
}

const std::optional<Vector<FourCC>>& MediaPlayer::allowedMediaVideoCodecIDs() const
{
    return protect(client())->allowedMediaVideoCodecIDs();
}

const std::optional<Vector<FourCC>>& MediaPlayer::allowedMediaAudioCodecIDs() const
{
    return protect(client())->allowedMediaAudioCodecIDs();
}

const std::optional<Vector<FourCC>>& MediaPlayer::allowedMediaCaptionFormatTypes() const
{
    return protect(client())->allowedMediaCaptionFormatTypes();
}

void MediaPlayer::applicationWillResignActive()
{
    protect(m_private)->applicationWillResignActive();
}

void MediaPlayer::applicationDidBecomeActive()
{
    protect(m_private)->applicationDidBecomeActive();
}

#if USE(AVFOUNDATION)

AVPlayer* MediaPlayer::objCAVFoundationAVPlayer() const
{
    return protect(m_private)->objCAVFoundationAVPlayer();
}

#endif

bool MediaPlayer::performTaskAtTime(Function<void(const MediaTime&)>&& task, const MediaTime& time)
{
    return protect(m_private)->performTaskAtTime(WTF::move(task), time);
}

bool MediaPlayer::shouldIgnoreIntrinsicSize()
{
    return protect(m_private)->shouldIgnoreIntrinsicSize();
}

void MediaPlayer::isLoopingChanged()
{
    protect(m_private)->isLoopingChanged();
}

void MediaPlayer::remoteEngineFailedToLoad()
{
    protect(client())->mediaPlayerEngineFailedToLoad();
}

SecurityOriginData MediaPlayer::documentSecurityOrigin() const
{
    return protect(client())->documentSecurityOrigin();
}

void MediaPlayer::setPreferredDynamicRangeMode(DynamicRangeMode mode)
{
    m_preferredDynamicRangeMode = mode;
    protect(m_private)->setPreferredDynamicRangeMode(mode);
}

void MediaPlayer::setPlatformDynamicRangeLimit(PlatformDynamicRangeLimit platformDynamicRangeLimit)
{
    if (m_platformDynamicRangeLimit == platformDynamicRangeLimit)
        return;
    m_platformDynamicRangeLimit = platformDynamicRangeLimit;
    protect(m_private)->setPlatformDynamicRangeLimit(platformDynamicRangeLimit);
}

void MediaPlayer::audioOutputDeviceChanged()
{
    protect(m_private)->audioOutputDeviceChanged();
}

std::optional<MediaPlayerIdentifier> MediaPlayer::identifier() const
{
    return protect(m_private)->identifier();
}

std::optional<VideoFrameMetadata> MediaPlayer::videoFrameMetadata()
{
    return protect(m_private)->videoFrameMetadata();
}

void MediaPlayer::startVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = true;
    protect(m_private)->startVideoFrameMetadataGathering();
}

void MediaPlayer::stopVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = false;
    protect(m_private)->stopVideoFrameMetadataGathering();
}

void MediaPlayer::renderVideoWillBeDestroyed()
{
    protect(m_private)->renderVideoWillBeDestroyed();
}

void MediaPlayer::setShouldDisableHDR(bool shouldDisable)
{
    protect(m_private)->setShouldDisableHDR(shouldDisable);
}

void MediaPlayer::playerContentBoxRectChanged(const LayoutRect& rect)
{
    protect(m_private)->playerContentBoxRectChanged(rect);
}

#if PLATFORM(COCOA)
void MediaPlayer::onNewVideoFrameMetadata(VideoFrameMetadata&& metadata, RetainPtr<CVPixelBufferRef>&& buffer)
{
    protect(client())->mediaPlayerOnNewVideoFrameMetadata(WTF::move(metadata), WTF::move(buffer));
}
#endif

String MediaPlayer::elementId() const
{
    return protect(client())->mediaPlayerElementId();
}

bool MediaPlayer::supportsPlayAtHostTime() const
{
    return protect(m_private)->supportsPlayAtHostTime();
}

bool MediaPlayer::supportsPauseAtHostTime() const
{
    return protect(m_private)->supportsPauseAtHostTime();
}

bool MediaPlayer::playAtHostTime(const MonotonicTime& hostTime)
{
    // It is invalid to call playAtHostTime() if the underlying
    // media player does not support it.
    ASSERT(supportsPlayAtHostTime());
    return protect(m_private)->playAtHostTime(hostTime);
}

bool MediaPlayer::pauseAtHostTime(const MonotonicTime& hostTime)
{
    // It is invalid to call pauseAtHostTime() if the underlying
    // media player does not support it.
    ASSERT(supportsPauseAtHostTime());
    return protect(m_private)->pauseAtHostTime(hostTime);
}

void MediaPlayer::setShouldCheckHardwareSupport(bool value)
{
    protect(m_private)->setShouldCheckHardwareSupport(value);
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
    protect(m_private)->setDefaultSpatialTrackingLabel(defaultSpatialTrackingLabel);
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
    protect(m_private)->setSpatialTrackingLabel(spatialTrackingLabel);
}
#endif

#if HAVE(SPATIAL_AUDIO_EXPERIENCE)
void MediaPlayer::setPrefersSpatialAudioExperience(bool value)
{
    if (m_prefersSpatialAudioExperience == value)
        return;
    m_prefersSpatialAudioExperience = value;
    protect(m_private)->prefersSpatialAudioExperienceChanged();
}
#endif

auto MediaPlayer::soundStageSize() const -> SoundStageSize
{
    return protect(client())->mediaPlayerSoundStageSize();
}

void MediaPlayer::soundStageSizeDidChange()
{
    protect(m_private)->soundStageSizeDidChange();
}

void MediaPlayer::setInFullscreenOrPictureInPicture(bool isInFullscreenOrPictureInPicture)
{
    if (m_isInFullscreenOrPictureInPicture == isInFullscreenOrPictureInPicture)
        return;

    m_isInFullscreenOrPictureInPicture = isInFullscreenOrPictureInPicture;
    protect(m_private)->isInFullscreenOrPictureInPictureChanged(isInFullscreenOrPictureInPicture);
}

bool MediaPlayer::isInFullscreenOrPictureInPicture() const
{
    return m_isInFullscreenOrPictureInPicture;
}

#if ENABLE(LINEAR_MEDIA_PLAYER)
bool MediaPlayer::supportsLinearMediaPlayer() const
{
    return protect(m_private)->supportsLinearMediaPlayer();
}
#endif

#if !RELEASE_LOG_DISABLED
const Logger& MediaPlayer::mediaPlayerLogger()
{
    return protect(client())->mediaPlayerLogger();
}
#endif

void MediaPlayer::setMessageClientForTesting(WeakPtr<MessageClientForTesting> internalMessageClient)
{
    m_internalMessageClient = WTF::move(internalMessageClient);
    protect(m_private)->setMessageClientForTesting(m_internalMessageClient);
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
    return protect(client())->playbackTargetType();
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
