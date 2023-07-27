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
#include "IntRect.h"
#include "LegacyCDMSession.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayerPrivate.h"
#include "MediaStrategy.h"
#include "OriginAccessPatterns.h"
#include "PlatformMediaResourceLoader.h"
#include "PlatformMediaSessionManager.h"
#include "PlatformScreen.h"
#include "PlatformStrategies.h"
#include "PlatformTextTrack.h"
#include "PlatformTimeRanges.h"
#include "SecurityOrigin.h"
#include "VideoFrame.h"
#include "VideoFrameMetadata.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include "InbandTextTrackPrivate.h"

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
#endif

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)
#include "MediaPlayerPrivateMediaSourceAVFObjC.h"
#endif

#if ENABLE(MEDIA_STREAM) && USE(AVFOUNDATION)
#include "MediaPlayerPrivateMediaStreamAVFObjC.h"
#endif

#if ENABLE(ALTERNATE_WEBM_PLAYER)
#include "MediaPlayerPrivateWebM.h"
#endif

#endif // PLATFORM(COCOA)

#if USE(EXTERNAL_HOLEPUNCH)
#include "MediaPlayerPrivateHolePunch.h"
#endif

namespace WebCore {

// a null player to make MediaPlayer logic simpler

class NullMediaPlayerPrivate final : public MediaPlayerPrivateInterface {
public:
    explicit NullMediaPlayerPrivate(MediaPlayer*) { }

    void load(const String&) final { }
#if ENABLE(MEDIA_SOURCE)
    void load(const URL&, const ContentType&, MediaSourcePrivateClient&) final { }
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

    double durationDouble() const final { return 0; }

    double currentTimeDouble() const final { return 0; }
    void seekDouble(double) final { }
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

    float maxTimeSeekable() const final { return 0; }
    double minTimeSeekable() const final { return 0; }
    const PlatformTimeRanges& buffered() const final { return PlatformTimeRanges::emptyRanges(); }

    double seekableTimeRangesLastModifiedTime() const final { return 0; }
    double liveUpdateInterval() const final { return 0; }

    unsigned long long totalBytes() const final { return 0; }
    bool didLoadingProgress() const final { return false; }

    void setPresentationSize(const IntSize&) final { }

    void paint(GraphicsContext&, const FloatRect&) final { }
    DestinationColorSpace colorSpace() final { return DestinationColorSpace::SRGB(); }
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

class NullMediaPlayerClient : public MediaPlayerClient {
private:
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

    RefPtr<PlatformMediaResourceLoader> mediaPlayerCreateResourceLoader() final { return nullptr; }

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    RefPtr<ArrayBuffer> mediaPlayerCachedKeyForKeyId(const String&) const final { return nullptr; }
#endif

    const std::optional<Vector<String>>& allowedMediaContainerTypes() const final { return nullOptionalStringVector(); }
    const std::optional<Vector<String>>& allowedMediaCodecTypes() const final { return nullOptionalStringVector(); }
    const std::optional<Vector<FourCC>>& allowedMediaVideoCodecIDs() const final { return nullOptionalFourCCVector(); }
    const std::optional<Vector<FourCC>>& allowedMediaAudioCodecIDs() const final { return nullOptionalFourCCVector(); }
    const std::optional<Vector<FourCC>>& allowedMediaCaptionFormatTypes() const final { return nullOptionalFourCCVector(); }
};

const Vector<ContentType>& MediaPlayerClient::mediaContentTypesRequiringHardwareSupport() const
{
    static NeverDestroyed<Vector<ContentType>> contentTypes;
    return contentTypes;
}

static MediaPlayerClient& nullMediaPlayerClient()
{
    static NeverDestroyed<NullMediaPlayerClient> client;
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
    registerRemotePlayerCallback() = WTFMove(callback);
}

static void buildMediaEnginesVector() WTF_REQUIRES_LOCK(mediaEngineVectorLock)
{
    ASSERT(mediaEngineVectorLock.isLocked());

#if USE(AVFOUNDATION)
    auto& registerRemoteEngine = registerRemotePlayerCallback();
#if ENABLE(MEDIA_SOURCE)
    if (registerRemoteEngine && platformStrategies()->mediaStrategy().mockMediaSourceEnabled())
        registerRemoteEngine(addMediaEngine, MediaPlayerEnums::MediaEngineIdentifier::MockMSE);
#endif

    if (DeprecatedGlobalSettings::isAVFoundationEnabled()) {
#if ENABLE(ALTERNATE_WEBM_PLAYER)
        if (PlatformMediaSessionManager::alternateWebMPlayerEnabled()) {
            if (registerRemoteEngine)
                registerRemoteEngine(addMediaEngine, MediaPlayerEnums::MediaEngineIdentifier::CocoaWebM);
            else
                MediaPlayerPrivateWebM::registerMediaEngine(addMediaEngine);
        }
#endif

#if PLATFORM(COCOA)
        if (registerRemoteEngine)
            registerRemoteEngine(addMediaEngine, MediaPlayerEnums::MediaEngineIdentifier::AVFoundation);
        else
            MediaPlayerPrivateAVFoundationObjC::registerMediaEngine(addMediaEngine);
#endif

#if ENABLE(MEDIA_SOURCE)
        if (registerRemoteEngine)
            registerRemoteEngine(addMediaEngine, MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE);
        else
            MediaPlayerPrivateMediaSourceAVFObjC::registerMediaEngine(addMediaEngine);
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
    mutableInstalledMediaEnginesVector().append(WTFMove(factory));
}

static const AtomString& applicationOctetStream()
{
    static MainThreadNeverDestroyed<const AtomString> applicationOctetStream("application/octet-stream"_s);
    return applicationOctetStream;
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
#else
        ASSERT_NOT_REACHED();
#endif
        return nullptr;
    }

    return engines[currentIndex].get();
}

static const MediaPlayerFactory* bestMediaEngineForSupportParameters(const MediaEngineSupportParameters& parameters, const HashSet<const MediaPlayerFactory*>& attemptedEngines = { }, const MediaPlayerFactory* current = nullptr)
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
        if (attemptedEngines.contains(engine.get()))
            continue;
        MediaPlayer::SupportsType engineSupport = engine->supportsTypeAndCodecs(parameters);
        if (engineSupport > supported) {
            supported = engineSupport;
            foundEngine = engine.get();
        }
    }

    return foundEngine;
}

const MediaPlayerFactory* MediaPlayer::nextMediaEngine(const MediaPlayerFactory* current)
{
    if (m_activeEngineIdentifier) {
        auto* engine = mediaEngine(m_activeEngineIdentifier.value());
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

    auto* nextEngine = engines[currentIndex + 1].get();
    
    if (m_attemptedEngines.contains(nextEngine))
        return nextMediaEngine(nextEngine);
    
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
    : m_client(&client)
    , m_reloadTimer(*this, &MediaPlayer::reloadTimerFired)
    , m_private(makeUnique<NullMediaPlayerPrivate>(this))
    , m_preferredDynamicRangeMode(DynamicRangeMode::Standard)
{
}

MediaPlayer::MediaPlayer(MediaPlayerClient& client, MediaPlayerEnums::MediaEngineIdentifier mediaEngineIdentifier)
    : m_client(&client)
    , m_reloadTimer(*this, &MediaPlayer::reloadTimerFired)
    , m_private(makeUnique<NullMediaPlayerPrivate>(this))
    , m_activeEngineIdentifier(mediaEngineIdentifier)
    , m_preferredDynamicRangeMode(DynamicRangeMode::Standard)
{
}

MediaPlayer::~MediaPlayer()
{
    ASSERT(!m_initializingMediaEngine);
}

void MediaPlayer::invalidate()
{
    m_client = &nullMediaPlayerClient();
}

bool MediaPlayer::load(const URL& url, const ContentType& contentType, const String& keySystem, bool requiresRemotePlayback)
{
    ASSERT(!m_reloadTimer.isActive());

    // Protect against MediaPlayer being destroyed during a MediaPlayerClient callback.
    Ref<MediaPlayer> protectedThis(*this);

    m_contentType = contentType;
    m_url = url;
    m_keySystem = keySystem.convertToASCIILowercase();
    m_requiresRemotePlayback = requiresRemotePlayback;
    m_contentMIMETypeWasInferredFromExtension = false;

#if ENABLE(MEDIA_SOURCE)
    m_mediaSource = nullptr;
#endif
#if ENABLE(MEDIA_STREAM)
    m_mediaStream = nullptr;
#endif

    // If the MIME type is missing or is not meaningful, try to figure it out from the URL.
    AtomString containerType { m_contentType.containerType() };
    if (containerType.isEmpty() || containerType == applicationOctetStream() || containerType == textPlainContentTypeAtom()) {
        if (m_url.protocolIsData())
            m_contentType = ContentType(mimeTypeFromDataURL(m_url.string()));
        else {
            auto lastPathComponent = url.lastPathComponent();
            size_t pos = lastPathComponent.reverseFind('.');
            if (pos != notFound) {
                auto extension = lastPathComponent.substring(pos + 1);
                String mediaType = MIMETypeRegistry::mediaMIMETypeForExtension(extension);
                if (!mediaType.isEmpty()) {
                    m_contentType = ContentType { WTFMove(mediaType) };
                    m_contentMIMETypeWasInferredFromExtension = true;
                }
            }
        }
    }

    loadWithNextMediaEngine(nullptr);
    return m_currentMediaEngine;
}

#if ENABLE(MEDIA_SOURCE)
bool MediaPlayer::load(const URL& url, const ContentType& contentType, MediaSourcePrivateClient& mediaSource)
{
    ASSERT(!m_reloadTimer.isActive());

    m_mediaSource = mediaSource;
    m_contentType = contentType;
    m_url = url;
    m_keySystem = emptyString();
    m_requiresRemotePlayback = false;
    m_contentMIMETypeWasInferredFromExtension = false;
    loadWithNextMediaEngine(nullptr);
    return m_currentMediaEngine;
}
#endif

#if ENABLE(MEDIA_STREAM)
bool MediaPlayer::load(MediaStreamPrivate& mediaStream)
{
    ASSERT(!m_reloadTimer.isActive());

    m_mediaStream = &mediaStream;
    m_keySystem = emptyString();
    m_requiresRemotePlayback = false;
    m_contentType = { };
    m_contentMIMETypeWasInferredFromExtension = false;
    loadWithNextMediaEngine(nullptr);
    return m_currentMediaEngine;
}
#endif

const MediaPlayerFactory* MediaPlayer::nextBestMediaEngine(const MediaPlayerFactory* current)
{
    MediaEngineSupportParameters parameters;
    parameters.type = m_contentType;
    parameters.url = m_url;
#if ENABLE(MEDIA_SOURCE)
    parameters.isMediaSource = !!m_mediaSource;
#endif
#if ENABLE(MEDIA_STREAM)
    parameters.isMediaStream = !!m_mediaStream;
#endif
    parameters.allowedMediaContainerTypes = allowedMediaContainerTypes();
    parameters.allowedMediaCodecTypes = allowedMediaCodecTypes();
    parameters.allowedMediaVideoCodecIDs = allowedMediaVideoCodecIDs();
    parameters.allowedMediaAudioCodecIDs = allowedMediaAudioCodecIDs();
    parameters.allowedMediaCaptionFormatTypes = allowedMediaCaptionFormatTypes();

    if (m_activeEngineIdentifier) {
        if (current)
            return nullptr;

        auto* engine = mediaEngine(m_activeEngineIdentifier.value());
        if (engine && engine->supportsTypeAndCodecs(parameters) != SupportsType::IsNotSupported)
            return engine;

        return nullptr;
    }

    return bestMediaEngineForSupportParameters(parameters, m_attemptedEngines, current);
}

void MediaPlayer::reloadAndResumePlaybackIfNeeded()
{
    client().mediaPlayerReloadAndResumePlaybackIfNeeded();
}

void MediaPlayer::loadWithNextMediaEngine(const MediaPlayerFactory* current)
{
#if ENABLE(MEDIA_SOURCE) 
#define MEDIASOURCE m_mediaSource
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
    client().mediaPlayerWillInitializeMediaEngine();

    const MediaPlayerFactory* engine = nullptr;

    if (!m_contentType.isEmpty() || MEDIASTREAM || MEDIASOURCE)
        engine = nextBestMediaEngine(current);

    // Exhaust all remaining engines
    if (!engine)
        engine = nextMediaEngine(current);

    // Don't delete and recreate the player unless it comes from a different engine.
    if (!engine) {
        LOG(Media, "MediaPlayer::loadWithNextMediaEngine - no media engine found for type \"%s\"", m_contentType.raw().utf8().data());
        m_currentMediaEngine = engine;
        m_private = nullptr;
    } else if (m_currentMediaEngine != engine) {
        m_currentMediaEngine = engine;
        m_attemptedEngines.add(engine);
        m_private = engine->createMediaEnginePlayer(this);
        if (m_private) {
            client().mediaPlayerEngineUpdated();
            if (m_pageIsVisible)
                m_private->setPageIsVisible(m_pageIsVisible);
            if (m_visibleInViewport)
                m_private->setVisibleInViewport(m_visibleInViewport);
            if (m_isGatheringVideoFrameMetadata)
                m_private->startVideoFrameMetadataGathering();
            if (m_processIdentity)
                m_private->setResourceOwner(m_processIdentity);
            m_private->prepareForPlayback(m_privateBrowsing, m_preload, m_preservesPitch, m_shouldPrepareToRender);
        }
    }

    if (m_private) {
#if ENABLE(MEDIA_SOURCE)
        if (m_mediaSource)
            m_private->load(m_url, m_contentMIMETypeWasInferredFromExtension ? ContentType() : m_contentType, *m_mediaSource);
        else
#endif
#if ENABLE(MEDIA_STREAM)
        if (m_mediaStream)
            m_private->load(*m_mediaStream);
        else
#endif
        m_private->load(m_url, m_contentMIMETypeWasInferredFromExtension ? ContentType() : m_contentType, m_keySystem);
    } else {
        m_private = makeUnique<NullMediaPlayerPrivate>(this);
        if (!m_activeEngineIdentifier
            && installedMediaEngines().size() > 1
            && (nextBestMediaEngine(m_currentMediaEngine) || nextMediaEngine(m_currentMediaEngine)))
            m_reloadTimer.startOneShot(0_s);
        else {
            client().mediaPlayerEngineUpdated();
            client().mediaPlayerResourceNotSupported();
        }
    }

    m_initializingMediaEngine = false;
    client().mediaPlayerDidInitializeMediaEngine();
}

void MediaPlayer::queueTaskOnEventLoop(Function<void()>&& task)
{
    ASSERT(isMainThread());
    client().mediaPlayerQueueTaskOnEventLoop(WTFMove(task));
}

bool MediaPlayer::hasAvailableVideoFrame() const
{
    return m_private->hasAvailableVideoFrame();
}

void MediaPlayer::prepareForRendering()
{
    m_shouldPrepareToRender = true;
    m_private->prepareForRendering();
}

void MediaPlayer::cancelLoad()
{
    m_private->cancelLoad();
}    

void MediaPlayer::prepareToPlay()
{
    Ref<MediaPlayer> protectedThis(*this);

    m_private->prepareToPlay();
}

void MediaPlayer::play()
{
    m_private->play();
}

void MediaPlayer::pause()
{
    m_private->pause();
}

void MediaPlayer::setBufferingPolicy(BufferingPolicy policy)
{
    m_private->setBufferingPolicy(policy);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

std::unique_ptr<LegacyCDMSession> MediaPlayer::createSession(const String& keySystem, LegacyCDMSessionClient& client)
{
    return m_private->createSession(keySystem, client);
}

void MediaPlayer::setCDM(LegacyCDM* cdm)
{
    m_private->setCDM(cdm);
}

void MediaPlayer::setCDMSession(LegacyCDMSession* session)
{
    m_private->setCDMSession(session);
}

void MediaPlayer::keyAdded()
{
    m_private->keyAdded();
}

#endif
    
#if ENABLE(ENCRYPTED_MEDIA)

void MediaPlayer::cdmInstanceAttached(CDMInstance& instance)
{
    m_private->cdmInstanceAttached(instance);
}

void MediaPlayer::cdmInstanceDetached(CDMInstance& instance)
{
    m_private->cdmInstanceDetached(instance);
}

void MediaPlayer::attemptToDecryptWithInstance(CDMInstance& instance)
{
    m_private->attemptToDecryptWithInstance(instance);
}

#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA) && ENABLE(ENCRYPTED_MEDIA)
void MediaPlayer::setShouldContinueAfterKeyNeeded(bool should)
{
    m_shouldContinueAfterKeyNeeded = should;
    m_private->setShouldContinueAfterKeyNeeded(should);
}
#endif

MediaTime MediaPlayer::duration() const
{
    return m_private->durationMediaTime();
}

MediaTime MediaPlayer::startTime() const
{
    return m_private->startTime();
}

MediaTime MediaPlayer::initialTime() const
{
    return m_private->initialTime();
}

MediaTime MediaPlayer::currentTime() const
{
    return m_private->currentMediaTime();
}

bool MediaPlayer::currentTimeMayProgress() const
{
    return m_private->currentMediaTimeMayProgress();
}

bool MediaPlayer::setCurrentTimeDidChangeCallback(CurrentTimeDidChangeCallback&& callback)
{
    return m_private->setCurrentTimeDidChangeCallback(WTFMove(callback));
}

MediaTime MediaPlayer::getStartDate() const
{
    return m_private->getStartDate();
}

void MediaPlayer::seekWithTolerance(const MediaTime& time, const MediaTime& negativeTolerance, const MediaTime& positiveTolerance)
{
    m_private->seekWithTolerance(time, negativeTolerance, positiveTolerance);
}

void MediaPlayer::seek(const MediaTime& time)
{
    m_private->seek(time);
}

void MediaPlayer::seekWhenPossible(const MediaTime& time)
{
    if (m_private->readyState() < MediaPlayer::ReadyState::HaveMetadata)
        m_pendingSeekRequest = time;
    else
        seek(time);
}

bool MediaPlayer::paused() const
{
    return m_private->paused();
}

bool MediaPlayer::seeking() const
{
    return m_private->seeking();
}

bool MediaPlayer::supportsFullscreen() const
{
    return m_private->supportsFullscreen();
}

bool MediaPlayer::canSaveMediaData() const
{
    return m_private->canSaveMediaData();
}

bool MediaPlayer::supportsScanning() const
{
    return m_private->supportsScanning();
}

bool MediaPlayer::supportsProgressMonitoring() const
{
    return m_private->supportsProgressMonitoring();
}

bool MediaPlayer::requiresImmediateCompositing() const
{
    return m_private->requiresImmediateCompositing();
}

FloatSize MediaPlayer::naturalSize()
{
    return m_private->naturalSize();
}

bool MediaPlayer::hasVideo() const
{
    return m_private->hasVideo();
}

bool MediaPlayer::hasAudio() const
{
    return m_private->hasAudio();
}

PlatformLayer* MediaPlayer::platformLayer() const
{
    return m_private->platformLayer();
}
    
#if ENABLE(VIDEO_PRESENTATION_MODE)

RetainPtr<PlatformLayer> MediaPlayer::createVideoFullscreenLayer()
{
    return m_private->createVideoFullscreenLayer();
}

void MediaPlayer::setVideoFullscreenLayer(PlatformLayer* layer, Function<void()>&& completionHandler)
{
    m_private->setVideoFullscreenLayer(layer, WTFMove(completionHandler));
}

void MediaPlayer::updateVideoFullscreenInlineImage()
{
    m_private->updateVideoFullscreenInlineImage();
}

void MediaPlayer::setVideoFullscreenFrame(FloatRect frame)
{
    m_private->setVideoFullscreenFrame(frame);
}

void MediaPlayer::setVideoFullscreenGravity(MediaPlayer::VideoGravity gravity)
{
    m_private->setVideoFullscreenGravity(gravity);
}

void MediaPlayer::setVideoFullscreenMode(MediaPlayer::VideoFullscreenMode mode)
{
    m_private->setVideoFullscreenMode(mode);
}

MediaPlayer::VideoFullscreenMode MediaPlayer::fullscreenMode() const
{
    return client().mediaPlayerFullscreenMode();
}

void MediaPlayer::videoFullscreenStandbyChanged()
{
    m_private->videoFullscreenStandbyChanged();
}

bool MediaPlayer::isVideoFullscreenStandby() const
{
    return client().mediaPlayerIsVideoFullscreenStandby();
}

#endif

FloatSize MediaPlayer::videoInlineSize() const
{
    return client().mediaPlayerVideoInlineSize();
}

void MediaPlayer::setVideoInlineSizeFenced(const FloatSize& size, WTF::MachSendRight&& fence)
{
    m_private->setVideoInlineSizeFenced(size, WTFMove(fence));
}

#if PLATFORM(IOS_FAMILY)

NSArray* MediaPlayer::timedMetadata() const
{
    return m_private->timedMetadata();
}

String MediaPlayer::accessLog() const
{
    return m_private->accessLog();
}

String MediaPlayer::errorLog() const
{
    return m_private->errorLog();
}

#endif

MediaPlayer::NetworkState MediaPlayer::networkState()
{
    return m_private->networkState();
}

MediaPlayer::ReadyState MediaPlayer::readyState() const
{
    return m_private->readyState();
}

double MediaPlayer::volume() const
{
    return m_volume;
}

void MediaPlayer::setVolume(double volume)
{
    m_volume = volume;
    m_private->setVolumeDouble(volume);
}

bool MediaPlayer::muted() const
{
    return m_muted;
}

void MediaPlayer::setMuted(bool muted)
{
    m_muted = muted;

    m_private->setMuted(muted);
}

bool MediaPlayer::hasClosedCaptions() const
{
    return m_private->hasClosedCaptions();
}

void MediaPlayer::setClosedCaptionsVisible(bool closedCaptionsVisible)
{
    m_private->setClosedCaptionsVisible(closedCaptionsVisible);
}

double MediaPlayer::rate() const
{
    return m_private->rate();
}

void MediaPlayer::setRate(double rate)
{
    m_private->setRateDouble(rate);
}

double MediaPlayer::effectiveRate() const
{
    return m_private->effectiveRate();
}

double MediaPlayer::requestedRate() const
{
    return client().mediaPlayerRequestedPlaybackRate();
}

bool MediaPlayer::preservesPitch() const
{
    return m_preservesPitch;
}

void MediaPlayer::setPreservesPitch(bool preservesPitch)
{
    m_preservesPitch = preservesPitch;
    m_private->setPreservesPitch(preservesPitch);
}

void MediaPlayer::setPitchCorrectionAlgorithm(PitchCorrectionAlgorithm pitchCorrectionAlgorithm)
{
    if (m_pitchCorrectionAlgorithm == pitchCorrectionAlgorithm)
        return;

    m_pitchCorrectionAlgorithm = pitchCorrectionAlgorithm;
    m_private->setPitchCorrectionAlgorithm(pitchCorrectionAlgorithm);
}

const PlatformTimeRanges& MediaPlayer::buffered() const
{
    return m_private->buffered();
}

const PlatformTimeRanges& MediaPlayer::seekable() const
{
    return m_private->seekable();
}

MediaTime MediaPlayer::maxTimeSeekable() const
{
    return m_private->maxMediaTimeSeekable();
}

MediaTime MediaPlayer::minTimeSeekable() const
{
    return m_private->minMediaTimeSeekable();
}

double MediaPlayer::seekableTimeRangesLastModifiedTime()
{
    return m_private->seekableTimeRangesLastModifiedTime();
}

void MediaPlayer::bufferedTimeRangesChanged()
{
    client().mediaPlayerBufferedTimeRangesChanged();
}

void MediaPlayer::seekableTimeRangesChanged()
{
    client().mediaPlayerSeekableTimeRangesChanged();
}

double MediaPlayer::liveUpdateInterval()
{
    return m_private->liveUpdateInterval();
}

void MediaPlayer::didLoadingProgress(DidLoadingProgressCompletionHandler&& callback) const
{
    m_private->didLoadingProgressAsync(WTFMove(callback));
}

void MediaPlayer::setPresentationSize(const IntSize& size)
{
    if (m_presentationSize == size)
        return;

    m_presentationSize = size;
    m_private->setPresentationSize(size);
}

void MediaPlayer::setPageIsVisible(bool visible)
{
    m_pageIsVisible = visible;
    m_private->setPageIsVisible(visible);
}

void MediaPlayer::setVisibleForCanvas(bool visible)
{
    if (visible == m_visibleForCanvas)
        return;

    m_visibleForCanvas = visible;
    m_private->setVisibleForCanvas(visible);
}

void MediaPlayer::setVisibleInViewport(bool visible)
{
    if (visible == m_visibleInViewport)
        return;

    m_visibleInViewport = visible;
    m_private->setVisibleInViewport(visible);
}

void MediaPlayer::setResourceOwner(const ProcessIdentity& processIdentity)
{
    m_processIdentity = processIdentity;
    m_private->setResourceOwner(processIdentity);
}

MediaPlayer::Preload MediaPlayer::preload() const
{
    return m_preload;
}

void MediaPlayer::setPreload(MediaPlayer::Preload preload)
{
    m_preload = preload;
    m_private->setPreload(preload);
}

void MediaPlayer::paint(GraphicsContext& p, const FloatRect& r)
{
    m_private->paint(p, r);
}

#if !USE(AVFOUNDATION)

bool MediaPlayer::copyVideoTextureToPlatformTexture(GraphicsContextGL* context, PlatformGLObject texture, GCGLenum target, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, bool premultiplyAlpha, bool flipY)
{
    return m_private->copyVideoTextureToPlatformTexture(context, texture, target, level, internalFormat, format, type, premultiplyAlpha, flipY);
}

#endif

RefPtr<VideoFrame> MediaPlayer::videoFrameForCurrentTime()
{
    return m_private->videoFrameForCurrentTime();
}


#if PLATFORM(COCOA) && !HAVE(AVSAMPLEBUFFERDISPLAYLAYER_COPYDISPLAYEDPIXELBUFFER)
void MediaPlayer::willBeAskedToPaintGL()
{
    m_private->willBeAskedToPaintGL();
}
#endif

RefPtr<NativeImage> MediaPlayer::nativeImageForCurrentTime()
{
    return m_private->nativeImageForCurrentTime();
}

DestinationColorSpace MediaPlayer::colorSpace()
{
    return m_private->colorSpace();
}

bool MediaPlayer::shouldGetNativeImageForCanvasDrawing() const
{
    return m_private->shouldGetNativeImageForCanvasDrawing();
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

    const MediaPlayerFactory* engine = bestMediaEngineForSupportParameters(parameters);
    if (!engine)
        return SupportsType::IsNotSupported;

    return engine->supportsTypeAndCodecs(parameters);
}

void MediaPlayer::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    for (auto& engine : installedMediaEngines()) {
        HashSet<String, ASCIICaseInsensitiveHash> engineTypes;
        engine->getSupportedTypes(engineTypes);
        types.add(engineTypes.begin(), engineTypes.end());
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
    return m_private->supportsPictureInPicture();
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

bool MediaPlayer::isCurrentPlaybackTargetWireless() const
{
    return m_private->isCurrentPlaybackTargetWireless();
}

String MediaPlayer::wirelessPlaybackTargetName() const
{
    return m_private->wirelessPlaybackTargetName();
}

MediaPlayer::WirelessPlaybackTargetType MediaPlayer::wirelessPlaybackTargetType() const
{
    return m_private->wirelessPlaybackTargetType();
}

bool MediaPlayer::wirelessVideoPlaybackDisabled() const
{
    return m_private->wirelessVideoPlaybackDisabled();
}

void MediaPlayer::setWirelessVideoPlaybackDisabled(bool disabled)
{
    m_private->setWirelessVideoPlaybackDisabled(disabled);
}

void MediaPlayer::currentPlaybackTargetIsWirelessChanged(bool isCurrentPlaybackTargetWireless)
{
    client().mediaPlayerCurrentPlaybackTargetIsWirelessChanged(isCurrentPlaybackTargetWireless);
}

bool MediaPlayer::canPlayToWirelessPlaybackTarget() const
{
    return m_private->canPlayToWirelessPlaybackTarget();
}

void MediaPlayer::setWirelessPlaybackTarget(Ref<MediaPlaybackTarget>&& device)
{
    m_private->setWirelessPlaybackTarget(WTFMove(device));
}

void MediaPlayer::setShouldPlayToPlaybackTarget(bool shouldPlay)
{
    m_private->setShouldPlayToPlaybackTarget(shouldPlay);
}

#endif

double MediaPlayer::maxFastForwardRate() const
{
    return m_private->maxFastForwardRate();
}

double MediaPlayer::minFastReverseRate() const
{
    return m_private->minFastReverseRate();
}

void MediaPlayer::acceleratedRenderingStateChanged()
{
    m_private->acceleratedRenderingStateChanged();
}

bool MediaPlayer::supportsAcceleratedRendering() const
{
    return m_private->supportsAcceleratedRendering();
}

void MediaPlayer::setShouldMaintainAspectRatio(bool maintainAspectRatio)
{
    m_private->setShouldMaintainAspectRatio(maintainAspectRatio);
}

void MediaPlayer::requestHostingContextID(LayerHostingContextIDCallback&& callback)
{
    return m_private->requestHostingContextID(WTFMove(callback));
}

LayerHostingContextID MediaPlayer::hostingContextID() const
{
    return m_private->hostingContextID();
}

bool MediaPlayer::didPassCORSAccessCheck() const
{
    return m_private->didPassCORSAccessCheck();
}

bool MediaPlayer::isCrossOrigin(const SecurityOrigin& origin) const
{
    if (auto crossOrigin = m_private->isCrossOrigin(origin))
        return *crossOrigin;

    if (m_url.protocolIsData())
        return false;

    return !origin.canRequest(m_url, EmptyOriginAccessPatterns::singleton());
}

MediaPlayer::MovieLoadType MediaPlayer::movieLoadType() const
{
    return m_private->movieLoadType();
}

MediaTime MediaPlayer::mediaTimeForTimeValue(const MediaTime& timeValue) const
{
    return m_private->mediaTimeForTimeValue(timeValue);
}

double MediaPlayer::maximumDurationToCacheMediaTime() const
{
    return m_private->maximumDurationToCacheMediaTime();
}

unsigned MediaPlayer::decodedFrameCount() const
{
    return m_private->decodedFrameCount();
}

unsigned MediaPlayer::droppedFrameCount() const
{
    return m_private->droppedFrameCount();
}

unsigned MediaPlayer::audioDecodedByteCount() const
{
    return m_private->audioDecodedByteCount();
}

unsigned MediaPlayer::videoDecodedByteCount() const
{
    return m_private->videoDecodedByteCount();
}

void MediaPlayer::reloadTimerFired()
{
    m_private->cancelLoad();
    loadWithNextMediaEngine(m_currentMediaEngine);
}

template<typename T>
static void addToHash(HashSet<T>& toHash, HashSet<T>&& fromHash)
{
    if (toHash.isEmpty())
        toHash = WTFMove(fromHash);
    else
        toHash.add(fromHash.begin(), fromHash.end());
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
    m_privateBrowsing = privateBrowsingMode;
    if (m_private)
        m_private->setPrivateBrowsingMode(m_privateBrowsing);
}

// Client callbacks.
void MediaPlayer::networkStateChanged()
{
    if (m_private->networkState() >= MediaPlayer::NetworkState::FormatError)
        m_lastErrorMessage = m_private->errorMessage();
    // If more than one media engine is installed and this one failed before finding metadata,
    // let the next engine try.
    if (m_private->networkState() >= MediaPlayer::NetworkState::FormatError && m_private->readyState() < MediaPlayer::ReadyState::HaveMetadata) {
        client().mediaPlayerEngineFailedToLoad();
        if (!m_activeEngineIdentifier
            && installedMediaEngines().size() > 1
            && (nextBestMediaEngine(m_currentMediaEngine) || nextMediaEngine(m_currentMediaEngine))) {
            m_reloadTimer.startOneShot(0_s);
            return;
        }
    }
    client().mediaPlayerNetworkStateChanged();
}

void MediaPlayer::readyStateChanged()
{
    client().mediaPlayerReadyStateChanged();
    if (m_pendingSeekRequest && m_private->readyState() == MediaPlayer::ReadyState::HaveMetadata)
        seek(*std::exchange(m_pendingSeekRequest, std::nullopt));
}

void MediaPlayer::volumeChanged(double newVolume)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(newVolume);
    m_volume = m_private->volume();
#else
    m_volume = newVolume;
#endif
    client().mediaPlayerVolumeChanged();
}

void MediaPlayer::muteChanged(bool newMuted)
{
    if (newMuted == m_muted)
        return;

    m_muted = newMuted;
    client().mediaPlayerMuteChanged();
}

void MediaPlayer::timeChanged()
{
    client().mediaPlayerTimeChanged();
}

void MediaPlayer::sizeChanged()
{
    client().mediaPlayerSizeChanged();
}

void MediaPlayer::repaint()
{
    client().mediaPlayerRepaint();
}

void MediaPlayer::durationChanged()
{
    client().mediaPlayerDurationChanged();
}

void MediaPlayer::rateChanged()
{
    client().mediaPlayerRateChanged();
}

void MediaPlayer::playbackStateChanged()
{
    client().mediaPlayerPlaybackStateChanged();
}

void MediaPlayer::firstVideoFrameAvailable()
{
    client().mediaPlayerFirstVideoFrameAvailable();
}

void MediaPlayer::characteristicChanged()
{
    client().mediaPlayerCharacteristicChanged();
}

#if ENABLE(WEB_AUDIO)

AudioSourceProvider* MediaPlayer::audioSourceProvider()
{
    return m_private->audioSourceProvider();
}

#endif

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

RefPtr<ArrayBuffer> MediaPlayer::cachedKeyForKeyId(const String& keyId) const
{
    return client().mediaPlayerCachedKeyForKeyId(keyId);
}

void MediaPlayer::keyNeeded(const SharedBuffer& initData)
{
    client().mediaPlayerKeyNeeded(initData);
}

String MediaPlayer::mediaKeysStorageDirectory() const
{
    return client().mediaPlayerMediaKeysStorageDirectory();
}

#endif

#if ENABLE(ENCRYPTED_MEDIA)

void MediaPlayer::initializationDataEncountered(const String& initDataType, RefPtr<ArrayBuffer>&& initData)
{
    client().mediaPlayerInitializationDataEncountered(initDataType, WTFMove(initData));
}

void MediaPlayer::waitingForKeyChanged()
{
    client().mediaPlayerWaitingForKeyChanged();
}

bool MediaPlayer::waitingForKey() const
{
    if (!m_private)
        return false;
    return m_private->waitingForKey();
}
#endif

String MediaPlayer::referrer() const
{
    return client().mediaPlayerReferrer();
}

String MediaPlayer::userAgent() const
{
    return client().mediaPlayerUserAgent();
}

String MediaPlayer::engineDescription() const
{
    if (!m_private)
        return String();

    return m_private->engineDescription();
}

long MediaPlayer::platformErrorCode() const
{
    if (!m_private)
        return 0;

    return m_private->platformErrorCode();
}

CachedResourceLoader* MediaPlayer::cachedResourceLoader()
{
    return client().mediaPlayerCachedResourceLoader();
}

RefPtr<PlatformMediaResourceLoader> MediaPlayer::createResourceLoader()
{
    return client().mediaPlayerCreateResourceLoader();
}

void MediaPlayer::addAudioTrack(AudioTrackPrivate& track)
{
    client().mediaPlayerDidAddAudioTrack(track);
}

void MediaPlayer::removeAudioTrack(AudioTrackPrivate& track)
{
    client().mediaPlayerDidRemoveAudioTrack(track);
}

void MediaPlayer::addTextTrack(InbandTextTrackPrivate& track)
{
    client().mediaPlayerDidAddTextTrack(track);
}

void MediaPlayer::removeTextTrack(InbandTextTrackPrivate& track)
{
    client().mediaPlayerDidRemoveTextTrack(track);
}

void MediaPlayer::addVideoTrack(VideoTrackPrivate& track)
{
    client().mediaPlayerDidAddVideoTrack(track);
}

void MediaPlayer::removeVideoTrack(VideoTrackPrivate& track)
{
    client().mediaPlayerDidRemoveVideoTrack(track);
}

bool MediaPlayer::requiresTextTrackRepresentation() const
{
    return m_private->requiresTextTrackRepresentation();
}

void MediaPlayer::setTextTrackRepresentation(TextTrackRepresentation* representation)
{
    m_private->setTextTrackRepresentation(representation);
}

void MediaPlayer::syncTextTrackBounds()
{
    m_private->syncTextTrackBounds();
}

void MediaPlayer::tracksChanged()
{
    m_private->tracksChanged();
}

void MediaPlayer::notifyTrackModeChanged()
{
    if (m_private)
        m_private->notifyTrackModeChanged();
}

Vector<RefPtr<PlatformTextTrack>> MediaPlayer::outOfBandTrackSources()
{
    return client().outOfBandTrackSources();
}

void MediaPlayer::resetMediaEngines()
{
    Locker locker { mediaEngineVectorLock };

    mutableInstalledMediaEnginesVector().clear();
    haveMediaEnginesVector() = false;
}

#if USE(GSTREAMER)
void MediaPlayer::simulateAudioInterruption()
{
    if (!m_private)
        return;

    m_private->simulateAudioInterruption();
}
#endif

void MediaPlayer::beginSimulatedHDCPError()
{
    if (m_private)
        m_private->beginSimulatedHDCPError();
}

void MediaPlayer::endSimulatedHDCPError()
{
    if (m_private)
        m_private->endSimulatedHDCPError();
}

String MediaPlayer::languageOfPrimaryAudioTrack() const
{
    if (!m_private)
        return emptyString();
    
    return m_private->languageOfPrimaryAudioTrack();
}

size_t MediaPlayer::extraMemoryCost() const
{
    if (!m_private)
        return 0;

    return m_private->extraMemoryCost();
}

unsigned long long MediaPlayer::fileSize() const
{
    if (!m_private)
        return 0;
    
    return m_private->fileSize();
}

bool MediaPlayer::ended() const
{
    return m_private->ended();
}

std::optional<VideoPlaybackQualityMetrics> MediaPlayer::videoPlaybackQualityMetrics()
{
    if (!m_private)
        return std::nullopt;

    return m_private->videoPlaybackQualityMetrics();
}

String MediaPlayer::sourceApplicationIdentifier() const
{
    return client().mediaPlayerSourceApplicationIdentifier();
}

Vector<String> MediaPlayer::preferredAudioCharacteristics() const
{
    return client().mediaPlayerPreferredAudioCharacteristics();
}

void MediaPlayerFactorySupport::callRegisterMediaEngine(MediaEngineRegister registerMediaEngine)
{
    registerMediaEngine(addMediaEngine);
}

bool MediaPlayer::doesHaveAttribute(const AtomString& attribute, AtomString* value) const
{
    return client().doesHaveAttribute(attribute, value);
}

#if PLATFORM(IOS_FAMILY)
String MediaPlayer::mediaPlayerNetworkInterfaceName() const
{
    return client().mediaPlayerNetworkInterfaceName();
}

void MediaPlayer::getRawCookies(const URL& url, MediaPlayerClient::GetRawCookiesCallback&& completionHandler) const
{
    client().mediaPlayerGetRawCookies(url, WTFMove(completionHandler));
}
#endif

void MediaPlayer::setShouldDisableSleep(bool flag)
{
    if (m_private)
        m_private->setShouldDisableSleep(flag);
}

bool MediaPlayer::shouldDisableSleep() const
{
    return client().mediaPlayerShouldDisableSleep();
}

const Vector<ContentType>& MediaPlayer::mediaContentTypesRequiringHardwareSupport() const
{
    return client().mediaContentTypesRequiringHardwareSupport();
}

bool MediaPlayer::shouldCheckHardwareSupport() const
{
    return client().mediaPlayerShouldCheckHardwareSupport();
}

const std::optional<Vector<String>>& MediaPlayer::allowedMediaContainerTypes() const
{
    return client().allowedMediaContainerTypes();
}

const std::optional<Vector<String>>& MediaPlayer::allowedMediaCodecTypes() const
{
    return client().allowedMediaCodecTypes();
}

const std::optional<Vector<FourCC>>& MediaPlayer::allowedMediaVideoCodecIDs() const
{
    return client().allowedMediaVideoCodecIDs();
}

const std::optional<Vector<FourCC>>& MediaPlayer::allowedMediaAudioCodecIDs() const
{
    return client().allowedMediaAudioCodecIDs();
}

const std::optional<Vector<FourCC>>& MediaPlayer::allowedMediaCaptionFormatTypes() const
{
    return client().allowedMediaCaptionFormatTypes();
}

void MediaPlayer::applicationWillResignActive()
{
    m_private->applicationWillResignActive();
}

void MediaPlayer::applicationDidBecomeActive()
{
    m_private->applicationDidBecomeActive();
}

#if USE(AVFOUNDATION)

AVPlayer* MediaPlayer::objCAVFoundationAVPlayer() const
{
    return m_private->objCAVFoundationAVPlayer();
}

#endif

bool MediaPlayer::performTaskAtMediaTime(Function<void()>&& task, const MediaTime& time)
{
    return m_private->performTaskAtMediaTime(WTFMove(task), time);
}

bool MediaPlayer::shouldIgnoreIntrinsicSize()
{
    return m_private->shouldIgnoreIntrinsicSize();
}

void MediaPlayer::isLoopingChanged()
{
    m_private->isLoopingChanged();
}

void MediaPlayer::remoteEngineFailedToLoad()
{
    client().mediaPlayerEngineFailedToLoad();
}

SecurityOriginData MediaPlayer::documentSecurityOrigin() const
{
    return client().documentSecurityOrigin();
}

void MediaPlayer::setPreferredDynamicRangeMode(DynamicRangeMode mode)
{
    m_preferredDynamicRangeMode = mode;
    m_private->setPreferredDynamicRangeMode(mode);
}

void MediaPlayer::audioOutputDeviceChanged()
{
    m_private->audioOutputDeviceChanged();
}

MediaPlayerIdentifier MediaPlayer::identifier() const
{
    return m_private->identifier();
}

std::optional<VideoFrameMetadata> MediaPlayer::videoFrameMetadata()
{
    return m_private->videoFrameMetadata();
}

void MediaPlayer::startVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = true;
    m_private->startVideoFrameMetadataGathering();
}

void MediaPlayer::stopVideoFrameMetadataGathering()
{
    m_isGatheringVideoFrameMetadata = false;
    m_private->stopVideoFrameMetadataGathering();
}

void MediaPlayer::renderVideoWillBeDestroyed()
{
    m_private->renderVideoWillBeDestroyed();
}

void MediaPlayer::setShouldDisableHDR(bool shouldDisable)
{
    m_private->setShouldDisableHDR(shouldDisable);
}

void MediaPlayer::playerContentBoxRectChanged(const LayoutRect& rect)
{
    m_private->playerContentBoxRectChanged(rect);
}

#if PLATFORM(COCOA)
void MediaPlayer::onNewVideoFrameMetadata(VideoFrameMetadata&& metadata, RetainPtr<CVPixelBufferRef>&& buffer)
{
    client().mediaPlayerOnNewVideoFrameMetadata(WTFMove(metadata), WTFMove(buffer));
}
#endif

String MediaPlayer::elementId() const
{
    return client().mediaPlayerElementId();
}

bool MediaPlayer::supportsPlayAtHostTime() const
{
    return m_private->supportsPlayAtHostTime();
}

bool MediaPlayer::supportsPauseAtHostTime() const
{
    return m_private->supportsPauseAtHostTime();
}

bool MediaPlayer::playAtHostTime(const MonotonicTime& hostTime)
{
    // It is invalid to call playAtHostTime() if the underlying
    // media player does not support it.
    ASSERT(supportsPlayAtHostTime());
    return m_private->playAtHostTime(hostTime);
}

bool MediaPlayer::pauseAtHostTime(const MonotonicTime& hostTime)
{
    // It is invalid to call pauseAtHostTime() if the underlying
    // media player does not support it.
    ASSERT(supportsPauseAtHostTime());
    return m_private->pauseAtHostTime(hostTime);
}

#if !RELEASE_LOG_DISABLED
const Logger& MediaPlayer::mediaPlayerLogger()
{
    return client().mediaPlayerLogger();
}
#endif

String convertEnumerationToString(MediaPlayer::ReadyState enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
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
    static const NeverDestroyed<String> values[] = {
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
    static const NeverDestroyed<String> values[] = {
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
    static const NeverDestroyed<String> values[] = {
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
        ts << "resize";
        break;
    case MediaPlayerEnums::VideoGravity::ResizeAspect:
        ts << "resize-aspect";
        break;
    case MediaPlayerEnums::VideoGravity::ResizeAspectFill:
        ts << "resize-aspect-fill";
        break;
    }
    return ts;
}

String convertEnumerationToString(MediaPlayer::BufferingPolicy enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
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

}

#endif
