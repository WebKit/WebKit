/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef MediaPlayerPrivateGStreamer_h
#define MediaPlayerPrivateGStreamer_h

#if ENABLE(VIDEO)

#include "MediaPlayer.h"
#include "Timer.h"

#include <gtk/gtk.h>

typedef struct _GstElement GstElement;
typedef struct _GstMessage GstMessage;
typedef struct _GstBus GstBus;

namespace WebCore {

    class GraphicsContext;
    class IntSize;
    class IntRect;
    class String;

    gboolean mediaPlayerPrivateErrorCallback(GstBus* bus, GstMessage* message, gpointer data);
    gboolean mediaPlayerPrivateEOSCallback(GstBus* bus, GstMessage* message, gpointer data);
    gboolean mediaPlayerPrivateStateCallback(GstBus* bus, GstMessage* message, gpointer data);

    class MediaPlayerPrivate : Noncopyable
    {
    friend gboolean mediaPlayerPrivateErrorCallback(GstBus* bus, GstMessage* message, gpointer data);
    friend gboolean mediaPlayerPrivateEOSCallback(GstBus* bus, GstMessage* message, gpointer data);
    friend gboolean mediaPlayerPrivateStateCallback(GstBus* bus, GstMessage* message, gpointer data);

    public:
        MediaPlayerPrivate(MediaPlayer*);
        ~MediaPlayerPrivate();

        IntSize naturalSize();
        bool hasVideo();

        void load(String url);
        void cancelLoad();

        void play();
        void pause();

        bool paused() const;
        bool seeking() const;

        float duration();
        float currentTime() const;
        void seek(float);
        void setEndTime(float);

        void setRate(float);
        void setVolume(float);
        void setMuted(bool);

        int dataRate() const;

        MediaPlayer::NetworkState networkState();
        MediaPlayer::ReadyState readyState();

        float maxTimeBuffered();
        float maxTimeSeekable();
        unsigned bytesLoaded();
        bool totalBytesKnown();
        unsigned totalBytes();

        void setVisible(bool);
        void setRect(const IntRect&);

        void loadStateChanged();
        void rateChanged();
        void sizeChanged();
        void timeChanged();
        void volumeChanged();
        void didEnd();
        void loadingFailed();

        void repaint();
        void paint(GraphicsContext*, const IntRect&);
        static void getSupportedTypes(HashSet<String>&);
        static bool isAvailable() { return true; }

    private:

        void updateStates();
        void cancelSeek();
        void endPointTimerFired(Timer<MediaPlayerPrivate>*);
        float maxTimeLoaded();
        void startEndPointTimerIfNeeded();

        void createGSTPlayBin(String url);

    private:
        MediaPlayer* m_player;
        GstElement* m_playBin;
        GstElement* m_videoSink;
        GstElement* m_source;
        float m_rate;
        float m_endTime;
        bool m_isEndReached;
        double m_volume;
        MediaPlayer::NetworkState m_networkState;
        MediaPlayer::ReadyState m_readyState;
        bool m_startedPlaying;
        bool m_isStreaming;
        IntRect m_rect;
        bool m_visible;
        cairo_surface_t* m_surface;
    };
}

#endif
#endif
