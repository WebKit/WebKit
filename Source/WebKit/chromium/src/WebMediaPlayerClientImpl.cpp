// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebMediaPlayerClientImpl.h"

#if ENABLE(VIDEO)

#include "AudioSourceProvider.h"
#include "AudioSourceProviderClient.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "IntSize.h"
#include "KURL.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "PlatformContextSkia.h"
#include "RenderView.h"
#include "TimeRanges.h"
#include "VideoLayerChromium.h"
#include "WebAudioSourceProvider.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebKit.h"
#include "WebMediaPlayer.h"
#include "WebViewImpl.h"
#include "cc/CCProxy.h"
#include "platform/WebCString.h"
#include "platform/WebCanvas.h"
#include "platform/WebKitPlatformSupport.h"
#include "platform/WebRect.h"
#include "platform/WebSize.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include <public/WebMimeRegistry.h>

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerCompositor.h"
#endif

#include <wtf/Assertions.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace WebKit {

static PassOwnPtr<WebMediaPlayer> createWebMediaPlayer(WebMediaPlayerClient* client, Frame* frame)
{
    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(frame);

    if (!webFrame->client())
        return nullptr;
    return adoptPtr(webFrame->client()->createMediaPlayer(webFrame, client));
}

bool WebMediaPlayerClientImpl::m_isEnabled = false;

bool WebMediaPlayerClientImpl::isEnabled()
{
    return m_isEnabled;
}

void WebMediaPlayerClientImpl::setIsEnabled(bool isEnabled)
{
    m_isEnabled = isEnabled;
}

void WebMediaPlayerClientImpl::registerSelf(MediaEngineRegistrar registrar)
{
    if (m_isEnabled) {
        registrar(WebMediaPlayerClientImpl::create,
                  WebMediaPlayerClientImpl::getSupportedTypes,
                  WebMediaPlayerClientImpl::supportsType,
                  0,
                  0,
                  0);
    }
}

WebMediaPlayer* WebMediaPlayerClientImpl::mediaPlayer() const
{
    return m_webMediaPlayer.get();
}

// WebMediaPlayerClient --------------------------------------------------------

WebMediaPlayerClientImpl::~WebMediaPlayerClientImpl()
{
#if USE(ACCELERATED_COMPOSITING)
    MutexLocker locker(m_compositingMutex);
    if (m_videoFrameProviderClient)
        m_videoFrameProviderClient->stopUsingProvider();
    if (m_webMediaPlayer)
        m_webMediaPlayer->setStreamTextureClient(0);
#endif
}

void WebMediaPlayerClientImpl::networkStateChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->networkStateChanged();
}

void WebMediaPlayerClientImpl::readyStateChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->readyStateChanged();
#if USE(ACCELERATED_COMPOSITING)
    if (hasVideo() && supportsAcceleratedRendering() && m_videoLayer.isNull()) {
        m_videoLayer = WebVideoLayer::create(this);
        m_videoLayer.setOpaque(m_opaque);
    }
#endif
}

void WebMediaPlayerClientImpl::volumeChanged(float newVolume)
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->volumeChanged(newVolume);
}

void WebMediaPlayerClientImpl::muteChanged(bool newMute)
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->muteChanged(newMute);
}

void WebMediaPlayerClientImpl::timeChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->timeChanged();
}

void WebMediaPlayerClientImpl::repaint()
{
    ASSERT(m_mediaPlayer);
#if USE(ACCELERATED_COMPOSITING)
    if (!m_videoLayer.isNull() && supportsAcceleratedRendering())
        m_videoLayer.invalidate();
#endif
    m_mediaPlayer->repaint();
}

void WebMediaPlayerClientImpl::durationChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->durationChanged();
}

void WebMediaPlayerClientImpl::rateChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->rateChanged();
}

void WebMediaPlayerClientImpl::sizeChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->sizeChanged();
}

void WebMediaPlayerClientImpl::setOpaque(bool opaque)
{
#if USE(ACCELERATED_COMPOSITING)
    m_opaque = opaque;
    if (!m_videoLayer.isNull())
        m_videoLayer.setOpaque(m_opaque);
#endif
}

void WebMediaPlayerClientImpl::sawUnsupportedTracks()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->mediaPlayerClient()->mediaPlayerSawUnsupportedTracks(m_mediaPlayer);
}

float WebMediaPlayerClientImpl::volume() const
{
    if (m_mediaPlayer)
        return m_mediaPlayer->volume();
    return 0.0f;
}

void WebMediaPlayerClientImpl::playbackStateChanged()
{
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->playbackStateChanged();
}

WebMediaPlayer::Preload WebMediaPlayerClientImpl::preload() const
{
    if (m_mediaPlayer)
        return static_cast<WebMediaPlayer::Preload>(m_mediaPlayer->preload());
    return static_cast<WebMediaPlayer::Preload>(m_preload);
}

void WebMediaPlayerClientImpl::sourceOpened()
{
#if ENABLE(MEDIA_SOURCE)
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->sourceOpened();
#endif
}

WebKit::WebURL WebMediaPlayerClientImpl::sourceURL() const
{
#if ENABLE(MEDIA_SOURCE)
    ASSERT(m_mediaPlayer);
    return KURL(ParsedURLString, m_mediaPlayer->sourceURL());
#else
    return KURL();
#endif
}

void WebMediaPlayerClientImpl::keyAdded(const WebString& keySystem, const WebString& sessionId)
{
#if ENABLE(ENCRYPTED_MEDIA)
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->keyAdded(keySystem, sessionId);
#else
    UNUSED_PARAM(keySystem);
    UNUSED_PARAM(sessionId);
#endif
}

void WebMediaPlayerClientImpl::keyError(const WebString& keySystem, const WebString& sessionId, MediaKeyErrorCode errorCode, unsigned short systemCode)
{
#if ENABLE(ENCRYPTED_MEDIA)
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->keyError(keySystem, sessionId, static_cast<MediaPlayerClient::MediaKeyErrorCode>(errorCode), systemCode);
#else
    UNUSED_PARAM(keySystem);
    UNUSED_PARAM(sessionId);
    UNUSED_PARAM(errorCode);
    UNUSED_PARAM(systemCode);
#endif
}

void WebMediaPlayerClientImpl::keyMessage(const WebString& keySystem, const WebString& sessionId, const unsigned char* message, unsigned messageLength)
{
#if ENABLE(ENCRYPTED_MEDIA)
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->keyMessage(keySystem, sessionId, message, messageLength);
#else
    UNUSED_PARAM(keySystem);
    UNUSED_PARAM(sessionId);
    UNUSED_PARAM(message);
    UNUSED_PARAM(messageLength);
#endif
}

void WebMediaPlayerClientImpl::keyNeeded(const WebString& keySystem, const WebString& sessionId, const unsigned char* initData, unsigned initDataLength)
{
#if ENABLE(ENCRYPTED_MEDIA)
    ASSERT(m_mediaPlayer);
    m_mediaPlayer->keyNeeded(keySystem, sessionId, initData, initDataLength);
#else
    UNUSED_PARAM(keySystem);
    UNUSED_PARAM(sessionId);
    UNUSED_PARAM(initData);
    UNUSED_PARAM(initDataLength);
#endif
}

void WebMediaPlayerClientImpl::disableAcceleratedCompositing()
{
    m_supportsAcceleratedCompositing = false;
}

// MediaPlayerPrivateInterface -------------------------------------------------

void WebMediaPlayerClientImpl::load(const String& url)
{
    m_url = url;

    if (m_preload == MediaPlayer::None) {
        MutexLocker locker(m_compositingMutex);
#if ENABLE(WEB_AUDIO)
        m_audioSourceProvider.wrap(0); // Clear weak reference to m_webMediaPlayer's WebAudioSourceProvider.
#endif
        m_webMediaPlayer.clear();
        m_delayingLoad = true;
    } else
        loadInternal();
}

void WebMediaPlayerClientImpl::loadInternal()
{
    MutexLocker locker(m_compositingMutex);
#if ENABLE(WEB_AUDIO)
    m_audioSourceProvider.wrap(0); // Clear weak reference to m_webMediaPlayer's WebAudioSourceProvider.
#endif

    Frame* frame = static_cast<HTMLMediaElement*>(m_mediaPlayer->mediaPlayerClient())->document()->frame();
    m_webMediaPlayer = createWebMediaPlayer(this, frame);
    if (m_webMediaPlayer) {
#if ENABLE(WEB_AUDIO)
        // Make sure if we create/re-create the WebMediaPlayer that we update our wrapper.
        m_audioSourceProvider.wrap(m_webMediaPlayer->audioSourceProvider());
#endif
        m_webMediaPlayer->load(KURL(ParsedURLString, m_url));
    }
}

void WebMediaPlayerClientImpl::cancelLoad()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->cancelLoad();
}

#if USE(ACCELERATED_COMPOSITING)
PlatformLayer* WebMediaPlayerClientImpl::platformLayer() const
{
    ASSERT(m_supportsAcceleratedCompositing);
    return m_videoLayer.unwrap<VideoLayerChromium>();
}
#endif

PlatformMedia WebMediaPlayerClientImpl::platformMedia() const
{
    PlatformMedia pm;
    pm.type = PlatformMedia::ChromiumMediaPlayerType;
    pm.media.chromiumMediaPlayer = const_cast<WebMediaPlayerClientImpl*>(this);
    return pm;
}

void WebMediaPlayerClientImpl::play()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->play();
}

void WebMediaPlayerClientImpl::pause()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->pause();
}

#if USE(NATIVE_FULLSCREEN_VIDEO)
bool WebMediaPlayerClientImpl::enterFullscreen()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->enterFullscreen();
}

void WebMediaPlayerClientImpl::exitFullscreen()
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->exitFullscreen();
}

bool WebMediaPlayerClientImpl::canEnterFullscreen() const
{
    return m_webMediaPlayer && m_webMediaPlayer->canEnterFullscreen();
}
#endif

#if ENABLE(MEDIA_SOURCE)
WebCore::MediaPlayer::AddIdStatus WebMediaPlayerClientImpl::sourceAddId(const String& id, const String& type)
{
    if (!m_webMediaPlayer)
        return WebCore::MediaPlayer::NotSupported;

    return static_cast<WebCore::MediaPlayer::AddIdStatus>(m_webMediaPlayer->sourceAddId(id, type));
}

bool WebMediaPlayerClientImpl::sourceRemoveId(const String& id)
{
    if (!m_webMediaPlayer)
        return false;

    return m_webMediaPlayer->sourceRemoveId(id);
}

bool WebMediaPlayerClientImpl::sourceAppend(const unsigned char* data, unsigned length)
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->sourceAppend(data, length);
    return false;
}

void WebMediaPlayerClientImpl::sourceEndOfStream(WebCore::MediaPlayer::EndOfStreamStatus status)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->sourceEndOfStream(static_cast<WebMediaPlayer::EndOfStreamStatus>(status));
}
#endif

#if ENABLE(ENCRYPTED_MEDIA)
MediaPlayer::MediaKeyException WebMediaPlayerClientImpl::generateKeyRequest(const String& keySystem, const unsigned char* initData, unsigned initDataLength)
{
    if (!m_webMediaPlayer)
        return MediaPlayer::InvalidPlayerState;

    WebMediaPlayer::MediaKeyException result = m_webMediaPlayer->generateKeyRequest(keySystem, initData, initDataLength);
    return static_cast<MediaPlayer::MediaKeyException>(result);
}

MediaPlayer::MediaKeyException WebMediaPlayerClientImpl::addKey(const String& keySystem, const unsigned char* key, unsigned keyLength, const unsigned char* initData, unsigned initDataLength, const String& sessionId)
{
    if (!m_webMediaPlayer)
        return MediaPlayer::InvalidPlayerState;

    WebMediaPlayer::MediaKeyException result = m_webMediaPlayer->addKey(keySystem, key, keyLength, initData, initDataLength, sessionId);
    return static_cast<MediaPlayer::MediaKeyException>(result);
}

MediaPlayer::MediaKeyException WebMediaPlayerClientImpl::cancelKeyRequest(const String& keySystem, const String& sessionId)
{
    if (!m_webMediaPlayer)
        return MediaPlayer::InvalidPlayerState;

    WebMediaPlayer::MediaKeyException result = m_webMediaPlayer->cancelKeyRequest(keySystem, sessionId);
    return static_cast<MediaPlayer::MediaKeyException>(result);
}
#endif

void WebMediaPlayerClientImpl::prepareToPlay()
{
    if (m_delayingLoad)
        startDelayedLoad();
}

IntSize WebMediaPlayerClientImpl::naturalSize() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->naturalSize();
    return IntSize();
}

bool WebMediaPlayerClientImpl::hasVideo() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->hasVideo();
    return false;
}

bool WebMediaPlayerClientImpl::hasAudio() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->hasAudio();
    return false;
}

void WebMediaPlayerClientImpl::setVisible(bool visible)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->setVisible(visible);
}

float WebMediaPlayerClientImpl::duration() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->duration();
    return 0.0f;
}

float WebMediaPlayerClientImpl::currentTime() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->currentTime();
    return 0.0f;
}

void WebMediaPlayerClientImpl::seek(float time)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->seek(time);
}

bool WebMediaPlayerClientImpl::seeking() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->seeking();
    return false;
}

void WebMediaPlayerClientImpl::setEndTime(float time)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->setEndTime(time);
}

void WebMediaPlayerClientImpl::setRate(float rate)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->setRate(rate);
}

bool WebMediaPlayerClientImpl::paused() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->paused();
    return false;
}

bool WebMediaPlayerClientImpl::supportsFullscreen() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->supportsFullscreen();
    return false;
}

bool WebMediaPlayerClientImpl::supportsSave() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->supportsSave();
    return false;
}

void WebMediaPlayerClientImpl::setVolume(float volume)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->setVolume(volume);
}

MediaPlayer::NetworkState WebMediaPlayerClientImpl::networkState() const
{
    if (m_webMediaPlayer)
        return static_cast<MediaPlayer::NetworkState>(m_webMediaPlayer->networkState());
    return MediaPlayer::Empty;
}

MediaPlayer::ReadyState WebMediaPlayerClientImpl::readyState() const
{
    if (m_webMediaPlayer)
        return static_cast<MediaPlayer::ReadyState>(m_webMediaPlayer->readyState());
    return MediaPlayer::HaveNothing;
}

float WebMediaPlayerClientImpl::maxTimeSeekable() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->maxTimeSeekable();
    return 0.0f;
}

PassRefPtr<TimeRanges> WebMediaPlayerClientImpl::buffered() const
{
    if (m_webMediaPlayer) {
        const WebTimeRanges& webRanges = m_webMediaPlayer->buffered();

        // FIXME: Save the time ranges in a member variable and update it when needed.
        RefPtr<TimeRanges> ranges = TimeRanges::create();
        for (size_t i = 0; i < webRanges.size(); ++i)
            ranges->add(webRanges[i].start, webRanges[i].end);
        return ranges.release();
    }
    return TimeRanges::create();
}

int WebMediaPlayerClientImpl::dataRate() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->dataRate();
    return 0;
}

bool WebMediaPlayerClientImpl::totalBytesKnown() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->totalBytesKnown();
    return false;
}

unsigned WebMediaPlayerClientImpl::totalBytes() const
{
    if (m_webMediaPlayer)
        return static_cast<unsigned>(m_webMediaPlayer->totalBytes());
    return 0;
}

unsigned WebMediaPlayerClientImpl::bytesLoaded() const
{
    if (m_webMediaPlayer)
        return static_cast<unsigned>(m_webMediaPlayer->bytesLoaded());
    return 0;
}

void WebMediaPlayerClientImpl::setSize(const IntSize& size)
{
    if (m_webMediaPlayer)
        m_webMediaPlayer->setSize(WebSize(size.width(), size.height()));
}

void WebMediaPlayerClientImpl::paint(GraphicsContext* context, const IntRect& rect)
{
#if USE(ACCELERATED_COMPOSITING)
    // If we are using GPU to render video, ignore requests to paint frames into
    // canvas because it will be taken care of by VideoLayerChromium.
    if (acceleratedRenderingInUse())
        return;
#endif
    paintCurrentFrameInContext(context, rect);
}

void WebMediaPlayerClientImpl::paintCurrentFrameInContext(GraphicsContext* context, const IntRect& rect)
{
    // Normally GraphicsContext operations do nothing when painting is disabled.
    // Since we're accessing platformContext() directly we have to manually
    // check.
    if (m_webMediaPlayer && !context->paintingDisabled()) {
        PlatformGraphicsContext* platformContext = context->platformContext();
        WebCanvas* canvas = platformContext->canvas();
        m_webMediaPlayer->paint(canvas, rect, platformContext->getNormalizedAlpha());
    }
}

void WebMediaPlayerClientImpl::setPreload(MediaPlayer::Preload preload)
{
    m_preload = preload;

    if (m_webMediaPlayer)
        m_webMediaPlayer->setPreload(static_cast<WebMediaPlayer::Preload>(preload));

    if (m_delayingLoad && m_preload != MediaPlayer::None)
        startDelayedLoad();
}

bool WebMediaPlayerClientImpl::hasSingleSecurityOrigin() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->hasSingleSecurityOrigin();
    return false;
}

MediaPlayer::MovieLoadType WebMediaPlayerClientImpl::movieLoadType() const
{
    if (m_webMediaPlayer)
        return static_cast<MediaPlayer::MovieLoadType>(
            m_webMediaPlayer->movieLoadType());
    return MediaPlayer::Unknown;
}

float WebMediaPlayerClientImpl::mediaTimeForTimeValue(float timeValue) const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->mediaTimeForTimeValue(timeValue);
    return timeValue;
}

unsigned WebMediaPlayerClientImpl::decodedFrameCount() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->decodedFrameCount();
    return 0;
}

unsigned WebMediaPlayerClientImpl::droppedFrameCount() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->droppedFrameCount();
    return 0;
}

unsigned WebMediaPlayerClientImpl::audioDecodedByteCount() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->audioDecodedByteCount();
    return 0;
}

unsigned WebMediaPlayerClientImpl::videoDecodedByteCount() const
{
    if (m_webMediaPlayer)
        return m_webMediaPlayer->videoDecodedByteCount();
    return 0;
}

#if ENABLE(WEB_AUDIO)
AudioSourceProvider* WebMediaPlayerClientImpl::audioSourceProvider()
{
    return &m_audioSourceProvider;
}
#endif

#if USE(ACCELERATED_COMPOSITING)
bool WebMediaPlayerClientImpl::supportsAcceleratedRendering() const
{
    return m_supportsAcceleratedCompositing;
}

bool WebMediaPlayerClientImpl::acceleratedRenderingInUse()
{
    return !m_videoLayer.isNull() && m_videoLayer.active();
}

void WebMediaPlayerClientImpl::setVideoFrameProviderClient(WebVideoFrameProvider::Client* client)
{
    MutexLocker locker(m_compositingMutex);
    if (m_videoFrameProviderClient)
        m_videoFrameProviderClient->stopUsingProvider();
    m_videoFrameProviderClient = client;
    if (m_webMediaPlayer)
        m_webMediaPlayer->setStreamTextureClient(client ? this : 0);
}

WebVideoFrame* WebMediaPlayerClientImpl::getCurrentFrame()
{
    MutexLocker locker(m_compositingMutex);
    ASSERT(!m_currentVideoFrame);
    if (m_webMediaPlayer)
        m_currentVideoFrame = m_webMediaPlayer->getCurrentFrame();
    return m_currentVideoFrame;
}

void WebMediaPlayerClientImpl::putCurrentFrame(WebVideoFrame* videoFrame)
{
    MutexLocker locker(m_compositingMutex);
    ASSERT(videoFrame == m_currentVideoFrame);
    if (!videoFrame)
        return;
    if (m_webMediaPlayer)
        m_webMediaPlayer->putCurrentFrame(videoFrame);
    m_currentVideoFrame = 0;
}
#endif

PassOwnPtr<MediaPlayerPrivateInterface> WebMediaPlayerClientImpl::create(MediaPlayer* player)
{
    OwnPtr<WebMediaPlayerClientImpl> client = adoptPtr(new WebMediaPlayerClientImpl());
    client->m_mediaPlayer = player;

#if USE(ACCELERATED_COMPOSITING)
    Frame* frame = static_cast<HTMLMediaElement*>(
        client->m_mediaPlayer->mediaPlayerClient())->document()->frame();

    // This does not actually check whether the hardware can support accelerated
    // compositing, but only if the flag is set. However, this is checked lazily
    // in WebViewImpl::setIsAcceleratedCompositingActive() and will fail there
    // if necessary.
    client->m_supportsAcceleratedCompositing =
        frame->contentRenderer()->compositor()->hasAcceleratedCompositing();
#endif

    return client.release();
}

void WebMediaPlayerClientImpl::getSupportedTypes(HashSet<String>& supportedTypes)
{
    // FIXME: integrate this list with WebMediaPlayerClientImpl::supportsType.
    notImplemented();
}

#if ENABLE(ENCRYPTED_MEDIA)
MediaPlayer::SupportsType WebMediaPlayerClientImpl::supportsType(const String& type,
                                                                 const String& codecs,
                                                                 const String& keySystem)
{
#else
MediaPlayer::SupportsType WebMediaPlayerClientImpl::supportsType(const String& type,
                                                                 const String& codecs)
{
    String keySystem;
#endif
    WebMimeRegistry::SupportsType supportsType = WebKit::Platform::current()->mimeRegistry()->supportsMediaMIMEType(type, codecs, keySystem);

    switch (supportsType) {
    default:
        ASSERT_NOT_REACHED();
    case WebMimeRegistry::IsNotSupported:
        return MediaPlayer::IsNotSupported;
    case WebMimeRegistry::IsSupported:
        return MediaPlayer::IsSupported;
    case WebMimeRegistry::MayBeSupported:
        return MediaPlayer::MayBeSupported;
    }
    return MediaPlayer::IsNotSupported;
}

void WebMediaPlayerClientImpl::startDelayedLoad()
{
    ASSERT(m_delayingLoad);
    ASSERT(!m_webMediaPlayer);

    m_delayingLoad = false;

    loadInternal();
}

void WebMediaPlayerClientImpl::didReceiveFrame()
{
    // No lock since this gets called on the client's thread.
    ASSERT(CCProxy::isImplThread());
    m_videoFrameProviderClient->didReceiveFrame();
}

void WebMediaPlayerClientImpl::didUpdateMatrix(const float* matrix)
{
    // No lock since this gets called on the client's thread.
    ASSERT(CCProxy::isImplThread());
    m_videoFrameProviderClient->didUpdateMatrix(matrix);
}

WebMediaPlayerClientImpl::WebMediaPlayerClientImpl()
    : m_mediaPlayer(0)
    , m_currentVideoFrame(0)
    , m_delayingLoad(false)
    , m_preload(MediaPlayer::MetaData)
#if USE(ACCELERATED_COMPOSITING)
    , m_supportsAcceleratedCompositing(false)
    , m_opaque(false)
    , m_videoFrameProviderClient(0)
#endif
{
}

#if ENABLE(WEB_AUDIO)
void WebMediaPlayerClientImpl::AudioSourceProviderImpl::wrap(WebAudioSourceProvider* provider)
{
    m_webAudioSourceProvider = provider;
    if (m_webAudioSourceProvider)
        m_webAudioSourceProvider->setClient(m_client.get());
}

void WebMediaPlayerClientImpl::AudioSourceProviderImpl::setClient(AudioSourceProviderClient* client)
{
    if (client)
        m_client = adoptPtr(new WebMediaPlayerClientImpl::AudioClientImpl(client));
    else
        m_client.clear();

    if (m_webAudioSourceProvider)
        m_webAudioSourceProvider->setClient(m_client.get());
}

void WebMediaPlayerClientImpl::AudioSourceProviderImpl::provideInput(AudioBus* bus, size_t framesToProcess)
{
    ASSERT(bus);
    if (!bus)
        return;

    if (!m_webAudioSourceProvider) {
        bus->zero();
        return;
    }

    // Wrap the AudioBus channel data using WebVector.
    size_t n = bus->numberOfChannels();
    WebVector<float*> webAudioData(n);
    for (size_t i = 0; i < n; ++i)
        webAudioData[i] = bus->channel(i)->mutableData();

    m_webAudioSourceProvider->provideInput(webAudioData, framesToProcess);
}

void WebMediaPlayerClientImpl::AudioClientImpl::setFormat(size_t numberOfChannels, float sampleRate)
{
    if (m_client)
        m_client->setFormat(numberOfChannels, sampleRate);
}

#endif

} // namespace WebKit

#endif  // ENABLE(VIDEO)
