/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2009 Apple Inc. All rights reserved.

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
#include "MediaPlayerPrivatePhonon.h"

#include <limits>

#include "FrameView.h"
#include "GraphicsContext.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "TimeRanges.h"
#include "Widget.h"
#include <wtf/HashSet.h>
#include <wtf/text/CString.h>

#include <QDebug>
#include <QEvent>
#include <QMetaEnum>
#include <QPainter>
#include <QWidget>
#include <QUrl>

#include <phonon/audiooutput.h>
#include <phonon/backendcapabilities.h>
#include <phonon/path.h>
#include <phonon/mediaobject.h>
#include <phonon/videowidget.h>

using namespace Phonon;

#define LOG_MEDIAOBJECT() (LOG(Media, "%s", debugMediaObject(this, *m_mediaObject).constData()))

#if !LOG_DISABLED
static QByteArray debugMediaObject(WebCore::MediaPlayerPrivatePhonon* mediaPlayer, const MediaObject& mediaObject)
{
    QByteArray byteArray;
    QTextStream stream(&byteArray);

    const QMetaObject* metaObj = mediaPlayer->metaObject();
    QMetaEnum phononStates = metaObj->enumerator(metaObj->indexOfEnumerator("PhononState"));

    stream << "debugMediaObject -> Phonon::MediaObject(";
    stream << "State: " << phononStates.valueToKey(mediaObject.state());
    stream << " | Current time: " << mediaObject.currentTime();
    stream << " | Remaining time: " << mediaObject.remainingTime();
    stream << " | Total time: " << mediaObject.totalTime();
    stream << " | Meta-data: ";
    QMultiMap<QString, QString> map = mediaObject.metaData();
    for (QMap<QString, QString>::const_iterator it = map.constBegin();
        it != map.constEnd(); ++it) {
        stream << "(" << it.key() << ", " << it.value() << ")";
    }
    stream << " | Has video: " << mediaObject.hasVideo();
    stream << " | Is seekable: " << mediaObject.isSeekable();
    stream << ")";

    stream.flush();

    return byteArray;
}
#endif

using namespace WTF;

namespace WebCore {

MediaPlayerPrivatePhonon::MediaPlayerPrivatePhonon(MediaPlayer* player)
    : m_player(player)
    , m_networkState(MediaPlayer::Empty)
    , m_readyState(MediaPlayer::HaveNothing)
    , m_mediaObject(new MediaObject())
    , m_videoWidget(new VideoWidget(0))
    , m_audioOutput(new AudioOutput())
    , m_isVisible(false)
{
    // Hint to Phonon to disable overlay painting
    m_videoWidget->setAttribute(Qt::WA_DontShowOnScreen);
    m_videoWidget->setAttribute(Qt::WA_QuitOnClose, false);

    createPath(m_mediaObject, m_videoWidget);
    createPath(m_mediaObject, m_audioOutput);

    // Make sure we get updates for each frame
    m_videoWidget->installEventFilter(this);
    foreach (QWidget* widget, m_videoWidget->findChildren<QWidget*>())
        widget->installEventFilter(this);

    connect(m_mediaObject, SIGNAL(stateChanged(Phonon::State,Phonon::State)),
            this, SLOT(stateChanged(Phonon::State,Phonon::State)));
    connect(m_mediaObject, SIGNAL(metaDataChanged()), this, SLOT(metaDataChanged()));
    connect(m_mediaObject, SIGNAL(seekableChanged(bool)), this, SLOT(seekableChanged(bool)));
    connect(m_mediaObject, SIGNAL(hasVideoChanged(bool)), this, SLOT(hasVideoChanged(bool)));
    connect(m_mediaObject, SIGNAL(bufferStatus(int)), this, SLOT(bufferStatus(int)));
    connect(m_mediaObject, SIGNAL(finished()), this, SLOT(finished()));
    connect(m_mediaObject, SIGNAL(currentSourceChanged(Phonon::MediaSource)),
            this, SLOT(currentSourceChanged(Phonon::MediaSource)));
    connect(m_mediaObject, SIGNAL(aboutToFinish()), this, SLOT(aboutToFinish()));
    connect(m_mediaObject, SIGNAL(totalTimeChanged(qint64)), this, SLOT(totalTimeChanged(qint64)));
}

MediaPlayerPrivateInterface* MediaPlayerPrivatePhonon::create(MediaPlayer* player)
{
    return new MediaPlayerPrivatePhonon(player);
}

void MediaPlayerPrivatePhonon::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar(create, getSupportedTypes, supportsType, 0, 0, 0);
}


MediaPlayerPrivatePhonon::~MediaPlayerPrivatePhonon()
{
    LOG(Media, "MediaPlayerPrivatePhonon::dtor deleting videowidget");
    m_videoWidget->close();
    delete m_videoWidget;
    m_videoWidget = 0;

    LOG(Media, "MediaPlayerPrivatePhonon::dtor deleting audiooutput");
    delete m_audioOutput;
    m_audioOutput = 0;

    LOG(Media, "MediaPlayerPrivatePhonon::dtor deleting mediaobject");
    delete m_mediaObject;
    m_mediaObject = 0;
}

HashSet<String>& MediaPlayerPrivatePhonon::supportedTypesCache()
{
    static HashSet<String> supportedTypes;
    if (!supportedTypes.isEmpty())
        return supportedTypes;

    // FIXME: we should rebuild the MIME type cache every time the backend is changed,
    // however, this would have no effect on MIMETypeRegistry anyway, because it
    // pulls this data only once.

    QStringList types = Phonon::BackendCapabilities::availableMimeTypes();
    foreach (const QString& type, types) {
        QString first = type.split(QLatin1Char('/')).at(0);

        // We're only interested in types which are not supported by WebCore itself.
        if (first != QLatin1String("video")
            && first != QLatin1String("audio")
            && first != QLatin1String("application"))
            continue;
        if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
            continue;

        supportedTypes.add(String(type));
    }

    // These formats are supported by GStreamer, but not correctly advertised.
    if (supportedTypes.contains(String("video/x-h264"))
        || supportedTypes.contains(String("audio/x-m4a"))) {
        supportedTypes.add(String("video/mp4"));
        supportedTypes.add(String("audio/aac"));
    }

    if (supportedTypes.contains(String("video/x-theora")))
        supportedTypes.add(String("video/ogg"));

    if (supportedTypes.contains(String("audio/x-vorbis")))
        supportedTypes.add(String("audio/ogg"));

    if (supportedTypes.contains(String("audio/x-wav")))
        supportedTypes.add(String("audio/wav"));

    return supportedTypes;
}

void MediaPlayerPrivatePhonon::getSupportedTypes(HashSet<String>& types)
{
    types = supportedTypesCache();
}

MediaPlayer::SupportsType MediaPlayerPrivatePhonon::supportsType(const String& type, const String& codecs)
{
    if (type.isEmpty())
        return MediaPlayer::IsNotSupported;

    if (supportedTypesCache().contains(type))
        return codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported;
    return MediaPlayer::IsNotSupported;
}

bool MediaPlayerPrivatePhonon::hasVideo() const
{
    bool hasVideo = m_mediaObject->hasVideo();
    LOG(Media, "MediaPlayerPrivatePhonon::hasVideo() -> %s", hasVideo ? "true" : "false");
    return hasVideo;
}

bool MediaPlayerPrivatePhonon::hasAudio() const
{
    // FIXME: Phonon::MediaObject does not have such a hasAudio() function
    bool hasAudio = true;
    LOG(Media, "MediaPlayerPrivatePhonon::hasAudio() -> %s", hasAudio ? "true" : "false");
    return hasAudio;
}

void MediaPlayerPrivatePhonon::load(const String& url)
{
    LOG(Media, "MediaPlayerPrivatePhonon::load(\"%s\")", url.utf8().data());

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

    m_mediaObject->setCurrentSource(QUrl(url));
    m_audioOutput->setVolume(m_player->volume());
    setVisible(m_player->visible());
}

void MediaPlayerPrivatePhonon::cancelLoad()
{
    notImplemented();
}


void MediaPlayerPrivatePhonon::play()
{
    LOG(Media, "MediaPlayerPrivatePhonon::play()");
    m_mediaObject->play();
}

void MediaPlayerPrivatePhonon::pause()
{
    LOG(Media, "MediaPlayerPrivatePhonon::pause()");
    m_mediaObject->pause();
}


bool MediaPlayerPrivatePhonon::paused() const
{
    bool paused = m_mediaObject->state() == Phonon::PausedState;
    LOG(Media, "MediaPlayerPrivatePhonon::paused() --> %s", paused ? "true" : "false");
    return paused;
}

void MediaPlayerPrivatePhonon::seek(float position)
{
    LOG(Media, "MediaPlayerPrivatePhonon::seek(%f)", position);

    if (!m_mediaObject->isSeekable())
        return;

    if (position > duration())
        position = duration();

    m_mediaObject->seek(position * 1000.0f);
}

bool MediaPlayerPrivatePhonon::seeking() const
{
    return false;
}

float MediaPlayerPrivatePhonon::duration() const
{
    if (m_readyState < MediaPlayer::HaveMetadata)
        return 0.0f;

    float duration = m_mediaObject->totalTime() / 1000.0f;

    if (duration == 0.0f) // We are streaming
        duration = std::numeric_limits<float>::infinity();

    LOG(Media, "MediaPlayerPrivatePhonon::duration() --> %f", duration);
    return duration;
}

float MediaPlayerPrivatePhonon::currentTime() const
{
    float currentTime = m_mediaObject->currentTime() / 1000.0f;

    LOG(Media, "MediaPlayerPrivatePhonon::currentTime() --> %f", currentTime);
    return currentTime;
}

PassRefPtr<TimeRanges> MediaPlayerPrivatePhonon::buffered() const
{
    notImplemented();
    return TimeRanges::create();
}

float MediaPlayerPrivatePhonon::maxTimeSeekable() const
{
    notImplemented();
    return 0.0f;
}

unsigned MediaPlayerPrivatePhonon::bytesLoaded() const
{
    notImplemented();
    return 0;
}

unsigned MediaPlayerPrivatePhonon::totalBytes() const
{
    //notImplemented();
    return 0;
}

void MediaPlayerPrivatePhonon::setRate(float)
{
    notImplemented();
}

void MediaPlayerPrivatePhonon::setVolume(float volume)
{
    LOG(Media, "MediaPlayerPrivatePhonon::setVolume()");
    m_audioOutput->setVolume(volume);
}

void MediaPlayerPrivatePhonon::setMuted(bool muted)
{
    LOG(Media, "MediaPlayerPrivatePhonon::setMuted()");
    m_audioOutput->setMuted(muted);
}

MediaPlayer::NetworkState MediaPlayerPrivatePhonon::networkState() const
{
    const QMetaObject* metaObj = this->metaObject();
    QMetaEnum networkStates = metaObj->enumerator(metaObj->indexOfEnumerator("NetworkState"));
    LOG(Media, "MediaPlayerPrivatePhonon::networkState() --> %s", networkStates.valueToKey(m_networkState));
    return m_networkState;
}

MediaPlayer::ReadyState MediaPlayerPrivatePhonon::readyState() const
{
    const QMetaObject* metaObj = this->metaObject();
    QMetaEnum readyStates = metaObj->enumerator(metaObj->indexOfEnumerator("ReadyState"));
    LOG(Media, "MediaPlayerPrivatePhonon::readyState() --> %s", readyStates.valueToKey(m_readyState));
    return m_readyState;
}

void MediaPlayerPrivatePhonon::updateStates()
{
    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;

    Phonon::State phononState = m_mediaObject->state();

    if (phononState == Phonon::StoppedState) {
        if (m_readyState < MediaPlayer::HaveMetadata) {
            m_networkState = MediaPlayer::Loading; // FIXME: should this be MediaPlayer::Idle?
            m_readyState = MediaPlayer::HaveMetadata;
            m_mediaObject->pause();
        }
    } else if (phononState == Phonon::PausedState) {
        m_networkState = MediaPlayer::Loaded;
        m_readyState = MediaPlayer::HaveEnoughData;
    } else if (phononState == Phonon::ErrorState) {
         if (!m_mediaObject || m_mediaObject->errorType() == Phonon::FatalError) {
             // FIXME: is it possile to differentiate between different types of errors
             m_networkState = MediaPlayer::NetworkError;
             m_readyState = MediaPlayer::HaveNothing;
             cancelLoad();
         } else
             m_mediaObject->pause();
    }

    if (seeking())
        m_readyState = MediaPlayer::HaveNothing;

    if (m_networkState != oldNetworkState) {
        const QMetaObject* metaObj = this->metaObject();
        QMetaEnum networkStates = metaObj->enumerator(metaObj->indexOfEnumerator("NetworkState"));
        LOG(Media, "Network state changed from '%s' to '%s'",
                networkStates.valueToKey(oldNetworkState),
                networkStates.valueToKey(m_networkState));
        m_player->networkStateChanged();
    }

    if (m_readyState != oldReadyState) {
        const QMetaObject* metaObj = this->metaObject();
        QMetaEnum readyStates = metaObj->enumerator(metaObj->indexOfEnumerator("ReadyState"));
        LOG(Media, "Ready state changed from '%s' to '%s'",
                readyStates.valueToKey(oldReadyState),
                readyStates.valueToKey(m_readyState));
        m_player->readyStateChanged();
    }
}

void MediaPlayerPrivatePhonon::setVisible(bool visible)
{
    m_isVisible = visible;
    LOG(Media, "MediaPlayerPrivatePhonon::setVisible(%s)", visible ? "true" : "false");

    m_videoWidget->setVisible(m_isVisible);
}

void MediaPlayerPrivatePhonon::setSize(const IntSize& newSize)
{
    if (!m_videoWidget)
        return;

    LOG(Media, "MediaPlayerPrivatePhonon::setSize(%d,%d)",
                newSize.width(), newSize.height());

    QRect currentRect = m_videoWidget->rect();

    if (newSize.width() != currentRect.width() || newSize.height() != currentRect.height())
        m_videoWidget->resize(newSize.width(), newSize.height());
}

IntSize MediaPlayerPrivatePhonon::naturalSize() const
{
    if (!hasVideo()) {
        LOG(Media, "MediaPlayerPrivatePhonon::naturalSize() -> %dx%d",
                    0, 0);
        return IntSize();
    }

    if (m_readyState < MediaPlayer::HaveMetadata) {
        LOG(Media, "MediaPlayerPrivatePhonon::naturalSize() -> %dx%d",
                           0, 0);
        return IntSize();
    }

    QSize videoSize = m_videoWidget->sizeHint();
    IntSize naturalSize(videoSize.width(), videoSize.height());
    LOG(Media, "MediaPlayerPrivatePhonon::naturalSize() -> %dx%d",
            naturalSize.width(), naturalSize.height());
    return naturalSize;
}

bool MediaPlayerPrivatePhonon::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::UpdateRequest)
        m_player->repaint();

    return QObject::eventFilter(obj, event);
}

void MediaPlayerPrivatePhonon::paint(GraphicsContext* graphicsContect, const IntRect& rect)
{
    if (graphicsContect->paintingDisabled())
        return;

    if (!m_isVisible)
        return;

    QPainter* painter = graphicsContect->platformContext();

    painter->fillRect(rect, Qt::black);

    m_videoWidget->render(painter, QPoint(rect.x(), rect.y()),
            QRegion(0, 0, rect.width(), rect.height()));
}

// ====================== Phonon::MediaObject signals ======================

void MediaPlayerPrivatePhonon::stateChanged(Phonon::State newState, Phonon::State oldState)
{
    const QMetaObject* metaObj = this->metaObject();
    QMetaEnum phononStates = metaObj->enumerator(metaObj->indexOfEnumerator("PhononState"));
    LOG(Media, "MediaPlayerPrivatePhonon::stateChanged(newState=%s, oldState=%s)",
            phononStates.valueToKey(newState), phononStates.valueToKey(oldState));

    updateStates();
}

void MediaPlayerPrivatePhonon::metaDataChanged()
{
    LOG(Media, "MediaPlayerPrivatePhonon::metaDataChanged()");
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivatePhonon::seekableChanged(bool)
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivatePhonon::hasVideoChanged(bool hasVideo)
{
    LOG(Media, "MediaPlayerPrivatePhonon::hasVideoChanged(%s)", hasVideo ? "true" : "false");
}

void MediaPlayerPrivatePhonon::bufferStatus(int)
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivatePhonon::finished()
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivatePhonon::currentSourceChanged(const Phonon::MediaSource&)
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivatePhonon::aboutToFinish()
{
    notImplemented();
    LOG_MEDIAOBJECT();
}

void MediaPlayerPrivatePhonon::totalTimeChanged(qint64 totalTime)
{
#if OS(WINDOWS)
    LOG(Media, "MediaPlayerPrivatePhonon::totalTimeChanged(%I64d)", totalTime);
#else
    LOG(Media, "MediaPlayerPrivatePhonon::totalTimeChanged(%lld)", totalTime);
#endif
    LOG_MEDIAOBJECT();
}

} // namespace WebCore

#include "moc_MediaPlayerPrivatePhonon.cpp"
