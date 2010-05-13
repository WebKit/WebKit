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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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
#include "Frame.h"
#include "FrameView.h"
#include "IntRect.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayerPrivate.h"
#include "TimeRanges.h"

#if PLATFORM(QT)
#include <QtGlobal>
#endif

#if PLATFORM(MAC)
#include "MediaPlayerPrivateQTKit.h"
#elif OS(WINCE) && !PLATFORM(QT)
#include "MediaPlayerPrivateWince.h"
#elif PLATFORM(WIN)
#include "MediaPlayerPrivateQuickTimeVisualContext.h"
#include "MediaPlayerPrivateQuicktimeWin.h"
#elif PLATFORM(GTK)
#include "MediaPlayerPrivateGStreamer.h"
#elif PLATFORM(QT)
// QtMultimedia support is disabled currently.
#if 1 || (QT_VERSION < 0x040700)
#include "MediaPlayerPrivatePhonon.h"
#else
#include "MediaPlayerPrivateQt.h"
#endif
#elif PLATFORM(CHROMIUM)
#include "MediaPlayerPrivateChromium.h"
#endif

namespace WebCore {

const PlatformMedia NoPlatformMedia = { PlatformMedia::None, {0} };

// a null player to make MediaPlayer logic simpler

class NullMediaPlayerPrivate : public MediaPlayerPrivateInterface {
public:
    NullMediaPlayerPrivate(MediaPlayer*) { }

    virtual void load(const String&) { }
    virtual void cancelLoad() { }
    
    virtual void prepareToPlay() { }
    virtual void play() { }
    virtual void pause() { }    

    virtual PlatformMedia platformMedia() const { return NoPlatformMedia; }
#if USE(ACCELERATED_COMPOSITING)
    virtual PlatformLayer* platformLayer() const { return 0; }
#endif

    virtual IntSize naturalSize() const { return IntSize(0, 0); }

    virtual bool hasVideo() const { return false; }
    virtual bool hasAudio() const { return false; }

    virtual void setVisible(bool) { }

    virtual float duration() const { return 0; }

    virtual float currentTime() const { return 0; }
    virtual void seek(float) { }
    virtual bool seeking() const { return false; }

    virtual void setRate(float) { }
    virtual void setPreservesPitch(bool) { }
    virtual bool paused() const { return false; }

    virtual void setVolume(float) { }

    virtual bool supportsMuting() const { return false; }
    virtual void setMuted(bool) { }

    virtual bool hasClosedCaptions() const { return false; }
    virtual void setClosedCaptionsVisible(bool) { };

    virtual MediaPlayer::NetworkState networkState() const { return MediaPlayer::Empty; }
    virtual MediaPlayer::ReadyState readyState() const { return MediaPlayer::HaveNothing; }

    virtual float maxTimeSeekable() const { return 0; }
    virtual PassRefPtr<TimeRanges> buffered() const { return TimeRanges::create(); }

    virtual unsigned totalBytes() const { return 0; }
    virtual unsigned bytesLoaded() const { return 0; }

    virtual void setSize(const IntSize&) { }

    virtual void paint(GraphicsContext*, const IntRect&) { }

    virtual bool canLoadPoster() const { return false; }
    virtual void setPoster(const String&) { }

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    virtual void deliverNotification(MediaPlayerProxyNotificationType) { }
    virtual void setMediaPlayerProxy(WebMediaPlayerProxy*) { }
#endif

    virtual bool hasSingleSecurityOrigin() const { return true; }
};

static MediaPlayerPrivateInterface* createNullMediaPlayer(MediaPlayer* player) 
{ 
    return new NullMediaPlayerPrivate(player); 
}


// engine support

struct MediaPlayerFactory : Noncopyable {
    MediaPlayerFactory(CreateMediaEnginePlayer constructor, MediaEngineSupportedTypes getSupportedTypes, MediaEngineSupportsType supportsTypeAndCodecs) 
        : constructor(constructor)
        , getSupportedTypes(getSupportedTypes)
        , supportsTypeAndCodecs(supportsTypeAndCodecs)  
    { 
    }

    CreateMediaEnginePlayer constructor;
    MediaEngineSupportedTypes getSupportedTypes;
    MediaEngineSupportsType supportsTypeAndCodecs;
};

static void addMediaEngine(CreateMediaEnginePlayer, MediaEngineSupportedTypes, MediaEngineSupportsType);
static MediaPlayerFactory* chooseBestEngineForTypeAndCodecs(const String& type, const String& codecs);

static Vector<MediaPlayerFactory*>& installedMediaEngines() 
{
    DEFINE_STATIC_LOCAL(Vector<MediaPlayerFactory*>, installedEngines, ());
    static bool enginesQueried = false;

    if (!enginesQueried) {
        enginesQueried = true;
#if USE(GSTREAMER)
        MediaPlayerPrivateGStreamer::registerMediaEngine(addMediaEngine);
#else
#if PLATFORM(WIN)
        MediaPlayerPrivateQuickTimeVisualContext::registerMediaEngine(addMediaEngine);
#endif
        // FIXME: currently all the MediaEngines are named
        // MediaPlayerPrivate. This code will need an update when bug
        // 36663 is adressed.
        MediaPlayerPrivate::registerMediaEngine(addMediaEngine);
#endif

        // register additional engines here
    }
    
    return installedEngines;
}

static void addMediaEngine(CreateMediaEnginePlayer constructor, MediaEngineSupportedTypes getSupportedTypes, MediaEngineSupportsType supportsType)
{
    ASSERT(constructor);
    ASSERT(getSupportedTypes);
    ASSERT(supportsType);
    installedMediaEngines().append(new MediaPlayerFactory(constructor, getSupportedTypes, supportsType));
}

static MediaPlayerFactory* chooseBestEngineForTypeAndCodecs(const String& type, const String& codecs)
{
    Vector<MediaPlayerFactory*>& engines = installedMediaEngines();

    if (engines.isEmpty())
        return 0;

    MediaPlayerFactory* engine = 0;
    MediaPlayer::SupportsType supported = MediaPlayer::IsNotSupported;

    unsigned count = engines.size();
    for (unsigned ndx = 0; ndx < count; ndx++) {
        MediaPlayer::SupportsType engineSupport = engines[ndx]->supportsTypeAndCodecs(type, codecs);
        if (engineSupport > supported) {
            supported = engineSupport;
            engine = engines[ndx];
        }
    }

    return engine;
}

// media player

MediaPlayer::MediaPlayer(MediaPlayerClient* client)
    : m_mediaPlayerClient(client)
    , m_private(createNullMediaPlayer(this))
    , m_currentMediaEngine(0)
    , m_frameView(0)
    , m_preload(Auto)
    , m_visible(false)
    , m_rate(1.0f)
    , m_volume(1.0f)
    , m_muted(false)
    , m_preservesPitch(true)
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    , m_playerProxy(0)
#endif
{
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
    Vector<MediaPlayerFactory*>& engines = installedMediaEngines();
    if (!engines.isEmpty()) {
        m_currentMediaEngine = engines[0];
        m_private.clear();
        m_private.set(engines[0]->constructor(this));
    }
#endif
}

MediaPlayer::~MediaPlayer()
{
}

void MediaPlayer::load(const String& url, const ContentType& contentType)
{
    String type = contentType.type();
    String codecs = contentType.parameter("codecs");

    // if we don't know the MIME type, see if the extension can help
    if (type.isEmpty() || type == "application/octet-stream" || type == "text/plain") {
        int pos = url.reverseFind('.');
        if (pos >= 0) {
            String extension = url.substring(pos + 1);
            String mediaType = MIMETypeRegistry::getMediaMIMETypeForExtension(extension);
            if (!mediaType.isEmpty())
                type = mediaType;
        }
    }

    MediaPlayerFactory* engine = 0;
    if (!type.isEmpty())
        engine = chooseBestEngineForTypeAndCodecs(type, codecs);

    // if we didn't find an engine that claims the MIME type, just use the first engine
    if (!engine && !installedMediaEngines().isEmpty())
        engine = installedMediaEngines()[0];
    
    // don't delete and recreate the player unless it comes from a different engine
    if (engine && m_currentMediaEngine != engine) {
        m_currentMediaEngine = engine;
        m_private.clear();
        m_private.set(engine->constructor(this));
#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
        m_private->setMediaPlayerProxy(m_playerProxy);
#endif
        m_private->setPreload(m_preload);
        m_private->setPreservesPitch(preservesPitch());
    }

    if (m_private)
        m_private->load(url);
    else
        m_private.set(createNullMediaPlayer(this));
}    

bool MediaPlayer::hasAvailableVideoFrame() const
{
    return m_private->hasAvailableVideoFrame();
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

float MediaPlayer::duration() const
{
    return m_private->duration();
}

float MediaPlayer::startTime() const
{
    return m_private->startTime();
}

float MediaPlayer::currentTime() const
{
    return m_private->currentTime();
}

void MediaPlayer::seek(float time)
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

bool MediaPlayer::supportsSave() const
{
    return m_private->supportsSave();
}

IntSize MediaPlayer::naturalSize()
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

bool MediaPlayer::inMediaDocument()
{
    Frame* frame = m_frameView ? m_frameView->frame() : 0;
    Document* document = frame ? frame->document() : 0;
    
    return document && document->isMediaDocument();
}

PlatformMedia MediaPlayer::platformMedia() const
{
    return m_private->platformMedia();
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* MediaPlayer::platformLayer() const
{
    return m_private->platformLayer();
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

float MediaPlayer::volume() const
{
    return m_volume;
}

void MediaPlayer::setVolume(float volume)
{
    m_volume = volume;

    if (m_private->supportsMuting() || !m_muted)
        m_private->setVolume(volume);
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

float MediaPlayer::rate() const
{
    return m_rate;
}

void MediaPlayer::setRate(float rate)
{
    m_rate = rate;
    m_private->setRate(rate);   
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

PassRefPtr<TimeRanges> MediaPlayer::buffered()
{
    return m_private->buffered();
}

float MediaPlayer::maxTimeSeekable()
{
    return m_private->maxTimeSeekable();
}

unsigned MediaPlayer::bytesLoaded()
{
    return m_private->bytesLoaded();
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

void MediaPlayer::paint(GraphicsContext* p, const IntRect& r)
{
    m_private->paint(p, r);
}

void MediaPlayer::paintCurrentFrameInContext(GraphicsContext* p, const IntRect& r)
{
    m_private->paintCurrentFrameInContext(p, r);
}

MediaPlayer::SupportsType MediaPlayer::supportsType(ContentType contentType)
{
    String type = contentType.type();
    String codecs = contentType.parameter("codecs");
    MediaPlayerFactory* engine = chooseBestEngineForTypeAndCodecs(type, codecs);

    if (!engine)
        return IsNotSupported;

    return engine->supportsTypeAndCodecs(type, codecs);
}

void MediaPlayer::getSupportedTypes(HashSet<String>& types)
{
    Vector<MediaPlayerFactory*>& engines = installedMediaEngines();
    if (engines.isEmpty())
        return;

    unsigned count = engines.size();
    for (unsigned ndx = 0; ndx < count; ndx++)
        engines[ndx]->getSupportedTypes(types);
} 

bool MediaPlayer::isAvailable()
{
    return !installedMediaEngines().isEmpty();
} 

#if ENABLE(PLUGIN_PROXY_FOR_VIDEO)
void MediaPlayer::deliverNotification(MediaPlayerProxyNotificationType notification)
{
    m_private->deliverNotification(notification);
}

void MediaPlayer::setMediaPlayerProxy(WebMediaPlayerProxy* proxy)
{
    m_playerProxy = proxy;
    m_private->setMediaPlayerProxy(proxy);
}
#endif

#if USE(ACCELERATED_COMPOSITING)
void MediaPlayer::acceleratedRenderingStateChanged()
{
    m_private->acceleratedRenderingStateChanged();
}

bool MediaPlayer::supportsAcceleratedRendering() const
{
    return m_private->supportsAcceleratedRendering();
}
#endif // USE(ACCELERATED_COMPOSITING)

bool MediaPlayer::hasSingleSecurityOrigin() const
{
    return m_private->hasSingleSecurityOrigin();
}

MediaPlayer::MovieLoadType MediaPlayer::movieLoadType() const
{
    return m_private->movieLoadType();
}

// Client callbacks.
void MediaPlayer::networkStateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerNetworkStateChanged(this);
}

void MediaPlayer::readyStateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerReadyStateChanged(this);
}

void MediaPlayer::volumeChanged(float newVolume)
{
    m_volume = newVolume;
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerVolumeChanged(this);
}

void MediaPlayer::muteChanged(bool newMuted)
{
    m_muted = newMuted;
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerMuteChanged(this);
}

void MediaPlayer::timeChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerTimeChanged(this);
}

void MediaPlayer::sizeChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerSizeChanged(this);
}

void MediaPlayer::repaint()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerRepaint(this);
}

void MediaPlayer::durationChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerDurationChanged(this);
}

void MediaPlayer::rateChanged()
{
    if (m_mediaPlayerClient)
        m_mediaPlayerClient->mediaPlayerRateChanged(this);
}

}

#endif
