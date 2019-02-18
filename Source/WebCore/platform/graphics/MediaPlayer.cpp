/*
 * Copyright (C) 2007-2018 Apple Inc. All rights reserved.
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

#include "ContentType.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "IntRect.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayerPrivate.h"
#include "PlatformTimeRanges.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

#if ENABLE(VIDEO_TRACK)
#include "InbandTextTrackPrivate.h"
#endif

#if ENABLE(MEDIA_SOURCE)
#include "MediaSourcePrivateClient.h"
#endif

#if ENABLE(MEDIA_STREAM)
#include "MediaStreamPrivate.h"
#endif

#if USE(GSTREAMER)
#include "MediaPlayerPrivateGStreamer.h"
#define PlatformMediaEngineClassName MediaPlayerPrivateGStreamer
#if ENABLE(MEDIA_SOURCE) && ENABLE(VIDEO_TRACK)
#include "MediaPlayerPrivateGStreamerMSE.h"
#endif
#endif // USE(GSTREAMER)

#if USE(MEDIA_FOUNDATION)
#include "MediaPlayerPrivateMediaFoundation.h"
#define PlatformMediaEngineClassName MediaPlayerPrivateMediaFoundation
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

#endif // PLATFORM(COCOA)

#if PLATFORM(WIN) && USE(AVFOUNDATION) && !USE(GSTREAMER)
#include "MediaPlayerPrivateAVFoundationCF.h"
#endif

namespace WebCore {

#if !RELEASE_LOG_DISABLED
static RefPtr<Logger>& nullLogger()
{
    static NeverDestroyed<RefPtr<Logger>> logger;
    return logger;
}
#endif

// a null player to make MediaPlayer logic simpler

class NullMediaPlayerPrivate : public MediaPlayerPrivateInterface {
public:
    explicit NullMediaPlayerPrivate(MediaPlayer*) { }

    void load(const String&) override { }
#if ENABLE(MEDIA_SOURCE)
    void load(const String&, MediaSourcePrivateClient*) override { }
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override { }
#endif
    void cancelLoad() override { }

    void prepareToPlay() override { }
    void play() override { }
    void pause() override { }

    PlatformLayer* platformLayer() const override { return 0; }

    FloatSize naturalSize() const override { return FloatSize(); }

    bool hasVideo() const override { return false; }
    bool hasAudio() const override { return false; }

    void setVisible(bool) override { }

    double durationDouble() const override { return 0; }

    double currentTimeDouble() const override { return 0; }
    void seekDouble(double) override { }
    bool seeking() const override { return false; }

    void setRateDouble(double) override { }
    void setPreservesPitch(bool) override { }
    bool paused() const override { return true; }

    void setVolumeDouble(double) override { }

    bool supportsMuting() const override { return false; }
    void setMuted(bool) override { }

    bool hasClosedCaptions() const override { return false; }
    void setClosedCaptionsVisible(bool) override { };

    MediaPlayer::NetworkState networkState() const override { return MediaPlayer::Empty; }
    MediaPlayer::ReadyState readyState() const override { return MediaPlayer::HaveNothing; }

    float maxTimeSeekable() const override { return 0; }
    double minTimeSeekable() const override { return 0; }
    std::unique_ptr<PlatformTimeRanges> buffered() const override { return std::make_unique<PlatformTimeRanges>(); }

    double seekableTimeRangesLastModifiedTime() const override { return 0; }
    double liveUpdateInterval() const override { return 0; }

    unsigned long long totalBytes() const override { return 0; }
    bool didLoadingProgress() const override { return false; }

    void setSize(const IntSize&) override { }

    void paint(GraphicsContext&, const FloatRect&) override { }

    bool canLoadPoster() const override { return false; }
    void setPoster(const String&) override { }

    bool hasSingleSecurityOrigin() const override { return true; }
};

class NullMediaPlayerClient : public MediaPlayerClient {
public:
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

struct MediaPlayerFactory {
    CreateMediaEnginePlayer constructor;
    MediaEngineSupportedTypes getSupportedTypes;
    MediaEngineSupportsType supportsTypeAndCodecs;
    MediaEngineOriginsInMediaCache originsInMediaCache;
    MediaEngineClearMediaCache clearMediaCache;
    MediaEngineClearMediaCacheForOrigins clearMediaCacheForOrigins;
    MediaEngineSupportsKeySystem supportsKeySystem;
};

static void addMediaEngine(CreateMediaEnginePlayer&&, MediaEngineSupportedTypes, MediaEngineSupportsType, MediaEngineOriginsInMediaCache, MediaEngineClearMediaCache, MediaEngineClearMediaCacheForOrigins, MediaEngineSupportsKeySystem);

static Lock mediaEngineVectorLock;

static bool& haveMediaEnginesVector()
{
    static bool haveVector;
    return haveVector;
}

static Vector<MediaPlayerFactory>& mutableInstalledMediaEnginesVector()
{
    static NeverDestroyed<Vector<MediaPlayerFactory>> installedEngines;
    return installedEngines;
}

static void buildMediaEnginesVector()
{
    ASSERT(mediaEngineVectorLock.isLocked());

#if USE(AVFOUNDATION)
    if (DeprecatedGlobalSettings::isAVFoundationEnabled()) {

#if PLATFORM(COCOA)
        MediaPlayerPrivateAVFoundationObjC::registerMediaEngine(addMediaEngine);
#endif

#if ENABLE(MEDIA_SOURCE)
        MediaPlayerPrivateMediaSourceAVFObjC::registerMediaEngine(addMediaEngine);
#endif

#if ENABLE(MEDIA_STREAM)
        MediaPlayerPrivateMediaStreamAVFObjC::registerMediaEngine(addMediaEngine);
#endif

#if PLATFORM(WIN)
        MediaPlayerPrivateAVFoundationCF::registerMediaEngine(addMediaEngine);
#endif
    }
#endif // USE(AVFOUNDATION)

#if defined(PlatformMediaEngineClassName)
#if USE(GSTREAMER)
    if (DeprecatedGlobalSettings::isGStreamerEnabled())
#endif
        PlatformMediaEngineClassName::registerMediaEngine(addMediaEngine);
#endif

#if USE(GSTREAMER) && ENABLE(MEDIA_SOURCE) && ENABLE(VIDEO_TRACK)
    if (DeprecatedGlobalSettings::isGStreamerEnabled())
        MediaPlayerPrivateGStreamerMSE::registerMediaEngine(addMediaEngine);
#endif

    haveMediaEnginesVector() = true;
}

static const Vector<MediaPlayerFactory>& installedMediaEngines()
{
    {
        auto locker = holdLock(mediaEngineVectorLock);
        if (!haveMediaEnginesVector())
            buildMediaEnginesVector();
    }

    return mutableInstalledMediaEnginesVector();
}

static void addMediaEngine(CreateMediaEnginePlayer&& constructor, MediaEngineSupportedTypes getSupportedTypes, MediaEngineSupportsType supportsType,
    MediaEngineOriginsInMediaCache originsInMediaCache, MediaEngineClearMediaCache clearMediaCache, MediaEngineClearMediaCacheForOrigins clearMediaCacheForOrigins, MediaEngineSupportsKeySystem supportsKeySystem)
{
    ASSERT(constructor);
    ASSERT(getSupportedTypes);
    ASSERT(supportsType);

    mutableInstalledMediaEnginesVector().append(MediaPlayerFactory { WTFMove(constructor), getSupportedTypes, supportsType, originsInMediaCache, clearMediaCache, clearMediaCacheForOrigins, supportsKeySystem });
}

static const AtomicString& applicationOctetStream()
{
    static NeverDestroyed<const AtomicString> applicationOctetStream("application/octet-stream", AtomicString::ConstructFromLiteral);
    return applicationOctetStream;
}

static const AtomicString& textPlain()
{
    static NeverDestroyed<const AtomicString> textPlain("text/plain", AtomicString::ConstructFromLiteral);
    return textPlain;
}

static const MediaPlayerFactory* bestMediaEngineForSupportParameters(const MediaEngineSupportParameters& parameters, const MediaPlayerFactory* current = nullptr)
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
    MediaPlayer::SupportsType supported = MediaPlayer::IsNotSupported;
    for (auto& engine : installedMediaEngines()) {
        if (current) {
            if (current == &engine)
                current = nullptr;
            continue;
        }
        MediaPlayer::SupportsType engineSupport = engine.supportsTypeAndCodecs(parameters);
        if (engineSupport > supported) {
            supported = engineSupport;
            foundEngine = &engine;
        }
    }

    return foundEngine;
}

static const MediaPlayerFactory* nextMediaEngine(const MediaPlayerFactory* current)
{
    auto& engines = installedMediaEngines();
    if (engines.isEmpty())
        return nullptr;

    if (!current) 
        return &engines.first();

    size_t currentIndex = current - &engines.first();
    if (currentIndex + 1 >= engines.size())
        return nullptr;

    return &engines[currentIndex + 1];
}

// media player

Ref<MediaPlayer> MediaPlayer::create(MediaPlayerClient& client)
{
    return adoptRef(*new MediaPlayer(client));
}

MediaPlayer::MediaPlayer(MediaPlayerClient& client)
    : m_client(&client)
    , m_reloadTimer(*this, &MediaPlayer::reloadTimerFired)
    , m_private(std::make_unique<NullMediaPlayerPrivate>(this))
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

bool MediaPlayer::load(const URL& url, const ContentType& contentType, const String& keySystem)
{
    ASSERT(!m_reloadTimer.isActive());

    // Protect against MediaPlayer being destroyed during a MediaPlayerClient callback.
    Ref<MediaPlayer> protectedThis(*this);

    m_contentType = contentType;
    m_url = url;
    m_keySystem = keySystem.convertToASCIILowercase();
    m_contentMIMETypeWasInferredFromExtension = false;

#if ENABLE(MEDIA_SOURCE)
    m_mediaSource = nullptr;
#endif
#if ENABLE(MEDIA_STREAM)
    m_mediaStream = nullptr;
#endif

    // If the MIME type is missing or is not meaningful, try to figure it out from the URL.
    AtomicString containerType = m_contentType.containerType();
    if (containerType.isEmpty() || containerType == applicationOctetStream() || containerType == textPlain()) {
        if (m_url.protocolIsData())
            m_contentType = ContentType(mimeTypeFromDataURL(m_url.string()));
        else {
            String lastPathComponent = url.lastPathComponent();
            size_t pos = lastPathComponent.reverseFind('.');
            if (pos != notFound) {
                String extension = lastPathComponent.substring(pos + 1);
                String mediaType = MIMETypeRegistry::getMediaMIMETypeForExtension(extension);
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
bool MediaPlayer::load(const URL& url, const ContentType& contentType, MediaSourcePrivateClient* mediaSource)
{
    ASSERT(!m_reloadTimer.isActive());
    ASSERT(mediaSource);

    m_mediaSource = mediaSource;
    m_contentType = contentType;
    m_url = url;
    m_keySystem = emptyString();
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
    m_contentType = { };
    m_contentMIMETypeWasInferredFromExtension = false;
    loadWithNextMediaEngine(nullptr);
    return m_currentMediaEngine;
}
#endif

const MediaPlayerFactory* MediaPlayer::nextBestMediaEngine(const MediaPlayerFactory* current) const
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

    return bestMediaEngineForSupportParameters(parameters, current);
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

    const MediaPlayerFactory* engine = nullptr;

    if (!m_contentType.isEmpty() || MEDIASTREAM || MEDIASOURCE)
        engine = nextBestMediaEngine(current);

    // If no MIME type is specified or the type was inferred from the file extension, just use the next engine.
    if (!engine && (m_contentType.isEmpty() || m_contentMIMETypeWasInferredFromExtension))
        engine = nextMediaEngine(current);

    // Don't delete and recreate the player unless it comes from a different engine.
    if (!engine) {
        LOG(Media, "MediaPlayer::loadWithNextMediaEngine - no media engine found for type \"%s\"", m_contentType.raw().utf8().data());
        m_currentMediaEngine = engine;
        m_private = nullptr;
    } else if (m_currentMediaEngine != engine) {
        m_currentMediaEngine = engine;
        m_private = engine->constructor(this);
        client().mediaPlayerEngineUpdated(this);
        m_private->setPrivateBrowsingMode(m_privateBrowsing);
        m_private->setPreload(m_preload);
        m_private->setPreservesPitch(preservesPitch());
        if (m_shouldPrepareToRender)
            m_private->prepareForRendering();
    }

    if (m_private) {
#if ENABLE(MEDIA_SOURCE)
        if (m_mediaSource)
            m_private->load(m_url.string(), m_mediaSource.get());
        else
#endif
#if ENABLE(MEDIA_STREAM)
        if (m_mediaStream)
            m_private->load(*m_mediaStream);
        else
#endif
        m_private->load(m_url.string());
    } else {
        m_private = std::make_unique<NullMediaPlayerPrivate>(this);
        client().mediaPlayerEngineUpdated(this);
        client().mediaPlayerResourceNotSupported(this);
    }

    m_initializingMediaEngine = false;
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

bool MediaPlayer::canLoadPoster() const
{
    return m_private->canLoadPoster();
}

void MediaPlayer::setPoster(const String& url)
{
    m_private->setPoster(url);
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

void MediaPlayer::setShouldBufferData(bool shouldBuffer)
{
    m_private->setShouldBufferData(shouldBuffer);
}

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

std::unique_ptr<LegacyCDMSession> MediaPlayer::createSession(const String& keySystem, LegacyCDMSessionClient* client)
{
    return m_private->createSession(keySystem, client);
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

bool MediaPlayer::inMediaDocument() const
{
    return m_visible && client().mediaPlayerIsInMediaDocument();
}

PlatformLayer* MediaPlayer::platformLayer() const
{
    return m_private->platformLayer();
}
    
#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

void MediaPlayer::setVideoFullscreenLayer(PlatformLayer* layer, WTF::Function<void()>&& completionHandler)
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

MediaPlayer::ReadyState MediaPlayer::readyState()
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

    if (m_private->supportsMuting() || !m_muted)
        m_private->setVolumeDouble(volume);
}

bool MediaPlayer::muted() const
{
    return m_muted;
}

void MediaPlayer::setMuted(bool muted)
{
    m_muted = muted;

    if (m_private->supportsMuting())
        m_private->setMuted(muted);
    else
        m_private->setVolume(muted ? 0 : m_volume);
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

std::unique_ptr<PlatformTimeRanges> MediaPlayer::buffered()
{
    return m_private->buffered();
}

std::unique_ptr<PlatformTimeRanges> MediaPlayer::seekable()
{
    return m_private->seekable();
}

MediaTime MediaPlayer::maxTimeSeekable()
{
    return m_private->maxMediaTimeSeekable();
}

MediaTime MediaPlayer::minTimeSeekable()
{
    return m_private->minMediaTimeSeekable();
}

double MediaPlayer::seekableTimeRangesLastModifiedTime()
{
    return m_private->seekableTimeRangesLastModifiedTime();
}

double MediaPlayer::liveUpdateInterval()
{
    return m_private->liveUpdateInterval();
}

bool MediaPlayer::didLoadingProgress()
{
    return m_private->didLoadingProgress();
}

void MediaPlayer::setSize(const IntSize& size)
{ 
    m_size = size;
    m_private->setSize(size);
}

bool MediaPlayer::visible() const
{
    return m_visible;
}

void MediaPlayer::setVisible(bool b)
{
    m_visible = b;
    m_private->setVisible(b);
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

void MediaPlayer::paintCurrentFrameInContext(GraphicsContext& p, const FloatRect& r)
{
    m_private->paintCurrentFrameInContext(p, r);
}

bool MediaPlayer::copyVideoTextureToPlatformTexture(GraphicsContext3D* context, Platform3DObject texture, GC3Denum target, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY)
{
    return m_private->copyVideoTextureToPlatformTexture(context, texture, target, level, internalFormat, format, type, premultiplyAlpha, flipY);
}

NativeImagePtr MediaPlayer::nativeImageForCurrentTime()
{
    return m_private->nativeImageForCurrentTime();
}

MediaPlayer::SupportsType MediaPlayer::supportsType(const MediaEngineSupportParameters& parameters)
{
    // 4.8.10.3 MIME types - The canPlayType(type) method must return the empty string if type is a type that the 
    // user agent knows it cannot render or is the type "application/octet-stream"
    AtomicString containerType = parameters.type.containerType();
    if (containerType == applicationOctetStream())
        return IsNotSupported;

    const MediaPlayerFactory* engine = bestMediaEngineForSupportParameters(parameters);
    if (!engine)
        return IsNotSupported;

    return engine->supportsTypeAndCodecs(parameters);
}

void MediaPlayer::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    for (auto& engine : installedMediaEngines()) {
        HashSet<String, ASCIICaseInsensitiveHash> engineTypes;
        engine.getSupportedTypes(engineTypes);
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

#if USE(NATIVE_FULLSCREEN_VIDEO)

void MediaPlayer::enterFullscreen()
{
    m_private->enterFullscreen();
}

void MediaPlayer::exitFullscreen()
{
    m_private->exitFullscreen();
}

#endif

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

void MediaPlayer::currentPlaybackTargetIsWirelessChanged()
{
    client().mediaPlayerCurrentPlaybackTargetIsWirelessChanged(this);
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

#if USE(NATIVE_FULLSCREEN_VIDEO)

bool MediaPlayer::canEnterFullscreen() const
{
    return m_private->canEnterFullscreen();
}

#endif

void MediaPlayer::acceleratedRenderingStateChanged()
{
    m_private->acceleratedRenderingStateChanged();
}

bool MediaPlayer::supportsAcceleratedRendering() const
{
    return m_private->supportsAcceleratedRendering();
}

bool MediaPlayer::shouldMaintainAspectRatio() const
{
    return m_private->shouldMaintainAspectRatio();
}

void MediaPlayer::setShouldMaintainAspectRatio(bool maintainAspectRatio)
{
    m_private->setShouldMaintainAspectRatio(maintainAspectRatio);
}

bool MediaPlayer::hasSingleSecurityOrigin() const
{
    return m_private->hasSingleSecurityOrigin();
}

bool MediaPlayer::didPassCORSAccessCheck() const
{
    return m_private->didPassCORSAccessCheck();
}

bool MediaPlayer::wouldTaintOrigin(const SecurityOrigin& origin) const
{
    auto wouldTaint = m_private->wouldTaintOrigin(origin);
    if (wouldTaint.hasValue())
        return wouldTaint.value();

    if (m_url.protocolIsData())
        return false;

    return !origin.canRequest(m_url);
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
    
HashSet<RefPtr<SecurityOrigin>> MediaPlayer::originsInMediaCache(const String& path)
{
    HashSet<RefPtr<SecurityOrigin>> origins;
    for (auto& engine : installedMediaEngines()) {
        if (!engine.originsInMediaCache)
            continue;
        addToHash(origins, engine.originsInMediaCache(path));
    }
    return origins;
}

void MediaPlayer::clearMediaCache(const String& path, WallTime modifiedSince)
{
    for (auto& engine : installedMediaEngines()) {
        if (engine.clearMediaCache)
            engine.clearMediaCache(path, modifiedSince);
    }
}

void MediaPlayer::clearMediaCacheForOrigins(const String& path, const HashSet<RefPtr<SecurityOrigin>>& origins)
{
    for (auto& engine : installedMediaEngines()) {
        if (engine.clearMediaCacheForOrigins)
            engine.clearMediaCacheForOrigins(path, origins);
    }
}

bool MediaPlayer::supportsKeySystem(const String& keySystem, const String& mimeType)
{
    for (auto& engine : installedMediaEngines()) {
        if (engine.supportsKeySystem && engine.supportsKeySystem(keySystem, mimeType))
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
    // If more than one media engine is installed and this one failed before finding metadata,
    // let the next engine try.
    if (m_private->networkState() >= FormatError && m_private->readyState() < HaveMetadata) {
        client().mediaPlayerEngineFailedToLoad();
        if (installedMediaEngines().size() > 1 && (m_contentType.isEmpty() || nextBestMediaEngine(m_currentMediaEngine))) {
            m_reloadTimer.startOneShot(0_s);
            return;
        }
    }
    client().mediaPlayerNetworkStateChanged(this);
}

void MediaPlayer::readyStateChanged()
{
    client().mediaPlayerReadyStateChanged(this);
}

void MediaPlayer::volumeChanged(double newVolume)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(newVolume);
    m_volume = m_private->volume();
#else
    m_volume = newVolume;
#endif
    client().mediaPlayerVolumeChanged(this);
}

void MediaPlayer::muteChanged(bool newMuted)
{
    if (newMuted == m_muted)
        return;

    m_muted = newMuted;
    client().mediaPlayerMuteChanged(this);
}

void MediaPlayer::timeChanged()
{
    client().mediaPlayerTimeChanged(this);
}

void MediaPlayer::sizeChanged()
{
    client().mediaPlayerSizeChanged(this);
}

void MediaPlayer::repaint()
{
    client().mediaPlayerRepaint(this);
}

void MediaPlayer::durationChanged()
{
    client().mediaPlayerDurationChanged(this);
}

void MediaPlayer::rateChanged()
{
    client().mediaPlayerRateChanged(this);
}

void MediaPlayer::playbackStateChanged()
{
    client().mediaPlayerPlaybackStateChanged(this);
}

void MediaPlayer::firstVideoFrameAvailable()
{
    client().mediaPlayerFirstVideoFrameAvailable(this);
}

void MediaPlayer::characteristicChanged()
{
    client().mediaPlayerCharacteristicChanged(this);
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

bool MediaPlayer::keyNeeded(Uint8Array* initData)
{
    return client().mediaPlayerKeyNeeded(this, initData);
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

#if PLATFORM(WIN) && USE(AVFOUNDATION)
GraphicsDeviceAdapter* MediaPlayer::graphicsDeviceAdapter() const
{
    return client().mediaPlayerGraphicsDeviceAdapter(this);
}
#endif

CachedResourceLoader* MediaPlayer::cachedResourceLoader()
{
    return client().mediaPlayerCachedResourceLoader();
}

RefPtr<PlatformMediaResourceLoader> MediaPlayer::createResourceLoader()
{
    return client().mediaPlayerCreateResourceLoader();
}

#if ENABLE(VIDEO_TRACK)

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

#if ENABLE(AVF_CAPTIONS)

void MediaPlayer::notifyTrackModeChanged()
{
    if (m_private)
        m_private->notifyTrackModeChanged();
}

Vector<RefPtr<PlatformTextTrack>> MediaPlayer::outOfBandTrackSources()
{
    return client().outOfBandTrackSources();
}

#endif

#endif // ENABLE(VIDEO_TRACK)

void MediaPlayer::resetMediaEngines()
{
    auto locker = holdLock(mediaEngineVectorLock);

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

Optional<VideoPlaybackQualityMetrics> MediaPlayer::videoPlaybackQualityMetrics()
{
    if (!m_private)
        return WTF::nullopt;

    return m_private->videoPlaybackQualityMetrics();
}

void MediaPlayer::handlePlaybackCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    client().mediaPlayerHandlePlaybackCommand(command);
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

bool MediaPlayer::doesHaveAttribute(const AtomicString& attribute, AtomicString* value) const
{
    return client().doesHaveAttribute(attribute, value);
}

#if PLATFORM(IOS_FAMILY)
String MediaPlayer::mediaPlayerNetworkInterfaceName() const
{
    return client().mediaPlayerNetworkInterfaceName();
}

bool MediaPlayer::getRawCookies(const URL& url, Vector<Cookie>& cookies) const
{
    return client().mediaPlayerGetRawCookies(url, cookies);
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

bool MediaPlayer::performTaskAtMediaTime(WTF::Function<void()>&& task, MediaTime time)
{
    return m_private->performTaskAtMediaTime(WTFMove(task), time);
}

bool MediaPlayer::shouldIgnoreIntrinsicSize()
{
    return m_private->shouldIgnoreIntrinsicSize();
}

#if !RELEASE_LOG_DISABLED
const Logger& MediaPlayer::mediaPlayerLogger()
{
    return client().mediaPlayerLogger();
}
#endif

String convertEnumerationToString(MediaPlayerEnums::ReadyState enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("HaveNothing"),
        MAKE_STATIC_STRING_IMPL("HaveMetadata"),
        MAKE_STATIC_STRING_IMPL("HaveCurrentData"),
        MAKE_STATIC_STRING_IMPL("HaveFutureData"),
        MAKE_STATIC_STRING_IMPL("HaveEnoughData"),
    };
    static_assert(static_cast<size_t>(MediaPlayerEnums::HaveNothing) == 0, "MediaPlayerEnums::HaveNothing is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::HaveMetadata) == 1, "MediaPlayerEnums::HaveMetadata is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::HaveCurrentData) == 2, "MediaPlayerEnums::HaveCurrentData is not 2 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::HaveFutureData) == 3, "MediaPlayerEnums::HaveFutureData is not 3 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::HaveEnoughData) == 4, "MediaPlayerEnums::HaveEnoughData is not 4 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(MediaPlayerEnums::NetworkState enumerationValue)
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
    static_assert(static_cast<size_t>(MediaPlayerEnums::Empty) == 0, "MediaPlayerEnums::Empty is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::Idle) == 1, "MediaPlayerEnums::Idle is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::Loading) == 2, "MediaPlayerEnums::Loading is not 2 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::Loaded) == 3, "MediaPlayerEnums::Loaded is not 3 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::FormatError) == 4, "MediaPlayerEnums::FormatError is not 4 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::NetworkError) == 5, "MediaPlayerEnums::NetworkError is not 5 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::DecodeError) == 6, "MediaPlayerEnums::DecodeError is not 6 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(MediaPlayerEnums::Preload enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("None"),
        MAKE_STATIC_STRING_IMPL("MetaData"),
        MAKE_STATIC_STRING_IMPL("Auto"),
    };
    static_assert(!static_cast<size_t>(MediaPlayerEnums::None), "MediaPlayerEnums::None is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::MetaData) == 1, "MediaPlayerEnums::MetaData is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::Auto) == 2, "MediaPlayerEnums::Auto is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String convertEnumerationToString(MediaPlayerEnums::SupportsType enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("IsNotSupported"),
        MAKE_STATIC_STRING_IMPL("IsSupported"),
        MAKE_STATIC_STRING_IMPL("MayBeSupported"),
    };
    static_assert(!static_cast<size_t>(MediaPlayerEnums::IsNotSupported), "MediaPlayerEnums::IsNotSupported is not 0 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::IsSupported) == 1, "MediaPlayerEnums::IsSupported is not 1 as expected");
    static_assert(static_cast<size_t>(MediaPlayerEnums::MayBeSupported) == 2, "MediaPlayerEnums::MayBeSupported is not 2 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < WTF_ARRAY_LENGTH(values));
    return values[static_cast<size_t>(enumerationValue)];
}

}

#endif
