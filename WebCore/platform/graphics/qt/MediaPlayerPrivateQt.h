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

#ifndef MediaPlayerPrivateQt_h
#define MediaPlayerPrivateQt_h

#include "MediaPlayerPrivate.h"

#include <QMediaPlayer>
#include <QObject>

QT_BEGIN_NAMESPACE
class QMediaPlayerControl;
class QGraphicsVideoItem;
class QGraphicsScene;
QT_END_NAMESPACE

namespace WebCore {

class MediaPlayerPrivate : public QObject, public MediaPlayerPrivateInterface {

    Q_OBJECT

public:
    static MediaPlayerPrivateInterface* create(MediaPlayer* player);
    ~MediaPlayerPrivate();

    static void registerMediaEngine(MediaEngineRegistrar);
    static void getSupportedTypes(HashSet<String>&);
    static MediaPlayer::SupportsType supportsType(const String&, const String&);
    static bool isAvailable() { return true; }

    bool hasVideo() const;
    bool hasAudio() const;

    void load(const String &url);
    void cancelLoad();

    void play();
    void pause();

    bool paused() const;
    bool seeking() const;

    float duration() const;
    float currentTime() const;
    void seek(float);

    void setRate(float);
    void setVolume(float);

    bool supportsMuting() const;
    void setMuted(bool);

    MediaPlayer::NetworkState networkState() const;
    MediaPlayer::ReadyState readyState() const;

    PassRefPtr<TimeRanges> buffered() const;
    float maxTimeSeekable() const;
    unsigned bytesLoaded() const;
    unsigned totalBytes() const;

    void setVisible(bool);

    IntSize naturalSize() const;
    void setSize(const IntSize&);

    void paint(GraphicsContext*, const IntRect&);

    bool supportsFullscreen() const { return false; }

#if USE(ACCELERATED_COMPOSITING)
    // whether accelerated rendering is supported by the media engine for the current media.
    virtual bool supportsAcceleratedRendering() const { return true; }
    // called when the rendering system flips the into or out of accelerated rendering mode.
    virtual void acceleratedRenderingStateChanged();
    // returns an object that can be directly composited via GraphicsLayerQt (essentially a QGraphicsItem*)
    virtual PlatformLayer* platformLayer() const;
#endif

private slots:
    void mediaStatusChanged(QMediaPlayer::MediaStatus);
    void handleError(QMediaPlayer::Error);
    void stateChanged(QMediaPlayer::State);
    void nativeSizeChanged(const QSizeF&);
    void queuedSeekTimeout();
    void seekTimeout();
    void positionChanged(qint64);
    void durationChanged(qint64);
    void volumeChanged(int);
    void mutedChanged(bool);
    void repaint();

private:
    void updateStates();

private:
    MediaPlayerPrivate(MediaPlayer*);

    MediaPlayer* m_player;
    QMediaPlayer* m_mediaPlayer;
    QMediaPlayerControl* m_mediaPlayerControl;
    QGraphicsVideoItem* m_videoItem;
    QGraphicsScene* m_videoScene;

    mutable MediaPlayer::NetworkState m_networkState;
    mutable MediaPlayer::ReadyState m_readyState;

    IntSize m_currentSize;
    bool m_isVisible;
    bool m_isSeeking;
    bool m_composited;
    qint64 m_queuedSeek;
};
}

#endif // MediaPlayerPrivateQt_h
