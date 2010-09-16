/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "MediaPlayerPrivateQt.h"

#include "FrameLoaderClientQt.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLMediaElement.h"
#include "HTMLVideoElement.h"
#include "NetworkingContext.h"
#include "NotImplemented.h"
#include "TimeRanges.h"
#include "Widget.h"
#include "qwebframe.h"
#include "qwebpage.h"

#include <QGraphicsScene>
#include <QGraphicsVideoItem>
#include <QMediaPlayerControl>
#include <QMediaService>
#include <QNetworkAccessManager>
#include <QNetworkCookieJar>
#include <QNetworkRequest>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <limits>
#include <wtf/HashSet.h>
#include <wtf/text/CString.h>

using namespace WTF;

namespace WebCore {

MediaPlayerPrivateInterface* MediaPlayerPrivate::create(MediaPlayer* player)
{
    return new MediaPlayerPrivate(player);
}

void MediaPlayerPrivate::registerMediaEngine(MediaEngineRegistrar registrar)
{
    registrar(create, getSupportedTypes, supportsType);
}

void MediaPlayerPrivate::getSupportedTypes(HashSet<String> &supported)
{
    QStringList types = QMediaPlayer::supportedMimeTypes();

    for (int i = 0; i < types.size(); i++) {
        QString mime = types.at(i);
        if (mime.startsWith("audio/") || mime.startsWith("video/"))
            supported.add(mime);
    }
}

MediaPlayer::SupportsType MediaPlayerPrivate::supportsType(const String& mime, const String& codec)
{
    if (!mime.startsWith("audio/") && !mime.startsWith("video/"))
        return MediaPlayer::IsNotSupported;

    if (QMediaPlayer::hasSupport(mime, QStringList(codec)) >= QtMultimediaKit::ProbablySupported)
        return MediaPlayer::IsSupported;

    return MediaPlayer::MayBeSupported;
}

MediaPlayerPrivate::MediaPlayerPrivate(MediaPlayer* player)
    : m_player(player)
    , m_mediaPlayer(new QMediaPlayer)
    , m_mediaPlayerControl(0)
    , m_videoItem(new QGraphicsVideoItem)
    , m_videoScene(new QGraphicsScene)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_isVisible(false)
    , m_isSeeking(false)
    , m_composited(false)
    , m_queuedSeek(-1)
    , m_preload(MediaPlayer::Auto)
{
    m_mediaPlayer->bind(m_videoItem);
    m_videoScene->addItem(m_videoItem);

    // Signal Handlers
    connect(m_mediaPlayer, SIGNAL(mediaStatusChanged(QMediaPlayer::MediaStatus)),
            this, SLOT(mediaStatusChanged(QMediaPlayer::MediaStatus)));
    connect(m_mediaPlayer, SIGNAL(stateChanged(QMediaPlayer::State)),
            this, SLOT(stateChanged(QMediaPlayer::State)));
    connect(m_mediaPlayer, SIGNAL(error(QMediaPlayer::Error)),
            this, SLOT(handleError(QMediaPlayer::Error)));
    connect(m_mediaPlayer, SIGNAL(bufferStatusChanged(int)),
            this, SLOT(bufferStatusChanged(int)));
    connect(m_mediaPlayer, SIGNAL(durationChanged(qint64)),
            this, SLOT(durationChanged(qint64)));
    connect(m_mediaPlayer, SIGNAL(positionChanged(qint64)),
            this, SLOT(positionChanged(qint64)));
    connect(m_mediaPlayer, SIGNAL(volumeChanged(int)),
            this, SLOT(volumeChanged(int)));
    connect(m_mediaPlayer, SIGNAL(mutedChanged(bool)),
            this, SLOT(mutedChanged(bool)));
    connect(m_videoScene, SIGNAL(changed(QList<QRectF>)),
            this, SLOT(repaint()));
    connect(m_videoItem, SIGNAL(nativeSizeChanged(QSizeF)),
           this, SLOT(nativeSizeChanged(QSizeF)));

    // Grab the player control
    QMediaService* service = m_mediaPlayer->service();
    if (service) {
        m_mediaPlayerControl = qobject_cast<QMediaPlayerControl *>(
                service->requestControl(QMediaPlayerControl_iid));
    }
}

MediaPlayerPrivate::~MediaPlayerPrivate()
{
    delete m_mediaPlayer;
    delete m_videoScene;
}

bool MediaPlayerPrivate::hasVideo() const
{
    return m_mediaPlayer->isVideoAvailable();
}

bool MediaPlayerPrivate::hasAudio() const
{
    return true;
}

void MediaPlayerPrivate::load(const String& url)
{
    m_mediaUrl = url;

    // QtMultimedia does not have an API to throttle loading
    // so we handle this ourselves by delaying the load
    if (m_preload == MediaPlayer::None) {
        m_delayingLoad = true;
        return;
    }

    commitLoad(url);
}

void MediaPlayerPrivate::commitLoad(const String& url)
{
    // We are now loading
    if (m_networkState != MediaPlayer::Loading) {
        m_networkState = MediaPlayer::Loading;
        m_player->networkStateChanged();
    }

    // And we don't have any data yet
    if (m_readyState != MediaPlayer::HaveNothing) {
        m_readyState = MediaPlayer::HaveNothing;
        m_player->readyStateChanged();
    }

    const QUrl rUrl = QUrl(QString(url));
    const QString scheme = rUrl.scheme().toLower();

    // Grab the client media element
    HTMLMediaElement* element = static_cast<HTMLMediaElement*>(m_player->mediaPlayerClient());

    // Construct the media content with a network request if the resource is http[s]
    if (scheme == "http" || scheme == "https") {
        QNetworkRequest request = QNetworkRequest(rUrl);

        // Grab the current document
        Document* document = element->document();
        if (!document)
            document = element->ownerDocument();

        // Grab the frame and network manager
        Frame* frame = document ? document->frame() : 0;
        QNetworkAccessManager* manager = frame ? frame->loader()->networkingContext()->networkAccessManager() : 0;
        FrameLoaderClientQt* frameLoader =  frame ? static_cast<FrameLoaderClientQt*>(frame->loader()->client()) : 0;

        if (document && manager) {
            // Set the cookies
            QNetworkCookieJar* jar = manager->cookieJar();
            QList<QNetworkCookie> cookies = jar->cookiesForUrl(rUrl);

            // Don't set the header if there are no cookies.
            // This prevents a warning from being emitted.
            if (!cookies.isEmpty())
                request.setHeader(QNetworkRequest::CookieHeader, qVariantFromValue(cookies));

            // Set the refferer, but not when requesting insecure content from a secure page
            QUrl documentUrl = QUrl(QString(document->documentURI()));
            if (documentUrl.scheme().toLower() == "http" || scheme == "https")
                request.setRawHeader("Referer", documentUrl.toEncoded());

            // Set the user agent
            request.setRawHeader("User-Agent", frameLoader->userAgent(rUrl).utf8().data());
        }

        m_mediaPlayer->setMedia(QMediaContent(request));
    } else {
        // Otherwise, just use the URL
        m_mediaPlayer->setMedia(QMediaContent(rUrl));
    }

    // Set the current volume and mute status
    // We get these from the element, rather than the player, in case we have
    // transitioned from a media engine which doesn't support muting, to a media
    // engine which does.
    m_mediaPlayer->setMuted(element->muted());
    m_mediaPlayer->setVolume(static_cast<int>(element->volume() * 100.0));

    // Setting a media source will start loading the media, but we need
    // to pre-roll as well to get video size-hints and buffer-status
    if (element->paused())
        m_mediaPlayer->pause();
    else
        m_mediaPlayer->play();
}

void MediaPlayerPrivate::resumeLoad()
{
    m_delayingLoad = false;

    if (!m_mediaUrl.isNull())
        commitLoad(m_mediaUrl);
}

void MediaPlayerPrivate::cancelLoad()
{
    m_mediaPlayer->setMedia(QMediaContent());
    updateStates();
}

void MediaPlayerPrivate::prepareToPlay()
{
    if (m_mediaPlayer->media().isNull() || m_delayingLoad)
        resumeLoad();
}

void MediaPlayerPrivate::play()
{
    if (m_mediaPlayer->state() != QMediaPlayer::PlayingState)
        m_mediaPlayer->play();
}

void MediaPlayerPrivate::pause()
{
    if (m_mediaPlayer->state() == QMediaPlayer::PlayingState)
        m_mediaPlayer->pause();
}

bool MediaPlayerPrivate::paused() const
{
    return (m_mediaPlayer->state() != QMediaPlayer::PlayingState);
}

void MediaPlayerPrivate::seek(float position)
{
    if (!m_mediaPlayer->isSeekable())
        return;

    if (m_mediaPlayerControl && !m_mediaPlayerControl->availablePlaybackRanges().contains(position * 1000))
        return;

    if (m_isSeeking)
        return;

    if (position > duration())
        position = duration();

    // Seeking is most reliable when we're paused.
    // Webkit will try to pause before seeking, but due to the asynchronous nature
    // of the backend, the player may not actually be paused yet.
    // In this case, we should queue the seek and wait until pausing has completed
    // before attempting to seek.
    if (m_mediaPlayer->state() == QMediaPlayer::PlayingState) {
        m_mediaPlayer->pause();
        m_isSeeking = true;
        m_queuedSeek = static_cast<qint64>(position * 1000);

        // Set a timeout, so that in the event that we don't get a state changed
        // signal, we still attempt the seek.
        QTimer::singleShot(1000, this, SLOT(queuedSeekTimeout()));
    } else {
        m_isSeeking = true;
        m_mediaPlayer->setPosition(static_cast<qint64>(position * 1000));

        // Set a timeout, in case we don't get a position changed signal
        QTimer::singleShot(10000, this, SLOT(seekTimeout()));
    }
}

bool MediaPlayerPrivate::seeking() const
{
    return m_isSeeking;
}

float MediaPlayerPrivate::duration() const
{
    if (m_readyState < MediaPlayer::HaveMetadata)
        return 0.0f;

    float duration = m_mediaPlayer->duration() / 1000.0f;

    // We are streaming
    if (duration <= 0.0f)
        duration = std::numeric_limits<float>::infinity();

    return duration;
}

float MediaPlayerPrivate::currentTime() const
{
    float currentTime = m_mediaPlayer->position() / 1000.0f;
    return currentTime;
}

PassRefPtr<TimeRanges> MediaPlayerPrivate::buffered() const
{
    RefPtr<TimeRanges> buffered = TimeRanges::create();

    if (!m_mediaPlayerControl)
        return buffered;

    QMediaTimeRange playbackRanges = m_mediaPlayerControl->availablePlaybackRanges();

    foreach (const QMediaTimeInterval interval, playbackRanges.intervals()) {
        float rangeMin = static_cast<float>(interval.start()) / 1000.0f;
        float rangeMax = static_cast<float>(interval.end()) / 1000.0f;
        buffered->add(rangeMin, rangeMax);
    }

    return buffered.release();
}

float MediaPlayerPrivate::maxTimeSeekable() const
{
    if (!m_mediaPlayerControl)
        return 0;

    return static_cast<float>(m_mediaPlayerControl->availablePlaybackRanges().latestTime()) / 1000.0f;
}

unsigned MediaPlayerPrivate::bytesLoaded() const
{
    QLatin1String bytesLoadedKey("bytes-loaded");
    if (m_mediaPlayer->availableExtendedMetaData().contains(bytesLoadedKey))
        return m_mediaPlayer->extendedMetaData(bytesLoadedKey).toInt();

    return m_mediaPlayer->bufferStatus();
}

unsigned MediaPlayerPrivate::totalBytes() const
{
    if (m_mediaPlayer->availableMetaData().contains(QtMultimediaKit::Size))
        return m_mediaPlayer->metaData(QtMultimediaKit::Size).toInt();

    return 100;
}

void MediaPlayerPrivate::setPreload(MediaPlayer::Preload preload)
{
    m_preload = preload;
    if (m_delayingLoad && m_preload != MediaPlayer::None)
        resumeLoad();
}

void MediaPlayerPrivate::setRate(float rate)
{
    m_mediaPlayer->setPlaybackRate(rate);
}

void MediaPlayerPrivate::setVolume(float volume)
{
    m_mediaPlayer->setVolume(static_cast<int>(volume * 100.0));
}

bool MediaPlayerPrivate::supportsMuting() const
{
    return true;
}

void MediaPlayerPrivate::setMuted(bool muted)
{
    m_mediaPlayer->setMuted(muted);
}

MediaPlayer::NetworkState MediaPlayerPrivate::networkState() const
{
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivate::readyState() const
{
    return m_readyState;
}

void MediaPlayerPrivate::setVisible(bool visible)
{
    m_isVisible = visible;
}

void MediaPlayerPrivate::mediaStatusChanged(QMediaPlayer::MediaStatus)
{
    updateStates();
}

void MediaPlayerPrivate::handleError(QMediaPlayer::Error)
{
    updateStates();
}

void MediaPlayerPrivate::stateChanged(QMediaPlayer::State state)
{
    if (state != QMediaPlayer::PlayingState && m_isSeeking && m_queuedSeek >= 0) {
        m_mediaPlayer->setPosition(m_queuedSeek);
        m_queuedSeek = -1;
    }
}

void MediaPlayerPrivate::nativeSizeChanged(const QSizeF&)
{
    m_player->sizeChanged();
}

void MediaPlayerPrivate::queuedSeekTimeout()
{
    // If we haven't heard anything, assume the player is now paused
    // and we can attempt the seek
    if (m_isSeeking && m_queuedSeek >= 0) {
        m_mediaPlayer->setPosition(m_queuedSeek);
        m_queuedSeek = -1;

        // Set a timeout, in case we don't get a position changed signal
        QTimer::singleShot(10000, this, SLOT(seekTimeout()));
    }
}

void MediaPlayerPrivate::seekTimeout()
{
    // If we haven't heard anything, assume the seek succeeded
    if (m_isSeeking) {
        m_player->timeChanged();
        m_isSeeking = false;
    }
}

void MediaPlayerPrivate::positionChanged(qint64)
{
    // Only propogate this event if we are seeking
    if (m_isSeeking && m_queuedSeek == -1) {
        m_player->timeChanged();
        m_isSeeking = false;
    }
}

void MediaPlayerPrivate::bufferStatusChanged(int)
{
    notImplemented();
}

void MediaPlayerPrivate::durationChanged(qint64)
{
    m_player->durationChanged();
}

void MediaPlayerPrivate::volumeChanged(int volume)
{
    m_player->volumeChanged(static_cast<float>(volume) / 100.0);
}

void MediaPlayerPrivate::mutedChanged(bool muted)
{
    m_player->muteChanged(muted);
}

void MediaPlayerPrivate::updateStates()
{
    // Store the old states so that we can detect a change and raise change events
    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;

    QMediaPlayer::MediaStatus currentStatus = m_mediaPlayer->mediaStatus();
    QMediaPlayer::Error currentError = m_mediaPlayer->error();

    if (currentError != QMediaPlayer::NoError) {
        m_readyState = MediaPlayer::HaveNothing;
        if (currentError == QMediaPlayer::FormatError)
            m_networkState = MediaPlayer::FormatError;
        else
            m_networkState = MediaPlayer::NetworkError;
    } else if (currentStatus == QMediaPlayer::UnknownMediaStatus
               || currentStatus == QMediaPlayer::NoMedia) {
        m_networkState = MediaPlayer::Idle;
        m_readyState = MediaPlayer::HaveNothing;
    } else if (currentStatus == QMediaPlayer::LoadingMedia) {
        m_networkState = MediaPlayer::Loading;
        m_readyState = MediaPlayer::HaveNothing;
    } else if (currentStatus == QMediaPlayer::LoadedMedia) {
        m_networkState = MediaPlayer::Loading;
        m_readyState = MediaPlayer::HaveMetadata;
    } else if (currentStatus == QMediaPlayer::BufferingMedia) {
        m_networkState = MediaPlayer::Loading;
        m_readyState = MediaPlayer::HaveFutureData;
    } else if (currentStatus == QMediaPlayer::StalledMedia) {
        m_networkState = MediaPlayer::Loading;
        m_readyState = MediaPlayer::HaveCurrentData;
    } else if (currentStatus == QMediaPlayer::BufferedMedia
               || currentStatus == QMediaPlayer::EndOfMedia) {
        m_networkState = MediaPlayer::Idle;
        m_readyState = MediaPlayer::HaveEnoughData;
    } else if (currentStatus == QMediaPlayer::InvalidMedia) {
        m_networkState = MediaPlayer::NetworkError;
        m_readyState = MediaPlayer::HaveNothing;
    }

    // Log the state changes and raise the state change events
    // NB: The readyStateChanged event must come before the networkStateChanged event.
    // Breaking this invariant will cause the resource selection algorithm for multiple
    // sources to fail.
    if (m_readyState != oldReadyState)
        m_player->readyStateChanged();

    if (m_networkState != oldNetworkState)
        m_player->networkStateChanged();
}

void MediaPlayerPrivate::setSize(const IntSize& size)
{
    if (size == m_currentSize)
        return;

    m_currentSize = size;
    m_videoItem->setSize(QSizeF(QSize(size)));
}

IntSize MediaPlayerPrivate::naturalSize() const
{
    if (!hasVideo() || m_readyState < MediaPlayer::HaveMetadata)
        return IntSize();

    return IntSize(m_videoItem->nativeSize().toSize());
}

void MediaPlayerPrivate::paint(GraphicsContext* context, const IntRect& rect)
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_composited)
        return;
#endif
    if (context->paintingDisabled())
        return;

    if (!m_isVisible)
        return;

    // Grab the painter and widget
    QPainter* painter = context->platformContext();

    // Render the video
    m_videoScene->render(painter, QRectF(QRect(rect)), m_videoItem->sceneBoundingRect());
}

void MediaPlayerPrivate::repaint()
{
    m_player->repaint();
}

#if USE(ACCELERATED_COMPOSITING)
void MediaPlayerPrivate::acceleratedRenderingStateChanged()
{
    bool composited = m_player->mediaPlayerClient()->mediaPlayerRenderingCanBeAccelerated(m_player);
    if (composited == m_composited)
        return;

    m_composited = composited;
    if (composited)
        m_videoScene->removeItem(m_videoItem);
    else
        m_videoScene->addItem(m_videoItem);
}

PlatformLayer* MediaPlayerPrivate::platformLayer() const
{
    return m_composited ? m_videoItem : 0;
}
#endif

PlatformMedia MediaPlayerPrivate::platformMedia() const
{
    PlatformMedia pm;
    pm.type = PlatformMedia::QtMediaPlayerType;
    pm.media.qtMediaPlayer = const_cast<MediaPlayerPrivate*>(this);
    return pm;
}

} // namespace WebCore

#include "moc_MediaPlayerPrivateQt.cpp"
