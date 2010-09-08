/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009, 2010 Igalia S.L
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

#include <wtf/Forward.h>
#include "MediaPlayerPrivate.h"
#include "Timer.h"

#include <glib.h>
#include <gst/gst.h>

typedef struct _WebKitVideoSink WebKitVideoSink;
typedef struct _GstBuffer GstBuffer;
typedef struct _GstMessage GstMessage;
typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;

namespace WebCore {

class GraphicsContext;
class IntSize;
class IntRect;
class GStreamerGWorld;

gboolean mediaPlayerPrivateMessageCallback(GstBus* bus, GstMessage* message, gpointer data);
void mediaPlayerPrivateVolumeChangedCallback(GObject* element, GParamSpec* pspec, gpointer data);
void mediaPlayerPrivateMuteChangedCallback(GObject* element, GParamSpec* pspec, gpointer data);
void mediaPlayerPrivateSourceChangedCallback(GObject* element, GParamSpec* pspec, gpointer data);

class MediaPlayerPrivateGStreamer : public MediaPlayerPrivateInterface {
        friend gboolean mediaPlayerPrivateMessageCallback(GstBus* bus, GstMessage* message, gpointer data);
        friend void mediaPlayerPrivateRepaintCallback(WebKitVideoSink*, GstBuffer* buffer, MediaPlayerPrivateGStreamer* playerPrivate);
        friend void mediaPlayerPrivateSourceChangedCallback(GObject* element, GParamSpec* pspec, gpointer data);

        public:
            static void registerMediaEngine(MediaEngineRegistrar);

            IntSize naturalSize() const;
            bool hasVideo() const;
            bool hasAudio() const;

            void load(const String &url);
            void commitLoad();
            void cancelLoad();
            bool loadNextLocation();

            void prepareToPlay();
            void play();
            void pause();

            bool paused() const;
            bool seeking() const;

            float duration() const;
            float currentTime() const;
            void seek(float);

            void setRate(float);

            void setVolume(float);
            void volumeChanged();
            void volumeChangedTimerFired(Timer<MediaPlayerPrivateGStreamer>*);

            bool supportsMuting() const;
            void setMuted(bool);
            void muteChanged();
            void muteChangedTimerFired(Timer<MediaPlayerPrivateGStreamer>*);

            void setPreload(MediaPlayer::Preload);
            void fillTimerFired(Timer<MediaPlayerPrivateGStreamer>*);

            MediaPlayer::NetworkState networkState() const;
            MediaPlayer::ReadyState readyState() const;

            PassRefPtr<TimeRanges> buffered() const;
            float maxTimeSeekable() const;
            unsigned bytesLoaded() const;
            unsigned totalBytes() const;

            void setVisible(bool);
            void setSize(const IntSize&);

            void mediaLocationChanged(GstMessage*);
            void loadStateChanged();
            void sizeChanged();
            void timeChanged();
            void didEnd();
            void durationChanged();
            void loadingFailed(MediaPlayer::NetworkState);

            void repaint();
            void paint(GraphicsContext*, const IntRect&);

            bool hasSingleSecurityOrigin() const;

            bool supportsFullscreen() const;
            PlatformMedia platformMedia() const;

            GstElement* pipeline() const { return m_playBin; }
            bool pipelineReset() const { return m_resetPipeline; }

        private:
            MediaPlayerPrivateGStreamer(MediaPlayer*);
            ~MediaPlayerPrivateGStreamer();

            static MediaPlayerPrivateInterface* create(MediaPlayer* player);

            static void getSupportedTypes(HashSet<String>&);
            static MediaPlayer::SupportsType supportsType(const String& type, const String& codecs);
            static bool isAvailable();

            void cacheDuration();
            void updateStates();
            void cancelSeek();
            void endPointTimerFired(Timer<MediaPlayerPrivateGStreamer>*);
            float maxTimeLoaded() const;
            void startEndPointTimerIfNeeded();

            void createGSTPlayBin();
            bool changePipelineState(GstState state);

            void processBufferingStats(GstMessage* message);

        private:
            MediaPlayer* m_player;
            GstElement* m_playBin;
            GstElement* m_webkitVideoSink;
            GstElement* m_videoSinkBin;
            GstElement* m_fpsSink;
            GstElement* m_source;
            GstClockTime m_seekTime;
            bool m_changingRate;
            float m_endTime;
            bool m_isEndReached;
            MediaPlayer::NetworkState m_networkState;
            MediaPlayer::ReadyState m_readyState;
            mutable bool m_isStreaming;
            IntSize m_size;
            GstBuffer* m_buffer;
            GstStructure* m_mediaLocations;
            int m_mediaLocationCurrentIndex;
            bool m_resetPipeline;
            bool m_paused;
            bool m_seeking;
            bool m_buffering;
            float m_playbackRate;
            bool m_errorOccured;
            gfloat m_mediaDuration;
            bool m_startedBuffering;
            Timer<MediaPlayerPrivateGStreamer> m_fillTimer;
            float m_maxTimeLoaded;
            int m_bufferingPercentage;
            MediaPlayer::Preload m_preload;
            bool m_delayingLoad;
            bool m_mediaDurationKnown;
            RefPtr<GStreamerGWorld> m_gstGWorld;
    };
}

#endif // ENABLE(VIDEO)
#endif
