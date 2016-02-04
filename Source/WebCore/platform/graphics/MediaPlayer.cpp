/*
 * Copyright (C) 2007-2015 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)
#include "MediaPlayer.h"

#include "ContentType.h"
#include "Document.h"
#include "IntRect.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayerPrivate.h"
#include "PlatformTimeRanges.h"
#include "Settings.h"
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
#if ENABLE(MEDIA_STREAM) && USE(OPENWEBRTC)
#include "MediaPlayerPrivateGStreamerOwr.h"
#endif
#define PlatformMediaEngineClassName MediaPlayerPrivateGStreamer
#endif

#if USE(MEDIA_FOUNDATION)
#include "MediaPlayerPrivateMediaFoundation.h"
#define PlatformMediaEngineClassName MediaPlayerPrivateMediaFoundation
#endif

#if PLATFORM(COCOA)
#include "MediaPlayerPrivateQTKit.h"

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

const PlatformMedia NoPlatformMedia = { PlatformMedia::None, {0} };

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

    PlatformMedia platformMedia() const override { return NoPlatformMedia; }
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
    bool paused() const override { return false; }

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

    unsigned long long totalBytes() const override { return 0; }
    bool didLoadingProgress() const override { return false; }

    void setSize(const IntSize&) override { }

    void paint(GraphicsContext&, const FloatRect&) override { }

    bool canLoadPoster() const override { return false; }
    void setPoster(const String&) override { }

    bool hasSingleSecurityOrigin() const override { return true; }

#if ENABLE(ENCRYPTED_MEDIA)
    MediaPlayer::MediaKeyException generateKeyRequest(const String&, const unsigned char*, unsigned) override { return MediaPlayer::InvalidPlayerState; }
    MediaPlayer::MediaKeyException addKey(const String&, const unsigned char*, unsigned, const unsigned char*, unsigned, const String&) override { return MediaPlayer::InvalidPlayerState; }
    MediaPlayer::MediaKeyException cancelKeyRequest(const String&, const String&) override { return MediaPlayer::InvalidPlayerState; }
#endif
};

// engine support

struct MediaPlayerFactory {
    CreateMediaEnginePlayer constructor;
    MediaEngineSupportedTypes getSupportedTypes;
    MediaEngineSupportsType supportsTypeAndCodecs;
    MediaEngineGetSitesInMediaCache getSitesInMediaCache;
    MediaEngineClearMediaCache clearMediaCache;
    MediaEngineClearMediaCacheForSite clearMediaCacheForSite;
    MediaEngineSupportsKeySystem supportsKeySystem;
};

static void addMediaEngine(CreateMediaEnginePlayer, MediaEngineSupportedTypes, MediaEngineSupportsType, MediaEngineGetSitesInMediaCache, MediaEngineClearMediaCache, MediaEngineClearMediaCacheForSite, MediaEngineSupportsKeySystem);

static bool haveMediaEnginesVector;

static Vector<MediaPlayerFactory>& mutableInstalledMediaEnginesVector()
{
    static NeverDestroyed<Vector<MediaPlayerFactory>> installedEngines;
    return installedEngines;
}

static void buildMediaEnginesVector()
{
#if USE(AVFOUNDATION)
    if (Settings::isAVFoundationEnabled()) {

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

#if PLATFORM(MAC)
    if (Settings::isQTKitEnabled())
        MediaPlayerPrivateQTKit::registerMediaEngine(addMediaEngine);
#endif


#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER) && USE(OPENWEBRTC)
    MediaPlayerPrivateGStreamerOwr::registerMediaEngine(addMediaEngine);
#endif

#if defined(PlatformMediaEngineClassName)
    PlatformMediaEngineClassName::registerMediaEngine(addMediaEngine);
#endif

    haveMediaEnginesVector = true;
}

static const Vector<MediaPlayerFactory>& installedMediaEngines()
{
    if (!haveMediaEnginesVector)
        buildMediaEnginesVector();
    return mutableInstalledMediaEnginesVector();
}

static void addMediaEngine(CreateMediaEnginePlayer constructor, MediaEngineSupportedTypes getSupportedTypes, MediaEngineSupportsType supportsType,
    MediaEngineGetSitesInMediaCache getSitesInMediaCache, MediaEngineClearMediaCache clearMediaCache, MediaEngineClearMediaCacheForSite clearMediaCacheForSite, MediaEngineSupportsKeySystem supportsKeySystem)
{
    ASSERT(constructor);
    ASSERT(getSupportedTypes);
    ASSERT(supportsType);

    mutableInstalledMediaEnginesVector().append(MediaPlayerFactory { constructor, getSupportedTypes, supportsType, getSitesInMediaCache, clearMediaCache, clearMediaCacheForSite, supportsKeySystem });
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

static const AtomicString& codecs()
{
    static NeverDestroyed<const AtomicString> codecs("codecs", AtomicString::ConstructFromLiteral);
    return codecs;
}

static const MediaPlayerFactory* bestMediaEngineForSupportParameters(const MediaEngineSupportParameters& parameters, const MediaPlayerFactory* current = nullptr)
{
    if (parameters.type.isEmpty() && !parameters.isMediaSource && !parameters.isMediaStream)
        return nullptr;

    // 4.8.10.3 MIME types - In the absence of a specification to the contrary, the MIME type "application/octet-stream"
    // when used with parameters, e.g. "application/octet-stream;codecs=theora", is a type that the user agent knows 
    // it cannot render.
    if (parameters.type == applicationOctetStream()) {
        if (!parameters.codecs.isEmpty())
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

MediaPlayer::MediaPlayer(MediaPlayerClient& client)
    : m_client(client)
    , m_reloadTimer(*this, &MediaPlayer::reloadTimerFired)
    , m_private(std::make_unique<NullMediaPlayerPrivate>(this))
    , m_currentMediaEngine(0)
    , m_preload(Auto)
    , m_visible(false)
    , m_volume(1.0f)
    , m_muted(false)
    , m_preservesPitch(true)
    , m_privateBrowsing(false)
    , m_shouldPrepareToRender(false)
    , m_contentMIMETypeWasInferredFromExtension(false)
{
}

MediaPlayer::~MediaPlayer()
{
}

bool MediaPlayer::load(const URL& url, const ContentType& contentType, const String& keySystem)
{
    ASSERT(!m_reloadTimer.isActive());

    m_contentMIMEType = contentType.type().convertToASCIILowercase();
    m_contentTypeCodecs = contentType.parameter(codecs());
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
    if (m_contentMIMEType.isEmpty() || m_contentMIMEType == applicationOctetStream() || m_contentMIMEType == textPlain()) {
        if (m_url.protocolIsData())
            m_contentMIMEType = mimeTypeFromDataURL(m_url.string());
        else {
            String lastPathComponent = url.lastPathComponent();
            size_t pos = lastPathComponent.reverseFind('.');
            if (pos != notFound) {
                String extension = lastPathComponent.substring(pos + 1);
                String mediaType = MIMETypeRegistry::getMediaMIMETypeForExtension(extension);
                if (!mediaType.isEmpty()) {
                    m_contentMIMEType = mediaType;
                    m_contentMIMETypeWasInferredFromExtension = true;
                }
            }
        }
    }

    loadWithNextMediaEngine(0);
    return m_currentMediaEngine;
}

#if ENABLE(MEDIA_SOURCE)
bool MediaPlayer::load(const URL& url, const ContentType& contentType, MediaSourcePrivateClient* mediaSource)
{
    ASSERT(!m_reloadTimer.isActive());
    ASSERT(mediaSource);

    m_mediaSource = mediaSource;
    m_contentMIMEType = contentType.type().convertToASCIILowercase();
    m_contentTypeCodecs = contentType.parameter(codecs());
    m_url = url;
    m_keySystem = "";
    m_contentMIMETypeWasInferredFromExtension = false;
    loadWithNextMediaEngine(0);
    return m_currentMediaEngine;
}
#endif

#if ENABLE(MEDIA_STREAM)
bool MediaPlayer::load(MediaStreamPrivate* mediaStream)
{
    ASSERT(!m_reloadTimer.isActive());
    ASSERT(mediaStream);

    m_mediaStream = mediaStream;
    m_keySystem = "";
    m_contentMIMEType = "";
    m_contentMIMETypeWasInferredFromExtension = false;
    loadWithNextMediaEngine(0);
    return m_currentMediaEngine;
}
#endif

const MediaPlayerFactory* MediaPlayer::nextBestMediaEngine(const MediaPlayerFactory* current) const
{
    MediaEngineSupportParameters parameters;
    parameters.type = m_contentMIMEType;
    parameters.codecs = m_contentTypeCodecs;
    parameters.url = m_url;
#if ENABLE(ENCRYPTED_MEDIA)
    parameters.keySystem = m_keySystem;
#endif
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

    const MediaPlayerFactory* engine = nullptr;

    if (!m_contentMIMEType.isEmpty() || MEDIASTREAM || MEDIASOURCE)
        engine = nextBestMediaEngine(current);

    // If no MIME type is specified or the type was inferred from the file extension, just use the next engine.
    if (!engine && (m_contentMIMEType.isEmpty() || m_contentMIMETypeWasInferredFromExtension))
        engine = nextMediaEngine(current);

    // Don't delete and recreate the player unless it comes from a different engine.
    if (!engine) {
        LOG(Media, "MediaPlayer::loadWithNextMediaEngine - no media engine found for type \"%s\"", m_contentMIMEType.utf8().data());
        m_currentMediaEngine = engine;
        m_private = nullptr;
    } else if (m_currentMediaEngine != engine) {
        m_currentMediaEngine = engine;
        m_private = engine->constructor(this);
        m_client.mediaPlayerEngineUpdated(this);
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
        m_client.mediaPlayerEngineUpdated(this);
        m_client.mediaPlayerResourceNotSupported(this);
    }
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

#if ENABLE(ENCRYPTED_MEDIA)
MediaPlayer::MediaKeyException MediaPlayer::generateKeyRequest(const String& keySystem, const unsigned char* initData, unsigned initDataLength)
{
    return m_private->generateKeyRequest(keySystem.convertToASCIILowercase(), initData, initDataLength);
}

MediaPlayer::MediaKeyException MediaPlayer::addKey(const String& keySystem, const unsigned char* key, unsigned keyLength, const unsigned char* initData, unsigned initDataLength, const String& sessionId)
{
    return m_private->addKey(keySystem.convertToASCIILowercase(), key, keyLength, initData, initDataLength, sessionId);
}

MediaPlayer::MediaKeyException MediaPlayer::cancelKeyRequest(const String& keySystem, const String& sessionId)
{
    return m_private->cancelKeyRequest(keySystem.convertToASCIILowercase(), sessionId);
}
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
std::unique_ptr<CDMSession> MediaPlayer::createSession(const String& keySystem, CDMSessionClient* client)
{
    return m_private->createSession(keySystem, client);
}

void MediaPlayer::setCDMSession(CDMSession* session)
{
    m_private->setCDMSession(session);
}

void MediaPlayer::keyAdded()
{
    m_private->keyAdded();
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
    return m_visible && m_client.mediaPlayerIsInMediaDocument();
}

PlatformMedia MediaPlayer::platformMedia() const
{
    return m_private->platformMedia();
}

PlatformLayer* MediaPlayer::platformLayer() const
{
    return m_private->platformLayer();
}
    
#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
void MediaPlayer::setVideoFullscreenLayer(PlatformLayer* layer)
{
    m_private->setVideoFullscreenLayer(layer);
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
    return m_client.mediaPlayerFullscreenMode();
}
#endif

#if PLATFORM(IOS)
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
    return m_client.mediaPlayerRequestedPlaybackRate();
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

PassNativeImagePtr MediaPlayer::nativeImageForCurrentTime()
{
    return m_private->nativeImageForCurrentTime();
}

MediaPlayer::SupportsType MediaPlayer::supportsType(const MediaEngineSupportParameters& parameters, const MediaPlayerSupportsTypeClient* client)
{
    // 4.8.10.3 MIME types - The canPlayType(type) method must return the empty string if type is a type that the 
    // user agent knows it cannot render or is the type "application/octet-stream"
    if (parameters.type == applicationOctetStream())
        return IsNotSupported;

    const MediaPlayerFactory* engine = bestMediaEngineForSupportParameters(parameters);
    if (!engine)
        return IsNotSupported;

#if PLATFORM(COCOA)
    // YouTube will ask if the HTMLMediaElement canPlayType video/webm, then
    // video/x-flv, then finally video/mp4, and will then load a URL of the first type
    // in that list which returns "probably". When Perian is installed,
    // MediaPlayerPrivateQTKit claims to support both video/webm and video/x-flv, but
    // due to a bug in Perian, loading media in these formats will sometimes fail on
    // slow connections. <https://bugs.webkit.org/show_bug.cgi?id=86409>
    if (client && client->mediaPlayerNeedsSiteSpecificHacks()) {
        String host = client->mediaPlayerDocumentHost();
        if ((host.endsWith(".youtube.com", false) || equalLettersIgnoringASCIICase(host, "youtube.com"))
            && (parameters.type.startsWith("video/webm", false) || parameters.type.startsWith("video/x-flv", false)))
            return IsNotSupported;
    }
#else
    UNUSED_PARAM(client);
#endif

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
    return !installedMediaEngines().isEmpty();
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
    m_client.mediaPlayerCurrentPlaybackTargetIsWirelessChanged(this);
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

void MediaPlayer::getSitesInMediaCache(Vector<String>& sites)
{
    for (auto& engine : installedMediaEngines()) {
        if (!engine.getSitesInMediaCache)
            continue;
        Vector<String> engineSites;
        engine.getSitesInMediaCache(engineSites);
        sites.appendVector(engineSites);
    }
}

void MediaPlayer::clearMediaCache()
{
    for (auto& engine : installedMediaEngines()) {
        if (engine.clearMediaCache)
            engine.clearMediaCache();
    }
}

void MediaPlayer::clearMediaCacheForSite(const String& site)
{
    for (auto& engine : installedMediaEngines()) {
        if (engine.clearMediaCacheForSite)
            engine.clearMediaCacheForSite(site);
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
    m_private->setPrivateBrowsingMode(m_privateBrowsing);
}

// Client callbacks.
void MediaPlayer::networkStateChanged()
{
    // If more than one media engine is installed and this one failed before finding metadata,
    // let the next engine try.
    if (m_private->networkState() >= FormatError && m_private->readyState() < HaveMetadata) {
        m_client.mediaPlayerEngineFailedToLoad();
        if (installedMediaEngines().size() > 1 && (m_contentMIMEType.isEmpty() || nextBestMediaEngine(m_currentMediaEngine))) {
            m_reloadTimer.startOneShot(0);
            return;
        }
    }
    m_client.mediaPlayerNetworkStateChanged(this);
}

void MediaPlayer::readyStateChanged()
{
    m_client.mediaPlayerReadyStateChanged(this);
}

void MediaPlayer::volumeChanged(double newVolume)
{
#if PLATFORM(IOS)
    UNUSED_PARAM(newVolume);
    m_volume = m_private->volume();
#else
    m_volume = newVolume;
#endif
    m_client.mediaPlayerVolumeChanged(this);
}

void MediaPlayer::muteChanged(bool newMuted)
{
    m_muted = newMuted;
    m_client.mediaPlayerMuteChanged(this);
}

void MediaPlayer::timeChanged()
{
    m_client.mediaPlayerTimeChanged(this);
}

void MediaPlayer::sizeChanged()
{
    m_client.mediaPlayerSizeChanged(this);
}

void MediaPlayer::repaint()
{
    m_client.mediaPlayerRepaint(this);
}

void MediaPlayer::durationChanged()
{
    m_client.mediaPlayerDurationChanged(this);
}

void MediaPlayer::rateChanged()
{
    m_client.mediaPlayerRateChanged(this);
}

void MediaPlayer::playbackStateChanged()
{
    m_client.mediaPlayerPlaybackStateChanged(this);
}

void MediaPlayer::firstVideoFrameAvailable()
{
    m_client.mediaPlayerFirstVideoFrameAvailable(this);
}

void MediaPlayer::characteristicChanged()
{
    m_client.mediaPlayerCharacteristicChanged(this);
}

#if ENABLE(WEB_AUDIO)
AudioSourceProvider* MediaPlayer::audioSourceProvider()
{
    return m_private->audioSourceProvider();
}
#endif // WEB_AUDIO

#if ENABLE(ENCRYPTED_MEDIA)
void MediaPlayer::keyAdded(const String& keySystem, const String& sessionId)
{
    m_client.mediaPlayerKeyAdded(this, keySystem, sessionId);
}

void MediaPlayer::keyError(const String& keySystem, const String& sessionId, MediaPlayerClient::MediaKeyErrorCode errorCode, unsigned short systemCode)
{
    m_client.mediaPlayerKeyError(this, keySystem, sessionId, errorCode, systemCode);
}

void MediaPlayer::keyMessage(const String& keySystem, const String& sessionId, const unsigned char* message, unsigned messageLength, const URL& defaultURL)
{
    m_client.mediaPlayerKeyMessage(this, keySystem, sessionId, message, messageLength, defaultURL);
}

bool MediaPlayer::keyNeeded(const String& keySystem, const String& sessionId, const unsigned char* initData, unsigned initDataLength)
{
    return m_client.mediaPlayerKeyNeeded(this, keySystem, sessionId, initData, initDataLength);
}
#endif

#if ENABLE(ENCRYPTED_MEDIA_V2)
RefPtr<ArrayBuffer> MediaPlayer::cachedKeyForKeyId(const String& keyId) const
{
    return m_client.mediaPlayerCachedKeyForKeyId(keyId);
}

bool MediaPlayer::keyNeeded(Uint8Array* initData)
{
    return m_client.mediaPlayerKeyNeeded(this, initData);
}

String MediaPlayer::mediaKeysStorageDirectory() const
{
    return m_client.mediaPlayerMediaKeysStorageDirectory();
}
#endif

String MediaPlayer::referrer() const
{
    return m_client.mediaPlayerReferrer();
}

String MediaPlayer::userAgent() const
{
    return m_client.mediaPlayerUserAgent();
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
    return m_client.mediaPlayerGraphicsDeviceAdapter(this);
}
#endif

CachedResourceLoader* MediaPlayer::cachedResourceLoader()
{
    return m_client.mediaPlayerCachedResourceLoader();
}

PassRefPtr<PlatformMediaResourceLoader> MediaPlayer::createResourceLoader(std::unique_ptr<PlatformMediaResourceLoaderClient> client)
{
    return m_client.mediaPlayerCreateResourceLoader(WTFMove(client));
}

#if ENABLE(VIDEO_TRACK)
void MediaPlayer::addAudioTrack(PassRefPtr<AudioTrackPrivate> track)
{
    m_client.mediaPlayerDidAddAudioTrack(track);
}

void MediaPlayer::removeAudioTrack(PassRefPtr<AudioTrackPrivate> track)
{
    m_client.mediaPlayerDidRemoveAudioTrack(track);
}

void MediaPlayer::addTextTrack(PassRefPtr<InbandTextTrackPrivate> track)
{
    m_client.mediaPlayerDidAddTextTrack(track);
}

void MediaPlayer::removeTextTrack(PassRefPtr<InbandTextTrackPrivate> track)
{
    m_client.mediaPlayerDidRemoveTextTrack(track);
}

void MediaPlayer::addVideoTrack(PassRefPtr<VideoTrackPrivate> track)
{
    m_client.mediaPlayerDidAddVideoTrack(track);
}

void MediaPlayer::removeVideoTrack(PassRefPtr<VideoTrackPrivate> track)
{
    m_client.mediaPlayerDidRemoveVideoTrack(track);
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
    return m_client.outOfBandTrackSources();
}
#endif

#endif // ENABLE(VIDEO_TRACK)

void MediaPlayer::resetMediaEngines()
{
    mutableInstalledMediaEnginesVector().clear();
    haveMediaEnginesVector = false;
}

#if USE(GSTREAMER)
void MediaPlayer::simulateAudioInterruption()
{
    if (!m_private)
        return;

    m_private->simulateAudioInterruption();
}
#endif

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

#if ENABLE(MEDIA_SOURCE)
unsigned long MediaPlayer::totalVideoFrames()
{
    if (!m_private)
        return 0;

    return m_private->totalVideoFrames();
}

unsigned long MediaPlayer::droppedVideoFrames()
{
    if (!m_private)
        return 0;

    return m_private->droppedVideoFrames();
}

unsigned long MediaPlayer::corruptedVideoFrames()
{
    if (!m_private)
        return 0;

    return m_private->corruptedVideoFrames();
}

MediaTime MediaPlayer::totalFrameDelay()
{
    if (!m_private)
        return MediaTime::zeroTime();

    return m_private->totalFrameDelay();
}
#endif

bool MediaPlayer::shouldWaitForResponseToAuthenticationChallenge(const AuthenticationChallenge& challenge)
{
    return m_client.mediaPlayerShouldWaitForResponseToAuthenticationChallenge(challenge);
}

void MediaPlayer::handlePlaybackCommand(PlatformMediaSession::RemoteControlCommandType command)
{
    m_client.mediaPlayerHandlePlaybackCommand(command);
}

String MediaPlayer::sourceApplicationIdentifier() const
{
    return m_client.mediaPlayerSourceApplicationIdentifier();
}

Vector<String> MediaPlayer::preferredAudioCharacteristics() const
{
    return m_client.mediaPlayerPreferredAudioCharacteristics();
}

void MediaPlayerFactorySupport::callRegisterMediaEngine(MediaEngineRegister registerMediaEngine)
{
    registerMediaEngine(addMediaEngine);
}

bool MediaPlayer::doesHaveAttribute(const AtomicString& attribute, AtomicString* value) const
{
    return m_client.doesHaveAttribute(attribute, value);
}

#if PLATFORM(IOS)
String MediaPlayer::mediaPlayerNetworkInterfaceName() const
{
    return m_client.mediaPlayerNetworkInterfaceName();
}

bool MediaPlayer::getRawCookies(const URL& url, Vector<Cookie>& cookies) const
{
    return m_client.mediaPlayerGetRawCookies(url, cookies);
}
#endif

}

#endif
