/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef MediaPlayerPrivatePhonon_h
#define MediaPlayerPrivatePhonon_h

#include "MediaPlayer.h"
#include <wtf/Noncopyable.h>

#include <QObject>
#include <phononnamespace.h>

QT_BEGIN_NAMESPACE
class QWidget;
class QUrl;

namespace Phonon {
    class MediaObject;
    class VideoWidget;
    class AudioOutput;
    class MediaSource;
}
QT_END_NAMESPACE

namespace WebCore {

    class MediaPlayerPrivate : public QObject, Noncopyable {

        Q_OBJECT

    public:
        MediaPlayerPrivate(MediaPlayer*);
        ~MediaPlayerPrivate();

        // These enums are used for debugging
        Q_ENUMS(ReadyState NetworkState PhononState)

        enum ReadyState {
            DataUnavailable,
            CanShowCurrentFrame,
            CanPlay,
            CanPlayThrough
        };

        enum NetworkState {
            Empty,
            LoadFailed,
            Loading,
            LoadedMetaData,
            LoadedFirstFrame,
            Loaded
        };

        enum PhononState {
            LoadingState,
            StoppedState,
            PlayingState,
            BufferingState,
            PausedState,
            ErrorState
        };

        IntSize naturalSize() const;
        bool hasVideo() const;

        void load(String url);
        void cancelLoad();

        void play();
        void pause();

        bool paused() const;
        bool seeking() const;

        float duration() const;
        float currentTime() const;
        void seek(float);
        void setEndTime(float);

        void setRate(float);
        void setVolume(float);
        void setMuted(bool);

        int dataRate() const;

        MediaPlayer::NetworkState networkState() const;
        MediaPlayer::ReadyState readyState() const;

        float maxTimeBuffered() const;
        float maxTimeSeekable() const;
        unsigned bytesLoaded() const;
        bool totalBytesKnown() const;
        unsigned totalBytes() const;

        void setVisible(bool);
        void setRect(const IntRect&);

        void paint(GraphicsContext*, const IntRect&);
        static void getSupportedTypes(HashSet<String>&);
        static bool isAvailable()   { return true; }

    protected:
        bool eventFilter(QObject*, QEvent*);

    private slots:
        void stateChanged(Phonon::State, Phonon::State);
        void metaDataChanged();
        void seekableChanged(bool);
        void hasVideoChanged(bool);
        void bufferStatus(int);
        void finished();
        void currentSourceChanged(const Phonon::MediaSource&);
        void aboutToFinish();
        void totalTimeChanged(qint64);

    private:
        void updateStates();

        MediaPlayer* m_player;

        MediaPlayer::NetworkState m_networkState;
        MediaPlayer::ReadyState m_readyState;

        Phonon::MediaObject* m_mediaObject;
        Phonon::VideoWidget* m_videoWidget;
        Phonon::AudioOutput* m_audioOutput;

        bool m_isVisible;
    };
}

#endif // MediaPlayerPrivatePhonon_h
